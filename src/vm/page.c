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


static void page_destructor (struct hash_elem *page_hash, void *aux UNUSED);
void free_process_PT (void);
static struct page_table_entry *search_page (const void *address);
static bool page_into_frame (struct page_table_entry *pte);
bool page_in (void *fault_addr);
bool evict_target_page (struct page_table_entry *pte);
bool page_recentAccess (struct page_table_entry *pte);
struct page_table_entry *page_allocate (void *vaddr, bool read_only);
void clear_page (void *vaddr);
unsigned page_hash (const struct hash_elem *e, void *aux UNUSED);
bool addr_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);



static void page_destructor (struct hash_elem *page_hash, void *aux UNUSED)
{
  struct page_table_entry *pte = hash_entry (page_hash, struct page_table_entry, hash_elem);
  lock_page_frame (pte);
  if (pte->frame) frame_free (pte->frame);
  free (p);
}

//free page table of current process
void free_process_PT (void)
{
  struct thread *t = thread_current ();
  struct hash *curr_PT = t->pages;
  if (curr_PT) hash_destroy (curr_PT, page_destructor);
}

/* Returns the page containing the given virtual ADDRESS,
   or a null pointer if no such page exists.
   Allocates stack pages as necessary. */
static struct page_table_entry *search_page (const void *address)
{

  if (address < PHYS_BASE)
    {
      struct page_table_entry target_pte;
      struct hash_elem *e;

      /* Find existing page. */
//        round down to nearest page
    target_pte.addr = (void *) pg_round_down (address);

      e = hash_find (thread_current ()->pages, &target_page.hash_elem);
      if (e != NULL)
          return hash_entry (e, struct page_table_entry, hash_elem);

      /* -We need to determine if the program is attempting to access the stack.
         -First, we ensure that the address is not beyond the bounds of the stack space (1 MB in this
          case).
         -As long as the user is attempting to acsess an address within 32 bytes (determined by the space
          needed for a PUSHA command) of the stack pointers, we assume that the address is valid. In that
          case, we should allocate one more stack page accordingly.
      */

      //TODO: stack overflow + smaller than smallest value
      if ((target_pte.addr > PHYS_BASE - STACK_MAX) &&
        ((void *)thread_current()->user_esp - 32 < address)) {
        return page_allocate (target_page.addr, false);
      }
    }

  return NULL;
}

/* Returns a hash value for the page that E refers to. */
unsigned page_hash (const struct hash_elem *e, void *aux UNUSED)
{
    const struct page_table_entry *pte = hash_entry (e, struct page_table_entry, hash_elem);
    return ((uintptr_t) pte->addr) >> PGBITS;
}

/* Returns true if page A's address is smaller than B. */
bool addr_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
    const struct page_table_entry *a = hash_entry (a_, struct page_table_entry, hash_elem);
    const struct page_table_entry *b = hash_entry (b_, struct page_table_entry, hash_elem);

    return a->addr < b->addr;
}

/* Locks a frame for page P and pages it in.
   Returns true if successful, false on failure. */
