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
#include "/home/ubuntu/pintos-kaist/filesys/file.c"
#include "/home/ubuntu/pintos-kaist/include/lib/kernel/stdio.h" //pubuf 호츌

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_address(void *addr);




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



void exit (int status);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);

int read(int fd, void *buffer, unsigned size);
void seek(int fd, unsigned position);

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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	int syscall_num = f->R.rax; //rax에는 시스콜 넘버가 들어있음

	switch (syscall_num)
	{
	case SYS_HALT:
		power_off();
		break;
	case SYS_EXIT:  
		exit(f->R.rdi);
		break;
	case SYS_FORK:      
		//fork(f->R.rdi);
		break;
	case SYS_EXEC:      
		//exec(f->R.rdi);
		break;
	case SYS_WAIT:      
		//wait(f->R.rdi);
		break;
	case SYS_CREATE:      
		create(f->R.rdi,f->R.rsi);
		break;
	case SYS_REMOVE:      
		remove(f->R.rdi);
		break;
	case SYS_OPEN:      
		//open(f->R.rdi);	
		break;
	case SYS_FILESIZE:      
		//filesize(f->R.rdi);
		break;
	case SYS_READ:
	/*buffer 안에 fd 로 열려있는 파일로부터 size 바이트를 읽습니다. 
	실제로 읽어낸 바이트의 수 를 반환합니다 (파일 끝에서 시도하면 0). 
	파일이 읽어질 수 없었다면 -1을 반환합니다.(파일 끝이라서가 아닌 다른 조건에 때문에 못 읽은 경우)  */     
		//read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
    case SYS_WRITE:
        write(f->R.rdi, f->R.rsi, f->R.rdx);
        break;
	case SYS_SEEK:      
		//seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:      
		//tell(f->R.rdi);	
		break;
	case SYS_CLOSE:      
		//close(f->R.rdi);
		break;
	default:
		break;
	}
	//printf ("system call!\n");
	//thread_exit ();
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


/*
#define STDIN_FILENO 0 //입력
#define STDOUT_FILENO 1 //출력
*/
int read(int fd, void *buffer, unsigned size) {
    check_address(buffer);

    if (fd == 0) {
        char *input = (char *)buffer; // void 포인터를 char 포인터로 캐스팅
        *input = input_getc(); // 키보드 입력을 buffer에 저장
        return 1;
    } else {
        // fd 값이 0이 아닌 경우, 에러를 나타내는 -1을 반환
        return -1;
    }
}

close (int fd) {
	file_close(fd);
}

/*write는 buffer 에서 size바이트 만큼 읽어서 fd 파일에 써주는 함수*/
/*
buffer : 기록할 데이터를 저장한 버퍼의 주소값
size : 기록할 데이터의 크기
*/
int write(int fd, void *buffer, unsigned size)
{
	check_address(buffer); //유효성 검사 -> write-bad-ptr 통과

	if(fd==1)
	{
		putbuf(buffer, size);//fd값이 1일때 버퍼에 저장된 데이터를 화면에 출력 (putbuf()이용)
		return sizeof(buffer); //성공시 기록한 데이터의 바이트 수를 반환
	}
	else
	{
		return size;
	}
}

void seek(int fd, unsigned position)
{
	file_seek(fd, position);
}


// int open (const char *file) {
// 	check_address(file); // 먼저 주소 유효한지 늘 체크
// 	struct file *file_obj = filesys_open(file); // 열려고 하는 파일 객체 정보를 filesys_open()으로 받기
// 	// 제대로 파일 생성됐는지 체크
// 	if (file_obj == NULL) {
// 		return -1;
// 	}
// 	//int fd = add_file_to_fd_table(file_obj); // 만들어진 파일을 스레드 내 fdt 테이블에 추가

// 	// 만약 파일을 열 수 없으면] -1을 받음
// 	if (fd == -1) {
// 		file_close(file_obj);
// 	}

// 	return fd;
// }

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


/*
파일 객체를 현재 돌고 있는 스레디프알 디스크립터 테이블에 추가해 슬드가 이 파일을 관리할 수 있도록 함.
fd
*/
int add_file_to_fd_table(struct file *file) {
	struct thread *t = thread_current();
	struct file **fdt = t->file_dt;
	int fd = t->fdidx; //fd값은 2부터 출발
	
	while (t->file_dt[fd] != NULL && fd < FDT_COUNT_LIMIT) {
		fd++;
	}

	if (fd >= FDT_COUNT_LIMIT) {
		return -1;
	}
	t->fdidx = fd;
	fdt[fd] = file;
	return fd;

}

