#ifndef VM_SWAP_H
#define VM_SWAP_H 1
#include "vm/swap.h"
#include <bitmap.h>
#include <debug.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/page.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <stdbool.h>


/* The swap device. */
static struct block *swap_device;
/* Used swap pages. */
static struct bitmap *swap_map;
/* Protects swap_map. */
static struct lock swap_lock;
struct page;

void swap_init (void);
void swap_in (struct page *);
bool swap_out (struct page *);

#endif /* vm/swap.h */
