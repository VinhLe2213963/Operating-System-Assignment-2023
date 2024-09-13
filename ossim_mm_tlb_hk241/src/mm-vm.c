#include "os-cfg.h"

#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
    // Invalid region
    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;

    struct vm_rg_struct *rg_head = mm->mmap->vm_freerg_list;                // Head of free region list
    struct vm_rg_struct *new_free_rg = malloc(sizeof(struct vm_rg_struct)); // New free region
    struct vm_rg_struct *cur = NULL;                                        // Current region

    // Initialize new free region
    new_free_rg->rg_start = rg_elmt->rg_start;
    new_free_rg->rg_end = rg_elmt->rg_end;
    new_free_rg->rg_next = NULL;

    // Insert new free region to the list
    if (rg_head == NULL || rg_elmt->rg_start <= rg_head->rg_start) // Insert to the head
    {
        new_free_rg->rg_next = rg_head;
        mm->mmap->vm_freerg_list = new_free_rg;
    }
    else // Insert to the middle or tail
    {
        cur = rg_head;
        while (cur->rg_next != NULL && cur->rg_next->rg_start < new_free_rg->rg_end)
        {
            cur = cur->rg_next;
        }

        new_free_rg->rg_next = cur->rg_next;
        cur->rg_next = new_free_rg;
    }

    // Merge the free region
    struct vm_rg_struct *right_rg = new_free_rg->rg_next; // Merge with right region
    if (right_rg != NULL && new_free_rg->rg_end == right_rg->rg_start)
    {
        new_free_rg->rg_end = right_rg->rg_end;
        new_free_rg->rg_next = right_rg->rg_next;
        free(right_rg);
    }

    struct vm_rg_struct *left_rg = cur; // Merge with left region
    if (left_rg != NULL && left_rg->rg_end == new_free_rg->rg_start)
    {
        left_rg->rg_end = new_free_rg->rg_end;
        left_rg->rg_next = new_free_rg->rg_next;
        free(new_free_rg);
    }

    printf("Freeing region: rg_start=%lu rg_end=%lu\n", rg_elmt->rg_start, rg_elmt->rg_end);

    return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
    struct vm_area_struct *pvma = mm->mmap;

    if (mm->mmap == NULL)
        return NULL;

    int vmait = 0;

    while (vmait < vmaid)
    {
        if (pvma == NULL)
            return NULL;

        vmait++;
        pvma = pvma->vm_next;
    }

    return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;

    return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
    /*Allocate at the toproof */
    struct vm_rg_struct rgnode;

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
        caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
        *alloc_addr = rgnode.rg_start;
    }
    else
    {
        /*Attempt to increate limit to get space */
        struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
        int gap_size = cur_vma->vm_end - cur_vma->sbrk;
        if (gap_size < size)
        {
            int inc_sz = PAGING_PAGE_ALIGNSZ(size - gap_size);
            if (inc_vma_limit(caller, vmaid, inc_sz) < 0)
                return -1;
        }

        /*Successful increase limit */
        caller->mm->symrgtbl[rgid].rg_start = cur_vma->sbrk;
        caller->mm->symrgtbl[rgid].rg_end = cur_vma->sbrk + size;
        *alloc_addr = caller->mm->symrgtbl[rgid].rg_start;
        cur_vma->sbrk += size;
    }

#ifdef VMDBG
    printf("Free regions after alloc:\n");
    print_list_rg(caller->mm->mmap->vm_freerg_list);
#endif

#ifdef MMDBG
    printf("Using frames after alloc:\n");
    print_list_fp(caller->mram->fifo_fp_list);
#endif

    return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
    struct vm_rg_struct *rgnode;

    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return -1;

    /* TODO: Manage the collect freed region to freerg_list */
    rgnode = get_symrg_byid(caller->mm, rgid);

    for (int i = 0; i < caller->tlb->maxsz; i++) {
        if (caller->tlb->tlb_entry[i].pid == caller->pid && caller->tlb->tlb_entry[i].pgn == PAGING_PGN(rgnode->rg_start)) {
            caller->tlb->tlb_entry[i].validBit = 0;
        }
    }

    /*enlist the obsoleted memory region */
    enlist_vm_freerg_list(caller->mm, rgnode);

#ifdef VMDBG
    printf("Free regions after free:\n");
    print_list_rg(caller->mm->mmap->vm_freerg_list);
