// vim:ts=2:et
//============================================================================//
//                                 "ThreadPool.hpp":                          //
//============================================================================//
// Threads perform computations: ArgT -> ResT
//
#pragma once

#include "CircularBuffer.hpp"
#include <pthread.h>
#include <utility>

template<typename ArgT, typename ResT>
class ThreadPool
{
private:
  //==========================================================================//
  // Data Flds:                                                               //
  //==========================================================================//
  int          const    m_M;       // Number of Theads in Pool
  pthread_t*   const    m_threads; // Threads themselves

  // Job to be submittted to ThreadPool is a pair of ArgT and a ptr to the area
  // where the result is to be stored (may be NULL if no need to return a res):
  //
  using JobT = std::pair<ArgT, ResT*>;
  CircularBuffer<JobT>  m_circ;

  // The wrapper function which is required by the C-level PThread API. Will
  // call the actual "ThreadBody" method of this class:
  typedef void* (*WrapperFuncT) (void*);

  WrapperFuncT  const   m_wrapper;

  // The action which actually transforms a given ArgT into ResT:
  typedef ResT  (*ActionFuncT)(ArgT);
  ActionFuncT   const   m_action;

  // Mutex and CondVar for the critical section:
  pthread_mutex_t       m_mutex;
  pthread_cond_t        m_condVar;

public:
  //==========================================================================//
  // Methods:                                                                 //
  //==========================================================================//
  //--------------------------------------------------------------------------//
  // Non-Default Ctor:                                                        //
  //--------------------------------------------------------------------------//
  // a_M: Number of Threads
  // a_N: Buffer Capacity:
  //
  ThreadPool
  (
    int          a_M,
    int          a_N,
    WrapperFuncT a_wrapper,
    ActionFuncT  a_action
  )
  : m_M      (a_M),
    m_threads(new pthread_t[m_M]),
    m_circ   (a_N),
    m_wrapper(a_wrapper),
    m_action (a_action),
    m_mutex  (PTHREAD_MUTEX_INITIALIZER),
    m_condVar(PTHREAD_COND_INITIALIZER)
  {
    assert(m_M > 0 && m_wrapper != nullptr && m_action != nullptr);

    // Create the actual Threads (with default attrs), passing this obj ptr
    // as the arg:
    for (int i = 0; i < m_M; ++i)
    {
      int    rc = pthread_create(m_threads + i, NULL, m_wrapper, this);
      assert(rc == 0);
    }
  }

  //--------------------------------------------------------------------------//
  // "ThreadBody" (Internal):                                                 //
  //--------------------------------------------------------------------------//
  // Will be called from "m_wrapper":
  // Runs concurently in multiple threads, which all have access to this
  // ThreadPool object:
  //
  void ThreadBody()
  {
    while (true)
    {
      // "m_circ" is an object with shared R/W access between threads, so we
      // need to protect it against concurreny access:
      // CRITICAL SECTION BEGIN:
      int    rc = pthread_mutex_lock(&m_mutex);
      assert(rc == 0);

      // NB: "while" is requied because multiple threads can be released from
      // "pthread_cond_wait", and the CircularBuffer would be empties again:
      //
      if (m_circ.IsEmpty())
        // Wait on the CondVar until get a notification that a new Job is
        // available.
        // Mutex is automatically released while we are sleeping, but locked
        // again when we wake up:
        do
          pthread_cond_wait(&m_condVar, &m_mutex);
        while (m_circ.IsEmpty());

      // Get the next job to do:
      JobT job = m_circ.PopFront();
      rc = pthread_mutex_unlock (&m_mutex);
      assert(rc == 0);
      // CRITICAL SECTION END

      ArgT const& arg = job.first;
      ResT*       res = job.second; // May be NULL

      // Invoke Action to actually perform computations, stote the result at
      // "res" ptr:
      // NB: No problems with concurrent access to "m_action" here, because
      // "m_action" is a stateless function ptr:
      //
      if (res != NULL)
        *res = m_action(arg);
      else
        (void) m_action(arg);
    }
    __builtin_unreachable();
  }

  //--------------------------------------------------------------------------//
  // "SubmitJob":                                                             //
  //--------------------------------------------------------------------------//
  // TODO: Provide a mechanism to notify the Caller of Res being ready:
  //
  void SubmitJob(ArgT const& a_arg, ResT* a_res)
  {
    // NB: Will throw exception of the CircularBuffer is full:
    m_circ.PushBack(std::make_pair(a_arg, a_res));

    // Notify the CondVar on the job submitted:
    int    rc = pthread_cond_signal  (&m_condVar);
    assert(rc == 0);
  }
};
