#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{

  int status;
  if(argint(0, &status) < 0)
    return -1;

  exit(status);
  return 0;  // not reached
}

int
sys_wait(void)
{
  int *status=0;
  if(argptr(0, (char**)&status, 1) < 0)
    return -1;

  return wait(status);
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = proc->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int 
sys_sigsend(void){
  int pid;
  int signum;
  if(argint(1, &signum) < 0 || argint(0, &pid) < 0)
    return -1;
  if(signum>31 ||signum<0)
    return -1;
  if(pid==0)
    return -1;

  return send_signal(pid, signum);
}

int 
sys_signal(void){
  sighandler_t sighandler;
  int signum;
  if(argint(0, &signum) < 0 || argptr(1, (void*)&sighandler, 4) < 0)
    return -1;
  if(signum>31 ||signum<0)
    return -1;

  sighandler_t prev = proc-> sig_handlers[signum];
  proc-> sig_handlers[signum]=sighandler+1;
  if ((int)prev==0) return 0xfffffffe;
  else return (int)prev-1;
}

int 
sys_sigreturn(void){

  proc->tf->edi=        proc->tfb.edi;
  proc->tf->esi=        proc->tfb.esi;
  proc->tf->ebp=        proc->tfb.ebp;
  proc->tf->oesp=       proc->tfb.oesp;     
  proc->tf->ebx=        proc->tfb.ebx;
  proc->tf->edx=        proc->tfb.edx;
  proc->tf->ecx=        proc->tfb.ecx;
  proc->tf->eax=        proc->tfb.eax;
  proc->tf->gs=         proc->tfb.gs;
  proc->tf->padding1=   proc->tfb.padding1;
  proc->tf->fs=         proc->tfb.fs;
  proc->tf->padding2=   proc->tfb.padding2;
  proc->tf->es=         proc->tfb.es;
  proc->tf->padding3=   proc->tfb.padding3;
  proc->tf->ds=         proc->tfb.ds;
  proc->tf->padding4=   proc->tfb.padding4;
  proc->tf->trapno=     proc->tfb.trapno;
  proc->tf->err=        proc->tfb.err;
  proc->tf->eip=        proc->tfb.eip;
  proc->tf->cs=         proc->tfb.cs;
  proc->tf->padding5=   proc->tfb.padding5;
  proc->tf->eflags=     proc->tfb.eflags;
  proc->tf->esp=        proc->tfb.esp;
  proc->tf->ss=         proc->tfb.ss;
  proc->tf->padding6=   proc->tfb.padding6;
  proc->handling=0;
    
  return proc->tf->eax;
}

int
sys_schedp(void){
  int n;
  if(argint(0, &n) < 0)
    return -1;
  if(n>3 || n<0)
    return -1;
  
  update_policy(n);

  return 0;
}

int
sys_priority(void){
  int n;
  
  if(argint(0, &n) < 0)
    return -1;
  if(n>1000000 || n<0)
    return -1;

  update_priority(n);
  return 0;

}

int
sys_wait_stat(void){
  int *status=0;
  struct perf *stat;

  if(argptr(0, (char**)&status, 1) < 0 || argptr(1, (void*)&stat, sizeof(*stat)) < 0)
    return -1;

  return wait_stat(status, stat);
}

int
sys_running_time(void){
  return proc->rutime;
}