#endif

#ifdef MMDBG
    printf("Using frames after free:");
    print_list_fp(caller->mram->fifo_fp_list);
#endif

    return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
    int addr;

    /* By default using vmaid = 0 */
    return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
    return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
    uint32_t pte = mm->pgd[pgn];

    if (pte == 0)
    {
        return -1;
    }

    if (!PAGING_PAGE_PRESENT(pte))
    { /* Page is not online, make it actively living */
        printf("pg_getpage: page fault\n");

        int swpfpn;
        // int vicfpn;
        uint32_t *victim_pte;

        int tgtfpn = PAGING_PTE_SWPFPN(pte); // the target frame storing our variable

        /* TODO: Play with your paging theory here */
        /* Find victim page */
        if (find_victim_page(caller->mram, &victim_pte) < 0)
        {
            return -1;
        }

        int victim_fpn = PAGING_PTE_FPN(*victim_pte);
#ifdef CPU_TLB
        /* Update its online status of TLB (if needed) */
        for (int i = 0; i < caller->tlb->maxsz; i++)
        {
            if (caller->tlb->tlb_entry[i].pid == caller->pid && caller->tlb->tlb_entry[i].fpn == victim_fpn)
            {
                caller->tlb->tlb_entry[i].validBit = 0;
                break;
            }
        }
#endif
        /* Get free frame in MEMSWP */
        pthread_mutex_lock(&caller->active_mswp->lock);
        MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
        pthread_mutex_unlock(&caller->active_mswp->lock);

        /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
        /* Copy victim frame to swap */
        __swap_cp_page(caller->mram, victim_fpn, caller->active_mswp, swpfpn);
        /* Copy target frame from swap to mem */
        __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, victim_fpn);

        pthread_mutex_lock(&caller->active_mswp->lock);
        MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
        pthread_mutex_unlock(&caller->active_mswp->lock);

        /* Update page table */
        /* Update the victim page entry to SWAPPED */
        pte_set_swap(victim_pte, 0, swpfpn);

        /* Update its online status of the target page */
        /* Update the accessed page entry to PRESENT*/
        pte_set_fpn(&mm->pgd[pgn], victim_fpn);

        /* enlist page to fifo queue */
        pthread_mutex_lock(&caller->mram->fifo_lock);
        enlist_pgn_node(&caller->mram->fifo_fp_list, &mm->pgd[pgn], victim_fpn);
        pthread_mutex_unlock(&caller->mram->fifo_lock);



        *fpn = victim_fpn;

    }
    else
    {
        *fpn = PAGING_PTE_FPN(pte);
    }

    return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    MEMPHY_read(caller->mram, phyaddr, data);

#ifdef CPU_TLB
    /* Update TLB */
    tlb_update(caller->tlb, caller->pid, pgn, fpn);
#endif

    return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
    int pgn = PAGING_PGN(addr);
    int off = PAGING_OFFST(addr);
    int fpn;

    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

    int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

    MEMPHY_write(caller->mram, phyaddr, value);

#ifdef CPU_TLB
    /* Update TLB */
    tlb_update(caller->tlb, caller->pid, pgn, fpn);
#endif

    return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
        return -1;

    pg_getval(caller->mm, currg->rg_start + offset, data, caller);

    return 0;
}

/*pgwrite - PAGING-based read a region memory */
int pgread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t destination)
{
    BYTE data;
    int val = __read(proc, 0, source, offset, &data);

    proc->regs[destination] = (uint32_t)data;
#ifdef IODUMP
    printf("[PID=%d] read region=%d offset=%d value=%d\n", proc->pid, source, offset, data);
#endif

#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif

#ifdef MMDBG
    MEMPHY_dump(proc->mram);
#endif

    return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
    struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
        return -1;

    int exceed = (currg->rg_start + offset >= currg->rg_end);
    if (exceed)
    {
        printf("Write value failed\n");
        return -1;
    }

    int set_result = pg_setval(caller->mm, currg->rg_start + offset, value, caller);
    if (set_result == -1)
    {
        printf("Write value failed\n");
        return -1;
    }

    return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
#ifdef IODUMP
    printf("[PID=%d] write region=%d offset=%d value=%d\n", proc->pid, destination, offset, data);

#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif

    MEMPHY_dump(proc->mram);

#endif

    return __write(proc, 0, destination, offset, data);
}

