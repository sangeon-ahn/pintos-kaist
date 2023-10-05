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

bool thread_compare_priority (struct list_elem *a, struct list_elem *b, void *aux UNUSED);

/* Donate 추가 */
bool thread_compare_donate_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */

/* Priority Scheduling*/
/*
세마다운 함수는 세마포어를 이용하여 자원을 얻으려는 스레드가 사용하는 함수, 해당 세마포어의 값을 감소시키고,
필요할 경우 대기 상태로 들어가는 역할을 수행함
->정리하자면, 세마포어를 사용해서 리소스를 얻고, 리소스를 얻을  때까지 대기하는 세르드를 관리하는 함수
*/
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ()); // 인터럽트 컨텍스트에서 호출하지 않도록 확인

	old_level = intr_disable ();//인터럽트 비활성화
	while (sema->value == 0) {
		// list_push_back (&sema->waiters, &thread_current ()->elem);
		// 세마포어의 값이 0 인 경우, 다른 스레드가 자원을 해제할때까지 대기
		// 대가중인 스레드 리스트에 현재 스레드를 우선순위 순서로 삽입, 내림차순 
		list_insert_ordered (&sema->waiters, &thread_current ()->elem, thread_compare_priority, 0); // waiter에 스레드를 넣어줄 때 우선순위를 고려하여 넣을 수 있도록
		thread_block ();// 스레드를 블록하고 cpu양보
	}
	sema->value--;//세마포어의 값을 1 감소시키고 자원을 사용
	intr_set_level (old_level); // 인터럽트 복원
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.
   This function may be called from an interrupt handler. 
   세마포어 가동 또는 "V" 연산.  SEMA의 값 증가
   SEMA를 기다리는 사람들의 실타래가 있다면 깨워줍니다.
   이 함수는 인터럽트 핸들러에서 호출할 수 있다. */



/* 세마업 함수는, waiters 리스트에 있는동안 우선순위의 변동이 있을 수 있다. 그래서 thread_unblock 함수를
호출하기 전에 해당 리스트를 내림차순으로 정렬할 수 있도록 해야함 ^^, 그런데 unblock된 스레드가 현재 cpu를 점유하고
있는 스레드보다 우선순위가 높을 수 있다. 그래서 thread_preemption 함수를 불러와 cpu를 점유할 수 있도록해야함
*/
/* Priority Scheduling*/

void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable (); //인터럽트 비활성화

	if (!list_empty (&sema->waiters)){//대기중인 스레드가 있는경우
		list_sort (&sema->waiters, thread_compare_priority, 0);// 대기중인 스레드들을 우선순위에 따라 정렬, 내림차순으로
		thread_unblock (list_entry (list_pop_front (&sema->waiters),//대기중인 스레드 중에서 가장 높은 우선순위를 가진 스레드를 꺼내서 블록해제
					struct thread, elem));
	}
	/*
	unblock이 되면서 ready_list에 올라가는데 이때 ready_list에만 올라가는 것이 아닌 
	thread_yield 또한 호출하여 priority에 따른 스레드를 실행시켜야한다.
	*/
	sema->value++; //세마포어의 값을 증가시김
	//세마포어는 확실하게 up된상태에서 thread_yield를 호출해야한다 그렇지않으면 영원히 기다리기만한다
	thread_preemption(); //스레드 스케쥴링을 위한 선점 
	intr_set_level (old_level); //인터럽트 복원
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
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

/*이렇게 초기화된 락은 스레드 간의 상호배제를 구현하고, 크리티컬 섹션에 접근하는 동안 다른 스레드가 접근하지
못하도록 보호는데 사용*/
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);//락 구조체가 유효한지 확인 , 락이 null인지 확인

	lock->holder = NULL; // 락을 아무 스레드도 소유하지 않은 상태로 초기화
	sema_init (&lock->semaphore, 1); // 1로 초기화한다는 것은 락을 사용할 수 있는 상태로 만든다는것 ->뮤텍스
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL); // 락 구조체가 유효한지 확인
	ASSERT (!intr_context ()); // 인터럽트 컨텍스트에서 호출하지 않도록 확인
	ASSERT (!lock_held_by_current_thread (lock));// 현제 스레드가 이미 해당 락을 소유하고 있는지 확인

	/* Donate. sema_down 에 들어가기 전에 lock 을 가지고 있는 스레드에게 priority 를 양도 */
	struct thread *cur = thread_current ();//현재 스레드를 가져옴
	if (lock->holder) {  // 현재 lock 을 소유하고 있는 스레드 lock의 holder가 존재한다면
		cur->wait_on_lock = lock; // 현재 lock 을 소유하고 있는 스레드가 없다면 바로 해당 lock을 차지하고(wait_on_lock에 lock 추가), 
		list_insert_ordered (&lock->holder->donations, &cur->donation_elem, //락을 가지고 있는 스레드의 donation 리스트에 현재 스레드를 우선순위 순서로 삽입
					thread_compare_donate_priority, 0); // thread_compare_priority 의 역할을 donation_elem 에 대하여 하는 함수
		donate_priority (); 
	} // lock 을 누군가 소유하고 있다면 그 스레드에게 priority 를 넘겨주어야 한다

	sema_down (&lock->semaphore); // wating list에 들어감, 락을 획득하기 위해 대가, sema_down 내부에서는 락을 소유하게 됨

	cur->wait_on_lock = NULL; /* Donate*/ // 락을 확득 했으므로, 더이상 기다리지 않음
	lock->holder = cur; // 현재 스레드가 락을 소유함
	/* lock 에 대한 요청이 들어오면 sema_down 에서 멈췄다가 lock 이 사용가능하게 되면 자신이 lock 을 선점 */
}

