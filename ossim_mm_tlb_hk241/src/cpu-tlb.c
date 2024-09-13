/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee
 * a personal to use and modify the Licensed Source Code for
 * the sole purpose of studying during attending the course CO2018.
 */
// #ifdef CPU_TLB
/*
 * CPU TLB
 * TLB module cpu/cpu-tlb.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

int tlb_change_all_page_tables_of(struct pcb_t *proc, struct memphy_struct *mp)
{
    /* TODO update all page table directory info
     *      in flush or wipe TLB (if needed)
     */

    return 0;
}

// Flush tlb of a process
int tlb_flush_tlb_of(struct pcb_t *proc, struct tlb_struct *tlb)
{
    /* TODO flush tlb cached*/
    for (int i = 0; i < tlb->maxsz; i++)
    {
        if (tlb->tlb_entry[i].pid == proc->pid)
            tlb->tlb_entry[i].validBit = 0;
    }
    return 0;
}

// Flush all tlb
int tlb_flush_all(struct tlb_struct *tlb)
{
    /* TODO flush all TLB cached*/
    for (int i = 0; i < tlb->maxsz; i++)
    {
        tlb->tlb_entry[i].validBit = 0;
    }
    return 0;
}

/*
    update lru count
    do not care about valid bit
    return the number of entries that had greater lru count than the current entry
    counter implementation of lru
*/
int tlb_updatelru(struct tlb_struct *tlb, int index)
{
    int currentCount = tlb->tlb_entry[index].lruCount;
    int count = 0;
    for (int i = 0; i < tlb->maxsz; i++)
    {
        if (tlb->tlb_entry[i].lruCount > currentCount)
        {
            // Counter implementation of LRU requires all LRU counts greater than the one being accessed now to be decremented.
            tlb->tlb_entry[i].lruCount--;
            count++;
        }
    }
    tlb->tlb_entry[index].lruCount = tlb->maxsz - 1;
    return count;
}

// find the frame number of the page number in the TLB if valid
int tlb_search(struct pcb_t *proc, struct tlb_struct *tlb, int pgnum)
{
    /* TODO search TLB cached*/
    for (int i = 0; i < tlb->maxsz; i++)
    {
        if (tlb->tlb_entry[i].pid == proc->pid && 
            tlb->tlb_entry[i].validBit && 
            tlb->tlb_entry[i].pgn == pgnum)
        {
            tlb_updatelru(tlb, i);
            return tlb->tlb_entry[i].fpn;
        }
    }
    return -1;
}

/*
    This function returns the LRU index in TLB.
    This function does not check if the LRU is valid or not.
    It also does not check for placement before replacement.
    Following the counter implementation of LRU.
*/
int tlb_getlru(struct tlb_struct *tlb)
{

    for (int i = 0; i < tlb->maxsz; i++)
    {
        if (tlb->tlb_entry[i].lruCount == 0)
        {
            return i; // It would always end here
        }
    }

    // The function would always end after the above for loop
    // But just in case that the tlb dont have any lruCount == 0
    // We find the min lruCount and return that index
    int min = tlb->tlb_entry[0].lruCount;
    for (int i = 0; i < tlb->maxsz; i++)
    {
        if (tlb->tlb_entry[i].lruCount < min)
        {
            min = tlb->tlb_entry[i].lruCount;
        }
    }
    return min;
}

// get the first invalid entry in TLB
int tlb_get_invalid(struct tlb_struct *tlb)
{
    for (int i = 0; i < tlb->maxsz; i++)
    {
        if (tlb->tlb_entry[i].validBit == 0)
        {
            return i;
        }
    }
    return -1;
}

/*
    Finds an entry in the TLB and writes onto it.
    Returns the index of the new valid entry.
*/
int tlb_update(struct tlb_struct *tlb, int pid, int pgn, int fpn)
{
    int index = tlb_get_invalid(tlb);
    if (index < 0) // If there is no invalid entry, we replace with the lru entry
    {
        index = tlb_getlru(tlb);
    }
    tlb->tlb_entry[index].pid = pid;
    tlb->tlb_entry[index].pgn = pgn;
    tlb->tlb_entry[index].fpn = fpn;
    tlb->tlb_entry[index].validBit = 1;
    tlb_updatelru(tlb, index);

    return index;
}

