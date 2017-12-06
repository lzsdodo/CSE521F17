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


void page_destructor (struct hash_elem *page_hash, void *aux UNUSED);
void free_process_PT (void);
struct spt_entry *search_page (const void *address);
bool page_into_frame (struct spt_entry *pte);
//bool page_fault_load (void *fault_addr);
bool evict_target_page (struct spt_entry *pte);
bool LRU (struct spt_entry *pte);
struct spt_entry *pte_allocate (void *vaddr, bool read_only);
void clear_page (void *vaddr);
unsigned page_hash (const struct hash_elem *e, void *aux UNUSED);
bool addr_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED);
struct spt_entry * insert_PTE_into_currPT(struct spt_entry *input);



struct spt_entry *search_page (const void *address)
{

    struct spt_entry target_pte;

  if (address < PHYS_BASE)
    {
      size_t addr =  (void *) pg_round_down (address);

      target_pte.addr = addr;

        struct hash_elem *e = hash_find (thread_current()->SPT, &target_pte.hash_elem);
        if (e != NULL)  return hash_entry (e, struct spt_entry, hash_elem);

        else{

            void* user_stk_ptr = thread_current()->user_esp;

            bool inflow = addr > PHYS_BASE - STACK_MAX ? true : false;
            bool valid = user_stk_ptr - 32 < address? true:false;

            if(inflow && valid)  return pte_allocate (target_pte.addr, false);
        };


    }

  return NULL;
}

unsigned page_hash (const struct hash_elem *e, void *aux UNUSED)
{
    const struct spt_entry *pte = hash_entry (e, struct spt_entry, hash_elem);
    return ((uintptr_t) pte->addr) >> PGBITS;
}

//Returns true if page A's address is smaller than B
bool addr_less (const struct hash_elem *a_, const struct hash_elem *b_, void *aux UNUSED)
{
    const struct spt_entry *a = hash_entry (a_, struct spt_entry, hash_elem);
    const struct spt_entry *b = hash_entry (b_, struct spt_entry, hash_elem);

    return a->addr < b->addr;
}

// put a page into frame
bool page_into_frame (struct spt_entry *pte)
{

  pte->frame = frame_Alloc (pte);
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

  }
  else {
      memset (pte->frame->base, 0, PGSIZE);
  }

  return true;
}



bool evict_target_page (struct spt_entry *pte)
{
  bool dirty;
  bool evicted = false;

  ASSERT (pte->frame != NULL);
  ASSERT (lock_held_by_current_thread (&pte->frame->lock));



    // force page fault
    uint32_t *pd = pte->thread->pagedir;
    void *upage = pte->addr;
    pagedir_clear_page(pd,upage);

// set current frame's dirty value
  dirty = pagedir_is_dirty (pte->thread->pagedir, (const void *) pte->addr);

  if(dirty == false)  evicted = true;

  if (pte->file_ptr == NULL) evicted = swap_out(pte);

  else if (dirty == true) {
      if(pte->permission) evicted = swap_out(pte);

      else {
          evicted = file_write_at(pte->file_ptr,
                                  (const void *) pte->frame->base,
                                  pte->file_bytes, pte->file_offset);
      }
  }
  if(evicted == true)  pte->frame = NULL;
  return evicted;
}

bool LRU (struct spt_entry *pte)
{
  uint32_t curr_pd = pte->thread->pagedir;
  bool accessed = pagedir_is_accessed (curr_pd, pte->addr);
  if (accessed) pagedir_set_accessed (curr_pd, pte->addr, false);
  return !accessed;
}


struct spt_entry *pte_allocate (void *vaddr, bool read_only)
{
  struct thread *curr_thread = thread_current ();
  struct spt_entry *pte = malloc (sizeof *pte);

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


    struct spt_entry *ans = insert_PTE_into_currPT(pte);


  return ans;
}


struct spt_entry * insert_PTE_into_currPT(struct spt_entry *pte) {
    struct hash_elem *a = hash_insert(pte->thread->SPT, &pte->hash_elem);

    if (a == NULL) {
        return pte;
    } else {
        free(pte);
        return NULL;

    }
}





bool page_lock (const void *addr, bool will_write)
{
  struct spt_entry *pte = search_page (addr);
  bool success = true;

  if (pte == NULL || (pte->read_only && will_write)) {
      return false;
  }
  else {
      lock_page_frame (pte);
      if (pte->frame == NULL) {
          bool a1 = page_into_frame(pte);
          bool a2 = pagedir_set_page (thread_current()->pagedir,
                                      pte->addr,
                                      pte->frame->base,
                                      !pte->read_only);
          success = a1 && a2;
      }
      else {
          success = true;
      }
  }

  return success;
}


void page_unlock (const void *addr) {
    struct spt_entry *pte = search_page(addr);
    frame_unlock(pte);
}



void page_destructor (struct hash_elem *page_hash, void *aux UNUSED)
{
    struct spt_entry *pte = hash_entry (page_hash, struct spt_entry, hash_elem);
    lock_page_frame (pte);
    if (pte->frame) frame_free (pte->frame);
    free (pte);
}

//free page table of current process
void free_process_PT (void)
{
    struct thread *t = thread_current ();
    struct hash *curr_PT = t->SPT;
    if (curr_PT) hash_destroy (curr_PT, page_destructor);
}
void clear_page (void *addr)
{
    struct spt_entry *pte = search_page (addr);
    lock_page_frame (pte);
    if (pte->frame) {
        struct frame *f = pte->frame;
        if (pte->file_ptr && !pte->permission) {
            evict_target_page (pte);
        }
        frame_free (f);
    }
    hash_delete (thread_current()->SPT, &pte->hash_elem);
    free (pte);
}