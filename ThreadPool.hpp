// vim:ts=2:et
//============================================================================//
//                             "ThreadPool.hpp":                              //
//                            Generic Thread Pool                             //
//============================================================================//
#pragma once
#ifdef   USE_BOOST
#include <boost/circular_buffer.hpp>
#else
#include "CircularBuffer.hpp"
#endif
#include <boost/core/noncopyable.hpp>
#include <pthread.h>
#include <stdexcept>
#include <type_traits>
#include <cassert>
#include <iostream>
#include <vector>

namespace SiriusFMTM
{
  //==========================================================================//
  // "ThreadPool" Class:                                                      //
  //==========================================================================//
  // Func: WorkItem -> Res:
  //
  template<typename WorkItem, typename Res, typename Func>
  class ThreadPool: public boost::noncopyable
  {
  public:
    //------------------------------------------------------------------------//
    // "JobStatusE":                                                          //
    //------------------------------------------------------------------------//
    enum class JobStatusE: int
    {
      UNDEFINED    = 0,
      Queued       = 1,
      InProcessing = 2,
      Completed    = 3,
      Failed       = 4
    };

  private:
    //------------------------------------------------------------------------//
    // "JobDescr":                                                            //
    //------------------------------------------------------------------------//
    struct JobDescr
    {
      // Data Flds:
      WorkItem    m_wi;
      Res*        m_res;
      JobStatusE* m_status;

      // Default Ctor:
      JobDescr()
      : m_wi    (),
        m_res   (nullptr),
        m_status(nullptr)
      {}

      // Non-Default Ctor:
      // "WorkItem" is supposed to be small enough to be passed by copy:
      JobDescr(WorkItem a_wi, Res* a_res, JobStatusE* a_status)
      : m_wi    (a_wi),
        m_res   (a_res),
        m_status(a_status)
      {}
    };

    //------------------------------------------------------------------------//
    // Data Flds:                                                             //
    //------------------------------------------------------------------------//
    // {WorkItem, PtrWhereToPutTheRes>:
#   ifdef USE_BOOST
    using CB = boost::circular_buffer<JobDescr>;
#   else
    using CB = CircularBuffer        <JobDescr>;
#   endif
    std::vector<pthread_t>  m_threads;
    Func const*             m_func;
    CB                      m_buff;
    pthread_mutex_t         m_mutex;
    pthread_cond_t          m_cv;

  public:
    //------------------------------------------------------------------------//
    // Non-Default Ctor:                                                      //
    //------------------------------------------------------------------------//
    ThreadPool(size_t a_pool_sz, size_t a_buff_sz, Func const& a_func)
    : m_threads(a_pool_sz),
      m_func   (&a_func),
      m_buff   (a_buff_sz),  // Initially empty
      m_mutex  (PTHREAD_MUTEX_INITIALIZER),
      m_cv     (PTHREAD_COND_INITIALIZER)
    {
      // Create the Threads, handles will be stored in each "pt" via a Ref:
      for (pthread_t& pt: m_threads)
      {
        // No specific attrs, "this" ptr is the arg for ThreadBody:
        // ThreadPool* is auto-converted to void*:
        int rc  = pthread_create(&pt, nullptr, ThreadBodyS, this);
        if (rc != 0)
          throw std::runtime_error("ThreadPool::Ctor: Thread creation failed");
      }
      // Threads start immediately upon creation
    }

    // Deault Ctor is deleted:
    ThreadPool() = delete;

    //------------------------------------------------------------------------//
    // Dtor:                                                                  //
    //------------------------------------------------------------------------//
    ~ThreadPool()
    {
      // Cancel all Threads:
      for (pthread_t pt: m_threads)
        (void) pthread_cancel(pt);
    }

  private:
    //------------------------------------------------------------------------//
    // "ThreadBodyS":                                                         //
    //------------------------------------------------------------------------//
    // Must be compatible with C type (void*)->(void*), hence "static":
    // This is only a bridge function:
    //
    static void* ThreadBodyS(void* a_this)
    {
      // Will catch all exceptions:
      try
      {
        // "a_this" is actually a ptr to an obj of the "ThreadPool" class:
        // void* must be explicitly converted back to ThreadPool*:
        ThreadPool* obj = reinterpret_cast<ThreadPool*>(a_this);

        // Invoke the actual ThreadBody as a member function:
        assert(obj != nullptr);
        obj->ThreadBody();   // Runs in an infinite loop
      }
      catch (std::exception const& exn)
        { std::cerr << "EXCEPTION: " << exn.what() << std::endl;}

      // We only get here if there was an exception:
      return nullptr;
    }