/* remove_fifo_node - remove fifo node from fifo list
 * @caller: caller
 * @pte: page table entry
 */
void remove_fifo_node(struct pcb_t **caller, uint32_t pte)
{
    struct fifo_node *cur = (*caller)->mram->fifo_fp_list;
    struct fifo_node *prev = NULL;

    while (cur != NULL)
    {
        if (*(cur->pte) == pte)
        {
            if (prev == NULL)
            {
                (*caller)->mram->fifo_fp_list = cur->pg_next;
            }
            else
            {
                prev->pg_next = cur->pg_next;
            }
            free(cur);
            break;
        }
        prev = cur;
        cur = cur->pg_next;
    }
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memphy(struct pcb_t *caller)
{
    int pagenum, fpn;
    uint32_t pte;

    for (pagenum = 0; pagenum < caller->mm->mmap->vm_end / PAGING_PAGESZ; pagenum++)
    {
        pte = caller->mm->pgd[pagenum];

        if (!PAGING_PAGE_PRESENT(pte))
        {
            fpn = PAGING_PTE_SWPFPN(pte);
            // pthread_mutex_lock(&caller->active_mswp->lock);
            MEMPHY_put_freefp(caller->active_mswp, fpn);
            // pthread_mutex_unlock(&caller->active_mswp->lock);
        }
        else
        {
            fpn = PAGING_PTE_FPN(pte);
            // pthread_mutex_lock(&caller->active_mswp->lock);
            MEMPHY_put_freefp(caller->mram, fpn);
            // pthread_mutex_unlock(&caller->active_mswp->lock);
            pthread_mutex_lock(&caller->mram->fifo_lock);
            remove_fifo_node(&caller, pte);
            pthread_mutex_unlock(&caller->mram->fifo_lock);
        }
    }

    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
    struct vm_rg_struct *newrg;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    newrg = malloc(sizeof(struct vm_rg_struct));

    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = newrg->rg_start + size;

    return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    /* TODO validate the planned memory area is not overlapped */

    return OVERLAP(cur_vma->vm_start, cur_vma->vm_end, vmastart, vmaend);
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    int incnumpage = inc_amt / PAGING_PAGESZ;
    struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

    int old_end = cur_vma->vm_end;

    /*Validate overlap of obtained region */
    if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
        return -1; /*Overlap and failed allocation */

    /* The obtained vm area (only)
     * now will be alloc real ram region */
    cur_vma->vm_end += inc_sz;
    if (vm_map_ram(caller, area->rg_start, area->rg_end,
                   old_end, incnumpage, newrg) < 0)
        return -1; /* Map the memory to MEMRAM */

    return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct memphy_struct *mram, uint32_t **retpte)
{
    /* TODO: Implement the theorical mechanism to find the victim page */
    pthread_mutex_lock(&mram->fifo_lock);
    struct fifo_node *pg = mram->fifo_fp_list;
    struct fifo_node *prev = NULL;

    if (pg == NULL)
        return -1;

    while (pg->pg_next != NULL)
    {
        prev = pg;
        pg = pg->pg_next;
    }

    if (prev == NULL)
    {
        mram->fifo_fp_list = NULL;
    }
    else
    {
        prev->pg_next = NULL;
    }
    *retpte = pg->pte;
    free(pg);

    pthread_mutex_unlock(&mram->fifo_lock);
    return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
    struct vm_rg_struct *prev = NULL;

    if (rgit == NULL)
        return -1;

    /* Probe unintialized newrg */
    newrg->rg_start = newrg->rg_end = -1;

    /* Traverse on list of free vm region to find a fit space */
    while (rgit != NULL)
    {
        if (rgit->rg_start + size <= rgit->rg_end)
        { /* Current region has enough space */
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            /* Update left space in chosen region */
            if (rgit->rg_start + size < rgit->rg_end)
            {
                rgit->rg_start = rgit->rg_start + size;
            }
            else
            { /*Use up all space, remove current node */
                if (prev == NULL)
                    cur_vma->vm_freerg_list = NULL;
                else
                    prev->rg_next = rgit->rg_next;
                free(rgit);
            }
            return 0;
        }
        else
        {
            prev = rgit;
            rgit = rgit->rg_next; // Traverse next rg
        }
    }

    // if (newrg->rg_start == -1) // new region not found
    //     return -1;

    return -1;
}

#endif
