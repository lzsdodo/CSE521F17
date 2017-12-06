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

static struct list frame_list;
static size_t frame_cnt;
static struct lock FT_lock;



void frame_init (void)
{
  void*user_page_kaddr;
  lock_init (&FT_lock);
    list_init (&frame_list);


  while ((user_page_kaddr = palloc_get_page (PAL_USER)) != NULL)

    {
        struct frame* f=(struct frame *) malloc (sizeof(struct frame));
        lock_init (&f->lock);
        f->base = user_page_kaddr;
        f->pte = NULL;
        list_push_front(&frame_list, &f->elem);
    }


    frame_cnt = list_size(&frame_list);


}


struct frame* find_free_frame(void){
    lock_acquire (&FT_lock);
    struct list_elem *e;
    struct frame* fp;
    for(e=list_begin(&frame_list);e!=list_end(&frame_list);e=list_next(e))
    {
        fp=list_entry (e, struct frame, elem);
        bool acquired = lock_try_acquire (&fp->lock);
        if (acquired==false) continue;
        // if you locate an frame which do not contain page
        if (fp->pte == NULL) {
            lock_release (&FT_lock);
            return fp;
        }
        lock_release (&fp->lock);


    }
    return NULL;

}

struct frame* evict(struct spt_entry *input_p) {
    struct list_elem *e = e = list_begin(&frame_list);
    struct frame *fp;
    for (int i = 0; i < frame_cnt * 2; i++) {
        fp = list_entry(e,struct frame, elem);
        if (e == list_end(&frame_list)) e = list_begin(&frame_list);
        e = list_next(e);

        bool acquired = lock_try_acquire(&fp->lock);
        if (acquired == false) continue;

        if (!LRU(fp->pte)) {
            lock_release(&fp->lock);
            continue;
        }
        lock_release(&FT_lock);

        /* Evict this page and get a free frame */

        bool success = evict_target_page(fp->pte);

        if (!success) {
            lock_release(&fp->lock);
            // if you cannot evict, return no frame
            return NULL;
        }

        fp->pte = input_p;
        return fp;
    }
    lock_release (&FT_lock);
    return NULL;
}


// must some how get a free frame for current pte
struct frame *frame_Alloc (struct spt_entry *input_p) {

    size_t i;
    struct frame* free_f= find_free_frame();
    if(free_f){
        free_f ->pte = input_p;
        return free_f;

    }
    else{

        return evict(input_p);

    }
}

void lock_page_frame (struct spt_entry *pte)
{
    struct frame* f = pte->frame;

    if (f) {
        lock_acquire (&f->lock);
//        if (f != pte->frame) {
//            lock_release (&f->lock);
//        }
    }
}


void frame_unlock (struct spt_entry *pte)
{ struct frame* f = pte -> frame;
  lock_release (&f->lock);
}

void frame_free (struct frame *f)
{

    f->pte = NULL;
    lock_release (&f->lock);
}