    //------------------------------------------------------------------------//
    // "ThreadBody": Actual ThreadBody as a member function of this class:    //
    //------------------------------------------------------------------------//
    void ThreadBody()
    {
      // Run in an infinite loop:
      while (true)
      {
        // Obtain the next WorkItem and ResPtr from the Buff:
        // Lock the Mutex to access the Buff shared between Threads:
        int rc  = pthread_mutex_lock(&m_mutex);
        if (rc != 0)
          throw std::runtime_error("ThreadPool::ThreadBody: Mutex lock failed");

        // CRITICAL SECTION BEGIN ===========================================//
        while (m_buff.empty())
        {
          // Will have to wait for a new job to be "Submit"ted:
          rc = pthread_cond_wait(&m_cv, &m_mutex);
          assert(rc == 0);

          // If we got here, the "Submit"ter has called "pthread_cond_signal"
          // and THIS thread has been waken up. But this does NOT guarantee
          // that the Buff is non-empty now, so need to re-check in the inner
          // loop. IMPORTANT: The Mutex is automatically locked again, so the
          // check is safe!
        }
        // If we got here, the Mutex is locked and the Buff is non-empty:
        // Get the front job from the Buff: {WorkItem, ResPtr}:
#       ifdef USE_BOOST
        JobDescr job = m_buff.front();
        m_buff.pop_front();
#       else
        JobDescr job = m_buff.PopFront();
#       endif

        // And only now unlick the Mutex:
        rc = pthread_mutex_unlock(&m_mutex);
        if (rc != 0)
          throw std::runtime_error
            ("ThreadPool::ThreadBody: Mutex unlock failed");
        // CRITICAL SECTION END =============================================//

        // We have now got the WorkItem, process it via the actual "Func":
        // For syntactic correctness in all cases, need this "constexpr if":
        // Catch exceptions locally to prevent exit from the main loop:
        try
        {
          if (job.m_status != nullptr)
            *job.m_status = JobStatusE::InProcessing;

          if constexpr(std::is_void_v<Res>) // Res is void
          {
            assert(job.m_res == nullptr);   // No value to return
            (*m_func)(job.m_wi);
          }
          else                              // Res is NOT void
          {
            Res res = (*m_func)(job.m_wi);
            // Save the res vis the Submitter-specified ptr (if not NULL):
            if (job.m_res != nullptr)
              *job.m_res = res;
          }
          if (job.m_status != nullptr)
            *job.m_status = JobStatusE::Completed;
        }
        catch(...)
        {
          // Mark the job as Failed:
          if (job.m_status != nullptr)
            *job.m_status = JobStatusE::Failed;
        }
      }
      __builtin_unreachable();
    }

  public:
    //------------------------------------------------------------------------//
    // "Submit": Used by Clients to submit a Job={WorkItem,ResPtr}:           //
    //------------------------------------------------------------------------//
    // Returns "true" iff submission successful (i.e. the Buff was not full):
    //
    bool Submit
    (
      WorkItem    a_wi,
      Res*        a_res    = nullptr,
      JobStatusE* a_status = nullptr
    )
    {
      // CRITICAL SECTION BEGIN =============================================//
      int rc  = pthread_mutex_lock(&m_mutex);
      if (rc != 0)
        throw std::runtime_error("ThreadPool::Sunmit: Mutex lock failed");

      // Now check if there is space left in the Buff:
      if (m_buff.full())
      {
        rc = pthread_mutex_unlock(&m_mutex);
        assert(rc == 0);
        // No space!
        if (a_status != nullptr)
          *a_status = JobStatusE::Failed;
        return false;
      }
      // Do submit (at the back of the circular queue) and unlock the mutex:
      m_buff.push_back(JobDescr(a_wi, a_res, a_status));
      rc = pthread_mutex_unlock(&m_mutex);
      // CRITICAL SECTION END ===============================================//

      // Only after unlocking, signal the new condition to the thread(s)
      // (otherwise, subsequent mutex lock in pthread_cond_wait() would fail):
      if (a_status != nullptr)
        *a_status = JobStatusE::Queued;
      pthread_cond_signal(&m_cv);

      // Success!
      return true;
    }
  };
}
