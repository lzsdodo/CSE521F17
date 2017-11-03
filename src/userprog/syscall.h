#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
void* confirm_user_address(const void *user_address);
void quit(int status);
void exit_error();
#endif /* userprog/syscall.h */
