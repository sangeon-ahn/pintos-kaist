/* The main thread acquires a lock.  Then it creates two
   higher-priority threads that block acquiring the lock, causing
   them to donate their priorities to the main thread.  When the
   main thread releases the lock, the other threads should
   acquire it in priority order.

   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func acquire1_thread_func;
static thread_func acquire2_thread_func;

void
test_priority_donate_one (void) 
{
  struct lock lock;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&lock);
  lock_acquire (&lock);//원래 프로세스는 잠금 잠금을 소유
  thread_create ("acquire1", PRI_DEFAULT + 1, acquire1_thread_func, &lock);  //aquire1 프로세스가 원래 프로세스보다 우선순위가 높지만 잠금을 획득하지 못하고 차단되므로 원래 프로세스의 우선순위를 aquire1 우선순위로 올려야함
  msg ("This thread should have priority %d.  Actual priority: %d.",//출력결과
       PRI_DEFAULT + 1, thread_get_priority ());
  thread_create ("acquire2", PRI_DEFAULT + 2, acquire2_thread_func, &lock);//aquire2 프로세스가 원래 프로세스보다 높아서 lock을 획득할 수 없으므로 원래 프로세스의 우선순위를 acquire2의 우선순위로 올려야 원래 프로세스가 실행 후 ㅣock을 해제할 ㅅ ㅜ있따.
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  lock_release (&lock);//원래 프로세스는 잠금을 해제하고, acquire2의 기부가 종료되고 실행되며, acquire1의 기부가 종료되고 실행되며, 마지막으로 원래 프로세스가 종료됨
  msg ("acquire2, acquire1 must already have finished, in that order.");
  msg ("This should be the last line before finishing this test.");
}


/*
테스트에서 호출된 두 함수가 잠금을 획득하고 잠금을 해제한다.
*/
static void
acquire1_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire1: got the lock");
  lock_release (lock);
  msg ("acquire1: done");
}

static void
acquire2_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire2: got the lock");
  lock_release (lock);
  msg ("acquire2: done");
}
