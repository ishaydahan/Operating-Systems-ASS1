#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "date.h"//MYCODE

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);
static int policy=0;//MYCODE

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;
  update_policy(policy);//MYCODE
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;

  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;
  for (i=0 ; i<32 ; i++) np->sig_handlers[i]=proc->sig_handlers[i];//MYCODE - fork will not change the signals
  proc->sig_pending=0;//MYCODE - but will clear the pending
  proc->handling=0;//MYCODE

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);

  safestrcpy(np->name, proc->name, sizeof(proc->name));

  pid = np->pid;

  // lock to force the compiler to emit the np->state write last.
  acquire(&ptable.lock);


  np->state = RUNNABLE;

  np->ctime=ticks;//MYCODE
  np->ttime=0;//MYCODE
  np->stime=0;//MYCODE
  np->retime=0;//MYCODE
  np->rutime=0;//MYCODE
  update_policy(policy);//MYCODE

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(int status)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(proc->cwd);
  end_op();
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->status=status;//MYCODE
  proc->ttime=ticks;//MYCODE

  proc->state = ZOMBIE;

  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(int *status)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        if (status!=0) *status=p->status;//MYCODE
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------

// Multiply-with-carry algorithem by George Marsaglia
// CMWC working parts
#define CMWC_CYCLE 4096 // as Marsaglia recommends
#define CMWC_C_MAX 809430660 // as Marsaglia recommends
static unsigned long Q[CMWC_CYCLE];
static unsigned long c = 362436; // must be limited with CMWC_C_MAX (we will reinit it with seed)
static int divider=0;

unsigned long rand(void){
  struct rtcdate time;
  cmostime(&time);
  static int ranum=0;
  ranum = ranum+9999;
  int ans = (time.second+1)*(time.minute+1)*(time.hour+1)*ranum;
  while (ans==0){
    cprintf("panic random");
    ans = (time.second+1)*(time.minute+1)*(time.hour+1)*ranum;
  }
  return ans;
}

// Make 32 bit random number (some systems use 16 bit RAND_MAX)
unsigned long rand32(void)
{
    unsigned long result = 0;
    result = rand();
    result <<= 16;
    result |= rand();
    return result;
}

// Init all engine parts with seed
void initCMWC()
{
    int i;
    for (i = 0; i < CMWC_CYCLE; i++) Q[i] = rand32();
    do c = rand32(); while (c >= CMWC_C_MAX);
}

// CMWC engine
unsigned long randCMWC(void)
{
    static unsigned long i = CMWC_CYCLE - 1;
    unsigned long long t = 0;
    unsigned long long a = 18782; // as Marsaglia recommends
    unsigned long r = 0xfffffffe; // as Marsaglia recommends
    unsigned long x = 0;

    i = (i + 1) & (CMWC_CYCLE - 1);
    t = a * Q[i] + c;
    c = t >> 32;
    x = t + c;
    if (x < c)
    {
        x++;
        c++;
    }

    Q[i] = r - x;
    if(divider==0) return 0;
    return Q[i]%divider;
}

