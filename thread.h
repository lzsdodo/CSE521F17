#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/fix-point.h"
/* States in a thread's life cycle. */
enum thread_status
{
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

struct thread
{
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */


    /*****************************ticks to sleep **************************************/
    int64_t ticksRemain;				/* to store the ticks to sleep */
    struct list_elem elem;              /* List element. */

    /* MLFQS */
    int nice;
    fixed_t recent_cpu;

    /*********************   lock    *********************************/
    struct lock *targetLock;            /* the lock that the thread is try to acquire but cannot obtain for now */
    struct lock *donatedLock;		    /*  lock for which the thread has been donated priority due to it*/
    struct list listOfInfo;				/* To store the infos in a list */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
};

/*******************************************************************/
struct info
{
    int histPriority;
    struct lock* histDonatedlock;
    struct list_elem elem;
};
/*******************************************************************/

struct list sleep_list;

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

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);
/* MLFQS */
int thread_get_ready_threads(void);
fixed_t calculate_load_avg(int ready_threads);
fixed_t calculate_recent_cpu(struct thread *thread);
int calculate_priority(struct thread *thread);

void thread_mlfqs_increase_recent_cpu(void);
void thread_mlfqs_update_priority(struct thread *thread);
void thread_mlfqs_update_args(void);


void removeHistoryLocks(struct list priList,  struct lock* targetLock);
void checkYield(struct thread* t);


//**************************************************added----//

#endif /* threads/thread.h */