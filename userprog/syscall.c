#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/file.h"

void syscall_entry(void);
void syscall_handler(struct intr_frame *);
// int write_(int fd, const void *buffer, unsigned int size);
int write_(int fd, void *buffer, unsigned size);
void check_address(void *addr);
bool create_(const char *file, unsigned initial_size);
bool remove_(const char *file);
int exit_(int status);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* Segment selector msr */
#define MSR_LSTAR 0xc0000082		/* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */
#define FDCOUNT_LIMIT 
void syscall_init(void)
{
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// TODO: Your implementation goes here.
	struct thread *curr_t = thread_current();
	switch (f->R.rax)
	{
	case SYS_HALT: // power off! 핀토스를 종료, 강종 느낌
		power_off();
		break;
	case SYS_EXIT:
		// 유저: exit(EXIT_NUM, status) 호출
		// 프로세스 디스크립터에 exit_status값 저장
		curr_t->exit_status = f->R.rdi;
		// curr_t->parent_p->dead_child[child_pid] = curr_t->exit_status; // dead_threads 배열에는 자식 쓰레드의 exit_status가 들어간다.
		printf("%s: exit(%d)\n", curr_t->name, f->R.rdi);
		thread_exit();
		break;
	case SYS_FORK:
		// struct thread* curr_t = thread_current();
		f->R.rax = process_fork(f->R.rdi, f);
		break;
	case SYS_EXEC:
		f->R.rax = f->R.rdi;					// exec() 시스템 콜 호출 후 반환 값은 인터럽트 프레임의 exa에 저장
		// exec();
		break;
	case SYS_WAIT:
		f->R.rax = process_wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create_(f->R.rdi, f->R.rsi);
		printf("%s: create(%d)\n", curr_t->name, f->R.rdi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove_(f->R.rdi);
		printf("%s: remove\n", curr_t->name);
		break;
	case SYS_OPEN:
		f->R.rax = open_(f->R.rdi);
		printf("%s: open\n", curr_t->name);
		break;
	case SYS_FILESIZE:
		f->R.rax = file_length(curr_t->file_dt[curr_t->fdidx]);
		// filesize();
		break;
	case SYS_READ:
		// read();
		break;
	case SYS_WRITE:
		write_(f->R.rdi, f->R.rsi, f->R.rdx);
		// printf("%s", f->R.rsi);
		// write();
		break;
	case SYS_SEEK:
		// seek();
		break;
	case SYS_TELL:

		break;
	case SYS_CLOSE:

		break;
	default:
		break;
	}
}
void check_address(void *addr)
{
	// /*주소 값이 유저 영역에서 사용하는 주소 값인지 확인 하는 함수
	// 		유저 영역을 벗어난 영역일 경우 프로세스
	// 		종료(exit(-1)) *
	// 	/
	if (!is_user_vaddr(addr))
	{ // 유저 영억이 아니거나,
		exit_(-1);
	}
	if (addr == NULL)
	{ // null이면 프로세스 종료.
		exit_(-1);
	}
	// 	if (pml4_get_page(thread_current()->pml4, addr) == NULL)
	// 		exit(-1);
}
bool create_(const char *file, unsigned initial_size)
{
	// 파일 생성에 성공했다면 true, 실패했다면 false
	check_address(file);
	return filesys_create(file, initial_size);
}

bool remove_(const char *file)
{
	check_address(file);
	return filesys_remove(file);
}

int open_(const char *file_name)
{
	check_address(file_name);
	// 동시성 제어 필요
	struct file *curr_file = filesys_open(file_name);
	if (curr_file == NULL) // 파일 열기 실패
	{
		return -1;
	}
	int fd = process_add_file(curr_file); // 파일 디스크립터 할당
	if (fd == -1)
	{
		return -1;
	}
	else
	{
		return fd;
	}
}

int exit_(int status)
{
	thread_exit();
	return status;
}
int write_(int fd, void *buffer, unsigned size)
{
	// check_address(buffer); //유효성 검사 -> write-bad-ptr 통과

	if (fd == 1)
	{
		putbuf(buffer, size);  // fd값이 1일때 버퍼에 저장된 데이터를 화면에 출력 (putbuf()이용)
		return sizeof(buffer); // 성공시 기록한 데이터의 바이트 수를 반환
	}
	else
	{
		return size;
	}
}

// int exec_(const *cmd_line){
// 	//자식 프로세스를 생성하고 프로그램을 실행시키는 시스템 콜
// 	//프로세스를 생성하는 함수 이용
// 	//프로세스 생성에 성공시 생성된 프로세스의 pid값을 반환, 실패시 -1반환
// 	//부모 프로세스는 자식 프로세스의 응용 프로그램이 메모리에 탑재 될때까지 대기
// 	//sema_down
// 	//cmd_line : 새로운 프로세스에 실행할 프로그램 명령어
// 	//메모리 탑재 성공시 유저 프로그램 실행, 실패시 스레드 종료
// }

struct file *fd_to_struct_filep(int d){
	if(fd < 0 || fd >= FDCOUNT_LIMIT) {
		return NULL;
	}
}