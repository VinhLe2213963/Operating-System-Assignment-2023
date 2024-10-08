#ifndef OSMM_H
#define OSMM_H

#define PAGING_MAX_MMSWP 4 /* max number of supported swapped space */
#define PAGING_MAX_SYMTBL_SZ 30

#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>

typedef char BYTE;
typedef uint32_t addr_t;
//typedef unsigned int uint32_t;

struct fifo_node{
   uint32_t *pte; // pointer to page table entry
   int fpn;
   struct fifo_node *pg_next; 
};

/*
 *  Memory region struct
 */
struct vm_rg_struct {
   unsigned long rg_start;
   unsigned long rg_end;

   struct vm_rg_struct *rg_next;
};

/*
 *  Memory area struct
 */
struct vm_area_struct {
   unsigned long vm_id;
   unsigned long vm_start;
   unsigned long vm_end;

   unsigned long sbrk;
/*
 * Derived field
 * unsigned long vm_limit = vm_end - vm_start
 */
   struct mm_struct *vm_mm;
   struct vm_rg_struct *vm_freerg_list;
   struct vm_area_struct *vm_next;
};

/* 
 * Memory management struct
 */
struct mm_struct {
   uint32_t *pgd;

   struct vm_area_struct *mmap;

   /* Currently we support a fixed number of symbol */
   struct vm_rg_struct symrgtbl[PAGING_MAX_SYMTBL_SZ];

   /* list of free page */
   // struct pgn_t *fifo_pgn;
};

/*
 * FRAME/MEM PHY struct
 */
struct framephy_struct { 
   int fpn;
   struct framephy_struct *fp_next;

   /* Resereed for tracking allocated framed */
   struct mm_struct* owner;
};

struct memphy_struct {
   /* Basic field of data and size */
   BYTE *storage;
   int maxsz;
   
   /* Sequential device fields */ 
   int rdmflg;
   int cursor;

   /* Management structure */
   struct framephy_struct *free_fp_list;
   struct fifo_node *fifo_fp_list;

   pthread_mutex_t lock;
   pthread_mutex_t fifo_lock;
};
/* 
 * TLB_entry/TLB struct
 */
struct tlb_entry {
   uint32_t pid;
   int validBit;
   int pgn;
   int fpn;
   int lruCount;
};

struct tlb_struct {
   struct tlb_entry *tlb_entry;
   int maxsz;
};

#endif
