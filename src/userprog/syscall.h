#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void* confirm_user_address(const void *user_address);
void exit_proc(int status);
void exit_error(int input);
#endif /* userprog/syscall.h */
