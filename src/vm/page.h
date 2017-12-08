#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "devices/block.h"
#include "filesys/off_t.h"
#include "threads/synch.h"
/* Maximum size of process stack, in bytes. */
/* Right now it is 1 megabyte. */
#define STACK_MAX (1024 * 1024)
/* Virtual page. */
// struct supplementary page table entry
struct spt_entry {

    void *addr;                 /* User virtual address. */
    struct thread *thread;      /* thread that owns current spt */
    struct frame *occupied_frame;        /* Page frame. */
    block_sector_t sector;       /* Starting sector of swap area, or -1. */
    bool read_only;             /* Read-only  */
    bool pinned;            /* cannot write back when false, if true, allow swap */
    struct file *file_ptr;          /* File. */
    off_t file_offset;          /* Offset in file. */
    off_t file_bytes;           /* Bytes to read/write, 1...PGSIZE. */
    struct hash_elem hash_elem; /* struct thread `pages' hash element. */
};

void free_process_PT (void);
struct spt_entry *pte_allocate (void *, bool read_only);
void clear_page (void *vaddr);
bool page_fault_load (void *fault_addr);
bool evict_target_page (struct spt_entry *);
bool LRU (struct spt_entry *);
bool page_lock (const void *, bool will_write);
void page_unlock (const void *);

hash_hash_func page_hash;
hash_less_func addr_less;

#endif /* vm/page.h */
