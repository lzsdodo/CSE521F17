#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"

/* Virtual page. */
// struct page
struct page_table_entry {
    /* Immutable members. */
    void *addr;                 /* User virtual address. */
    bool read_only;             /* Read-only page? */
    struct thread *thread;      /* Owning thread. */

    /* Accessed only in owning process context. */
    struct hash_elem hash_elem; /* struct thread `pages' hash element. */

    /* Set only in owning process context with frame->lock_page_frame held.
       Cleared only with scan_lock and frame->lock_page_frame held. */
    struct frame *frame;        /* Page frame. */

    /* Swap information, protected by frame->lock_page_frame. */
    block_sector_t sector;       /* Starting sector of swap area, or -1. */

    /* Memory-mapped file information, protected by frame->lock_page_frame. */
    bool permission;               /* False to write back to file,
                                   true to write back to swap. */
    struct file *file_ptr;          /* File. */
    off_t file_offset;          /* Offset in file. */
    off_t file_bytes;           /* Bytes to read/write, 1...PGSIZE. */
};

void free_process_PT (void);
struct page *page_allocate (void *, bool read_only);
void clear_page (void *vaddr);

bool page_in (void *fault_addr);
bool evict_target_page (struct page_table_entry *);
bool page_recentAccess (struct page_table_entry *);

bool page_lock (const void *, bool will_write);
void page_unlock (const void *);

hash_hash_func page_hash;
hash_less_func addr_less;

#endif /* vm/page.h */
