#ifndef VM_SWAP_H
#define VM_SWAP_H
#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <stdbool.h>



static struct block *swapping_block;
static struct bitmap *swap_map;
static struct lock swap_lock;

void swap_init (void);
void swap_in (struct spt_entry *pte);
bool swap_out (struct spt_entry *pte);


#endif /* vm/swap.h */
