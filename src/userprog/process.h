#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
static struct p_info* look_up_child_thread(tid_t child_tid);
static struct list_elem* look_up_elem(tid_t child_tid);
#endif /* userprog/process.h */
