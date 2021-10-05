// vim:ts=2:et
//============================================================================//
//                               "CircularBuffer.hpp":                        //
//                    Templated Class for Circular (Ring) Buffers             //
//============================================================================//
#pragma once

#include <cassert>
#include <stdexcept>

template<typename T>
class CircularBuffer
{
private:
  //==========================================================================//
  // Data Flds:                                                               //
  //==========================================================================//
  int const m_N;        // Buffer capacity
  T*  const m_buff;
  int       m_front;    // Idx of the front entry
  int       m_back;     // Idx of the back  entry
  int       m_count;    // Number of entries currently in the buffer

public:
  //==========================================================================//
  // Consts and Methods:                                                      //
  //==========================================================================//
  //--------------------------------------------------------------------------//
  // Non-Default Ctor:                                                        //
  //--------------------------------------------------------------------------//
  CircularBuffer(int a_N)
  : m_N    (a_N),
    m_buff (new T[m_N]),
    m_front(-1),
    m_back (-1),
    m_count(0)
  {
    assert(m_N > 1 && IsEmpty());
  }

  // To be on a safe side: Disallow Default Ctor and Copy Ctor:
  CircularBuffer()                      = delete;
  CircularBuffer(CircularBuffer const&) = delete;

  //--------------------------------------------------------------------------//
  // Dtor:                                                                    //
  //--------------------------------------------------------------------------//
  ~CircularBuffer()
    { delete[] m_buff; }  // Free the previously-allocated memory

  //--------------------------------------------------------------------------//
  // "IsEmpty": Test for Emptiness:                                           //
  //--------------------------------------------------------------------------//
  // Marked "const" as it does not alter any data flds:
  //
  bool IsEmpty() const
  {
    assert((m_front == -1 || (0 <= m_front && m_front <= m_N-1)) &&
           (m_back  == -1 || (0 <= m_back  && m_back  <= m_N-1)) &&
           0 <= m_count   && m_count <= m_N);

    bool isEmpty = (m_count == 0);

    assert(!isEmpty || (m_front == -1 && m_back == -1));
    return  isEmpty;
  }

  //--------------------------------------------------------------------------//
  // "IsFull":                                                                //
  //--------------------------------------------------------------------------//
  bool IsFull() const
  {
    bool   isFull = (m_count == m_N);
    assert(!(isFull && IsEmpty()));
    return isFull;
  }

  //--------------------------------------------------------------------------//
  // "PushBack":                                                              //
  //--------------------------------------------------------------------------//
  void PushBack(T const& a_t)
  {
    // Trivial Cases:
    if (IsEmpty())
    {
      m_front   = 0;
      m_back    = 0;
      m_buff[0] = a_t;
      m_count   = 1;
      assert(!IsEmpty());
      return;
    }
    if (IsFull())
      throw std::runtime_error("CircularBuffer::PushBack(): IsFull");

    // Generic Case: Neither Empty nor Full:
    ++m_back;
    m_back = m_back % m_N;
    assert(m_back != m_front);     // Because buffer was not full

    // Store the new entry at new back:
    m_buff[m_back] = a_t;
    ++m_count;
  }

  //--------------------------------------------------------------------------//
  // "PopFront":                                                              //  //--------------------------------------------------------------------------//
  // Returns the former front entry:
  //
  T PopFront()
  {
    // Trivial Case:
    if (IsEmpty())
      throw std::runtime_error("CircularBuffer::PopFront(): IsEmpty");

    // Generic Case:
    T res = m_buff[m_front];
    --m_count;
    if (m_count == 0)
    {
      m_front = -1;
      m_back  = -1;
      assert(IsEmpty());
    }
    else
    {
      ++m_front;
      m_front = m_front % m_N;
    }
    return res;
  }
};
