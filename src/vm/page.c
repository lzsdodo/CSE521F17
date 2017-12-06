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
bool page_fault_load (void *fault_addr);
bool evict_target_page (struct page_table_entry *pte);
bool LRU (struct page_table_entry *pte);
struct page_table_entry *pte_allocate (void *vaddr, bool read_only);
void clear_page (void *vaddr);
unsigned page_hash (const struct hash_elem *e, void *aux UNUSED);
bool addr_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct page_table_entry * insert_PTE_into_currPT(struct page_table_entry *input);



static void page_destructor (struct hash_elem *page_hash, void *aux UNUSED)
{
  struct page_table_entry *pte = hash_entry (page_hash, struct page_table_entry, hash_elem);
  lock_page_frame (pte);
  if (pte->frame) frame_free (pte->frame);
  free (pte);
}

//free page table of current process
void free_process_PT (void)
{
  struct thread *t = thread_current ();
  struct hash *curr_PT = t->full_PT;
  if (curr_PT) hash_destroy (curr_PT, page_destructor);
}

static struct page_table_entry *search_page (const void *address)
{

    struct page_table_entry target_pte;

  if (address < PHYS_BASE)
    {
      size_t addr =  (void *) pg_round_down (address);

      target_pte.addr = addr;

        struct hash_elem *e = hash_find (thread_current()->full_PT, &target_pte.hash_elem);
        if (e != NULL)  return hash_entry (e, struct page_table_entry, hash_elem);

        else{

            void* user_stk_ptr = thread_current()->user_esp;

            bool inflow = addr > PHYS_BASE - STACK_MAX ? true : false;
            bool valid = user_stk_ptr - 32 < address? true:false;

            if(inflow && valid)  return pte_allocate (target_pte.addr, false);
        };


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

// put a page into frame
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
      off_t read_bytes = file_read_at (pte->file_ptr,
                                       pte->frame->base,
                                       pte->file_bytes,
                                       pte->file_offset);
      off_t zero_bytes = PGSIZE - read_bytes;

      memset (pte->frame->base + read_bytes, 0, zero_bytes);
      if (read_bytes != pte->file_bytes) {
        printf ("bytes read (%"PROTd") != bytes requested (%"PROTd")\n", read_bytes, pte->file_bytes);
      }
  }
  else {
      memset (pte->frame->base, 0, PGSIZE);
  }

  return true;
}

/* Faults in the page containing FAULT_ADDR.
   Returns true if successful, false on failure. */
bool page_fault_load (void *fault_addr)
{

    bool success;
    struct thread* curr = thread_current();
    if (curr->full_PT == NULL) return false;
    struct page_table_entry *pte = search_page (fault_addr);
    if (pte == NULL) return false;


    //TODO: really need lock frame?
  lock_page_frame (pte);

  if (pte->frame == NULL)
    {
        bool paged_in = page_into_frame (pte);
      if (paged_in == false) return false;
    }


  success = pagedir_set_page (curr->pagedir,
                              pte->addr,
                              pte->frame->base,
                              !pte->read_only);

  frame_unlock (pte->frame);

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

bool LRU (struct page_table_entry *pte)
{
//  ASSERT (pte->frame != NULL);
//  ASSERT (lock_held_by_current_thread (&pte->frame->lock));
  uint32_t curr_pd = pte->thread->pagedir;
  bool accessed = pagedir_is_accessed (curr_pd, pte->addr);
  if (accessed) pagedir_set_accessed (curr_pd, pte->addr, false);
  return !accessed;
}

/**
 * allocate the vaddr to a page, and push it into page table.
 */
struct page_table_entry *pte_allocate (void *vaddr, bool read_only)
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
  }


    struct page_table_entry *ans = insert_PTE_into_currPT(pte);


  return ans;
}


struct page_table_entry * insert_PTE_into_currPT(struct page_table_entry *pte) {
    struct hash_elem *a = hash_insert(pte->thread->full_PT, &pte->hash_elem);

    if (a == NULL) {
        return pte;
    } else {
        free(pte);
        return NULL;

    }
}


void clear_page (void *addr)
{
  struct page_table_entry *pte = search_page (addr);
  lock_page_frame (pte);
  if (pte->frame) {
      struct frame *f = pte->frame;
      if (pte->file_ptr && !pte->permission) {
          evict_target_page (pte);
      }
      frame_free (f);
  }
  hash_delete (thread_current()->full_PT, &pte->hash_elem);
  free (pte);
}


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


void page_unlock (const void *addr) {
    struct page_table_entry *pte = search_page(addr);
    frame_unlock(pte->frame);
}
