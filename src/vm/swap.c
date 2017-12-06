#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"



/* Number of sectors per page. */
#define PAGE_SECTORS (PGSIZE / BLOCK_SECTOR_SIZE)

/* Sets up swap. */
void swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device == NULL) {
      printf ("no swap device--swap disabled\n");
      swap_map = bitmap_create (0);
  }
  else {
      swap_map = bitmap_create (block_size (swap_device) / PAGE_SECTORS);
  }

  if (swap_map == NULL)
      PANIC ("couldn't create swap bitmap");

  lock_init (&swap_lock);
}

/* Swaps in page P, which must have a locked frame
   (and be swapped out). */
void swap_in (struct spt_entry *pte)
{
  size_t i;

//  ASSERT (pte->frame != NULL);
//  ASSERT (lock_held_by_current_thread (&pte->frame->lock));
//  ASSERT (pte->sector != (block_sector_t) -1);

  for (i = 0; i < PAGE_SECTORS; i++){
    block_read (swap_device,
                pte->sector + i,
                pte->frame->base + i * BLOCK_SECTOR_SIZE);

  }

  bitmap_reset (swap_map, pte->sector / PAGE_SECTORS);
  pte->sector = (block_sector_t) -1;
}

/* Swaps out page P, which must have a locked frame. */
bool swap_out (struct spt_entry *pte)
{
  size_t slot;
  size_t i;

  ASSERT (pte->frame != NULL);
  ASSERT (lock_held_by_current_thread (&pte->frame->lock));

  lock_acquire (&swap_lock);
  slot = bitmap_scan_and_flip (swap_map, 0, 1, false);
  lock_release (&swap_lock);
  if (slot == BITMAP_ERROR)
      return false;

  pte->sector = slot * PAGE_SECTORS;

  /*  Write out page sectors for each modified block. */
  for (i = 0; i < PAGE_SECTORS; i++) {
      block_write (swap_device, pte->sector + i,
                   (uint8_t *) pte->frame->base + i * BLOCK_SECTOR_SIZE);
  }

  pte->permission = false;
  pte->file_ptr = NULL;
  pte->file_offset = 0;
  pte->file_bytes = 0;

  return true;
}
