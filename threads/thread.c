#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* ----------alarm----------------- */
static struct list blocked_list;
static int64_t next_tick_to_awake;		/* sleep_list에서 대기중인 스레드들의 wakeup_tick 값 중 최솟값을 저장하기 위한 변수 추가 */


void make_thread_sleep(int64_t ticks);		/* 실행 중인 스레드를 슬립으로 만듦, Thread를 blocked 상태로 만들고 sleep queue에 삽입하여 대기 */
void make_thread_wakeup(int64_t ticks);		/* 슬립 큐에서 깨워야 할 스레드를 찾아서 깨움 */


static bool tick_less(const struct list_elem *a_, const struct list_elem *b_,
		  void *aux UNUSED);


/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
   /* 현재 실행 중인 코드를 스레드로 변환하여 스레딩 시스템을 초기화합니다. 
   	이것은 일반적으로 동작할 수 없으며 'loader.S'가 스택의 하단을 페이지 경계에 놓도록 주의했기 때문에 가능합니다.
	또한 실행 대기열 및 tid lock을 초기화합니다.

	이 함수를 호출한 후 다음을 사용하여 스레드를 작성하기 전에 페이지 할당자를 초기화해야 합니다.
	thread_create().
	이 함수가 끝날 때까지 thread_current()를 호출하는 것은 안전하지 않습니다. */
void
thread_init (void) {
	ASSERT (intr_get_level () == INTR_OFF); // 인터럽트 관련 레지스터 핀을 끊는 것 intr비활성화.
	// cpu는 레지스터에서 0,1로 동작 수행하는데 그게 isa

	/* Reload the temporal gdt for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the global thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init(&blocked_list);
	list_init (&destruction_req);

	/* Set up a thread structure for the running thread. */
	initial_thread = running_thread (); // 지금 현재 위치를 가져옴 ㅡㅡ
	init_thread (initial_thread, "main", PRI_DEFAULT);
	initial_thread->status = THREAD_RUNNING;
	initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* Create the idle thread. */
	struct semaphore idle_started;
	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);

	/* Start preemptive thread scheduling. */
	intr_enable (); // 인터럽트 활성화

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) {
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}


/*ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡalarmㅡㅡㅡㅡㅡㅡㅡㅡㅡ*/
void make_thread_sleep(int64_t ticks)
{
	enum intr_level old_level;
	old_level = intr_disable();
	ASSERT(!intr_context());
	ASSERT(intr_get_level() == INTR_OFF);
	// printf("in thread sleep\n");
	struct thread *t = thread_current();
	
	// ticks를 어디다 쓸까
	// 넣을때 정렬해서 넣기
	//  list_push_back(&blocked_list, &thread_current() -> elem);
	// list, 현재 리스트에 삽입하려는 thread, list에 있는 스레드

	t->thread_tick_count = ticks;
	t->status = THREAD_BLOCKED;
	list_insert_ordered(&blocked_list, &(t->elem), tick_less, NULL);
	schedule();
	intr_set_level(old_level);
}

void make_thread_wakeup(int64_t ticks)	
{
		
		// 이렇게 구현하려면 순서대로 정렬해서 넣어줘야힘
		while (!list_empty(&blocked_list) && list_entry(list_front(&blocked_list), struct thread, elem) -> thread_tick_count <= ticks)
		{
			struct thread * t = list_entry(list_pop_front(&blocked_list), struct thread, elem);
			// list_pop_front(&blocked_list);
			thread_unblock(t);
		}
	}
		

static bool
tick_less(const struct list_elem *a_, const struct list_elem *b_,
		  void *aux UNUSED)
{
	const struct thread *a = list_entry(a_, struct thread, elem);
	const struct thread *b = list_entry(b_, struct thread, elem);

	return a->thread_tick_count < b->thread_tick_count;
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */



/* Sets the current thread's priority to NEW_PRIORITY. 현재 스레드의 우선순위를 새 우선순위로 정한다. */ 
void
thread_set_priority (int new_priority) {
  thread_current ()->init_priority = new_priority;
  refresh_priority (); /* Donate 추가 */ // 우선순위 내림차순 정렬
  thread_preemption (); /* Priority Scheduling 추가 */
  
}

tid_t
thread_create (const char *name, int priority,
		thread_func *function, void *aux) {
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO); // 페이지 할당
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */
	init_thread (t, name, priority); // thread 구조체 초기화ㅏ
	tid = t->tid = allocate_tid (); // tid 할당

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread;
	t->tf.R.rdi = (uint64_t) function;
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);
	thread_preemption(); /* Priority Scheduling 추가 */

	return tid;
}


/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) {
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) {
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);
	// list_push_back (&ready_list, &t->elem);
	list_insert_ordered(&ready_list, &t->elem, thread_compare_priority, 0); /* Priority Scheduling 추가 */
	t->status = THREAD_READY;
	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) {
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();
	if (curr != idle_thread)
		// list_push_back (&ready_list, &curr->elem);
		list_insert_ordered(&ready_list, &curr->elem, thread_compare_priority, 0); /* Priority Scheduling 추가 */
	do_schedule (THREAD_READY);
	intr_set_level (old_level);
}


