#include "vm/page.h"
#include <stdio.h>
#include <string.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

/* Maximum size of process stack, in bytes. */
/* Right now it is 1 megabyte. */
#define STACK_MAX (1024 * 1024)

//free a page of current process
static void destroy_page (struct hash_elem *p_, void *aux UNUSED)
{
  struct page *p = hash_entry (p_, struct page, hash_elem);
  frame_lock (p);
  if (p->frame) frame_free (p->frame);
  free (p);
}

/* free a page table of current process */
void free_current_page_table (void)
{
  struct hash *curr_PT = thread_current ()->pages;
  if (curr_PT != NULL) hash_destroy (curr_PT, destroy_page);
}

/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
static struct page *look_up_page (const void *address)
{

  if (address < PHYS_BASE)
    {
      struct page target_page;
      struct hash_elem *e;

      /* Find existing page. */
//        round down to nearest page
    target_page.addr = (void *) pg_round_down (address);

      e = hash_find (thread_current ()->pages, &target_page.hash_elem);
      if (e != NULL)
          return hash_entry (e, struct page, hash_elem);

      /* -We need to determine if the program is attempting to access the stack.
         -First, we ensure that the address is not beyond the bounds of the stack space (1 MB in this
          case).
         -As long as the user is attempting to acsess an address within 32 bytes (determined by the space
          needed for a PUSHA command) of the stack pointers, we assume that the address is valid. In that
          case, we should allocate one more stack page accordingly.
      */

      //TODO: modify here
      if ((target_page.addr > PHYS_BASE - STACK_MAX) && ((void *)thread_current()->user_esp - 32 < address))
      {
        return page_allocate (target_page.addr, false);
      }
    }

  return NULL;
}

/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
static bool do_page_in (struct page *p)
{
  /* Get a frame for the page. */
  p->frame = frame_alloc_and_lock (p);
  if (p->frame == NULL) return false;

  /* Copy data into the frame. */
  if (p->sector != (block_sector_t) -1)
    {
      swap_in (p);
    }

  else if (p->file_ptr != NULL)
    {
      /* Get data from file. */
      off_t read_bytes = file_read_at (p->file_ptr, p->frame->base,p->file_bytes, p->file_offset);
      off_t zero_bytes = PGSIZE - read_bytes;

      memset (p->frame->base + read_bytes, 0, zero_bytes);
      if (read_bytes != p->file_bytes){
        printf ("bytes read (%"PROTd") != bytes requested (%"PROTd")\n",read_bytes, p->file_bytes);
      }

    }
  else
    {
      /* Provide all-zero page. */
      memset (p->frame->base, 0, PGSIZE);
    }

  return true;
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
bool page_in (void *fault_addr)
{
  struct page *p;
  bool success;

  /* Can't handle page faults without a hash table. */
  if (thread_current ()->pages == NULL)
    return false;

  p = look_up_page (fault_addr);
  if (p == NULL)
    return false;

  frame_lock (p);
  if (p->frame == NULL)
    {
      if (!do_page_in (p))
        return false;
    }
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  /* Install frame into page table. */
  success = pagedir_set_page (thread_current ()->pagedir, p->addr,
                              p->frame->base, !p->read_only);

  /* Release frame. */
  frame_unlock (p->frame);

  return success;
}

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
bool evict_target_page (struct page *p)
{
  bool dirty;
  bool evicted = false;

  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  /* Mark page not present in page table, forcing accesses by the
     process to fault.  This must happen before checking the
     dirty bit, to prevent a race with the process dirtying the
     page. */
  pagedir_clear_page(p->thread->pagedir, (void *) p->addr);

  /* Has the frame been modified? */
  /* If the frame has been modified, set 'dirty' to true. */
  dirty = pagedir_is_dirty (p->thread->pagedir, (const void *) p->addr);

  if(dirty == false)
  {
      evicted = true;
  }

  if (p->file_ptr == NULL)
  {
      evicted = swap_out(p);
  }


  else if (dirty == true){

      if(p->permission)
      {
          evicted = swap_out(p);
      }
      else
      {
          evicted = file_write_at(p->file_ptr, (const void *) p->frame->base, p->file_bytes, p->file_offset);
      }
  }

 // set frame to null if successfully evicted
  if(evicted == true)p->frame = NULL;
  return evicted;
}

bool page_recentAccess (struct page *p)
{
  ASSERT (p->frame != NULL);
  ASSERT (lock_held_by_current_thread (&p->frame->lock));

  uint32_t curr_pd = p ->thread -> pagedir;

  // calling method to determine this
  bool accessed= pagedir_is_accessed (curr_pd, p->addr);
  if (accessed) pagedir_set_accessed (curr_pd, p->addr, false);
  return accessed;
}

/* Adds a mapping for user virtual address VADDR to the page hash
   table.  Fails if VADDR is already mapped or if memory
   allocation fails. */
struct page *page_allocate (void *vaddr, bool read_only)
{
  struct thread *t = thread_current ();
  struct page *p = malloc (sizeof *p);
  if (p != NULL)
    {
      p->addr = pg_round_down (vaddr);
      p->read_only = read_only;
      p->permission = !read_only;
      p->frame = NULL;
      p->sector = (block_sector_t) -1;
      p->file_ptr = NULL;
      p->file_offset = 0;
      p->file_bytes = 0;
      p->thread = thread_current ();
      if (hash_insert (t->pages, &p->hash_elem) != NULL)
        {
          /* Already mapped. */
          free (p);
          p = NULL;
        }
    }
  return p;
}

/* Evicts the page containing address VADDR
   and removes it from the page table. */
void page_deallocate (void *vaddr)
{
  struct page *p = look_up_page (vaddr);
  ASSERT (p != NULL);
  frame_lock (p);
  if (p->frame)
    {
      struct frame *f = p->frame;
      if (p->file_ptr && !p->permission)
        evict_target_page (p);
      frame_free (f);
    }
  hash_delete (thread_current ()->pages, &p->hash_elem);
  free (p);
}

/* Returns a hash value for the page that E refers to. */
unsigned page_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct page *p = hash_entry (e, struct page, hash_elem);
  return ((uintptr_t) p->addr) >> PGBITS;
}

/* Returns true if page A's address is smaller than B. */
bool page_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
  const struct page *a = hash_entry (a_, struct page, hash_elem);
  const struct page *b = hash_entry (b_, struct page, hash_elem);

  return a->addr < b->addr;
}

/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
bool page_lock (const void *addr, bool will_write)
{
  struct page *p = look_up_page (addr);


  if (p == NULL || (p->read_only && will_write)) return false;

  frame_lock (p);
  if (p->frame == NULL){

      bool a1 = do_page_in(p);
      bool a2 = pagedir_set_page (thread_current ()->pagedir, p->addr,p->frame->base, !p->read_only);
      return (a1 && a2);
  }
  else
      return true;
}
/* Unlocks a page locked with page_lock(). */
void page_unlock (const void *addr)
{
  struct page *p = look_up_page (addr);
  ASSERT (p != NULL);
  frame_unlock (p->frame);
}
