// zlox_syscall.c Defines the implementation of a system call system.

#include "zlox_syscall.h"
#include "zlox_isr.h"
#include "zlox_monitor.h"
#include "zlox_keyboard.h"
#include "zlox_elf.h"
#include "zlox_task.h"
#include "zlox_kheap.h"
#include "zlox_fs.h"
#include "zlox_paging.h"
#include "zlox_ata.h"

static ZLOX_VOID zlox_syscall_handler(ZLOX_ISR_REGISTERS * regs);
// _zlox_reboot() and _zlox_shutdown() is in zlox_shutdown.s
ZLOX_VOID _zlox_reboot();
ZLOX_VOID _zlox_shutdown();
//  _zlox_idle_cpu is in zlox_process.s
ZLOX_VOID _zlox_idle_cpu();

ZLOX_DEFN_SYSCALL1(monitor_write, ZLOX_SYSCALL_MONITOR_WRITE, const char*);
ZLOX_DEFN_SYSCALL1(monitor_write_hex, ZLOX_SYSCALL_MONITOR_WRITE_HEX, ZLOX_UINT32);
ZLOX_DEFN_SYSCALL1(monitor_write_dec, ZLOX_SYSCALL_MONITOR_WRITE_DEC, ZLOX_UINT32);
ZLOX_DEFN_SYSCALL1(monitor_put, ZLOX_SYSCALL_MONITOR_PUT, char);
ZLOX_DEFN_SYSCALL1(execve, ZLOX_SYSCALL_EXECVE, const char*);
ZLOX_DEFN_SYSCALL3(get_tskmsg,ZLOX_SYSCALL_GET_TSKMSG,void *,void *,ZLOX_BOOL);
ZLOX_DEFN_SYSCALL1(wait, ZLOX_SYSCALL_WAIT, void *);
ZLOX_DEFN_SYSCALL1(set_input_focus, ZLOX_SYSCALL_SET_INPUT_FOCUS, void *);
ZLOX_DEFN_SYSCALL0(get_currentTask,ZLOX_SYSCALL_GET_CURRENT_TASK);
ZLOX_DEFN_SYSCALL1(exit, ZLOX_SYSCALL_EXIT, int);
ZLOX_DEFN_SYSCALL1(finish, ZLOX_SYSCALL_FINISH, void *);
ZLOX_DEFN_SYSCALL1(get_args, ZLOX_SYSCALL_GET_ARGS, void *);
ZLOX_DEFN_SYSCALL1(get_init_esp, ZLOX_SYSCALL_GET_INIT_ESP, void *);
ZLOX_DEFN_SYSCALL1(umalloc, ZLOX_SYSCALL_UMALLOC, int);
ZLOX_DEFN_SYSCALL1(ufree, ZLOX_SYSCALL_UFREE, void *);
ZLOX_DEFN_SYSCALL4(read_fs,ZLOX_SYSCALL_READ_FS,void *,int,int,void *);
ZLOX_DEFN_SYSCALL2(readdir_fs,ZLOX_SYSCALL_READDIR_FS,void *,int);
ZLOX_DEFN_SYSCALL2(finddir_fs,ZLOX_SYSCALL_FINDDIR_FS,void *,void *);
ZLOX_DEFN_SYSCALL0(get_fs_root,ZLOX_SYSCALL_GET_FS_ROOT);
ZLOX_DEFN_SYSCALL2(get_frame_info,ZLOX_SYSCALL_GET_FRAME_INFO,void **,void *);
ZLOX_DEFN_SYSCALL0(get_kheap,ZLOX_SYSCALL_GET_KHEAP);
ZLOX_DEFN_SYSCALL3(get_version,ZLOX_SYSCALL_GET_VERSION,void *,void *,void *);
ZLOX_DEFN_SYSCALL0(reboot,ZLOX_SYSCALL_REBOOT);
ZLOX_DEFN_SYSCALL0(shutdown,ZLOX_SYSCALL_SHUTDOWN);
ZLOX_DEFN_SYSCALL0(idle_cpu,ZLOX_SYSCALL_IDLE_CPU);
ZLOX_DEFN_SYSCALL3(atapi_drive_read_sector,ZLOX_SYSCALL_ATAPI_DRIVE_READ_SECTOR,ZLOX_UINT32,ZLOX_UINT32,void *);
ZLOX_DEFN_SYSCALL2(atapi_drive_read_capacity,ZLOX_SYSCALL_ATAPI_DRIVE_READ_CAPACITY,ZLOX_UINT32,void *);
ZLOX_DEFN_SYSCALL0(ata_get_ide_info,ZLOX_SYSCALL_ATA_GET_IDE_INFO);

