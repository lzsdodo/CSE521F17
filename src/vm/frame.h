#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdbool.h>
#include "threads/synch.h"

/* A physical frame. */
struct frame {
    struct lock lock;               /* Prevent simultaneous access. */
    void *base;                     /* Kernel virtual base address. */
    struct page_table_entry *pte;  /* Mapped process page, if any. */
  };

void frame_init (void);

struct frame *frame_alloc_and_lock (struct page_table_entry *pte);
void lock_page_frame (struct page_table_entry *pte);

void frame_free (struct frame *f);
void frame_unlock (struct frame *f);

#endif /* vm/frame.h */
