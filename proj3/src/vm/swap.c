#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"

#define SECTOR_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

void swap_init (void)
{
  swapping_block = block_get_role (BLOCK_SWAP);
  if (swapping_block == NULL) {
      PANIC("no block gg");
      return;
  }
    swap_map = bitmap_create (block_size (swapping_block) / SECTOR_PER_PAGE);

  if (swap_map == NULL)
      PANIC ("couldn't create swap bitmap");

   bitmap_set_all(swap_map, 0);
  lock_init (&swap_lock);
}


void swap_in (struct spt_entry *pte)
{

    lock_acquire(&swap_lock);
    if (!swapping_block || !swap_map)
    {
        return;
    }

  for (size_t i = 0; i < SECTOR_PER_PAGE; i++){

    block_read (swapping_block,
                pte->sector + i,
                pte->occupied_frame->base + i * BLOCK_SECTOR_SIZE);

  }
    lock_release(&swap_lock);
  bitmap_reset (swap_map, pte->sector / SECTOR_PER_PAGE);
  pte->sector = -1;

}


bool swap_out (struct spt_entry *pte)
{

    if (!swapping_block || !swap_map)
    {
        PANIC("go back to configure");
    }

    lock_acquire (&swap_lock);
    size_t free_index = bitmap_scan_and_flip (swap_map, 0, 1, false);

    if (free_index == BITMAP_ERROR){
        PANIC("Swap partition is full!");
    }

     pte->sector = free_index * SECTOR_PER_PAGE;
      pte->location = false;
     pte->file_ptr = NULL;
     pte->file_offset = 0;
     pte->file_bytes = 0;

//  Write out page sectors for each modified block.
  for ( size_t i = 0; i < SECTOR_PER_PAGE; i++)
    {
      const void *buf =  pte->occupied_frame->base + i * BLOCK_SECTOR_SIZE;

      block_write (swapping_block,
                   pte->sector + i,
                   buf);
  }
    lock_release (&swap_lock);


  return true;
}

