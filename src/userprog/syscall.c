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
void* confirm_user_address(const void*);
struct info_p* get_current_process();
extern bool running;



void syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void syscall_handler (struct intr_frame *f UNUSED)
{
    int* p = f->esp;
	confirm_user_address(p);
    int system_call = *((int*) f->esp);
	switch (system_call)
	{
        //shut down
		case SYS_HALT:shutdown_power_off();
		break;
		case SYS_EXIT:confirm_user_address(p+1);
            int status = *(p+1);
            f->eax = status;
		    quit(status);
		break;
         /**in: filename in (p+1)
          * out : return pid or -1
          */
		case SYS_EXEC:
		confirm_user_address(*(p+1));
		    f->eax = exec_proc(*(p+1));
		break;
        /** wait a child thread
         * in: tid at p + 1
         * out :exit -1 or status change
         */
		case SYS_WAIT:
            f->eax = process_wait(*(p+1));
            break;
        /** create a file
         * in: filename(p+1), initia_size(p+2)
         * out : f -> eax = some shit
         */
		case SYS_CREATE:
		confirm_user_address(*(p+1));
  //	confirm_user_address(p+2);
		f->eax = filesys_create(*(p+1),*(p+2));
		break;
        /**
         * file name im (p+1)
         *
         */
		case SYS_REMOVE:
		  confirm_user_address(*(p+1));
          f -> eax = filesys_remove(*(p+1));
		break;
        /**
         * in: filename in p
         * out: return new file if successful, null ptr if failure
         */
		case SYS_OPEN:
            // pass open NULL/bad ptr
            confirm_user_address(*(p+1));
            struct thread* curr = thread_current();

            // TODO: we need lock here
            acquire_filesys_lock();
            struct file* fptr = filesys_open (*(p+1));
            release_filesys_lock();
            if(fptr==NULL) f->eax = -1;
            else {
                struct file_info *pfile = (struct file_info*)malloc(sizeof(*pfile));
                pfile->target_file = fptr;
                pfile->handle = curr->fd_count += 2;
                list_push_back (&curr->process_files, &pfile->elem);
                f->eax = pfile->handle;
            }
            break;
        /**
         * close file according to handler in p+1
         */
		case SYS_CLOSE:confirm_user_address(p+1);
            close_file(&thread_current()->process_files,*(p+1));
            break;
        /**
         * output filesize according to handler in p + 1
         */
		case SYS_FILESIZE:
            confirm_user_address(p+1);
            struct file_info* targetFile_info = look_up(*(p+1));
            f->eax = file_length (targetFile_info->target_file);
            break;
        /**
         * Reads size bytes from the file open as fd into buffer.
         * Returns the number of bytes actually read (0 at end of file),
         * or -1 if the file could not be read (due to a condition other than end of file).
         * Fd 0 reads from the keyboard using input_getc().
         *
         * in: p+1: fd/handler
         *     p+2: buffer
         *     p+3: unassigned size
         */
		case SYS_READ:
            confirm_user_address(p+1);
            // pass read bad ptr
		    confirm_user_address(*(p+2));

		if(*(p+1)==0){
			uint8_t* buffer = *(p+2);
			for(int i=0;i<*(p+3);i++) buffer[i] = input_getc();
			f->eax = *(p+3);
		}
		else{
			struct file_info* fptr = look_up(*(p+1));
			if(fptr==NULL) f->eax=-1;
			else{ f->eax = file_read (fptr->target_file, *(p+2), *(p+3));}
		}
		break;
		case SYS_WRITE:confirm_user_address(p+1);
		confirm_user_address(*(p+2));
        if(*(p+3) <= 0){
            f -> eax = 0;
            return;
        }

		if(*(p+1)==1)
		{
			putbuf(*(p+2),*(p+3));
			f->eax = *(p+3);
		}
		else
		{
			struct file_info* fptr = look_up(*(p+1));
			if(fptr==NULL)
				f->eax=-1;
			else
			{
				f->eax = file_write (fptr->target_file, *(p+2), *(p+3));
			}
		}
		break;
        /**
        * void seek (int fd, unsigned position)
        */
		case SYS_SEEK:
            confirm_user_address(p+1);
	    	confirm_user_address(p+2);
		    file_seek(look_up(*(p+1))->target_file,*(p+2));
		break;
        /**
         * unsigned tell (int fd)
         */
		case SYS_TELL:
		confirm_user_address(p+1);
		f->eax = file_tell(look_up(*(p+1))->target_file);
		break;

		default: printf("I don know how to handle this %d\n",*p);
            thread_exit();
	}
}

// validate a user address, JZ
void* confirm_user_address(const void *user_address)
{

    if (is_user_vaddr(user_address) )
    {
        void *target_file = pagedir_get_page(thread_current()->pagedir, user_address);
        if (is_kernel_vaddr(target_file))
        {
            return target_file;
        }
    }
    quit(-1);

    return;

}
// TODO: review this
int exec_proc(char *f_name)
{
    // we need lock here
    acquire_filesys_lock();
    // allocate space
    char * fn_cp = malloc (strlen(f_name)+1);
    char * save_ptr;

    strlcpy(fn_cp, f_name, strlen(f_name)+1);
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
        return process_execute(f_name);
    }
}

void quit(int status)
{
    struct thread* curr = thread_current();
    struct p_info* currPross = get_current_process();
    currPross -> is_over = true;
    currPross -> return_record = status;

    curr -> return_record = status;

    if(curr->parent->waiting_for_t == curr ->tid && currPross -> is_parent_over == false){
        sema_up(&curr ->parent-> wait_process_sema);
    }
    else if(currPross -> is_parent_over == true){
        free(currPross);
    }


    thread_exit();
}
void close_file(struct list* process_files, int handle)
{   struct list_elem *e;
	struct file_info *f;

      for (e = list_begin (process_files); e != list_end (process_files); e = list_next (e)) {
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



/**
 * get the current thread's corresponding p_info
 * @return current thread's p_info*
 */
struct info_p* get_current_process(){
    struct list_elem *e;
    struct p_info* ans = NULL;
    for (e = list_begin (&thread_current()->parent->child_process); e != list_end (&thread_current()->parent->child_process); e = list_next (e))
    {
        struct p_info * target_process = list_entry (e, struct p_info, elem);
        if(target_process->tid == thread_current()->tid)
        {
            ans = target_process;
        }
    }

    return ans;
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
