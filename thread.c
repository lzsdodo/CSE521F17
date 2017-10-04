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
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/fix-point.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

static struct list sleep_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame
{
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
};

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

fixed_t load_avg;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);


void refreshPriority(struct thread *t, int priority, struct lock *lock);
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
void thread_init (void) {
    ASSERT (intr_get_level () == INTR_OFF);

    lock_init (&tid_lock);
    list_init (&ready_list);
    list_init (&all_list);
    list_init (&sleep_list);

    /* Set up a thread structure for the running thread. */
    initial_thread = running_thread ();
    init_thread (initial_thread, "main", PRI_DEFAULT);
    initial_thread->status = THREAD_RUNNING;
    initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void thread_start (void)
{
    load_avg = FP_CONST(0);
    /* Create the idle thread. */
    struct semaphore idle_started;
    sema_init (&idle_started, 0);
    thread_create ("idle", PRI_MIN, idle, &idle_started);

    /* Start preemptive thread scheduling. */
    intr_enable ();

    /* Wait for the idle thread to initialize idle_thread. */
    sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void thread_tick (void)
{
    struct thread *t = thread_current ();

    /* Update statistics. */
    if (t == idle_thread)
        idle_ticks++;
#ifdef USERPROG
        else if (t->pagedir != NULL)
    user_ticks++;
#endif
    else
        kernel_ticks++;

    /* Enforce preemption. */
    if (++thread_ticks >= TIME_SLICE)
        intr_yield_on_return ();
}

/* Prints thread statistics. */
void thread_print_stats (void) {
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
tid_t thread_create (const char *name, int priority, thread_func *function, void *aux)
{
    struct thread *t;
    struct kernel_thread_frame *kf;
    struct switch_entry_frame *ef;
    struct switch_threads_frame *sf;
    tid_t tid;
    enum intr_level old_level;

    ASSERT (function != NULL);

    /* Allocate thread. */
    t = palloc_get_page (PAL_ZERO);
    if (t == NULL)
        return TID_ERROR;

    /* Initialize thread. */
    init_thread (t, name, priority);
    tid = t->tid = allocate_tid ();



    /* Prepare thread for first run by initializing its stack.
       Do this atomically so intermediate values for the 'stack'
       member cannot be observed. */
    old_level = intr_disable ();

    /* Stack frame for kernel_thread(). */
    kf = alloc_frame (t, sizeof *kf);
    kf->eip = NULL;
    kf->function = function;
    kf->aux = aux;

    /* Stack frame for switch_entry(). */
    ef = alloc_frame (t, sizeof *ef);
    ef->eip = (void (*) (void)) kernel_thread;

    /* Stack frame for switch_threads(). */
    sf = alloc_frame (t, sizeof *sf);
    sf->eip = switch_entry;
    sf->ebp = 0;

    intr_set_level (old_level);

    /* Add to run queue. */
    thread_unblock (t);

    return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().
   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void thread_block (void)
{
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

/*******************************************
    modifyed as insert ordered
  *******************************************/
void thread_unblock (struct thread *t)
{
    enum intr_level old_level;
    struct thread *cur=thread_current();
    ASSERT (is_thread (t));
    ASSERT (t->status == THREAD_BLOCKED);
    old_level = intr_disable ();
    list_insert_ordered(&ready_list, &t->elem, cmpPriority, NULL);
    t->status = THREAD_READY;

    if(cur!=idle_thread) {
        if(t->priority > cur->priority ){
            thread_yield();
        }
    }
    intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *thread_name (void)
{
    return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *thread_current (void)
{
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
tid_t thread_tid (void) {
    return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void thread_exit (void)
{
    ASSERT (!intr_context ());

#ifdef USERPROG
    process_exit ();
#endif

    /* Remove thread from all threads list, set our status to dying,
       and schedule another process.  That process will destroy us
       when it call schedule_tail(). */
    intr_disable ();
    list_remove (&thread_current()->allelem);
    thread_current ()->status = THREAD_DYING;
    schedule ();
    NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
/******************** in priority scheduling, the current thread is sorted again and put in the proper place according to priority ***********************/
/******good thing is, since a thread's priority can change often, this operation accounts for that uncertainty***/
void thread_yield (void)
{
    struct thread *cur = thread_current ();
    enum intr_level old_level = intr_disable ();
    ASSERT (!intr_context ());
    if (cur != idle_thread)  list_push_back(&ready_list, &cur->elem);
    cur->status = THREAD_READY;
    schedule ();
    intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void thread_foreach (thread_action_func *func, void *aux)
{
    struct list_elem *e;

    ASSERT (intr_get_level () == INTR_OFF);

    for (e = list_begin (&all_list); e != list_end (&all_list);
         e = list_next (e))
    {
        struct thread *t = list_entry (e, struct thread, allelem);
        func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
/************************************************************************************
If the thread wants to increase  priority, ok
If the thread wants to lower priority:
            If it has been donated priority, then its actual starting priority is set to the value.
            If it has not been donated, set its priority to new_priority and yield to cpu
**************************************************************************************/
void thread_set_priority (int new_priority) {
    struct thread *cur=thread_current();
    /*** if the current threads's priority is larger than the value to reset, checks are performed***/
    if(new_priority < cur->priority)
    {
        /*** if the current threads has not been donated locks and thus priority, set it to new priority**/
        if(!cur->donatedLock)
        {
            cur->priority = new_priority;
            list_sort(&ready_list,cmpPriority,NULL);
            struct thread *topThread = list_entry(list_begin(&ready_list), struct thread, elem);
            /*** perform yield on current thread**/
            if(new_priority < topThread->priority)thread_yield();
        }
        else {
            list_entry( list_back(&cur->listOfInfo), struct info, elem)-> histPriority = new_priority;
        }
    }
        /** if it wants to incrase priority no problem*/
    else {
        cur->priority = new_priority;
    }
}

/* Returns the current thread's priority. */
int thread_get_priority (void)
{
    return thread_current ()->priority;
}

/* Idle thread.  Executes when no other thread is ready to run.
   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void idle (void *idle_started_ UNUSED)
{
    struct semaphore *idle_started = idle_started_;
    idle_thread = thread_current ();
    sema_up (idle_started);

    for (;;)
    {
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
static void kernel_thread (thread_func *function, void *aux)
{
    ASSERT (function != NULL);

    intr_enable ();       /* The scheduler runs with interrupts off. */
    function (aux);       /* Execute the thread function. */
    thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *running_thread (void)
{
    uint32_t *esp;

    /* Copy the CPU's stack pointer into `esp', and then round that
       down to the start of a page.  Because `struct thread' is
       always at the beginning of a page and the stack pointer is
       somewhere in the middle, this locates the curent thread. */
    asm ("mov %%esp, %0" : "=g" (esp));
    return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool is_thread (struct thread *t)
{
    return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */

/**********************  perform initializations thareads     *************************************************/
static void init_thread (struct thread *t, const char *name, int priority)
{
    ASSERT (t != NULL);
    ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
    ASSERT (name != NULL);

    memset (t, 0, sizeof *t);
    t->status = THREAD_BLOCKED;
    strlcpy (t->name, name, sizeof t->name);
    t->stack = (uint8_t *) t + PGSIZE;
    t->priority = priority;
    t->magic = THREAD_MAGIC;

    /**********************a    alarm *    *************************************************/
    t->ticksRemain = -10;
    /**********************  priority scheduling    **************************************************/
    t->targetLock = NULL;
    t->donatedLock = NULL;
    list_init(&t->listOfInfo);

    t->nice = 0;
    t->recent_cpu = FP_CONST (0);

    list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *alloc_frame (struct thread *t, size_t size)
{
    /* Stack data is always allocated in word-size units. */
    ASSERT (is_thread (t));
    ASSERT (size % sizeof (uint32_t) == 0);

    t->stack -= size;
    return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
/**********************  pop the thread with highest priority    **************************************************/
static struct thread *next_thread_to_run (void)
{
    if (list_empty (&ready_list))
        return idle_thread;
    else{
        list_sort(&ready_list,cmpPriority,NULL);
        return list_entry (list_pop_front (&ready_list), struct thread, elem);
    }

}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.
   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).
   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.
   After this function and its caller returns, the thread switch
   is complete. */
void thread_schedule_tail (struct thread *prev)
{
    struct thread *cur = running_thread ();

    ASSERT (intr_get_level () == INTR_OFF);

    /* Mark us as running. */
    cur->status = THREAD_RUNNING;

    /* Start new time slice. */
    thread_ticks = 0;

#ifdef USERPROG
    /* Activate the new address space. */
  process_activate ();
#endif

    /* If the thread we switched from is dying, destroy its struct
       thread.  This must happen late so that thread_exit() doesn't
       pull out the rug under itself.  (We don't free
       initial_thread because its memory was not obtained via
       palloc().) */
    if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread)
    {
        ASSERT (prev != cur);
        palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void schedule (void)
{
    struct thread *cur = running_thread ();
    struct thread *next = next_thread_to_run ();
    struct thread *prev = NULL;

    ASSERT (intr_get_level () == INTR_OFF);
    ASSERT (cur->status != THREAD_RUNNING);
    ASSERT (is_thread (next));

    if (cur != next)
        prev = switch_threads (cur, next);
    thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t allocate_tid (void)
{
    static tid_t next_tid = 1;
    tid_t tid;

    lock_acquire (&tid_lock);
    tid = next_tid++;
    lock_release (&tid_lock);

    return tid;
}

uint32_t thread_stack_ofs = offsetof (struct thread, stack);




/*********************************************************************************
Part of priority donation nested, called in lock_acquire, lock_release
******************************************************************************************/
void refreshPriority(struct thread *t, int targetP, struct lock *dueTolock) {

    struct list_elem *e;
    struct info *tempInfo = (struct info *)malloc(sizeof(struct info *));

    /**************  store the donation information in an info struct ************************************************************/
    tempInfo->histPriority = t-> priority;
    tempInfo->histDonatedlock= t->donatedLock;


    /****************************************************************************************************
     * If t's priority is to be increased , then create  a info structure of previous priority,
       if the thread was donated a priority for this particular lock before, remove that from the list,
       an and if the lock is a new one, push it in the threads priority list. Then**/

    if(t->priority < targetP)
    {
        enum intr_level old_level=intr_disable();
        /**************  serach t's info history to discard any previous donation history of this lock ***
         * *********************************************************/

        removeTargetLockFromPList(dueTolock, t);

        /****** insert this donation history in infoList ************************************************************/

        if(t->donatedLock != dueTolock )list_push_front(&t->listOfInfo, &tempInfo->elem);
        else free(tempInfo);
        t->priority = targetP;
        t->donatedLock = dueTolock;
        checkYield(t);

    }

    /**
     * releasing the dueTo lock
     */

    else
    {
        enum intr_level old_level=intr_disable();
        t->donatedLock = dueTolock;
        t->priority = targetP;

        checkYield(t);
        intr_set_level(old_level);
    }
}

/**  if t is running and for some fucking sake we discover it has  smaller priority than the buddy
 * at the front of ready queue. let it run, but have to ensure this cannot happen again*/
void checkYield(struct thread* t){
        struct thread *nextThread= list_entry(list_begin(&ready_list), struct thread, elem);
        if(t->priority < nextThread ->priority)
            thread_yield();
}




/* ready_threads is the number of threads that are
  either running or ready to run at time of update
  (not including the idle thread) */
int thread_get_ready_threads(void)
{
    if (thread_current() != idle_thread)
        return list_size(&ready_list) + 1;
    else
        return list_size(&ready_list);
}



/* Every second (timer_ticks () % TIMER_FREQ == 0)
   Recent CPU is updated for all threads using this formula
   (whether running, ready, or blocked)
   recent_cpu = (2*load_avg) / (2*load_avg+1) * recent_cpu + nice */
fixed_t calculate_recent_cpu(struct thread *thread)
{
    return FP_ADD_MIX (FP_MULT (FP_DIV (FP_MULT_MIX (load_avg, 2), FP_ADD_MIX (FP_MULT_MIX(load_avg, 2), 1)), thread->recent_cpu), thread->nice);
}


/* Increase recent cpu of the running thread by 1 */
void thread_mlfqs_increase_recent_cpu(void)
{
    ASSERT (thread_mlfqs);
    ASSERT (intr_context ());

    struct thread *cur = thread_current();
    if (cur == idle_thread)return;
    else cur->recent_cpu = FP_ADD_MIX(cur->recent_cpu, 1);
}






/* Updated every second (timer_ticks () % TIMER_FREQ == 0)
   load_avg = (59/60) * load_avg + (1/60) * ready_threads */
fixed_t calculate_load_avg(int ready_threads)
{
    return FP_ADD (FP_DIV_MIX (FP_MULT_MIX (load_avg, 59), 60), \
         FP_DIV_MIX (FP_CONST (ready_threads), 60));
}




/* Every 4th tick (timer_ticks () % TIME_SLICE == 0)
   Priority for all threads is recalculated
   (running and ready threads is enough)
   priority = PRI_MAX - (recent_cpu / 4) - (nice * 2) */
int calculate_priority(struct thread *thread)
{
    int p= FP_INT_PART (FP_SUB_MIX (FP_SUB (FP_CONST (PRI_MAX), FP_DIV_MIX (thread->recent_cpu, 4)), 2 * thread->nice));
    if( p < PRI_MIN) return PRI_MIN;
    else if(p > PRI_MAX) return PRI_MAX;
    else return p;
}


/* Update priority */
void thread_mlfqs_update_priority(struct thread *thread)
{
    ASSERT(thread_mlfqs);
    if (thread != idle_thread)
        thread->priority = calculate_priority(thread);
}


/* Update 2 parameters: recent_cpu and load_avg */
void thread_mlfqs_update_args(void)
{
    ASSERT (thread_mlfqs);
    ASSERT (intr_context ());

    int ready_threads = list_size (&ready_list);
    if (thread_current () != idle_thread) ready_threads++;
    load_avg = calculate_load_avg(ready_threads);


   struct list_elem *e = list_begin(&all_list);

    for( e = list_begin(&all_list); e!=list_end(&all_list); e = list_next(e))
    {
        struct thread *temo = list_entry(e, struct thread, allelem);
        if(temo!= idle_thread) {
            temo->recent_cpu = calculate_recent_cpu(temo);
            thread_mlfqs_update_priority(temo);
        }
    }
}
/* Sets the current thread's nice value to NICE. */
void thread_set_nice (int nice UNUSED)
{
    /* Every thread has a nice value between -20 and 20 directly
    under its control. 0 is neutral, +20 means be very generous
    and give away your time, -20 means grab as much CPU as you can. */
    thread_current()->nice = nice;
    thread_mlfqs_update_priority(thread_current());
    thread_yield();
}

/* Returns the current thread's nice value. */
int thread_get_nice (void) {
    return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int thread_get_load_avg (void)
{
    return FP_ROUND(FP_MULT_MIX(load_avg, 100));
}

/* Returns 100 times the current thread's recent_cpu value. */
int thread_get_recent_cpu (void)
{
    /* Not yet implemented. */
    return FP_ROUND(FP_MULT_MIX(thread_current()->recent_cpu, 100));
}
















