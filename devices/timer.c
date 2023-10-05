#include "devices/timer.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* See [8254] for hardware details of the 8254 timer chip. */

#if TIMER_FREQ < 19
#error 8254 timer requires TIMER_FREQ >= 19
#endif
#if TIMER_FREQ > 1000
#error TIMER_FREQ <= 1000 recommended
#endif

/* Number of timer ticks since OS booted. */
static int64_t ticks;

/* Number of loops per timer tick.
   Initialized by timer_calibrate(). */
static unsigned loops_per_tick;

static intr_handler_func timer_interrupt;
static bool too_many_loops (unsigned loops);
static void busy_wait (int64_t loops);
static void real_time_sleep (int64_t num, int32_t denom);

/*-------alarm---------------*/
void make_thread_awake(int64_t ticks);
void make_thread_sleep(int64_t ticks);

/* Sets up the 8254 Programmable Interval Timer (PIT) to
   interrupt PIT_FREQ times per second, and registers the
   corresponding interrupt. */
void
timer_init (void) {
	/* 8254 input frequency divided by TIMER_FREQ, rounded to
	   nearest. */
	uint16_t count = (1193180 + TIMER_FREQ / 2) / TIMER_FREQ;

	outb (0x43, 0x34);    /* CW: counter 0, LSB then MSB, mode 2, binary. */
	outb (0x40, count & 0xff);
	outb (0x40, count >> 8);

	intr_register_ext (0x20, timer_interrupt, "8254 Timer");
}


/* Calibrates loops_per_tick, used to implement brief delays. */
void
timer_calibrate (void) {
	unsigned high_bit, test_bit;

	ASSERT (intr_get_level () == INTR_ON);
	printf ("Calibrating timer...  ");

	/* Approximate loops_per_tick as the largest power-of-two
	   still less than one timer tick. */
	loops_per_tick = 1u << 10;
	while (!too_many_loops (loops_per_tick << 1)) {
		loops_per_tick <<= 1;
		ASSERT (loops_per_tick != 0);
	}

	/* Refine the next 8 bits of loops_per_tick. */
	high_bit = loops_per_tick;
	for (test_bit = high_bit >> 1; test_bit != high_bit >> 10; test_bit >>= 1)
		if (!too_many_loops (high_bit | test_bit))
			loops_per_tick |= test_bit;

	printf ("%'"PRIu64" loops/s.\n", (uint64_t) loops_per_tick * TIMER_FREQ);
}

/* Returns the number of timer ticks since the OS booted. */
int64_t
timer_ticks (void) {
	enum intr_level old_level = intr_disable ();
	int64_t t = ticks;
	intr_set_level (old_level);
	barrier ();
	return t;
}

/* Returns the number of timer ticks elapsed since THEN, which should be a value once returned by timer_ticks(). 
	Timer_ticks()에 의해 반환된 값인 THEN 이후로 경과된 타이머 ticks 수를 반환한다.

	timer_ticks() 함수는 현재 ticks 값을 반환하는 함수로 start 에 현재 시간(ticks)을 저장한다. 
	timer_elapsed() 함수는 특정시간 이후로 경과된 시간(ticks) 를 반환한다. 
	즉, timer_elapsed(start) 는 start 이후로 경과된 시간(ticks)을 반환한다.*/
int64_t
timer_elapsed (int64_t then) {
	return timer_ticks () - then;
}