/*tlballoc - CPU TLB-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    int addr, val;

    /* By default using vmaid = 0 */
    val = __alloc(proc, 0, reg_index, size, &addr);

    /* TODO update TLB CACHED frame num of the new allocated page(s)*/
    /* by using tlb_cache_read()/tlb_cache_write()*/

    int pgn = PAGING_PGN(addr);
    int pte = proc->mm->pgd[pgn];
    int fpn = PAGING_PTE_FPN(pte);
    tlb_update(proc->tlb, proc->pid, pgn, fpn);

    return val;
}

/*pgfree - CPU TLB-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlbfree_data(struct pcb_t *proc, uint32_t reg_index)
{
    return __free(proc, 0, reg_index);

    /* TODO update TLB CACHED frame num of freed page(s)*/
    /* by using tlb_cache_read()/tlb_cache_write()*/
}

/*tlbread - CPU TLB-based read a region memory
 *@proc: Process executing the instruction
 *@source: index of source register
 *@offset: source address = [source] + [offset]
 *@destination: destination storage
 */
int tlbread(struct pcb_t *proc, uint32_t source,
            uint32_t offset, uint32_t destination)
{
    BYTE data;

    struct vm_rg_struct *cur_rg = get_symrg_byid(proc->mm, source);

    if (cur_rg == NULL)
        return -1;

    if (cur_rg->rg_start + offset >= cur_rg->rg_end)
        return -1;

    int pgn = PAGING_PGN((cur_rg->rg_start + offset));

    int fpn = tlb_search(proc, proc->tlb, pgn);

    int val;
    if (fpn >= 0)
    {
        printf("TLB hit at read region=%d offset=%d\n",
               source, offset);
        int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + offset;
        MEMPHY_read(proc->mram, phyaddr, &data);
        val = 0;
    }
    else
    {
        printf("TLB miss at read region=%d offset=%d\n",
               source, offset);
        val = __read(proc, 0, source, offset, &data);
    }

    proc->regs[destination] = (uint32_t)data;

#ifdef IODUMP
    printf("[PID=%d] read region=%d offset=%d value=%d\n", proc->pid, source, offset, data);
#endif

#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif

#ifdef MMDBG
    MEMPHY_dump(proc->mram);
    TLBMEMPHY_dump(proc->tlb);
#endif

    return val;
}

/*tlbwrite - CPU TLB-based write a region memory
 *@proc: Process executing the instruction
 *@data: data to be wrttien into memory
 *@destination: index of destination register
 *@offset: destination address = [destination] + [offset]
 */
int tlbwrite(struct pcb_t *proc, BYTE data,
             uint32_t destination, uint32_t offset)
{
    int val;

    struct vm_rg_struct *cur_rg = get_symrg_byid(proc->mm, destination);

    if (cur_rg == NULL)
        return -1;

    if (cur_rg->rg_start + offset >= cur_rg->rg_end)
    {
        printf("Write out of bound\n");
        return -1;
    }

    int pgn = PAGING_PGN((cur_rg->rg_start + offset));

    int fpn = tlb_search(proc, proc->tlb, pgn);

    if (fpn >= 0)
    {
        printf("TLB hit at write region=%d offset=%d value=%d\n",
               destination, offset, data);

        int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + offset;
        MEMPHY_write(proc->mram, phyaddr, data);
        val = 0;
    }
    else
    {
        printf("TLB miss at write region=%d offset=%d value=%d\n",
               destination, offset, data);

        val = __write(proc, 0, destination, offset, data);
    }

#ifdef IODUMP
    printf("[PID=%d] write region=%d offset=%d value=%d\n", proc->pid, destination, offset, data);
#endif

#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif

#ifdef MMDBG
    MEMPHY_dump(proc->mram);
    TLBMEMPHY_dump(proc->tlb);
#endif

    return val;
}

// #endif
