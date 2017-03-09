#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "syscall.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
extern void sigret(void);
struct spinlock tickslock;
uint ticks;

void
sig_check(struct trapframe *tf){
  //we are not is user space or proc null
  if(proc == 0 || (tf->cs&3) != DPL_USER) return;
  proc->tf=tf;
  //if there are pending signals
  if(proc->sig_pending!=0){
    int ans=1;
    int tempans=0;
    char command[16];
    char i;
    for(i=0; i<32; i++){
      tempans=ans&proc->sig_pending;
      if(tempans!=0){
        //deafult handler
        if (proc->sig_handlers[(int)i]==0){
            if (proc->handling) return;
            proc->handling=1;          
            proc->sig_pending=proc->sig_pending&(~ans);
            cprintf("A signal %d was accepted by process %d\n",(int)i, proc->pid);
            proc->handling=0;            
        //non deafualt handler    
        }else{
            if (proc->handling) return;
            proc->handling=1;
            proc->sig_pending=proc->sig_pending&(~ans);
            //backup trapframe
            proc->tfb.edi=       proc->tf->edi;
            proc->tfb.esi=       proc->tf->esi;
            proc->tfb.ebp=       proc->tf->ebp;
            proc->tfb.oesp=      proc->tf->oesp;     
            proc->tfb.ebx=       proc->tf->ebx;
            proc->tfb.edx=       proc->tf->edx;
            proc->tfb.ecx=       proc->tf->ecx;
            proc->tfb.eax=       proc->tf->eax;
            proc->tfb.gs=        proc->tf->gs;
            proc->tfb.padding1=  proc->tf->padding1;
            proc->tfb.fs=        proc->tf->fs;
            proc->tfb.padding2=  proc->tf->padding2;
            proc->tfb.es=        proc->tf->es;
            proc->tfb.padding3=  proc->tf->padding3;
            proc->tfb.ds=        proc->tf->ds;
            proc->tfb.padding4=  proc->tf->padding4;
            proc->tfb.trapno=    proc->tf->trapno;
            proc->tfb.err=       proc->tf->err;
            proc->tfb.eip=       proc->tf->eip;
            proc->tfb.cs=        proc->tf->cs;
            proc->tfb.padding5=  proc->tf->padding5;
            proc->tfb.eflags=    proc->tf->eflags;
            proc->tfb.esp=       proc->tf->esp;
            proc->tfb.ss=        proc->tf->ss;
            proc->tfb.padding6=  proc->tf->padding6;
            //fake ret add
            command[0] = 0xff;
            command[1] = 0xff;
            command[2] = 0xff;
            command[3] = 0xff;
            //args
            command[4] = i;
            command[5] = 0x00;
            command[6] = 0x00;
            command[7] = 0x00;
            //mov eax,SYS_sigreturn
            command[8] = 0xb8;
            command[9] = SYS_sigreturn;
            command[10] = 0x00;
            command[11] = 0x00;
            command[12] = 0x00;
            //int 40
            command[13] = 0xcd;
            command[14] = 0x40;
            command[15] = 0x00;
            //copy command to stack-16
            memmove((void*)proc->tf->esp-16, command, 16);
            //grab add of sigret(stack-8)
            uint a = proc->tf->esp-8;
            //replace between fake add and sigret add
            memmove((void*)proc->tf->esp-16, &a, 4);
            //update "new" stack add
            proc->tf->esp=proc->tf->esp-16;
            //update new ip
            proc->tf->eip=(uint)proc->sig_handlers[(int)i]-1;

            return;
        }
      }
      ans=ans<<1;
    }
  }
return;
}


void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
  
  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(proc->killed)
      exit(0);
    proc->tf = tf;
    syscall();
    if(proc->killed)
      exit(0);
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpu->id == 0){
      acquire(&tickslock);
      ticks++;
      stat_update();
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpu->id, tf->cs, tf->eip);
    lapiceoi();
    break;
   
  //PAGEBREAK: 13
  default:
    if(proc == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpu->id, tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            proc->pid, proc->name, tf->trapno, tf->err, cpu->id, tf->eip, 
            rcr2());
    proc->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running 
  // until it gets to the regular system call return.)
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit(0);

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(proc && proc->state == RUNNING && tf->trapno == T_IRQ0+IRQ_TIMER)
    yield();

  // Check if the process has been killed since we yielded
  if(proc && proc->killed && (tf->cs&3) == DPL_USER)
    exit(0);
}
