#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"

/* A physical frame. */
struct frame {
    struct lock lock;               /* one access at a time */
    void *base;                     /* Kernel virtual base address. */
    struct spt_entry *pte;  /* Mapped process page, if any. */
    struct list_elem elem;
};

void frame_init (void);

struct frame *frame_Alloc (struct spt_entry *pte);
void lock_page_frame (struct spt_entry *pte);

void frame_free (struct frame *f);
void frame_unlock (struct spt_entry *pte);

#endif /* vm/frame.h */