/* Returns the current thread's priority. 현재 스레드의 우선순위를 반환한다. */
int
thread_get_priority (void) {
	return thread_current ()->priority;
}

/* Priority Scheduling*/
bool thread_compare_priority (struct list_elem *a, struct list_elem *b, void *aux UNUSED) {
    return list_entry (a, struct thread, elem)->priority > list_entry (b, struct thread, elem)->priority;
}

/* Donate*/
bool
thread_compare_donate_priority (const struct list_elem *a,const struct list_elem *b, void *aux UNUSED)
{
	return list_entry (a, struct thread, donation_elem)->priority > list_entry (b, struct thread, donation_elem)->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
/*
thread_start()에서 thread_create 하는 순간 idle thread가 생성, 그리고 idle함수가 실행된다.
idle thread는 여기서 한번 스케쥴을 받고 세마업을 하여 thread_start의 마지막 세마다운을 풀어주며 thread_start가 작업을 마치고
run_action()을 실핼할 수 있도록 idle 자신은 블락된다. idle 스레드는 핀토스에서 실행 가능한 스레드가 하나도 없을 때 wake 되어 다시
작동한다.이는 cpu가 무조건 하나의 thread는 실행하고 있는 상태를 만들기 위함
*/
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
/* 파라미터를 보면, thread_func *function 은 이 kernel 이 실행할 함수, void *aux 는 보조 파라미터로 
synchronization 을 위한 세마포 등이 들어옴. 여기서 실행시키는 function은 이 thread가 종료될때가지 실행되는 main 함수라고 
즉, 이 function 은 idle thread 라고 불리는 thread 를 하나 실행시키는데, 이 idle thread 는 하나의 c 프로그램에서 
하나의 main 함수 안에서 여러 함수호출들이 이루어지는 것처럼, pintos kernel 위에서 여러 thread 들이 
동시에 실행될 수 있도록 하는 단 하나의 main thread 인 셈이다. 우리의 목적은 이 idle thread 위에 여러 thread 들이 동시에 실행되도록 만드는 것이다.  */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);

	intr_enable ();       /* The scheduler runs with interrupts off. */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);
	t->status = THREAD_BLOCKED;//스레드 상태를 블락으로 설정
	strlcpy (t->name, name, sizeof t->name);//스레드 이름 복사
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);//스레드 스택 포인터
	t->priority = priority; //스레드 우선순위 설정 빼면, 스레드가 생성될떄 우선순위가 초기화 되지 않아 임의의 값을 가지게 되어 초기화해야됨
	t->magic = THREAD_MAGIC;//스레드 마법번호 설정

	t->init_priority = priority;//초기 우선순위 설정
	/* Donate*/
	t->wait_on_lock = NULL;//스레득 대기 중인 락 초기화
	list_init (&t->donations); // 도네이션 리스트 초기화. list_init => head<->tail

}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is 
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return idle_thread. */
static struct thread *
next_thread_to_run (void) {
	if (list_empty (&ready_list))
		return idle_thread;
	else				// list에서 맨앞껄 빼는데 그 노드의 주소값 리턴, _ , _
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}			// elem을 thread로 보게하는 함수

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	ASSERT (intr_get_level () == INTR_OFF);
	ASSERT (thread_current()->status == THREAD_RUNNING);
	//종료 요청 목록에서 스레드를 꺼내 메모리 해제
	while (!list_empty (&destruction_req)) {
		struct thread *victim =
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim);
	}
	//현재 스레드의 상태를 주어진 상태로 변경
	thread_current ()->status = status;
	//다음 스레드 선택하여 실행
	schedule ();
}

/* 스레드간의 우선순위를 기부하는데 사용, 중첩된 기부를 처리
	depth 는 nested 의 최대 깊이를 지정해주기 위해 사용했다(max_depth = 8). 
	스레드의 wait_on_lock 이 NULL 이 아니라면 스레드가 lock 에 걸려있다는 소리
	그 lock 을 점유하고 있는 holder 스레드에게 priority를 넘겨주는 방식을 깊이 8의 스레드까지 반복한다. 
	wait_on_lock == NULL 이라면 더 이상 donation 을 진해할 필요가 없으므로 break
	이러한 방식으로 중첩된 기부는 대기중인 락의 소유자에게 우선순위를 계속해서 전달하여 우선순위 역전 문제를
	해결 최대 8번의 중첩 기부를 처리하여 예기치 않은 상황을 방지하고, 우선순위 역전을 적절히 관리*/
void
donate_priority (void)
{ /* nest donation */
  int depth;
  struct thread *cur = thread_current ();

  for (depth = 0; depth < 8; depth++){ //테스트 케이스가 8번까지 있어서 
    if (!cur->wait_on_lock)
		break;//스레드가 대기중인 락이 없으면 종료
		//현재 스레드가 대기중인 락을 가진 스레드의 우선순위를 현재 스레드의 우선순위로 업데이트
      struct thread *holder = cur->wait_on_lock->holder;
      holder->priority = cur->priority;
      cur = holder;// 다음 레빌의 기부를 처리하기 위해 현재 스레드 업데이트
  }
}
/* sheduling 함수는 thread_yield(), thread_block(), thread_exit() 함수 내의 거의 마지막 부분에 실행되어 
	CPU 의 소유권을 현재 실행중인 스레드에서 다음에 실행될 스레드로 넘겨주는 작업을 한다. */
