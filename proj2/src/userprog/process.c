#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
extern struct list all_list;

static struct p_info* look_up_child_thread(tid_t child_tid);
static struct list_elem* look_up_elem(tid_t child_tid);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
// TODO: decide to revise or not
tid_t process_execute (const char *file_name)
{
    struct thread* curr = thread_current();
    char *fn_copy;
    tid_t tid;
    char *f_name;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (!fn_copy) return -1;
  strlcpy (fn_copy, file_name, PGSIZE);

  char *save_ptr;
    /** allocate space for f_name, cannot use it directly for thread creation*/
  f_name = malloc(strlen(file_name)+1);
  strlcpy (f_name, file_name, strlen(file_name)+1);
  f_name = strtok_r (f_name," ",&save_ptr);

  /* Create a child thread to execute FILE_NAME. */
  /*Start process will make this sema avaliable if load successful*/
  tid = thread_create (f_name, PRI_DEFAULT, start_process, fn_copy);
  free(f_name);
  if (tid == -1) palloc_free_page (fn_copy);
    /**modify semaphore*/
  sema_down(&curr->load_process_sema);

  /** this load_success is set in start_process*/
  if(thread_current()->load_success == false) tid = -1;

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void start_process (void *file_name_)
{
    struct thread* curr = thread_current();
  struct intr_frame if_;
  bool success;
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name_, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name_);
  if (!success) {
      curr ->parent-> load_success=false;
      process_exit();
  }
  else {
      curr ->parent->load_success=true;
  }
  sema_up(&curr->parent->load_process_sema);


  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
// TODO: DONE
int process_wait (tid_t child_tid)
{
    struct thread *curr = thread_current();
    struct p_info *ch=NULL;
    struct list_elem *currE=NULL;
    int eax;
    ch = look_up_child_thread(child_tid);
    currE = look_up_elem(child_tid);
     if(ch == NULL) {
        eax = -1;
     }
     else{
         curr -> waiting_for_t = child_tid;
         if(ch -> is_over== false) {
             sema_down(&curr-> wait_process_sema);
         }
         eax = ch -> return_record;
         list_remove(currE);
         free(ch);
    }
    return eax;

}

// TODO: DONE
void process_exit (void)
{
      struct thread *curr = thread_current ();
      struct p_info* currProcess = get_current_process();
      uint32_t *pd;
      pd = curr->pagedir;
      printf("%s: exit(%d)\n",curr->name,currProcess->return_record);
      close_all_files(&curr->process_files);
      close_all_process(&curr -> child_process);
        if(curr->self){
            file_close(curr->self);
        }
        if(pd != NULL){
            curr->pagedir = NULL;
            pagedir_destroy (pd);
        }


}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void process_activate (void)
{
  struct thread *t = thread_current ();
  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);
  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, char * cmdline);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  acquire_filesys_lock();
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();
  
  /* Open executable file. */

  char * fn_cp = malloc (strlen(file_name)+1);
  strlcpy(fn_cp, file_name, strlen(file_name)+1);
  
  char * save_ptr;
  fn_cp = strtok_r(fn_cp," ",&save_ptr);

  file = filesys_open (fn_cp);
  free(fn_cp);

  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
        free(file);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp,file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;
  success = true;

    // deny writing to running exe
  file_deny_write(file);

  
 done:
    t->self = file;
    release_filesys_lock();
    return success;
}

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);
/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;

}
/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}
/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */

// TODO: to modify
static bool setup_stack (void **esp, char * file_name)
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

/** argument parsing  */
    char *token;
    char *save_ptr;
  int argc = 0,i;

  char * copy = malloc(strlen(file_name)+1);
  strlcpy (copy, file_name, strlen(file_name)+1);


  for (token = strtok_r (copy, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)){
      argc++;
  }

  int *argv = calloc(argc,sizeof(int));

  for (token = strtok_r (file_name, " ", &save_ptr),i=0; token != NULL; token = strtok_r (NULL, " ", &save_ptr),i++)
    {
      *esp -= strlen(token) + 1;
      memcpy(*esp,token,strlen(token) + 1);
      argv[i]=*esp;
    }
// TODO: split up

  while((int)*esp%4!=0)
  {
    *esp-=sizeof(char);
    char x = 0;
    memcpy(*esp,&x,sizeof(char));

  }

  int zero = 0;

  *esp-=sizeof(int);
  memcpy(*esp,&zero,sizeof(int));

  for(i=argc-1;i>=0;i--)
  {
    *esp-=sizeof(int);
    memcpy(*esp,&argv[i],sizeof(int));
  }

  int pt = *esp;
  *esp-=sizeof(int);
  memcpy(*esp,&pt,sizeof(int));

  *esp-=sizeof(int);
  memcpy(*esp,&argc,sizeof(int));

  *esp-=sizeof(int);
  memcpy(*esp,&zero,sizeof(int));

  free(copy);
  free(argv);

  return success;
}



/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}



static struct p_info* look_up_child_thread(tid_t child_tid){
    struct list_elem *e;
    struct p_info *ch=NULL;
    struct thread* curr = thread_current();
    for (e = list_begin (&curr->child_process); e != list_end (&curr->child_process); e = list_next (e)) {
        struct p_info *f = list_entry (e, struct p_info, elem);
        if(f->tid == child_tid) {
            ch = f;
            break;
        }
    }
    return ch;
}
static struct list_elem* look_up_elem(tid_t child_tid){

    struct list_elem *e;
    struct list_elem *targetE = NULL;
    for (e = list_begin (&thread_current()->child_process); e != list_end (&thread_current()->child_process); e = list_next (e)) {
        struct p_info *f = list_entry (e, struct p_info, elem);
        if(f->tid == child_tid) {
            targetE = e;
            break;
        }
    }
    return targetE;
}

void close_all_process(struct list* child_process) {
    struct list_elem *e;
    while (!list_empty(child_process)) {
        e = list_pop_front(child_process);
        struct p_info *p = list_entry(e,struct p_info, elem);
        list_remove(e);
        if (p->is_over == true)
            free(p);
        else {
            p->is_parent_over = true;
        }
    }
}
