#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "list.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
void* confirm_user_address(const void*);
struct proc_file* list_search(struct list* files, int fd);
void exit_error(int input);

extern bool running;

struct proc_file {
	struct file* ptr;
	int fd;
	struct list_elem elem;
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int * p = f->esp;

	confirm_user_address(p);
  int system_call = * p;

	switch (system_call)
	{
		case SYS_HALT:
		shutdown_power_off();
		break;
		case SYS_EXIT:
		confirm_user_address(p+1);
		exit_proc(*(p+1));
		break;

		case SYS_EXEC:
		confirm_user_address(p+1);
		confirm_user_address(*(p+1));
		f->eax = exec_proc(*(p+1));
		break;

		case SYS_WAIT:
		confirm_user_address(p+1);
		f->eax = process_wait(*(p+1));
		break;

		case SYS_CREATE:
		confirm_user_address(p+5);
		confirm_user_address(*(p+4));
		acquire_filesys_lock();
		f->eax = filesys_create(*(p+4),*(p+5));
		release_filesys_lock();
		break;

		case SYS_REMOVE:
		confirm_user_address(p+1);
		confirm_user_address(*(p+1));
		acquire_filesys_lock();
		if(filesys_remove(*(p+1))==NULL)
			f->eax = false;
		else
			f->eax = true;
		release_filesys_lock();
		break;

		case SYS_OPEN:
		confirm_user_address(p+1);
		confirm_user_address(*(p+1));

		acquire_filesys_lock();
		struct file* fptr = filesys_open (*(p+1));
		release_filesys_lock();
		if(fptr==NULL)
			f->eax = -1;
		else
		{
			struct proc_file *pfile = malloc(sizeof(*pfile));
			pfile->ptr = fptr;
			pfile->fd = thread_current()->fd_count;
			thread_current()->fd_count++;
			list_push_back (&thread_current()->files, &pfile->elem);
			f->eax = pfile->fd;

		}
		break;

		case SYS_FILESIZE:
		confirm_user_address(p+1);
		acquire_filesys_lock();
		f->eax = file_length (list_search(&thread_current()->files, *(p+1))->ptr);
		release_filesys_lock();
		break;

		case SYS_READ:
		confirm_user_address(p+7);
		confirm_user_address(*(p+6));
		if(*(p+5)==0)
		{
			int i;
			uint8_t* buffer = *(p+6);
			for(i=0;i<*(p+7);i++)
				buffer[i] = input_getc();
			f->eax = *(p+7);
		}
		else
		{
			struct proc_file* fptr = list_search(&thread_current()->files, *(p+5));
			if(fptr==NULL)
				f->eax=-1;
			else
			{
				acquire_filesys_lock();
				f->eax = file_read (fptr->ptr, *(p+6), *(p+7));
				release_filesys_lock();
			}
		}
		break;

		case SYS_WRITE:
		confirm_user_address(p+7);
		confirm_user_address(*(p+6));
		if(*(p+5)==1)
		{
			putbuf(*(p+6),*(p+7));
			f->eax = *(p+7);
		}
		else
		{
			struct proc_file* fptr = list_search(&thread_current()->files, *(p+5));
			if(fptr==NULL)
				f->eax=-1;
			else
			{
				acquire_filesys_lock();
				f->eax = file_write (fptr->ptr, *(p+6), *(p+7));
				release_filesys_lock();
			}
		}
		break;

		case SYS_SEEK:
		confirm_user_address(p+5);
		acquire_filesys_lock();
		file_seek(list_search(&thread_current()->files, *(p+4))->ptr,*(p+5));
		release_filesys_lock();
		break;

		case SYS_TELL:
		confirm_user_address(p+1);
		acquire_filesys_lock();
		f->eax = file_tell(list_search(&thread_current()->files, *(p+1))->ptr);
		release_filesys_lock();
		break;

		case SYS_CLOSE:
		confirm_user_address(p+1);
		acquire_filesys_lock();
		close_file(&thread_current()->files,*(p+1));
		release_filesys_lock();
		break;


		default:
		printf("Default %d\n",*p);
	}
}

