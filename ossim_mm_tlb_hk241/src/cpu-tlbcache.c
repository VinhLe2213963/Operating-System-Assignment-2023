/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee
 * a personal to use and modify the Licensed Source Code for
 * the sole purpose of studying during attending the course CO2018.
 */
#include "os-cfg.h"
#ifdef CPU_TLB
/*
 * Memory physical based TLB Cache
 * TLB cache module tlb/tlbcache.c
 *
 * TLB cache is physically memory phy
 * supports random access
 * and runs at high speed
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

#define init_tlbcache(mp, sz, ...) init_memphy(mp, sz, (1, ##__VA_ARGS__))

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_read(struct memphy_struct *mp, int pid, int pgnum, BYTE value)
{
    /* TODO: the identify info is mapped to
     *      cache line by employing:
     *      direct mapped, associated mapping etc.
     */
    return 0;
}

/*
 *  tlb_cache_write write TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */
int tlb_cache_write(struct memphy_struct *mp, int pid, int pgnum, BYTE value)
{
    /* TODO: the identify info is mapped to
     *      cache line by employing:
     *      direct mapped, associated mapping etc.
     */
    return 0;
}

/*
 *  TLBMEMPHY_read natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int TLBMEMPHY_read(struct memphy_struct *mp, int addr, BYTE *value)
{
    if (mp == NULL)
        return -1;

    /* TLB cached is random access by native */
    *value = mp->storage[addr];

    return 0;
}

/*
 *  TLBMEMPHY_write natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int TLBMEMPHY_write(struct memphy_struct *mp, int addr, BYTE data)
{
    MEMPHY_write(mp, addr, data);

    return 0;
}

/*
 *  TLBMEMPHY_format natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 */

int TLBMEMPHY_dump(struct tlb_struct *mp)
{
    /*TODO dump memphy contnt mp->storage
     *     for tracing the memory content
     */
    if (mp == NULL)
        return -1;

    printf("TLB Memory dump:\n");

    for (int i = 0; i < mp->maxsz; i++)
    {
        printf("TLB entry %d: pid=%d, valid=%d, pgn=%d, fpn=%d, lru=%d\n",
               i, mp->tlb_entry[i].pid, mp->tlb_entry[i].validBit,
               mp->tlb_entry[i].pgn, mp->tlb_entry[i].fpn, mp->tlb_entry[i].lruCount);
    }

    return 0;
}



/*
 *  Init TLBMEMPHY struct
 */
int init_tlbmemphy(struct tlb_struct *mp, int max_size)
{
    mp->tlb_entry = (struct tlb_entry *)malloc(max_size * sizeof(struct tlb_entry));
    for (int i = 0; i < max_size; i++)
    {
        mp->tlb_entry[i].pid = -1;
        mp->tlb_entry[i].validBit = 0;
        mp->tlb_entry[i].pgn = -1;
        mp->tlb_entry[i].fpn = -1;
        mp->tlb_entry[i].lruCount = 0;
    }
    mp->maxsz = max_size;
    return 0;
}

#endif
