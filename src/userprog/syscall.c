#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "list.h"
#include "process.h"

static void syscall_handler (struct intr_frame *);
struct file_info* look_up(int handle);
void exit_error();
void* confirm_user_address(const void*);

extern bool running;

struct file_info {
	struct file* target_file;
	int handle;
	struct list_elem elem;
};

void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
  int * p = f->esp;

	confirm_user_address(p);

    int system_call = *((int*) f->esp);

	switch (system_call)
	{
        //shut down
		case SYS_HALT:
		shutdown_power_off();
		break;

		case SYS_EXIT:
		confirm_user_address(p+1);
			int status = *(p+1);
		exit_proc(status);
//            struct list_elem *e;
//            // Iterate over current threads' parent's child process List,
//            for (e = list_begin (&thread_current()->parent->child_process); e != list_end (&thread_current()->parent->child_process); e = list_next (e))
//            {
//                struct child *f = list_entry (e, struct child, elem);
//                if(f->tid == thread_current()->tid)
//                {
//                    f->used = true;
//                    f->return_record = status;
//                }
//            }
//
//            thread_current()->return_record = status;
//            // if current thread's parent is waiting on current thread, make the semaphore now obtainable
//            if(thread_current()->parent->waiting_for_t == thread_current()->tid)
//                sema_up(&thread_current()->parent->load_process_sema);
//
//            thread_exit();
		break;

         // execute program
		case SYS_EXEC:
		confirm_user_address(p+1);
		confirm_user_address(*(p+1));
		f->eax = exec_proc(*(p+1));
		break;
            // wait for child process to finish
		case SYS_WAIT:
		confirm_user_address(p+1);
		f->eax = process_wait(*(p+1));
		break;
        // create new file
		case SYS_CREATE:
		confirm_user_address(p+5);
		confirm_user_address(*(p+4));
//		acquire_filesys_lock();
		f->eax = filesys_create(*(p+4),*(p+5));
//		release_filesys_lock();
		break;

            // delete file, seems like p + 1 is the place where file is stored.
            // EZ  XD~~
		case SYS_REMOVE:
		confirm_user_address(p+1);
		confirm_user_address(*(p+1));
//		acquire_filesys_lock();
          f -> eax = filesys_remove(*(p+1));
//		if(filesys_remove(*(p+1))==NULL)
//			f->eax = false;
//		else
//			f->eax = true;
//		release_filesys_lock();
		break;

            // open a file
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
			struct file_info *pfile = malloc(sizeof(*pfile));
			pfile->target_file = fptr;
			pfile->handle = thread_current()->fd_count;
			thread_current()->fd_count++;
			list_push_back (&thread_current()->process_files, &pfile->elem);
			f->eax = pfile->handle;

		}
		break;

            // close the file
        case SYS_CLOSE:
            confirm_user_address(p+1);
            acquire_filesys_lock();
            close_file(&thread_current()->process_files,*(p+1));
            release_filesys_lock();
            break;

            // return size of file
		case SYS_FILESIZE:
		confirm_user_address(p+1);
		acquire_filesys_lock();
		f->eax = file_length (look_up( *(p+1))->target_file);
		release_filesys_lock();
		break;

            // read the file
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
			struct file_info* fptr = look_up( *(p+5));
			if(fptr==NULL)
				f->eax=-1;
			else
			{
				acquire_filesys_lock();
				f->eax = file_read (fptr->target_file, *(p+6), *(p+7));
				release_filesys_lock();
			}
		}
		break;
        // printf and write into file
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
			struct file_info* fptr = look_up(*(p+5));
			if(fptr==NULL)
				f->eax=-1;
			else
			{
				acquire_filesys_lock();
				f->eax = file_write (fptr->target_file, *(p+6), *(p+7));
				release_filesys_lock();
			}
		}
		break;

            // move file pointer
		case SYS_SEEK:
		confirm_user_address(p+5);
		acquire_filesys_lock();
		file_seek(look_up(*(p+4))->target_file,*(p+5));
		release_filesys_lock();
		break;

            // return file pointer
		case SYS_TELL:
		confirm_user_address(p+1);
		acquire_filesys_lock();
		f->eax = file_tell(look_up(*(p+1))->target_file);
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
	  strlcpy(fn_cp, file_name, strlen(file_name)+1);
	  
	  char * save_ptr;
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

void exit_error(){
    thread_current() -> return_record = -1;
    thread_exit();
}


/**
 * Terminates the current user program, returning status to the kernel.
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
//	void *target_file = pagedir_get_page(thread_current()->pagedir, user_address);
//	if (!is_kernel_vaddr(target_file))
//	{
//		exit_proc(-1);
//		return 0;
//	}
//	return target_file;
    if (is_user_vaddr(user_address))
    {
        void *target_file = pagedir_get_page(thread_current()->pagedir, user_address);
        if (is_kernel_vaddr(target_file))
        {
            return target_file;
        }
    }
    exit_proc(-1);
//    exit_error();
    return 0;

}

void exit_proc(int status)
{
    struct list_elem *e;

    for (e = list_begin (&thread_current()->parent->child_process); e != list_end (&thread_current()->parent->child_process); e = list_next (e))
    {
        struct p_info *f = list_entry (e, struct p_info, elem);
        if(f->tid == thread_current()->tid)
        {
            f->is_over = true;
            //TODO: set its child process,s parent_isOver to true.
            f->return_record = status;
        }
    }

    thread_current()->return_record = status;
    // if current thread's parent is waiting on current thread, make the semaphore now obtainable
    if(thread_current()->parent->waiting_for_t == thread_current()->tid)
        sema_up(&thread_current()->parent-> load_process_sema);

    thread_exit();
}

void close_file(struct list* process_files, int handle)
{

	struct list_elem *e;

	struct file_info *f;

      for (e = list_begin (process_files); e != list_end (process_files);
           e = list_next (e))
        {
          f = list_entry (e, struct file_info, elem);
          if(f->handle == handle)
          {
          	file_close(f->target_file);
          	list_remove(e);
          }
        }

    free(f);
}

void close_all_files(struct list* process_files)
{

	struct list_elem *e;

	while(!list_empty(process_files))
	{
		e = list_pop_front(process_files);

		struct file_info *f = list_entry (e, struct file_info, elem);
          
	      	file_close(f->target_file);
	      	list_remove(e);
	      	free(f);


	}

      
}


struct file_info* look_up( int handle)
{
    struct list* process_files  =& thread_current() -> process_files;
    struct list_elem *e;

    for (e = list_begin (process_files); e != list_end (process_files);
         e = list_next (e))
    {
        struct file_info *f = list_entry (e, struct file_info, elem);
        if(f->handle == handle)
            return f;
    }
    return NULL;
}
//TODO 6: create a look_up handle for child
// look up handle could be used in syscall/ exit process, and process/wait_process.