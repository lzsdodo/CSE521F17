#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <kernel/list.h>
#include <threads/synch.h>


enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };


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

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    int64_t waketick;


//    /**
//     *
//     * suggested by TA
//     * uinit32_t *pagedirec
//     * struct file* exec
//     * struct list process_files
//     * struct thread *parent
//     * struct list child_list
//     * struct p_info
//     *      semaphore load_sema
//     *      bool load_bool
//     *      struct semaphore wait_sema
//     *      bool waited
//     *      bool is_alive
//     *      bool parent_alive
//     *
//     * /



    uint32_t *pagedir;                  /* Page directory. */
    struct file *self;
    struct list process_files;
    struct thread* parent;
    struct list child_process;
    int return_record;

    bool load_success;
    int fd_count;

    //TODO: split child_lock into sema load, sema wait
    struct semaphore child_lock;
    struct semaphore wait_sema;

    struct child {
        tid_t tid;
        struct list_elem elem;
       int return_record;
        bool used;
    };


    // the threadId that the current thread is waiting on
    tid_t waitingon;



    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
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

bool cmp_waketick(struct list_elem *first, struct list_elem *second, void *aux);

#endif /* threads/thread.h */