/* Suspends execution for approximately TICKS timer ticks. */
// void // timer_sleep() 함수가 호출되면, 해당 스레드는 ready상태가 아닌 blocked상태로 전환된다. blocked된 스레드들은 일어날 시간이 되면 일어나야 한다.
// timer_sleep (int64_t ticks) { // system timer : 초당 100회의 ticks 발생. (1 tick = 10ms)
// 	/* Busy waiting : Thread가 CPU를 점유하면서 대기하고 있는 상태. CPU 자원이 낭비 되고, 소모 전력이 불필요하게 낭비될 수 있다. */
void
timer_sleep (int64_t ticks) {
	// int64_t start = timer_ticks (); //현재 tic을 start에 넣어서
	
	ASSERT (intr_get_level () == INTR_ON); //interrupt 상태인지 확인
	// printf("<1>\n");
	// while (timer_elapsed (start) < ticks) //start로 부터 시간이 얼마나 지났는지

	// 	thread_yield ();
	// printf("Switch : %d\n", swch);
	
	// printf("in 'while' ... running\n");
	make_thread_sleep(timer_ticks () + ticks); //현재 시간부터 재워야할 tick을 더해주어야함
	// printf("timer_sleep finish\n");
	// thread_print_stats();
}

/* Suspends execution for approximately MS milliseconds. */
void
timer_msleep (int64_t ms) {
	real_time_sleep (ms, 1000);
}

/* Suspends execution for approximately US microseconds. */
void
timer_usleep (int64_t us) {
	real_time_sleep (us, 1000 * 1000);
}

/* Suspends execution for approximately NS nanoseconds. */
void
timer_nsleep (int64_t ns) {
	real_time_sleep (ns, 1000 * 1000 * 1000);
}

/* Prints timer statistics. */
void
timer_print_stats (void) {
	printf ("Timer: %"PRId64" ticks\n", timer_ticks ());
}


/* Timer interrupt handler. */
/* timer 인터럽트는 매 tick 마다 ticks 라는 변수를 증가시킴으로서 시간을 잰다. 
	이렇게 증가한 ticks 가 TIME_SLICE 보다 커지는 순간에 intr_yield_on_return() 이라는 인터럽트가 실행되는데, 
	이 인터럽트는 결과적으로 thread_yield() 를 실행시킨다. 
	즉 위의 scheduling 함수들이 호출되지 않더라도 일정 시간마다 자동으로 scheduling 이 발생한다. */
static void
timer_interrupt (struct intr_frame *args UNUSED) {
	ticks++;
	thread_tick ();
	make_thread_wakeup(ticks);
}
/* Returns true if LOOPS iterations waits for more than one timer
   tick, otherwise false. */
static bool
too_many_loops (unsigned loops) {
	/* Wait for a timer tick. */
	int64_t start = ticks;
	while (ticks == start)
		barrier ();

	/* Run LOOPS loops. */
	start = ticks;
	busy_wait (loops);

	/* If the tick count changed, we iterated too long. */
	barrier ();
	return start != ticks;
}

/* Iterates through a simple loop LOOPS times, for implementing
   brief delays.

   Marked NO_INLINE because code alignment can significantly
   affect timings, so that if this function was inlined
   differently in different places the results would be difficult
   to predict. */
static void NO_INLINE
busy_wait (int64_t loops) {
	while (loops-- > 0)
		barrier ();
}

/* Sleep for approximately NUM/DENOM seconds. */
static void
real_time_sleep (int64_t num, int32_t denom) {
	/* Convert NUM/DENOM seconds into timer ticks, rounding down.

	   (NUM / DENOM) s
	   ---------------------- = NUM * TIMER_FREQ / DENOM ticks.
	   1 s / TIMER_FREQ ticks
	   */
	int64_t ticks = num * TIMER_FREQ / denom;

	ASSERT (intr_get_level () == INTR_ON);
	if (ticks > 0) {
		/* We're waiting for at least one full timer tick.  Use
		   timer_sleep() because it will yield the CPU to other
		   processes. */
		timer_sleep (ticks);
	} else {
		/* Otherwise, use a busy-wait loop for more accurate
		   sub-tick timing.  We scale the numerator and denominator
		   down by 1000 to avoid the possibility of overflow. */
		ASSERT (denom % 1000 == 0);
		busy_wait (loops_per_tick * num / 1000 * TIMER_FREQ / (denom / 1000));
	}
}