static void
schedule (void) {
	/* 현재 실행중인 thread 를 thread A 라고 하고, 다음에 실행될 스레드를 thread B 라고 하겠다. 
		*cur 은 thread A, *next 는 next_thread_to_run() 이라는 함수(ready queue 에서 다음에 실행될 스레드를 골라서 return 
		지금은 round-robin 방식으로 고른다.)에 의해 thread B 를 가르키게 됨 */
	struct thread *curr = running_thread ();
	struct thread *next = next_thread_to_run ();


	ASSERT (intr_get_level () == INTR_OFF); // scheduling 도중에는 인터럽트가 발생하면 안됨
	ASSERT (curr->status != THREAD_RUNNING); // CPU 소유권을 넘겨주기 전에 running 스레드는 그 상태를 running 외의 다른 상태로 바꾸어주는 작업이 되어 있어야 하고 이를 확인하는 부분이다.
	ASSERT (is_thread (next)); // next_thread_to_run() 에 의해 올바른 thread 가 return 되었는지 확인한다.

	/* Mark us as running. */
	next->status = THREAD_RUNNING; // 상태 변경

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct thread. 
		   This must happen late so that thread_exit() doesn't pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is currently used bye the stack.
		   The real destruction logic will be called at the beginning of the schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}

/* Donate,donations 리스트에서 스레드들을 지우는 함수, 특정 락을 해제할때, 해당 락을 기다리는 스레드에게서
도네이션을 받은 스레드의 엔트리를 도네이션 리스트에서 삭제하는 역할을 함, 이를통해 도네이션 관련 정보를 정리하고 우선순위
역전 문제를 효과적으로 처리
cur->donations 리스트를 처음부터 끝까지 돌면서 리스트에 있는 스레드가 priority 를 빌려준 이유 
즉,wait_on_lock이 이번에 release 하는 lock 이라면 해당하는 스레드를 리스트에서 지워준다. 
list.c 에 구현되어 있는 list_remove 함수를 사용 모든 donations 리스트와 관련된 
작업에서는 elem 이 아니라 donation_elem을 사용해야함 */
void 
remove_with_lock (struct lock *lock) /* lock을 해지 했을 때, waiters 리스트에서 해당 엔트리를 삭제 하기 위한 함수 */
{
  struct list_elem *e;
  struct thread *cur = thread_current ();

  /* donation 리스트 순회 */
  /* 현재 쓰레드의 waiters 리스트를 확인하여 해지할 lock을 보유하고 있는 엔트리를 삭제 */
  for (e = list_begin (&cur->donations); e != list_end (&cur->donations); e = list_next (e)){
    struct thread *t = list_entry (e, struct thread, donation_elem);
    if (t->wait_on_lock == lock)// 만약 스레드t가 특정 락을 기다리고 있다면
      list_remove (&t->donation_elem);// 해당 스레드의 도네이션 엔트리를 도네이션 리스트에서 제거
  }
}

/* Donate 추가 */
void
refresh_priority (void)
{
  struct thread *cur = thread_current ();

  cur->priority = cur->init_priority; //현재 우선순위를 초기 우선순위로 재설정 (현재 donate 된 Priority 에서 원래의 priority로 돌려준다)
  
  //nest donate라서 돌아갈 priority가 여러개라면
  if (!list_empty (&cur->donations)) { // donations 리스트에 스레드가 남아있다면 가장 높은 priority 를 가져와야 함
	//donations 리스트의 원소들을 우선순위를 기준으로 내림차순으로 정렬
    list_sort (&cur->donations, thread_compare_donate_priority, 0); 

	//donations 리스트에서 우선순위가 가장 높은 스레드를 가져옴
	struct thread *front = list_entry (list_front (&cur->donations), struct thread, donation_elem);
    //현재 스레드의 우선순위를 가장 높은 스레드의 우선순위와 비교하여 더 높은 우선순위를 적용.
	if (front->priority > cur->priority)
      cur->priority = front->priority;
  }
}

/*
현재 실행중인 스레드의 우선순위와 준비리스트의 가장 높은 우선순위를 가진 스레드와 비교한다.
만약 준비 리스트가 비어있지 않고, 현재 스레드의 우선순위가 준비리스트의 가장 높은 우선순위를 가진 스레드보다
작다면 스레드 선점이 필요한 상황, 따라서 thread_yield 함수를 호출해 현재 스레드를 스케줄링 큐로 돌려주고
스케쥴러에게 cpu제어권을 양보하게됨, 이렇게 함으로써 우선순위가 더 높은 스레드가 실행될 수 있게됨
*/
void thread_preemption (void){
    if (!intr_context() && !list_empty (&ready_list) && thread_current ()->priority < list_entry (list_front (&ready_list), struct thread, elem)->priority)
        thread_yield ();
}

