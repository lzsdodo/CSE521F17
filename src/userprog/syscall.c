#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/directory.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/page.h"


static int sys_halt (void);
static int sys_exit (int status);
static int sys_exec (const char *ufile);
static int sys_wait (tid_t);
static int sys_create (const char *ufile, unsigned initial_size);
static int sys_remove (const char *ufile);
static int sys_open (const char *ufile);
static int sys_filesize (int handle);
static int sys_read (int handle, void *udst_, unsigned size);
static int sys_write (int handle, void *usrc_, unsigned size);
static int sys_seek (int handle, unsigned position);
static int sys_tell (int handle);
static int sys_close (int handle);

void clear_mapping (struct mapping *m);
static int sys_mapping (int handle, void *addr);
static int sys_munmap (int mapping);
static inline bool
get_user (uint8_t *dst, const uint8_t *usrc);
static void syscall_handler (struct intr_frame *);
static void copy_in (void *, const void *, size_t);
int add_file_to_mapping(struct file* file,
                        off_t ofs,
                        uint8_t* addr,
                        uint32_t page_read_bytes,
                        uint32_t page_zero_bytes,
                        struct mapping *m);

static bool  verify_user (const void *uaddr);
static struct lock fs_lock;

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init (&fs_lock);
}

/* System call handler. */
static void
syscall_handler (struct intr_frame *f)
{
  typedef int syscall_function (int, int, int);

  /* A system call. */
  struct syscall
    {
      size_t arg_cnt;           /* Number of arguments. */
      syscall_function *func;   /* Implementation. */
    };

  /* Table of system calls. */
  static const struct syscall syscall_table[] =
    {
      {0, (syscall_function *) sys_halt},
      {1, (syscall_function *) sys_exit},
      {1, (syscall_function *) sys_exec},
      {1, (syscall_function *) sys_wait},
      {2, (syscall_function *) sys_create},
      {1, (syscall_function *) sys_remove},
      {1, (syscall_function *) sys_open},
      {1, (syscall_function *) sys_filesize},
      {3, (syscall_function *) sys_read},
      {3, (syscall_function *) sys_write},
      {2, (syscall_function *) sys_seek},
      {1, (syscall_function *) sys_tell},
      {1, (syscall_function *) sys_close},
      {2, (syscall_function *) sys_mapping},
      {1, (syscall_function *) sys_munmap},
    };

  const struct syscall *sc;
  unsigned call_nr;
  int args[3];

  /* Get the system call. */
  copy_in (&call_nr, f->esp, sizeof call_nr);
  if (call_nr >= sizeof syscall_table / sizeof *syscall_table)
    thread_exit ();
  sc = syscall_table + call_nr;

  /* Get the system call arguments. */
  ASSERT (sc->arg_cnt <= sizeof args / sizeof *args);
  memset (args, 0, sizeof args);
  copy_in (args, (uint32_t *) f->esp + 1, sizeof *args * sc->arg_cnt);


  f->eax = sc->func (args[0], args[1], args[2]);
}


static void copy_in (void *dst_, const void *usrc_, size_t size)
{
  uint8_t *dst = dst_;
  const uint8_t *usrc = usrc_;

  while (size > 0)
    {
      size_t chunk_size = PGSIZE - pg_ofs (usrc);
      if (chunk_size > size)
        chunk_size = size;

      if (!page_lock (usrc, false))
        thread_exit ();
      memcpy (dst, usrc, chunk_size);
      page_unlock (usrc);

      dst += chunk_size;
      usrc += chunk_size;
      size -= chunk_size;
    }
}

/* Creates a copy of user string US in kernel memory
   and returns it as a page that must be freed with
   palloc_free_page().
   Truncates the string at PGSIZE bytes in size.
   Call thread_exit() if any of the user accesses are invalid. */
static char *
copy_in_string (const char *us)
{
  char *ks;
  char *upage;
  size_t length;

  ks = palloc_get_page (0);
  if (ks == NULL)
    thread_exit ();
  length = 0;
  for (length = 0; length < PGSIZE; length++)
    {
        upage = pg_round_down (us);
        bool locked = page_lock(upage,false);
          if (!locked || us >= (char *) PHYS_BASE || !get_user (ks + length, us))
          {
              palloc_free_page (ks);
              thread_exit ();
          }

            ks[length] = *us;
          if (ks[length] == '\0')
            {
              page_unlock (upage);
              return ks;
            }

         us++;



      page_unlock (upage);
    }

    ks[PGSIZE - 1] = '\0';
    return ks;


}



static inline bool
get_user (uint8_t *dst, const uint8_t *usrc)
{
    int eax;
    asm ("movl $1f, %%eax; movb %2, %%al; movb %%al, %0; 1:"
    : "=m" (*dst), "=&a" (eax) : "m" (*usrc));
    return eax != 0;
}


/* Halt system call. */
static int
sys_halt (void)
{
  shutdown_power_off ();
}

