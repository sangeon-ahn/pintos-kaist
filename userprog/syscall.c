#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

/*Project2 추가 헤더*/
#include "threads/init.h"
#include "include/devices/input.h"
#include "include/filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"
// #include "/home/ubuntu/pintos-kaist/include/lib/kernel/stdio.h" //pubuf 호츌

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);

struct lock filesys_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

/*----project2----*/
#define STDIN_FILENO 0 //입력
#define STDOUT_FILENO 1 //출력


void halt(void);
void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);

int read(int fd, void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
void seek (int fd, unsigned position);
// int write(int fd, void *buffer, unsigned size);
int write(int fd, const void* buffer, unsigned int size);

// static struct file *process_get_file(int fd);
static struct file *process_get_file(int fd);
int process_add_file(struct file *f);
void process_close_file (int fd);

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	/* 유저 스택에 저장되어 있는 시스템 콜 넘버를 가져와야지 일단 */
	int sys_number = f->R.rax; // rax: 시스템 콜 넘버
    /* 
	인자 들어오는 순서:
	1번째 인자: %rdi
	2번째 인자: %rsi
	3번째 인자: %rdx
	4번째 인자: %r10
	5번째 인자: %r8
	6번째 인자: %r9 
	*/
	// TODO: Your implementation goes here.
	switch(sys_number) {
		case SYS_HALT:
			halt();
		case SYS_EXIT:
			exit(f->R.rdi);
		case SYS_FORK:
			//fork(f->R.rdi);		
		case SYS_EXEC:
			//exec(f->R.rdi);
		case SYS_WAIT:
			//wait(f->R.rdi);
		case SYS_CREATE:
			create(f->R.rdi, f->R.rsi);		
		case SYS_REMOVE:
			remove(f->R.rdi);		
		case SYS_OPEN:
			open(f->R.rdi);		
		case SYS_FILESIZE:
			filesize(f->R.rdi);
		case SYS_READ:
			read(f->R.rdi, f->R.rsi, f->R.rdx);
		case SYS_WRITE:
			write(f->R.rdi, f->R.rsi, f->R.rdx);		
		case SYS_SEEK:
			//seek(f->R.rdi, f->R.rsi);		
		case SYS_TELL:
			//tell(f->R.rdi);		
		case SYS_CLOSE:
			//close(f->R.rdi);	
	}
	// printf ("system call!\n");
	// thread_exit ();
}
/* pintos 종료시키는 함수 */
void halt(void){
	power_off();
}

void exit (int status) {
		struct thread *curr= thread_current();
		curr -> exit_status = status;
		printf("%s: exit(%d)\n",curr->name, status);
		thread_exit();
}

bool create (const char *file, unsigned initial_size) {
	check_address(file);
	if (filesys_create(file, initial_size))
		return true;
	else
		return false;
}

bool remove (const char *file) {
	check_address(file);
	if (filesys_remove(file))
		return true;
	else
		return false;
}

/*-------파일디스크립터 함수--------*/
int open (const char *file) {
	check_address(file); // 먼저 주소 유효한지 늘 체크
	struct file *f =filesys_open(file);
	int fd = process_add_file(f);

	if(f ==NULL)
		return -1;
	if (fd == -1)
		file_close(f);
	return fd;
}

int filesize(int fd){
	struct file *f = process_get_file(fd);
	if (f == NULL) return -1;
	return file_length(f);
}

/*
#define STDIN_FILENO 0 //입력
#define STDOUT_FILENO 1 //출력
fd가 STDIN에 해당하는 경우엔 input_getc()를 통해 최대 size만큼의 바이트를 읽어주고, 실제로 읽어들이는데 성공한 크기를 readsize로 리턴
fd가 일반 파일에 해당하는 경우엔, filesys_lock 을 활용해 파일을 안전하게 읽어들이도록 함fd가 유효한 파일이면, read는 이 filesys_lock을 
획득해 파일을 안전하게 읽어들이고, 다 읽으면 락을 해제
*/
/*
read는, fd 에서 size 바이트만큼 읽어서 buffer에 저장해주는 함수
*/
int read(int fd, void *buffer, unsigned size) {
    check_address(buffer); //유효성 검사
	unsigned char *buf = buffer;
	int readsize;

	struct file *f = process_get_file(fd);

	if (f == NULL) return -1;
	if (f == STDOUT_FILENO) return -1;

    if (fd == STDIN_FILENO) {
		for(readsize=0; readsize<size; readsize++){
		char c = input_getc();
		*buf++ = c;
		if (c =='\0');
			break;
			}
		} 
	else {
		readsize = file_read(f, buffer,size);
    }
	return readsize;
}
/*write는 buffer 에서 size바이트 만큼 읽어서 fd 파일에 써주는 함수*/
/*
buffer : 기록할 데이터를 저장한 버퍼의 주소값
size : 기록할 데이터의 크기
*/
int write(int fd, const void* buffer, unsigned int size)
{
	check_address(buffer); //유효성 검사 -> write-bad-ptr 통과
	struct file *f = process_get_file(fd);
	int writesize;

	if (f == NULL) return -1;
	if (f == STDOUT_FILENO) return -1;

	if(fd==STDIN_FILENO)
	{
		putbuf(buffer, size);//fd값이 1일때 버퍼에 저장된 데이터를 화면에 출력 (putbuf()이용)
		writesize = size; //성공시 기록한 데이터의 바이트 수를 반환
	}
	else
	{	lock_acquire(&filesys_lock);
		writesize = file_write(f,buffer,size);
		lock_release(&filesys_lock);
	}
	return writesize;
}

