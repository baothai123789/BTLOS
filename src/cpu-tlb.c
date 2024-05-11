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
#include <pthread.h>
#ifdef CPU_TLB
static pthread_mutex_t cpu_tlb = PTHREAD_MUTEX_INITIALIZER;
int tlb_change_all_page_tables_of(struct pcb_t *proc, struct memphy_struct *mp)
{
  /* TODO update all page table directory info
   *      in flush or wipe TLB (if needed)
   */

  return 0;
}

int tlb_flush_tlb_of(struct pcb_t *proc, struct memphy_struct *mp)
{
  /* TODO flush tlb cached*/
  return 0;
}

/*tlballoc - CPU TLB-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  pthread_mutex_lock(&cpu_tlb);
  int addr, val;

  /* By default using vmaid = 0 */
  val = __alloc(proc, 0, reg_index, size, &addr);

  /* TODO update TLB CACHED frame num of the new allocated page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  TLBMEMPHY_dump(proc->tlb);
  pthread_mutex_unlock(&cpu_tlb);
  return val;
}

/*pgfree - CPU TLB-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int tlbfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  pthread_mutex_lock(&cpu_tlb);
  __free(proc, 0, reg_index);
  /* TODO update TLB CACHED frame num of freed page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  TLBMEMPHY_dump(proc->tlb);
  pthread_mutex_unlock(&cpu_tlb);
  return 0;
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
  BYTE data = -1;
  int frmnum = -1, val = 0;
  /* TODO retrieve TLB CACHED frame num of accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  /* frmnum is return value of tlb_cache_read/write value*/
  struct vm_rg_struct *region = get_symrg_byid(proc->mm, source);
  if (check_if_in_freerg_list(proc, 0, region) < 0)
  {
    printf("REGION READ NULL\n");
    return -1;
  }
  int addr = region->rg_start + offset;

  int page = PAGING_PGN(addr);

  tlb_cache_read(proc->tlb, proc->pid, page, &frmnum);
#ifdef IODUMP
  if (frmnum >= 0)
  {
    printf("TLB hit at read region=%d offset=(%d\n",
           source, offset);
  }
  else
  {
    printf("TLB miss at read region=%d offset=%d\n",
           source, offset);
  }
#ifdef PAGETBL_DUMP
  TLBMEMPHY_dump(proc->tlb);
#endif
  MEMPHY_dump(proc->mram);
#endif

  if (frmnum >= 0)
  {
    int physical_addr = (frmnum << PAGING_ADDR_FPN_HIBIT) + offset;
    MEMPHY_read(proc->mram, physical_addr, &data);
  }
  else
  {
    val = __read(proc, 0, source, offset, &data);
    if (val == 0)
    {
      tlb_cache_write(proc->tlb, proc->pid, page, data);
    }
    TLBMEMPHY_dump(proc->tlb);
  }
  destination = (uint32_t)data;

  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/

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
  int val = 0;
  BYTE frmnum = -1;
  /* TODO retrieve TLB CACHED frame num of accessing page(s))*/
  /* by using tlb_cache_read()/tlb_cache_write()
  frmnum is return value of tlb_cache_read/write value*/
  struct vm_rg_struct *region = get_symrg_byid(proc->mm, (int)destination);
  if (region == NULL)
  {
    printf("REGION WRITE NULL\n");
    return -1;
  }

  int addr = region->rg_start + offset;
  int page = PAGING_PGN(addr);
  frmnum = PAGING_FPN(page);
  tlb_cache_write(proc->tlb, proc->pid, page, frmnum);

#ifdef IODUMP
  if (frmnum >= 0)
  {
    printf("TLB hit at write region=%d offset=%d value=%d\n",
           destination, offset, data);
  }
  else
  {
    printf("TLB miss at write region=%d offset=%d value=%d\n",
           destination, offset, data);
  }
#ifdef PAGETBL_DUMP
  TLBMEMPHY_dump(proc->tlb);
#endif
  MEMPHY_dump(proc->mram);
#endif

  if (frmnum >= 0)
  {

    int phyaddr = (frmnum << PAGING_ADDR_FPN_LOBIT) + offset;
    MEMPHY_write(proc->mram, phyaddr, data);
  }
  else
  {
    val = __write(proc, 0, destination, offset, data);
    if (val == 0)
    {
      // printf("pid: %d page: %d value: %d 333333333333333333333333\n", proc->pid, page, data);
      tlb_cache_write(proc->tlb, proc->pid, page, data);
    }
    TLBMEMPHY_dump(proc->tlb);
  }
  /* TODO update TLB CACHED with frame num of recent accessing page(s)*/
  /* by using tlb_cache_read()/tlb_cache_write()*/
  return val;
}

#endif
