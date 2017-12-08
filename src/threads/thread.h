#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <hash.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "threads/fixed_point.h"
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
    int normal_priority;                /* Priority, without donations. */
    struct list donors;                 /* Threads donating priority to us. */
    struct list_elem donor_elem;        /* Element in donors list. */
    struct thread *donee;               /* Thread we're donating to. */
    struct lock *want_lock;             /* Lock we're waiting to acquire. */
    int nice;                           /* Niceness. */
    fixed_point_t recent_cpu;           /* Recent amount of CPU time. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Owned by process.c. */
    int exit_code;                      /* Exit code. */
    struct wait_status *wait_status;    /* This process's completion status. */
    struct list children;               /* Completion status of children. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* Alarm clock. */
    int64_t wakeup_time;                /* Time to wake this thread up. */
    struct list_elem timer_elem;        /* Element in timer_wait_list. */
    struct semaphore timer_sema;        /* Semaphore. */

    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct hash *SPT;                   /* Supplementary Page table. */
    struct file *bin_file;              /* Executable. */

    /* Owned by syscall.c. */
    struct list fds;                    /* List of file descriptors. */
    struct list list_mmap_files;               /* Memory-mapped files. */
    int next_handle;                    /* Next handle value. */
    void *user_esp;                     /* User's stack pointer. */

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* Tracks the completion of a process.
   Reference held by both the parent, in its `children' list,
   and by the child, in its `wait_status' pointer. */
struct wait_status
  {
    struct list_elem elem;              /* `children' list element. */
    struct lock lock;                   /* Protects ref_cnt. */
    int ref_cnt;                        /* 2=child and parent both alive,
                                           1=either child or parent alive,
                                           0=child and parent both dead. */
    tid_t tid;                          /* Child thread id. */
    int exit_code;                      /* Child exit code, if dead. */
    struct semaphore dead;              /* 1=child alive, 0=child dead. */
  };

/* Binds a mapping id to a region of memory and a file. */
struct mapping
{
    struct list_elem elem;      /* List element. */
    int map_handle;                 /* Mapping id. */
    struct file *file;          /* File*/


    uint8_t *base;              /* memory mapping starts here : ) */
    size_t page_cnt;            /* Number of pages mapped. */
};

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
bool thread_lower_priority (const struct list_elem *a_,
                            const struct list_elem *b_,
                            void *aux UNUSED);
void thread_yield_to_higher_priority (void);
void thread_recompute_priority (struct thread *t);

#endif /* threads/thread.h */
