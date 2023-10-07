#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h"
#ifdef VM
#include "vm/vm.h"
#endif


/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. 실행 일시 정지 -> 다시 실행 가능*/
	THREAD_DYING        /* About to be destroyed. 실행 종료 -> 다시 실행 불가*/
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority.  가장 낮은 우선순위 idle thread*/
#define PRI_DEFAULT 31                  /* Default priority. 맨 처음 스레드를 생성했을때의 초기값*/
#define PRI_MAX 63                      /* Highest priority. */
/*init exit_status*/
#define INIT_EXIT_STATUS 99999
/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              | 스레드 스택 
 *           |            intr_frame           | kernel stack에 데이터가 쌓이다가 데이터 오버플로우가 발생하는 것을 magic으로 확인
 *           |                :                | magic을 넘어가면 스택 오버플로우
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	struct list donations;
	struct list_elem d_elem;
	struct lock* wait_on_lock;
	struct list lock_list;
	int64_t thread_tick_count;
	
	/*-------------------project2------------------------------*/
	struct list child_list;				//fork 할때마다 child 리스트에 추가가 되는건지, 정렬해야하는지, child list의 child에도 우선순위가 있는지
	struct list_elem c_elem;			//child list elem
	int creat_flag;						//성공적으로 자식 프로세스를 생성시켰는지 확인하는 플래그
	int exit_status;					//프로그램의 종료 상태를 나타내는 멤버
	struct thread *parent_p;			//부모 프로세스 디스크립터 포인터 필드
	// struct file *fdt[128];				//file 정보 file -> inode. open_cnt
	int next_fd;						//다음 파일 디스크립터 정보(number) 1씩 증가
	struct semaphore wait_process_sema;
#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);
void make_thread_sleep(int64_t ticks);
void make_thread_wakeup(int64_t ticks);

int thread_get_priority (void);
void thread_set_priority (int);
bool priority_cmp(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
void sort_ready_list(void);
void print_ready_list(void);
void test_list_max(void);
bool priority_cmp_for_done_max(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);
bool priority_cmp_for_cond_waiters_max(const struct list_elem *a_, const struct list_elem *b_,void *aux UNUSED);

bool priority_cmp_for_waiters_max(const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

void do_iret (struct intr_frame *tf);
void thread_preemption(void);
#endif /* threads/thread.h */