// int write(int fd, const void* buffer, unsigned int size){
//   if(fd==1){
//     putbuf(buffer, size);
//     return size;
//   }
//   return -1;
// }

/*
seek은 fd에서 read하거나 write할 다음 위치를, position으로 변경해주는 함수
seek 시스템콜은 이미 존재하는 함수인 file_seek호출
*/
void seek (int fd, unsigned position) {
	struct file *f = process_get_file(fd);
	if (f>2)
		file_seek(f, position);
}
/*
tell은, seek에서 조정해주는 fd의 pos를 그대로 가져오는 함수
번호가 fd인 fdt로 열어준 파일에서, Read혹은 write 해줄 다음 위치를 리턴해줌
파일 시작점 기준으로 byte 단위로 리턴해줌
*/
unsigned tell (int fd) {
	struct file *f = process_get_file(fd);
	if (f>2)
		file_tell(f);
}

/*
close는 fd에 해당하는 파일과 파일 디스크립터를 닫아주는 함수
번호가 fd인 파일디스크립터를 닫아준다.
fd에 해당하는 파일을 현재 실행중인 쓰레디의 fd테이블에서는 null로 바꿔 제거해주고
file_close함수를 호출해서 해당 파일 자체를 닫아준다.
*/
void close (int fd){
	struct file *f = process_get_file(fd);

	if(f == NULL)
		return;
	if(fd < 2) return;
    
	process_close_file(fd);
	file_close(f);
}
/*---------project2-----------*/
/*
		check_address 구현 
/*1.포인터가 가리키는 주소가 유저영역의 주소인지 확인 ->이미 구현되어있는 is_user_vaddr() 사용
  2. 잘못된 접근일 경우 프로세스 종료 -> exit(-1)
  3. 포인터가 가리키는 주소가 유저 영역 내에 있지만 페이지로 할당하지 않은 영역일 수도 있다.- > 페이지 할당영역인지 확인
  이미 구현되어있는 pml4_get_page()를 사용해 유저 가상 주소와 대응하는 물리주소를 확인해 해당 물리 주소와 연결된
  커널 가장 주소를 반환나하거나 해당 물리조사가 가상주소와 매핑되지 않은 영역이면 null을 반환
*/
void check_address(void *addr)
{
    if (addr == NULL)
        exit(-1);
    if (!is_user_vaddr(addr))
        exit(-1);
    if (pml4_get_page(thread_current()->pml4, addr) == NULL)
        exit(-1);
}


/*------------파일 디스크립터 관련함수 ----------*/

/*프로세스의 파일 디스크립터 테이블을 검색하여 파일 객체의 주소를 리턴*/
/*현재 실행중인 쓰레드의 파일 디스크립터 테이블의 fd에 NULL이 아니라 무언가가 들어있는지 확인해주는 함수*/
// struct file *process_get_file(int fd){
// 	if(fd<0 || fd>=FDT_COUNT_LIMIT)
// 		return NULL;
// 	struct file *f = thread_current() -> file_dt[fd];
// 	return f;
// }
// struct file *process_get_file (int fd)
// {
// 	struct thread *curr = thread_current ();
// 	if (fd < 0 || fd >= FDT_COUNT_LIMIT)
// 		return NULL;

// 	return curr->file_dt[fd];
// }
struct file *process_get_file (int fd){
	if (fd < 0 || fd >= FDT_COUNT_LIMIT)
		return NULL;
	struct file *f = thread_current()->file_dt[fd];
	return f;
}
/*
현재 실행중인 쓰레드의 fd_table에서 주어진 파일 디스크립터(fd)를 NULL로 변경해 닫아주는 함수
*/
void process_close_file (int fd){
	if(fd<0 || fd>=FDT_COUNT_LIMIT)
		return NULL;
	struct file *f = thread_current() -> file_dt[fd];
	file_close(f);
}
/*
파일 객체에 대한 파일 디스크립터 생성
어떤 파일 구조체가 주어졌을 때, 현재 쓰레드의 fd_table에 빈 공간이 있는 경우, 거기에 f를 추가해주는 함수
*/                      
int process_add_file(struct file *f){
	struct thread *curr = thread_current();
	struct file **curr_fd_table = curr->file_dt;

	for(int index = curr ->fdidx; index<FDT_COUNT_LIMIT; index++){
		if(curr_fd_table[index] == NULL){
			curr_fd_table[index] == NULL;
			curr->fdidx-index;
			return curr->fdidx;
		}
	}
	return -1;
}