/**
 * Runs the executable whose name is given in cmd_line, passing any given arguments, and returns the new process's program id (pid).
 * Must return pid -1, which otherwise should not be a valid pid, if the program cannot load or run for any reason.
 * Thus, the parent process cannot return from the exec until it knows whether the child process successfully loaded its executable.
 * You must use appropriate synchronization to ensure this.
 * @param file_name
 * @return
 */

int exec_proc(char *file_name)
{
	acquire_filesys_lock();
	// allocate space
	char * fn_cp = malloc (strlen(file_name)+1);
	// TODO: what's this?
	  strlcpy(fn_cp, file_name, strlen(file_name)+1);
	  
	  char * save_ptr;
	//TODO: which token does it return
	  fn_cp = strtok_r(fn_cp," ",&save_ptr);

	// open file with given name
	 struct file* currFile = filesys_open (fn_cp);

		// if program cannot load the file
	  if(!currFile)
	  {
	  	release_filesys_lock();
	  	return -1;
	  }
	  else
	  {
	  	file_close(currFile);
	  	release_filesys_lock();
		  // return pid
	  	return process_execute(file_name);
	  }
}

void exit_error(int status){
    thread_current() -> return_record = status;
    thread_exit();
}


/**
 * Terminates the current user program, returning status to the kernel.
 * 	// TODO: how return status to kernel?
 * If the process's parent waits for it (see below), this is the status that will be returned.
 * Conventionally, a status of 0 indicates success and nonzero values indicate errors.
 * @param status
 */
// validate a user address, JZ's work
void* confirm_user_address(const void *user_address)
{
//	if (!is_user_vaddr(user_address))
//	{
//		exit_proc(-1);
//		return 0;
//	}
//	void *ptr = pagedir_get_page(thread_current()->pagedir, user_address);
//	if (!is_kernel_vaddr(ptr))
//	{
//		exit_proc(-1);
//		return 0;
//	}
//	return ptr;
    if (is_user_vaddr(user_address))
    {
        void *ptr = pagedir_get_page(thread_current()->pagedir, user_address);
        if (is_kernel_vaddr(ptr))
        {
            return ptr;
        }
    }
    exit_proc(-1);
//    exit_error(-1);
    return 0;

}

void exit_proc(int status)
{
    struct list_elem *e;
    // Iterate over current threads' parent's child process List,
    for (e = list_begin (&thread_current()->parent->child_proc); e != list_end (&thread_current()->parent->child_proc); e = list_next (e))
    {
        struct child *f = list_entry (e, struct child, elem);
        if(f->tid == thread_current()->tid)
        {
            f->used = true;
            f->return_record = status;
        }
    }

    thread_current()->return_record = status;
    // if current thread's parent is waiting on current thread, make the semaphore now obtainable
    if(thread_current()->parent->waitingon == thread_current()->tid)
        sema_up(&thread_current()->parent->child_lock);

    thread_exit();
}



struct proc_file* list_search(struct list* files, int fd)
{

	struct list_elem *e;

      for (e = list_begin (files); e != list_end (files);
           e = list_next (e))
        {
          struct proc_file *f = list_entry (e, struct proc_file, elem);
          if(f->fd == fd)
          	return f;
        }
   return NULL;
}

void close_file(struct list* files, int fd)
{

	struct list_elem *e;

	struct proc_file *f;

      for (e = list_begin (files); e != list_end (files);
           e = list_next (e))
        {
          f = list_entry (e, struct proc_file, elem);
          if(f->fd == fd)
          {
          	file_close(f->ptr);
          	list_remove(e);
          }
        }

    free(f);
}

void close_all_files(struct list* files)
{

	struct list_elem *e;

	while(!list_empty(files))
	{
		e = list_pop_front(files);

		struct proc_file *f = list_entry (e, struct proc_file, elem);
          
	      	file_close(f->ptr);
	      	list_remove(e);
	      	free(f);


	}

      
}