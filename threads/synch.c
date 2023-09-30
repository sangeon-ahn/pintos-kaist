/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any).
   SEMA 세마포어를 VALUE 값으로 초기화한다. 세마포어는 그것을 조작하기 위한 두 개의 원자적 연산자와 함께하는 음수가 아닌 정수이다:

down 또는 "P": 값이 양수가 될 때까지 기다린 후, 그 값을 감소시킨다.

up 또는 "V": 값을 증가시킨다 (그리고 기다리는 스레드가 있다면 하나를 깨운다).*/
void sema_init(struct semaphore *sema, unsigned value)
{
   ASSERT(sema != NULL);

   sema->value = value;
   list_init(&sema->waiters); // waiter를 초기화
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function.
   세마포어에 대한 Down 또는 "P" 연산이다. SEMA의 값이 양수가 될 때까지 기다린 후 원자적으로 그 값을 감소시킨다.

   이 함수는 슬립(sleep) 상태가 될 수 있으므로, 인터럽트 핸들러 내에서 호출되어서는 안 된다.
   이 함수는 인터럽트가 비활성화된 상태에서 호출될 수 있지만,
   만약 이 함수가 슬립 상태가 된다면, 다음에 스케줄링될 스레드는 아마도 인터럽트를 다시 활성화시킬 것이다. 이것은 sema_down 함수이다.

   스레드를 blocked로 만들어준다
   */

void sema_down(struct semaphore *sema)
{
   enum intr_level old_level;

   ASSERT(sema != NULL);
   ASSERT(!intr_context());

   old_level = intr_disable(); // 인터럽트 비활
   while (sema->value == 0)
   { // sema 값이 0이면 무한 루프를 돈다
      // 현재 러닝 중인 스레드(자기자신)를 웨이트 리스트에 넣고 블락시킴
      // 해당 스레드는 블락 상태가 되고, 다른 스레드로 스케줄링이 됨
      // sema_up이 호출되는 순간 웨이트 리스트에 있는 것들 중 맨앞에 있는 것을 unblock시키고 sema 값을 증가시킴
      list_push_back(&sema->waiters, &thread_current()->elem); // ELEM을 LIST의 끝에 삽입하여, 그것이 LIST의 마지막 요소가 된다.

      thread_block(); //
   }
   sema->value--;             // value 감소 시키고
   intr_set_level(old_level); // 이전 인터럽트 상태를 인자로 넣어서(원상복귀)
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool sema_try_down(struct semaphore *sema)
{
   enum intr_level old_level;
   bool success;

   ASSERT(sema != NULL);

   old_level = intr_disable();
   if (sema->value > 0) // value가 0이상이면
   {
      sema->value--; // value감소시키고
      success = true;
   }
   else
      success = false;
   intr_set_level(old_level);

   return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void sema_up(struct semaphore *sema)
{
   enum intr_level old_level;

   ASSERT(sema != NULL);

   old_level = intr_disable();
   //

   struct thread *t;
   struct list_elem *e;
   if (!list_empty(&sema->waiters))
   {
      e = list_max(&sema->waiters, compare_pri_less, NULL);
      t = list_entry(e, struct thread, elem);
      list_remove(e);
      thread_unblock(t); // 블락된 스레드를 러닝 상태로 바꿔줌
      
   } // waiters에 thread가 있다면

   sema->value++; // sema 구조체에서 value를 ++
   thread_yield();
   intr_set_level(old_level);
}

static void sema_test_helper(void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void sema_self_test(void)
{
   struct semaphore sema[2]; // 세마포어 두개
   int i;

   printf("Testing semaphores...");
   sema_init(&sema[0], 0);
   sema_init(&sema[1], 0);
   thread_create("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
   for (i = 0; i < 10; i++)
   {
      sema_up(&sema[0]);   // 첫번째 세마포어는 계속 value를 up해주고 -> waiters에서 pop하고 unblock
      sema_down(&sema[1]); // 두번째 세마포어는 계속 value를 down해준다 -> waiters에 계속 넣어준다
   }
   printf("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper(void *sema_)
{
   struct semaphore *sema = sema_;
   int i;

   for (i = 0; i < 10; i++)
   {
      sema_down(&sema[0]); // 반대로
      sema_up(&sema[1]);
   }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
/*락의 특징
  1. 락은 한번에 하나의 스레드만 소유할 수 있다.
  2. 현재 락을 소유하고 있는 스레드가 락을 소유하려 시도 하는것은 오류
  3. 락은 초기값이 1이다
  4. 락은 동일한 스레드가 락을 해제하고 획득할수있다.
  세마포어의 특징
  1. 세마포어는 1 이상의 값을 가질 수 있다.세마포어의 value는 동시에 수행될 수 있는 최대 작업의 수
  2. 세마포어는 소유자가 없다. 즉 하나의 스레드가 세마포어를 down하고 다른 스레드가 up할 수 있다.*/
void lock_init(struct lock *lock)
{
   ASSERT(lock != NULL);

   lock->holder = NULL;
   sema_init(&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/*이미 락을 보유하고 있는 스레드는 락을 다시 소유하려 시도해서는 안된다.
인터럽트가 비활성화된 상태에서 이 함수가 호출되면 스레드가 sleep(block) 상태로 전환해야할때, 인터럽트는 다시 활성화된다.*/
void lock_acquire(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(!lock_held_by_current_thread(lock)); // 현재 스레드가 락을 소유하고 있지 않아야함

   struct lock *temp;
   struct thread *temp_t;

   if (lock->holder == NULL)
   {

      sema_down(&lock->semaphore);
      lock->holder = thread_current();
      // 락 리스트에 현재 소유한 락 정보 저장
      list_push_back(&(lock->holder->lock_list), &lock->l_elem);
      lock->holder->priority = thread_current()->priority;
   }
   else
   {
      if (lock->holder->priority < thread_current()->priority)
      { // 현재 우선순위가 더 높아서 우선순위 기부를 해주어야함
         temp = lock;
         temp_t = thread_current();
         // thread_current()->waiting_lock = lock;

         while (temp != NULL)
         {
            if (list_empty(&(temp->holder->donations)))
            {
               list_push_back(&(temp->holder->donations), &temp_t->d_elem); // donations 리스트에 현재 스레드 추가
               temp->holder->priority = temp_t->priority;
               
            }
            else
            {
               // 1. donations의 스레드의 waiting_lock을 검사해서 현재 스레드의 waiting_lock과 같은 lock이 있다면
               struct list_elem *e;
               struct thread *t;

               for (e = list_begin(&(temp->holder->donations)); e != list_end(&(temp->holder->donations)); e = list_next(e))
               {
                  t = list_entry(e, struct thread, d_elem);
                  if (t->waiting_lock == temp)
                  {
                     // 1.1 둘의 우선순위를 비교해 더 높은 우선순위를 donations에 저장한다.
                     // 1.2 현재 스레드의 우선순위가 더 높다면
                     list_remove(e); // 기존에 있던 스레드를 지우고, 현재 스레드를 추가하고
                     list_push_back(&(temp->holder->donations), &temp_t->d_elem);
                     temp->holder->priority = temp_t->priority;
                     break;
                  }
               }
               if (e == list_tail(&(temp->holder->donations)))
               { // 2. 같은 락이 없다면 바로 push(e==tail 없다는거니까 바로 푸시)
                  list_push_back(&(temp->holder->donations), &temp_t->d_elem);
                  temp->holder->priority = temp_t->priority;
               }
            }
            temp_t = temp->holder;
            temp = temp->holder->waiting_lock;
         }
      }
      thread_current()->waiting_lock = lock;
      sema_down(&lock->semaphore); // 얘를 탈출하면 락을 가질수있는 상태가 되니까
      lock->holder = thread_current();
      list_push_back(&(lock->holder->lock_list), &lock->l_elem);
   }
   
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool lock_try_acquire(struct lock *lock)
{
   bool success;

   ASSERT(lock != NULL);
   ASSERT(!lock_held_by_current_thread(lock));

   success = sema_try_down(&lock->semaphore);
   if (success)
      lock->holder = thread_current();
   return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void lock_release(struct lock *lock)
{
   ASSERT(lock != NULL);
   ASSERT(lock_held_by_current_thread(lock));

   // 릴리즈하려는 락과 관련된 donors가 donations에 있는지 확인
   if (!list_empty(&(lock->holder->donations)))
   {
      for (struct list_elem *e = list_begin(&(lock->holder->donations)); e != list_end(&(lock->holder->donations)); e = list_next(e))
      {
         struct thread *t = list_entry(e, struct thread, d_elem);

         if (t->waiting_lock == lock)
         {
            // 있다면 -> 릴리즈하려는 락과 관련된 donors를 donations에서 삭제
            list_remove(e);
            break;
         }
      }
      if (list_empty(&(lock->holder->donations)))
      { // donations 비어있는지 확인
         // 비어있다면 -> 더 이상 소유하고 있는 락이 없으므로 원래 우선 순위로 돌아감, 락 릴리즈, 세마업
         thread_current()->priority = thread_current()->origin_priority;
      }
      else
      {

         struct list_elem *max_elem = list_max(&(thread_current()->donations), compare_pri_less_don, NULL); // donations 리스트에서 가장 높은 우선순위를 가진 스레드를 찾는다
         struct thread *max_thread = list_entry(max_elem, struct thread, d_elem);

         thread_current()->priority = max_thread->priority; // donations에서 가장 높은 우선순위를 가진 스레드의 우선순위를 저장
      }
   }
   list_remove(&lock->l_elem); // donations 릴리즈하려는 락 제거
   lock->holder = NULL;
   sema_up(&lock->semaphore);
   // thread_yield();
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool lock_held_by_current_thread(const struct lock *lock)
{
   ASSERT(lock != NULL);

   return lock->holder == thread_current();
}

/* One semaphore in a list. */
struct semaphore_elem
{
   struct list_elem elem;      /* List element. */
   struct semaphore semaphore; /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void cond_init(struct condition *cond)
{
   ASSERT(cond != NULL);

   list_init(&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void cond_wait(struct condition *cond, struct lock *lock)
{
   struct semaphore_elem waiter;

   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock)); // 현재 스레드가 락을 보유한 상태면

   sema_init(&waiter.semaphore, 0);              // waiter의 세마포어를 초기화하며, 초기 value는 0으로 설정
   list_insert_ordered(&cond->waiters, &waiter.elem, compare_pri, NULL); // waiter 원소를 cond의 waiters목록에 추가
   lock_release(lock);                           // 락 해제
   sema_down(&waiter.semaphore);                 // waiter의 세마포어를 대기상태로 만든다.
   // 조건 추가 :
   lock_acquire(lock); // 조건이 만족되면, 락을 다시 획득한다. 이전에 세마포어가 신호를 받아 대기상태에서 벗어났다면 이제 락을 다시 획득하려 시도한다.
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler.
   함수가 조건변수(COND)에 대기하고 있는 스레드 중 하나를 깨우기 위해 사용된다.
   함수를 호출하기 전에 락이 획득되어 있어야 하며, 인터럽트 핸들러 내에서는 이 함수를 사용하면 안된다.*/
void cond_signal(struct condition *cond, struct lock *lock UNUSED)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);
   ASSERT(!intr_context());
   ASSERT(lock_held_by_current_thread(lock));

   if (!list_empty(&cond->waiters))
      sema_up(&list_entry(list_pop_front(&cond->waiters),
                          struct semaphore_elem, elem)
                   ->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void cond_broadcast(struct condition *cond, struct lock *lock)
{
   ASSERT(cond != NULL);
   ASSERT(lock != NULL);

   while (!list_empty(&cond->waiters))
      cond_signal(cond, lock);
}