/* Tries to acquires LOCK and returns true if successful or false on failure.
   The lock must not already be held by the current thread.

   This function will not sleep, so it may be called within an interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */


/*락을 해제하는 스레드가 사용하는 함수, 락을 해제할 때 도네이션과 우서ㅏㄴ순위 복구를 수행*/
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock)); // 현재 스레드가 해당 락을 소유하고 있는지 확인

	/*--------Donate--------------*/
	remove_with_lock (lock); //현재 스레드가 소유한 락을 사용하여 donation list에 넣었던 것 삭제해줌
	refresh_priority (); // donation list에서 스레드를 제거한 후,뺀 나머지들을 다시 우선순위(내림차순)으로 정렬.

	lock->holder = NULL; // 락을 소유한 스레드 정보 초기화
	sema_up (&lock->semaphore);// 락을 해제하고, 대기 중인 스레드 중 하나를 깨워서 락을 얻을 수 있도록 함
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);
	//lock->holder : 해당 락을 소유한 스레드를 가리키는 포인터
	//thread_current : 현재 실행중인 스레드를 반환하는 함수
	// 두개가 동일한 스레드를 가리킨다면, true를 반환하고 현재 스레드가 해당 락을 소유하고 있다는것으로 판단
	//주로 락을 해제할때 락을 소유한 스레드가 맞는지 확인하는 용도로 사용
	return lock->holder == thread_current ();
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */

/*조건변수(condition)을 초기화하는 함수, 스레드간의 신호를 주고 받거나 대기하는데 사용*/
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters); // waiters 리스트 초기화, 조건 변수를 기다리는 스레드들의 목록
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

/* 조건 변수를 기반으로 스레드를 대기시크는 함수, 주어진 락을 해제한 후 대기*/
void
cond_wait (struct condition *cond, struct lock *lock) {

	ASSERT (cond != NULL);//조건 변수 구조체가 유효한지 확인
	ASSERT (lock != NULL);//락 구조체가 유효한지 확인
	ASSERT (!intr_context ());//인터럽트 컨텍스트에서 호출하지 않도록 확인
	ASSERT (lock_held_by_current_thread (lock));//현재 스레드가 해당 락을 소유하고 있는지 확인
	
	struct semaphore_elem waiter; // waiter를 위한 세마포어 생성,waiter 스레드가 조건변수를 통해 신호를 받을 때 사용
	
	sema_init (&waiter.semaphore, 0);// waiter의 세마포어를 0으로 초기화
	// list_push_back (&cond->waiters, &waiter.elem);
	list_insert_ordered (&cond->waiters, &waiter.elem, sema_compare_priority, 0);//조건 변수의 waiters 리스트에 waiter 추가, 우선순위가 높은 스레드가 먼저 대기할 ㅅ ㅜ있도록

	lock_release (lock);//락을 해제하여 다른 스레드가 락을 획득할 수 있도록함
	sema_down (&waiter.semaphore);// 대가지 세마포어를 이용하여 대기하고, 신호를 받을 때까지 대기
	lock_acquire (lock);//락을 다시 획득하여 다음 작업을 수행
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */

/*조건변수에 대기중엔 스레드 중 하나를 깨우는 함수인데 여기서 주어진 lock은 필요하지만 
실제로는 이용하지 않음*/
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)){//대기중인 스레드가 있는지 확인
		list_sort (&cond->waiters, sema_compare_priority, 0); //대가지들을 우선순위에 따라 정렬
		//대기 중인 스레드 중 우선순위가 가장 높은 스레드를 깨움
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/*
조건 변수에 대기중인 모든 스레드를 꺠우는 함수,락과 같이 사용
*/
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))//조건 변수의 대기자가 비어있지 않는 동안
		cond_signal (cond, lock);//cond_signal 함수를 호출해서 모든 대기중인 스레드를 깨움
}


/*Priority Scheduling */
/*
cond-signal을 보니 list_entry로 나오는 구조체가 struct semaphore_elem이기 때문에
따라서 넣을때고 이 구조체에 맞게 넣어야함
*/
bool sema_compare_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
	//리스트 원소에서 세마포어 원소를 추출
	struct semaphore_elem *a_sema = list_entry (a, struct semaphore_elem, elem);
	struct semaphore_elem *b_sema = list_entry (b, struct semaphore_elem, elem);
	// 세마포어 원소의 waiters 리스트를 가져옴
	struct list *waiter_a_sema = &(a_sema->semaphore.waiters);
	struct list *waiter_b_sema = &(b_sema->semaphore.waiters);
	/*
	waiters 리스트에서 가장 높은 우선순위를 가진 스레드를 비교하여
	a_sema, b_sema보다 더 높은 우선순위를 가지면 true를 반환하고, 그렇지 않으면 false를 반환한다.
	*/
	return list_entry (list_begin (waiter_a_sema), struct thread, elem)->priority
		 > list_entry (list_begin (waiter_b_sema), struct thread, elem)->priority;
}