// vim:ts=2:et
//============================================================================//
//                             "ThreadPool.hpp":                              //
//                            Generic Thread Pool                             //
//============================================================================//
#pragma once
#include <boost/circular_buffer.hpp>
#include <boost/core/noncopyable.hpp>
#include <pthread.h>
#include <stdexcept>
#include <type_traits>
#include <cassert>
#include <iostream>

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
  private:
    //------------------------------------------------------------------------//
    // Data Flds:                                                             //
    //------------------------------------------------------------------------//
    // {WorkItem, PtrWhereToPutTheRes>:
    using CB = boost::circular_buffer<std::pair<WorkItem, Res*>>;
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
        auto job = m_buff.front();
        m_buff.pop_front();

        // And only now unlick the Mutex:
        rc = pthread_mutex_unlock(&m_mutex);
        if (rc != 0)
          throw std::runtime_error
            ("ThreadPool::ThreadBody: Mutex unlock failed");

        // We have now got the WorkItem, process it via the actual "Func":
        // For syntactic correctness in all cases, need this "constexpr if":
        // Catch exceptions locally to prevent exit from the main loop:
        try
        {
          if constexpr(std::is_void_v<Res>) // Res is void
          {
            assert(job.second == nullptr);  // No value to return
            (*m_func)(job.first);
          }
          else                              // Res is NOT void
          {
            Res res = (*m_func)(job.first);
            // Save the res vis the Submitter-specified ptr (if not NULL):
            if (job.second != nullptr)
              *job.second = res;
          }
        }
        catch(...){}  // Catch and ignore ANY exceptions
      }
      __builtin_unreachable();
    }

  public:
    //------------------------------------------------------------------------//
    // "Submit": Used by Clients to submit a Job={WorkItem,ResPtr}:           //
    //------------------------------------------------------------------------//
    // Returns "true" iff submission successful (i.e. the Buff was not full):
    //
    bool Submit(WorkItem a_wi, Res* a_res = nullptr)
    {
      int rc  = pthread_mutex_lock(&m_mutex);
      if (rc != 0)
        throw std::runtime_error("ThreadPool::Sunmit: Mutex lock failed");

      // Now check if there is space left in the Buff:
      if (m_buff.full())
      {
        rc = pthread_mutex_unlock(&m_mutex);
        assert(rc == 0);
        return false; // No space!
      }
      // Do submit (at the back of the circular queue) and unlock the mutex:
      m_buff.push_back({a_wi, a_res});
      rc = pthread_mutex_unlock(&m_mutex);

      // Only after unlicking, signal the new condition to the thread(s)
      // (otherwise, subsequent mutex lock in pthread_cond_wait() would fail):
      pthread_cond_signal(&m_cv);

      // Success!
      return true;
    }
  };
}
