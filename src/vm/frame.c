#include "vm/frame.h"
#include <stdio.h>
#include "vm/page.h"
#include "devices/timer.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "list.h"
//static struct frame *total_frames;
static struct list frame_list;
static size_t frame_cnt;

static struct lock scan_lock;
static size_t hand;


void frame_init (void)
{
  void*user_page_kaddr;
  lock_init (&scan_lock);
    list_init (&frame_list);

//  total_frames = malloc (sizeof *total_frames * init_ram_pages);
//  if (total_frames == NULL) PANIC ("not enough memory");



  while ((user_page_kaddr = palloc_get_page (PAL_USER)) != NULL)

    {
        struct frame* f=(struct frame *) malloc (sizeof(struct frame));
        lock_init (&f->lock);
        f->base = user_page_kaddr;
        f->pte = NULL;
        list_push_front(&frame_list, &f->elem);
    }


    frame_cnt = list_size(&frame_list);



//        {
//        //returns its kernel virtual address.
//         struct frame *f = &total_frames[frame_cnt];
//         lock_init (&f->lock);
//         f->base = user_page_kaddr;
//         f->pte = NULL;
//        frame_cnt++;
//    }


}


struct frame* find_free_frame(void){
    lock_acquire (&scan_lock);
    struct list_elem *e;
    struct frame* fp;
    for(e=list_begin(&frame_list);e!=list_end(&frame_list);e=list_next(e))
    {
        fp=list_entry (e, struct frame, elem);
        bool acquired = lock_try_acquire (&fp->lock);
        if (acquired==false) continue;
        // if you locate an frame which do not contain page
        if (fp->pte == NULL) {
            lock_release (&scan_lock);
            return fp;
        }
        lock_release (&fp->lock);


    }
    return NULL;

}



//struct frame* find_free_frame(struct frame*total_frames){
//    size_t i;
//
//    lock_acquire (&scan_lock);
//
//    /* Find a free frame. */
//    for (i = 0; i < frame_cnt; i++) {
//        struct frame *f = &total_frames[i];
//
//        bool acquired = lock_try_acquire (&f->lock);
//
//        if (acquired==false) continue;
//
//        // if you locate an frame which do not contain page
//        if (f->pte == NULL) {
//            lock_release (&scan_lock);
//            return f;
//        }
//        lock_release (&f->lock);
//    }
//
//    return NULL;
//}



/* Tries to allocate and lock a frame for PAGE.
   Returns the frame if successful, false on failure. */
static struct frame *try_frame_alloc_and_lock (struct page_table_entry *input_p)
{
  size_t i;

    struct frame* free_f= find_free_frame();
    if(free_f){
        free_f ->pte = input_p;
        return free_f;

    }
    else{

        // evict a frame to get a free frame
//        for (i = 0; i < frame_cnt*2; i++) {
//            /* Get a frame. */
//            struct frame *f = &total_frames[hand];
//            if (++hand >= frame_cnt) hand = 0;
//
//            bool acquired = lock_try_acquire (&f->lock);
//
//            if (acquired==false) continue;
//
//
//            if (!LRU (f->pte)) {
//                lock_release (&f->lock);
//                continue;
//            }
//
//            lock_release (&scan_lock);
//
//            /* Evict this frame. */
//            if (!evict_target_page (f->pte)) {
//                lock_release (&f->lock);
//                return NULL;
//            }
//
//            f->pte = input_p;
//            return f;
//        }

        struct list_elem *e = e=list_begin(&frame_list);
        struct frame* fp;
        for(int i = 0; i < frame_cnt*2; i++){
            fp = list_entry (e, struct frame, elem);
            if(e == list_end(&frame_list)) e = list_begin(&frame_list);
            e = list_next(e);

            bool acquired = lock_try_acquire (&fp->lock);
            if (acquired==false) continue;

            if (!LRU (fp->pte)) {
                lock_release (&fp->lock);
                continue;
            }
            lock_release (&scan_lock);

            /* Evict this frame. */
            if (!evict_target_page (fp->pte)) {
                lock_release (&fp->lock);
                return NULL;
            }

            fp->pte = input_p;
            return fp;

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