/* Exit system call. */
static int
sys_exit (int exit_code)
{
  thread_current ()->exit_code = exit_code;
  thread_exit ();
  NOT_REACHED ();
}

/* Exec system call. */
static int
sys_exec (const char *ufile)
{
  tid_t tid;
  char *kfile = copy_in_string (ufile);

  lock_acquire (&fs_lock);
  tid = process_execute (kfile);
  lock_release (&fs_lock);

  palloc_free_page (kfile);

  return tid;
}

/* Wait system call. */
static int
sys_wait (tid_t child)
{
  return process_wait (child);
}

/* Create system call. */
static int
sys_create (const char *ufile, unsigned initial_size)
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  lock_acquire (&fs_lock);
  ok = filesys_create (kfile, initial_size);
  lock_release (&fs_lock);

  palloc_free_page (kfile);

  return ok;
}

/* Remove system call. */
static int
sys_remove (const char *ufile)
{
  char *kfile = copy_in_string (ufile);
  bool ok;

  lock_acquire (&fs_lock);
  ok = filesys_remove (kfile);
  lock_release (&fs_lock);

  palloc_free_page (kfile);

  return ok;
}

/* A file descriptor, for binding a file handle to a file. */
struct file_descriptor
  {
    struct list_elem elem;      /* List element. */
    struct file *file;          /* File. */
    int handle;                 /* File handle. */
  };

/* Open system call. */
static int
sys_open (const char *ufile)
{
  char *kfile = copy_in_string (ufile);
  struct file_descriptor *fd;
  int handle = -1;

  fd = malloc (sizeof *fd);
  if (fd != NULL)
    {
      lock_acquire (&fs_lock);
      fd->file = filesys_open (kfile);
      if (fd->file != NULL)
        {
          struct thread *cur = thread_current ();
          handle = fd->handle = cur->next_handle++;
          list_push_front (&cur->fds, &fd->elem);
        }
      else
        free (fd);
      lock_release (&fs_lock);
    }

  palloc_free_page (kfile);
  return handle;
}

/* Returns the file descriptor associated with the given handle.
   Terminates the process if HANDLE is not associated with an
   open file. */
static struct file_descriptor *
lookup_fd (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds);
       e = list_next (e))
    {
      struct file_descriptor *fd;
      fd = list_entry (e, struct file_descriptor, elem);
      if (fd->handle == handle)
        return fd;
    }

  thread_exit ();
}

/* Filesize system call. */
static int
sys_filesize (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  int size;

  lock_acquire (&fs_lock);
  size = file_length (fd->file);
  lock_release (&fs_lock);

  return size;
}

/* Read system call. */
static int
sys_read (int handle, void *udst_, unsigned size)
{
  uint8_t *udst = udst_;
  struct file_descriptor *fd = lookup_fd (handle);
  int bytes_read = 0;



  while (size > 0)
    {
      /* How much to read into this page? */
      size_t page_left = PGSIZE - pg_ofs (udst);
      size_t read_amt = size < page_left ? size : page_left;
      off_t retval;

        if(handle == STDIN_FILENO)
        {
            size_t i;
            for (i = 0; i < read_amt; i++)
            {
                char c = input_getc ();
                bool read_in = page_lock(udst,true);
                if (!read_in) thread_exit ();

                udst[i] = c;
                page_unlock (udst);
            }
            bytes_read = read_amt;
            return bytes_read;
        }

        else
        {
            bool read_in = page_lock (udst, true);

            if (!read_in)  thread_exit ();
            retval = file_read (fd->file, udst, read_amt);
            page_unlock (udst);
        }

      /* Check success. */
      if (retval < 0)
        {
          if (bytes_read == 0)
            bytes_read = -1;
          break;
        }
      bytes_read += retval;

      if (retval != (off_t) read_amt)
        {
          break;
        }

      /* Advance. */
      udst += retval;
      size -= retval;
    }

  return bytes_read;
}

/* Write system call. */
static int
sys_write (int handle, void *usrc_, unsigned size)
{
  uint8_t *usrc = usrc_;
  struct file_descriptor *fd = NULL;
  int bytes_written = 0;

  /* Lookup up file descriptor. */
  if (handle != STDOUT_FILENO)
    fd = lookup_fd (handle);

  while (size > 0)
    {
      /* How much bytes to write to this page? */
      size_t page_left = PGSIZE - pg_ofs (usrc);
      size_t write_amt = size < page_left ? size : page_left;
      off_t retval;

      /* Write from page into file. if its read only failure. */
        bool access_page = page_lock (usrc, false);
      if (!access_page) thread_exit ();




      /* Do the write. */
      if (handle == STDOUT_FILENO)
        {
          putbuf (usrc, write_amt);
          retval = write_amt;
        }
      else  retval = file_write (fd->file, usrc, write_amt);

        // unlock the page after writtign
      page_unlock (usrc);


      if (retval < 0)
        {
          if (bytes_written == 0)
            bytes_written = -1;
          break;
        }
      bytes_written += retval;

      /* If it was a short write we're done. */
      if (retval != (off_t) write_amt)
        break;

      /* Advance. */
      usrc += retval;
      size -= retval;
    }

  return bytes_written;
}