static ZLOX_VOID * syscalls[ZLOX_SYSCALL_NUMBER] =
{
	&zlox_monitor_write,
	&zlox_monitor_write_hex,
	&zlox_monitor_write_dec,
	&zlox_monitor_put,
	&zlox_execve,
	&zlox_get_tskmsg,
	&zlox_wait,
	&zlox_set_input_focus,
	&zlox_get_currentTask,
	&zlox_exit,
	&zlox_finish,
	&zlox_get_args,
	&zlox_get_init_esp,
	&zlox_umalloc,
	&zlox_ufree,
	&zlox_read_fs,
	&zlox_readdir_fs,
	&zlox_finddir_fs,
	&zlox_get_fs_root,
	&zlox_get_frame_info,
	&zlox_get_kheap,
	&zlox_get_version,
	&_zlox_reboot,
	&_zlox_shutdown,
	&_zlox_idle_cpu,
	&zlox_atapi_drive_read_sector,
	&zlox_atapi_drive_read_capacity,
	&zlox_ata_get_ide_info,
};

ZLOX_UINT32 num_syscalls = ZLOX_SYSCALL_NUMBER;

ZLOX_VOID zlox_initialise_syscalls()
{
	// Register our syscall handler.
	zlox_register_interrupt_callback (0x80, &zlox_syscall_handler);
}

static ZLOX_VOID zlox_syscall_handler(ZLOX_ISR_REGISTERS * regs)
{
	// Firstly, check if the requested syscall number is valid.
	// The syscall number is found in EAX.
	if (regs->eax >= num_syscalls)
		return;

	// Get the required syscall location.
	ZLOX_VOID * location = syscalls[regs->eax];

	ZLOX_UINT32 oldesp,newesp; 

	// We don't know how many parameters the function wants, so we just
	// push them all onto the stack in the correct order. The function will
	// use all the parameters it wants, and we can pop them all back off afterwards.
	ZLOX_SINT32 ret;

	asm volatile("mov %%esp, %0" : "=r"(oldesp));

	asm volatile (" \
	  push %1; \
	  push %2; \
	  push %3; \
	  push %4; \
	  push %5; \
	  call *%6; \
	  pop %%ebx; \
	  pop %%ebx; \
	  pop %%ebx; \
	  pop %%ebx; \
	  pop %%ebx; \
	" : "=a" (ret) : "D" (regs->edi), "S" (regs->esi), "d" (regs->edx), "c" (regs->ecx), "b" (regs->ebx), "0" (location));

	asm volatile("mov %%esp, %0" : "=r"(newesp));

	if(oldesp != newesp)
	{
		if(oldesp > newesp)
			regs = (ZLOX_ISR_REGISTERS *)((ZLOX_UINT32)regs - (oldesp - newesp));
		else
			regs = (ZLOX_ISR_REGISTERS *)((ZLOX_UINT32)regs + (newesp - oldesp));
	}

	if(regs->eax == ZLOX_SYSCALL_EXECVE && ret > 0)
	{
		regs->eip = ret;
	}

	regs->eax = ret;
}

ZLOX_SINT32 zlox_get_version(ZLOX_SINT32 * major, ZLOX_SINT32 * minor, ZLOX_SINT32 * revision)
{
	*major = ZLOX_MAJOR_VERSION;
	*minor = ZLOX_MINOR_VERSION;
	*revision = ZLOX_REVISION;
	return 0;
}

