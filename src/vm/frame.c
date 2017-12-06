#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct frame *total_frames;
static size_t frame_cnt;

static struct lock scan_lock;
static size_t hand;


void frame_init (void)
{
  void*user_page_kaddr;
  lock_init (&scan_lock);
  total_frames = malloc (sizeof *total_frames * init_ram_pages);
  if (total_frames == NULL) PANIC ("not enough memory");

  while ((user_page_kaddr = palloc_get_page (PAL_USER)) != NULL)

    {
        //returns its kernel virtual address.
         struct frame *f = &total_frames[frame_cnt];
         lock_init (&f->lock);
         f->base = user_page_kaddr;
         f->pte = NULL;
        frame_cnt++;
    }
}


struct frame* find_free_frame(struct frame*total_frames){
    size_t i;

    lock_acquire (&scan_lock);

    /* Find a free frame. */
    for (i = 0; i < frame_cnt; i++) {
        struct frame *f = &total_frames[i];

        bool acquired = lock_try_acquire (&f->lock);

        if (acquired==false) continue;

        // if you locate an frame which do not contain page
        if (f->pte == NULL) {
            lock_release (&scan_lock);
            return f;
        }
        lock_release (&f->lock);
    }

    return NULL;
}

struct frame* eviction(struct frame* total_frames){

}


/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
static struct frame *try_frame_alloc_and_lock (struct page_table_entry *input_p)
{
  size_t i;

    struct frame* free_f= find_free_frame(total_frames);
    if(free_f){
        free_f ->pte = input_p;
        return free_f;

    }
    else{

        // evict a frame to get a free frame
        for (i = 0; i < frame_cnt*2; i++) {
            /* Get a frame. */
            struct frame *f = &total_frames[hand];
            if (++hand >= frame_cnt) hand = 0;

            bool acquired = lock_try_acquire (&f->lock);

            if (acquired==false) continue;

//            if (f->pte == NULL) {
//                f->pte = input_p;
//                lock_release (&scan_lock);
//                return f;
//            }

            if (!LRU (f->pte)) {
                lock_release (&f->lock);
                continue;
            }

            lock_release (&scan_lock);

            /* Evict this frame. */
            if (!evict_target_page (f->pte)) {
                lock_release (&f->lock);
                return NULL;
            }

            f->pte = input_p;
            return f;
        }

    }





  lock_release (&scan_lock);
  return NULL;
}





//TODO: remove this method
struct frame *frame_alloc_and_lock (struct page_table_entry *pte) {
  struct frame *f = try_frame_alloc_and_lock (pte);
  return f;
}


void lock_page_frame (struct page_table_entry *pte)
{

  struct frame *f = pte->frame;

  if (f) {
      lock_acquire (&f->lock);
      if (f != pte->frame) {
          lock_release (&f->lock);
      }
  }
}


void frame_free (struct frame *f)
{

  f->pte = NULL;
  lock_release (&f->lock);
}


void frame_unlock (struct frame *f)
{
  lock_release (&f->lock);
}