static bool page_into_frame (struct page_table_entry *pte)
{

  pte->frame = frame_alloc_and_lock (pte);
  if (pte->frame == NULL) return false;

  /* Copy data into the frame. */
  if (pte->sector != (block_sector_t) -1) {
      swap_in (pte);
  }
  else if (pte->file_ptr) {
      // read data from files
      off_t read_bytes = file_read_at (pte->file_ptr, pte->frame->base,
                                       pte->file_bytes, pte->file_offset);
      off_t zero_bytes = PGSIZE - read_bytes;

      memset (pte->frame->base + read_bytes, 0, zero_bytes);
      if (read_bytes != pte->file_bytes) {
        printf ("bytes read (%"PROTd") != bytes requested (%"PROTd")\n",read_bytes, p->file_bytes);
      }
  }
  else {
    memset (pte->frame->base, 0, PGSIZE);
  }

  return true;
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
bool page_in (void *fault_addr)
{

    bool success;
    struct thread* curr = thread_current();
    if (curr -> pages == NULL) return false;
    struct page_table_entry *target_pte = search_page (fault_addr);
    if (target_pte == NULL) return false;


    //TODO: really need lock frame?
  lock_page_frame (target_pte);

  if (target_pte->frame == NULL)
    {
        bool paged_in = page_into_frame (target_pte);
      if (paged_in == false) return false;
    }
//  ASSERT (lock_held_by_current_thread (&target_pte->frame->lock));

  success = pagedir_set_page (thread_current()->pagedir, target_pte->addr,
                              target_pte->frame->base, !target_pate->read_only);

  /* Release frame. */
  frame_unlock (target_pte->frame);

  return success;
}

/* Evicts page P.
   P must have a locked frame.
   Return true if successful, false on failure. */
bool evict_target_page (struct page_table_entry *pte)
{
  bool dirty;
  bool evicted = false;

  ASSERT (pte->frame != NULL);
  ASSERT (lock_held_by_current_thread (&pte->frame->lock));

  /* Mark page not present in page table, forcing accesses by the
     process to fault.  This must happen before checking the
     dirty bit, to prevent a race with the process dirtying the
     page. */
  pagedir_clear_page(pte->thread->pagedir, (void *) pte->addr);

  /* Has the frame been modified? */
  /* If the frame has been modified, set 'dirty' to true. */
  dirty = pagedir_is_dirty (pte->thread->pagedir, (const void *) pte->addr);

  if(dirty == false) {
      evicted = true;
  }

  if (pte->file_ptr == NULL) {
      evicted = swap_out(pte);
  }
  else if (dirty == true) {
      if(pte->permission) {
          evicted = swap_out(pte);
      }
      else {
          evicted = file_write_at(pte->file_ptr, (const void *) pte->frame->base,
                                  pte->file_bytes, pte->file_offset);
      }
  }

 // set frame to null if successfully evicted
  if(evicted == true) {
      pte->frame = NULL;
  }
  return evicted;
}

bool page_recentAccess (struct page_table_entry *pte)
{
//  ASSERT (pte->frame != NULL);
//  ASSERT (lock_held_by_current_thread (&pte->frame->lock));
  uint32_t curr_pd = pte->thread->pagedir;
  bool accessed = pagedir_is_accessed (curr_pd, pte->addr);
  if (accessed) pagedir_set_accessed (curr_pd, pte->addr, false);
  return accessed;
}

//TODO: can examine mapping first?
struct page_table_entry *page_allocate (void *vaddr, bool read_only)
{
  struct thread *curr_thread = thread_current ();
  struct page_table_entry *pte = malloc (sizeof *pte);

  if (pte) {
      pte->thread = curr_thread;
      pte->addr = pg_round_down (vaddr);
      pte->read_only = read_only;
      pte->permission = !read_only;
      pte->frame = NULL;
      pte->sector = (block_sector_t) -1;
      pte->file_ptr = NULL;
      pte->file_offset = 0;
      pte->file_bytes = 0;

      struct hash_elem * p_mapping = hash_insert (curr_thread->pages, &pte->hash_elem);
      if (p_mapping) {
          free (pte);
          pte = NULL;
      }
  }

  return pte;
}


void clear_page (void *addr)
{
  struct page_table_entry *pte = search_page (addr);
//  ASSERT (pte != NULL);
  lock_page_frame (pte);
  if (pte->frame) {
      struct frame *f = pte->frame;
      if (pte->file_ptr && !pte->permission) {
          evict_target_page (pte);
      }
      frame_free (f);
  }
  hash_delete (thread_current()->pages, &pte->hash_elem);
  free (pte);
}



/* Tries to lock the page containing ADDR into physical memory.
   If WILL_WRITE is true, the page must be writeable;
   otherwise it may be read-only.
   Returns true if successful, false on failure. */
bool page_lock (const void *addr, bool will_write)
{
  struct page_table_entry *pte = search_page (addr);
  bool success = true;

  if (pte == NULL || (pte->read_only && will_write)) {
      return false;
  }
  else {
      lock_page_frame (pte);
      if (pte->frame == NULL) {
          bool a1 = page_into_frame(pte);
          bool a2 = pagedir_set_page (thread_current()->pagedir, pte->addr,
                                      pte->frame->base, !pte->read_only);
          success = a1 && a2;
      }
      else {
          success = true;
      }
  }

  return success;
}

/* Unlocks a page locked with page_lock(). */
void page_unlock (const void *addr) {
    struct page_table_entry *pte = search_page(addr);
  //  ASSERT (pte != NULL);
    frame_unlock(pte->frame);
}