/* Seek system call. */
static int
sys_seek (int handle, unsigned position)
{
  struct file_descriptor *fd = lookup_fd (handle);

  lock_acquire (&fs_lock);
  if ((off_t) position >= 0)
    file_seek (fd->file, position);
  lock_release (&fs_lock);

  return 0;
}

/* Tell system call. */
static int
sys_tell (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  unsigned position;

  lock_acquire (&fs_lock);
  position = file_tell (fd->file);
  lock_release (&fs_lock);

  return position;
}

/* Close system call. */
static int
sys_close (int handle)
{
  struct file_descriptor *fd = lookup_fd (handle);
  lock_acquire (&fs_lock);
  file_close (fd->file);
  lock_release (&fs_lock);
  list_remove (&fd->elem);
  free (fd);
  return 0;
}


static bool  verify_user (const void *uaddr)
{
    return (uaddr < PHYS_BASE
            && pagedir_get_page (thread_current ()->pagedir, uaddr) != NULL);
}

static struct mapping *lookup_mapping (int handle)
{
  struct thread *cur = thread_current ();
  struct list_elem *e;

  for (e = list_begin (&cur->list_mmap_files); e != list_end (&cur->list_mmap_files); e = list_next (e))
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      if (m->map_handle == handle)
        return m;
    }

  thread_exit ();
}


void clear_mapping (struct mapping *m)
{
  list_remove(&m->elem);
  for(int i = 0; i < m->page_cnt; i++)
  {

    if (pagedir_is_dirty(thread_current()->pagedir, ((const void *) ((m->base) + (PGSIZE * i)))))
    {

      file_write_at(m->file, (const void *) (m->base + (PGSIZE * i)), (PGSIZE*(m->page_cnt)), (PGSIZE * i));

    }
  }

  for(int i = 0; i < m->page_cnt; i++)
  {
      void *addr = (m->base) + (PGSIZE * i);
    clear_page(addr);
  }
}






void  syscall_exit (void)
{
  struct thread *cur = thread_current ();
  struct list_elem *e, *next;

  for (e = list_begin (&cur->fds); e != list_end (&cur->fds); e = next)
    {
      struct file_descriptor *fd = list_entry (e, struct file_descriptor, elem);
      next = list_next (e);
      lock_acquire (&fs_lock);
      file_close (fd->file);
      lock_release (&fs_lock);
      free (fd);
    }

  for (e = list_begin (&cur->list_mmap_files); e != list_end (&cur->list_mmap_files);
       e = next)
    {
      struct mapping *m = list_entry (e, struct mapping, elem);
      next = list_next (e);
      clear_mapping(m);
    }
}






static int sys_mapping (int handle, void *addr)
{
    struct file_descriptor *fd = lookup_fd (handle);
    struct mapping *m = malloc (sizeof *m);


    off_t read_bytes;

    if (m == NULL || addr == NULL || pg_ofs (addr) != 0) return -1;



    m->file = file_reopen (fd->file);
    m->map_handle = thread_current ()->next_handle++;

    if (m->file == NULL)
    {
        free (m);
        return -1;
    }

    m->base = addr;
    m->page_cnt = 0;

    // each file corresponds to a m.
    list_push_front (&thread_current ()->list_mmap_files, &m->elem);

    size_t offset = 0;
    read_bytes = file_length (m->file);
    while (read_bytes > 0)
    {

        uint32_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        uint32_t page_zero_bytes = PGSIZE - page_read_bytes;

        // this pages reflect the consequtive pages a file needs in the current mapping
        if(add_file_to_mapping(m->file,offset,
                               addr,page_read_bytes,
                               page_zero_bytes,m) == -1){
            return -1;
        }

        //advance
        read_bytes -= page_read_bytes;
        offset += page_read_bytes;
        m->page_cnt++;
    }

    return m->map_handle;
}


static int sys_munmap (int mapping)
{
    /* Get the map corresponding to the given map id, and attempt to unmap. */
    struct mapping *map = lookup_mapping(mapping);
    clear_mapping(map);
    return 0;
}

int add_file_to_mapping(struct file* file,
                        off_t ofs,
                        uint8_t* addr,
                        uint32_t page_read_bytes,
                        uint32_t page_zero_bytes,
                        struct mapping *m){
    // files are pushed into current thread's SPT
    struct spt_entry *pte = pte_allocate ((uint8_t *) addr + ofs, false);

    if (pte == NULL)
    {
        clear_mapping (m);
        return -1;
    }
    pte->pinned = false;
    pte->file_ptr = file;
    pte->file_offset = ofs;
    pte->file_bytes = page_read_bytes;


    return 0;

}

