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
//     * struct child
//     *      semaphore load_sema
//     *      bool load_bool
//     *      struct semaphore wait_sema
//     *      bool waited
//     *      bool is_over
//     *      bool parent_alive
//     *
//     * /



    uint32_t *pagedir;                  /* Page directory. */
    struct file *self;
    struct list process_files;
    struct thread* parent;
    struct list child_process;
    int return_record;

    // TODO 1: from TA's slides, load_success should be in child
    bool load_success;
    //TODO 2: what is this fd_count? why its initialzied to 2 in thread/init.c? can you change it to something in TA's suggested structure
    int fd_count;

    //TODO 3: if you use 2nd sema, 3/76 unpass, solve this.
    // if you want to reverse to 2/76, delete this field, and its initialization.
    struct semaphore load_process_sema;
    struct semaphore wait_process_sema;

// TODO: why do we need a parent alive ?

    // the threadId that the current thread is waiting on
    // TODO 5: resolve it to a bool with same functionality, try to put it in child
    tid_t waiting_for_t;

    struct p_info {
        // used to track the value
        tid_t tid;
       int return_record;
        bool is_over;
        //TODO 4: is_parent_alive
        struct list_elem elem;
    };





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
