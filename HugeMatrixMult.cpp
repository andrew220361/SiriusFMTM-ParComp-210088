// vim:ts=2:et
//===========================================================================//
//                             "HugeMatrixMult.cpp":                         //
//   Performance Test for Muti-Threaded Matrix Multiplication and Addition   //
//===========================================================================//
#include "ThreadPool.hpp"
#include <cstdlib>
#include <ctime>
#include <iostream>

namespace
{
  //=========================================================================//
  // "WorkItem" for Matrix Multiplication and Element Summation:             //
  //=========================================================================//
  struct WorkItem
  {
    long            m_N;
    long            m_N2;
    double const*   m_rowA;   // Of size "m_N"
    double const*   m_B;      // Whole "B", of size "m_N2"
    double*         m_rowC;   // Of size "m_N"
  };

  //=========================================================================//
  // "MultAndSum":                                                           //
  //=========================================================================//
  double MultAndSum(WorkItem a_wi)
  {
    double        sum  = 0.0;
    long          N    = a_wi.m_N;
    long          N2   = a_wi.m_N2;
    double const* rowA = a_wi.m_rowA;
    double const* B    = a_wi.m_B;
    double*       rowC = a_wi.m_rowC;

    for (long j = 0; j < N; ++j)
    {
      // Fill in rowC[j]:
      double* rowCj = rowC + j;
      *rowCj = 0.0;

      // Initial offset of B[*,j]:
      double const* colBj = B + j;
      for (long k = 0; k < N; ++k)
      {
        assert(colBj < B + N2);
        (*rowCj) += rowA[k] * (*colBj);
        colBj    += N;
      }

      // Increment the sum:
      sum += (*rowCj);
    }
    return sum;
  }
}

//===========================================================================//
// "main":                                                                   //
//===========================================================================//
int main(int argc, char* argv[])
{
  // Params: MtxSizeN NThreads
  if (argc < 3)
  {
    std::cerr << "Params: MatrixSize NThreads" << std::endl;
    return 1;
  }
  long N = atol(argv[1]);
  int  T = atoi(argv[2]);
  if (N <= 0 || T <= 0)
  {
    std::cerr << "Invalid MatrixSize or NThreads" << std::endl;
    return 1;
  }

  // Create square matrices of size N:
  long    N2   = N*N;
  double* A    = new double[N2];
  double* B    = new double[N2];
  double* C    = new double[N2];
  double* sums = new double[N];    // Sums of C entries for each row

  // Fill in "a" and "b" randomly:
  srand48(long(time(nullptr)));

  for (long n = 0; n < N2; ++n)
  {
    A[n] = drand48();
    B[n] = drand48();
  }

  // Create a ThreadPool:
  // "T" is the number of Threads, "N" is BuffSize:
  //
  SiriusFMTM::ThreadPool<WorkItem, double, decltype(MultAndSum)> TP
    (size_t(T), size_t(N), MultAndSum);

  // Completion flags:
  decltype(TP)::JobStatusE* stats = new decltype(TP)::JobStatusE[N];

  // Will create 1 job per each C row:
  for (long i = 0; i < N; ++i)
  {
    // According to our (C/C++) convention, matrices are stored row-wise:
    // "offI" is the offset of row "i" in "A" and "C":
    long     offI = i * N;
    WorkItem wi     { N, N2, A + offI, B, C + offI };
    double* resI  = sums  + i;   // Sum C[i,*] comes here!
    auto    stat  = stats + i;

    // Submit the WorkItem:
    TP.Submit(wi, resI, stat);
  }

  // Wait for completion of all WorkItems:
  while (true)
  {
    bool allCompleted = true;
    for (long i = 0; allCompleted && i < N; ++i)
      if (stats[i] != decltype(TP)::JobStatusE::Completed)
        allCompleted = false;

    if (allCompleted)
      break;

    // Otherwise, sleep for a short while (1 msec = 10^6 nsec):
    timespec   msec { 0, 1'000'000 };
    nanosleep(&msec, nullptr);
  }

  // All done, make the final sum:
  double total = 0.0;
  for (long i = 0; i < N; ++i)
    total += sums[i];
  std::cout << "N=" << N << ", TotalSum=" << total << std::endl;

  delete[] A;     A     = nullptr;
  delete[] B;     B     = nullptr;
  delete[] C;     C     = nullptr;
  delete[] sums;  sums  = nullptr;
  delete[] stats; stats = nullptr;
  return 0;
}
