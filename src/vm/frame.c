#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

static struct frame *frames;
static size_t frame_cnt;

static struct lock scan_lock;
static size_t hand;


void frame_init (void)
{
  void *base;
  lock_init (&scan_lock);
  frames = malloc (sizeof *frames * init_ram_pages);
  if (frames == NULL) PANIC ("out of memory allocating page frames");

  while ((base = palloc_get_page (PAL_USER)) != NULL)
    {
      struct frame *f = &frames[frame_cnt++];
      lock_init (&f->lock);
      f->base = base;
      f->pte = NULL;
    }
}

//
//struct frame* find_free_frame(void){
//    size_t i;
//    for (i = 0; i < frame_cnt; i++)
//    {
//        struct frame *f = &frames[i];
//        if (!lock_try_acquire (&f->lock)) continue;
//        if (f->pte == NULL)
//        {
//            f->pte = pte;
//            return f;
//        }
//        lock_release (&f->lock);
//    }
//}

/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
static struct frame *try_frame_alloc_and_lock (struct page_table_entry *pte)
{
  size_t i;

  lock_acquire (&scan_lock);

  /* Find a free frame. */
  for (i = 0; i < frame_cnt; i++) {
      struct frame *f = &frames[i];

      if (!lock_try_acquire (&f->lock)) continue;

      if (f->pte == NULL) {
          f->pte = pte;
          lock_release (&scan_lock);
          return f;
      }
      lock_release (&f->lock);
  }

    // evict a frame to get a free frame
  for (i = 0; i < frame_cnt * 2; i++) {
      /* Get a frame. */
      struct frame *f = &frames[hand];
      if (++hand >= frame_cnt)
        hand = 0;

      if (!lock_try_acquire (&f->lock)) continue;

      if (f->pte == NULL) {
          f->pte = pte;
          lock_release (&scan_lock);
          return f;
      }

      if (page_recentAccess (f->page)) {
          lock_release (&f->lock);
          continue;
      }

      lock_release (&scan_lock);

      /* Evict this frame. */
      if (!evict_target_page (f->page)) {
          lock_release (&f->lock);
          return NULL;
      }

      f->pte = pte;
      return f;
    }

  lock_release (&scan_lock);
  return NULL;
}





//TODO: remove this method
struct frame *frame_alloc_and_lock (struct page *page)
{

    struct frame *f = try_frame_alloc_and_lock (page);

  return f;
}


void lock_page_frame (struct page *p)
{

  struct frame *f = p->frame;
  if (f)
    {
      lock_acquire (&f->lock);
      if (f != p->frame)
        {
          lock_release (&f->lock);
        }
    }
}


void frame_free (struct frame *f)
{
//  ASSERT (lock_held_by_current_thread (&f->lock));

  f->page = NULL;
  lock_release (&f->lock);
}


void frame_unlock (struct frame *f)
{
//  ASSERT (lock_held_by_current_thread (&f->lock));
  lock_release (&f->lock);
}