void update_policy (int n){
  int changed = (n!=policy);
  policy=n;
  struct proc *p;
  int i=0;
  divider=0;

  if(policy==1){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state == RUNNABLE) divider=divider+1;
      p->ntickets=(1)*(p->state == RUNNABLE);      
    }
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++ ,i++){
      if (i==0){
        p->ntickets_down=0;
        p->ntickets_up=p->ntickets;
        if ((p->ntickets==0) && (p->state == RUNNABLE)) panic("0 tickets to RUNNABLE proc");
      }else{
        p->ntickets_down=ptable.proc[i-1].ntickets_up;
        p->ntickets_up=p->ntickets_down+p->ntickets;
        if ((p->ntickets==0) && (p->state == RUNNABLE)) panic("0 tickets to RUNNABLE proc");
      }
      //cprintf("down %d up %d %s\n",p->ntickets_down,  p->ntickets_up,  p->name);
    }

  }else if(policy==2){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(changed) p->priority=10;
      divider=divider+p->priority;
      p->ntickets=(1)*(p->priority)*(p->state == RUNNABLE);
    }
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++ ,i++){
      if (i==0){
        p->ntickets_down=0;
        p->ntickets_up=p->ntickets;
        if ((p->ntickets==0) && (p->state == RUNNABLE)) panic("0 tickets to RUNNABLE proc");
      }else{
        p->ntickets_down=ptable.proc[i-1].ntickets_up;
        p->ntickets_up=p->ntickets_down+p->ntickets;
        if ((p->ntickets==0) && (p->state == RUNNABLE)) panic("0 tickets to RUNNABLE proc");
      }
      //cprintf("down %d up %d %s\n",p->ntickets_down,  p->ntickets_up,  p->name);
    }

  }else if(policy==3){
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(changed) p->ntickets_p3=20;
      divider=divider+p->ntickets_p3;
      p->ntickets=(p->ntickets_p3)*(p->state == RUNNABLE);      
    }
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++ ,i++){
      if (i==0){
        p->ntickets_down=0;
        p->ntickets_up=p->ntickets;
        if ((p->ntickets==0) && (p->state == RUNNABLE)) panic("0 tickets to RUNNABLE proc");
      }else{
        p->ntickets_down=ptable.proc[i-1].ntickets_up;
        p->ntickets_up=p->ntickets_down+p->ntickets;
        if ((p->ntickets==0) && (p->state == RUNNABLE)) panic("0 tickets to RUNNABLE proc");
      }
      //cprintf("down %d up %d %s\n",p->ntickets_down,  p->ntickets_up,  p->name);
    }
    
  }else{
    //nothing
  }
  return;
}

void update_priority(int n){
  acquire(&ptable.lock);  
  proc->priority=n;  
  update_policy(policy);
  release(&ptable.lock);
}

void stat_update(void){
  struct proc *p;
  // Loop over process table looking for process to update.
  acquire(&ptable.lock);  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if (p->state==SLEEPING) p->stime++;
    else if(p->state==RUNNABLE) p->retime++;
    else if(p->state==RUNNING) p->rutime++;
    else ;
  }
  release(&ptable.lock);
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait_stat(int* status, struct perf* stat)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        if (status!=0) *status=p->status;
        stat->ctime= p->ctime;
        stat->ttime= p->ttime;
        stat->stime= p->stime;
        stat->retime= p->retime;
        stat->rutime= p->rutime;        
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

int
send_signal(int pid, int signum)
{
  int ans =1;
  ans = ans << signum;

  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid != pid)
      continue;

    p->sig_pending=p->sig_pending|ans;
    release(&ptable.lock);
    return 0;
  }
  release(&ptable.lock);
  return -1;
}

//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------
//-------------------------------------------------------------MY CODE -------------------------------------------------------------------------------

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  int myrnd=0;//MYCODE
  initCMWC();//MYCODE
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if (policy==1 || policy==2 || policy==3){//MYCODE
        myrnd=randCMWC();//MYCODE
        if (!(myrnd>=p->ntickets_down && myrnd<p->ntickets_up)) continue;//MYCODE
        if(p->state != RUNNABLE) panic("not runnable proc got ticket");//MYCODE
      }else
        if(p->state != RUNNABLE)
          continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      proc = p;
      switchuvm(p);
      p->state = RUNNING;
      update_policy(policy);//MYCODE
      swtch(&cpu->scheduler, proc->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock

  proc->state = RUNNABLE;

  //each time a process ends the quanta without performing
  //a blocking system call, the amount of the tickets owned be the process will be reduced by 1 (to the
  //minimum of 1)
  if (policy==3 && proc->ntickets_p3>1) proc->ntickets_p3=proc->ntickets_p3-1;//MYCODE
  update_policy(policy);//MYCODE

  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  proc->chan = chan;
  proc->state = SLEEPING;

  //Each time a process performs a blocking system call, it will receive additional
  //10 tickets (up to maximum of 100 tickets)
  if (policy==3 && proc->ntickets_p3<91) proc->ntickets_p3=proc->ntickets_p3+10;//MYCODE

  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{

  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == SLEEPING && p->chan == chan){
      p->state = RUNNABLE;
      update_policy(policy);//MYCODE
    }
  }

}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING){
        p->state = RUNNABLE;
        update_policy(policy);//MYCODE
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s %d", p->pid, state, p->name, p->ntickets);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
