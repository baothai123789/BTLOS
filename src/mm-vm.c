// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;
  // while (rg_node != NULL)
  // {
  //   // printf("00000000000000000000000000000000000000000000000000000000000000000000000%d %d %d %d\n", rg_node->rg_start, rg_node->rg_end, rg_elmt->rg_start, rg_elmt->rg_end);
  //   if (rg_node->rg_end == rg_elmt->rg_start)
  //   {
  //     rg_node->rg_end = rg_elmt->rg_end;
  //     // printf("ggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg");
  //     return 0;
  //   }
  //   else if (rg_node->rg_start == rg_elmt->rg_end)
  //   {
  //     // printf("hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh");
  //     rg_node->rg_start = rg_elmt->rg_start;
  //     return 0;
  //   }
  //   rg_node = rg_node->rg_next;
  // }
  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

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
  pthread_mutex_unlock(&mmvm_lock);
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  int align_size = PAGING_PAGE_ALIGNSZ(size);
  if (get_free_vmrg_area(caller, vmaid, align_size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
    int inc_sz = rgnode.rg_end - rgnode.rg_start;
    int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    int incnumpage = inc_amt / PAGING_PAGESZ;

    // printf("vvvvvvvvvvvvvvvvvvvvvvvvvvvvv%d %d %d\n", rgnode.rg_start, rgnode.rg_end, incnumpage);
    if (vm_map_ram(caller, rgnode.rg_start, rgnode.rg_end, rgnode.rg_start, incnumpage, newrg) < 0)
    {
      pthread_mutex_unlock(&mmvm_lock);
      // print_pgtbl(caller,0,-1);
      return -1;
    }

    *alloc_addr = rgnode.rg_start;
    print_pgtbl(caller, 0, -1);
    pthread_mutex_unlock(&mmvm_lock);
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/

  /*Attempt to increate limit to get space */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  int inc_sz = PAGING_PAGE_ALIGNSZ(size);
  // int inc_limit_ret
  int old_sbrk;

  old_sbrk = cur_vma->sbrk;

  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */
  inc_vma_limit(caller, vmaid, inc_sz);
  /*Successful increase limit */
  caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
  caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

  *alloc_addr = old_sbrk;

  struct vm_area_struct *remain_rg = get_vma_by_num(caller->mm, vmaid);
  if (old_sbrk + size < remain_rg->sbrk)
  {
    struct vm_rg_struct *rg_free = malloc(sizeof(struct vm_rg_struct));
    rg_free->rg_start = old_sbrk + size;
    rg_free->rg_end = remain_rg->sbrk;
    enlist_vm_freerg_list(caller->mm, rg_free);
  }
  print_pgtbl(caller, 0, -1);
  pthread_mutex_unlock(&mmvm_lock);

  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int clear_pgn_node(struct pcb_t *proc, int pgn)
{
  struct pgn_t *newlist = NULL;
  struct pgn_t *temp = proc->mm->fifo_pgn;
  if (temp == NULL)
    return -1;
  while (temp != NULL)
  {
    if (temp->pgn == pgn)
    {
      if (newlist == NULL)
      {
        proc->mm->fifo_pgn = temp->pg_next;
      }
      else
      {
        newlist->pg_next = temp->pg_next;
      }
    }
    newlist = temp;
    temp = temp->pg_next;
  }
  return 0;
}
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  pthread_mutex_lock(&mmvm_lock);

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }

  /* TODO: Manage the collect freed region to freerg_list */
  struct vm_rg_struct *rgnode = get_symrg_byid(caller->mm, rgid);

  if (rgnode->rg_start == 0 && rgnode->rg_end == 0) // end == start
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  int inc_sz = rgnode->rg_end - rgnode->rg_start;
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;
  int pgn = PAGING_PGN(rgnode->rg_start);
  for (int i = 0; i < incnumpage; i++)
  {

#ifdef CPU_TLB
    tlb_clear_tlb_entry(caller->tlb, caller->pid, pgn + i);
#endif
    MEMPHY_put_freefp(caller->mram, PAGING_FPN(caller->mm->pgd[pgn + i]));
    CLRBIT(caller->mm->pgd[pgn + i], PAGING_PTE_PRESENT_MASK);
    clear_pgn_node(caller, pgn + i);
  }
  struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
  freerg_node->rg_start = rgnode->rg_start;
  freerg_node->rg_end = rgnode->rg_end;
  freerg_node->rg_next = NULL;

  rgnode->rg_start = 0;
  rgnode->rg_end = 0;
  rgnode->rg_next = NULL;

  /*enlist the obsoleted memory region */
  enlist_vm_freerg_list(caller->mm, freerg_node);
  print_pgtbl(caller, 0, -1);
  pthread_mutex_unlock(&mmvm_lock);
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

  if (!PAGING_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    int vicpgn, swpfpn;
    int vicfpn;
    uint32_t vicpte;
    int tgtfpn = PAGING_SWP(pte); // the target frame storing our variable

    /* TODO: Play with your paging theory here */
    /* Find victim page */
    if (find_victim_page(caller->mm, &vicpgn) == -1)
    {
      return -1;
    }
    /* Get free frame in MEMSWP */
    if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) == -1)
    {
      return -1;
    }
    vicpte = mm->pgd[vicpgn];
    vicfpn = PAGING_FPN(vicpte);

    /* Do swap frame from MEMRAM to MEMSWP and vice versa*/
    /* Copy victim frame to swap */
    __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
    /* Copy target frame from swap to mem */
    __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);

    // print_pgtbl(caller, 0, -1);
    // printf("setaaaaaaaaaaaaaaaa%d: %d: %08x\n", vicpgn, swpfpn, mm->pgd[vicpgn]);

    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

    // print_pgtbl(caller, 0, -1);

    /* Update its online status of the target page */
    pte_set_fpn(&mm->pgd[pgn], vicfpn);

    enlist_pgn_node(&caller->mm->fifo_pgn, pgn);
  }

  *fpn = PAGING_FPN(pte);
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
  /* Get the page to MEMRAM, swap from MEMSWAP    if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */
  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
  MEMPHY_write(caller->mram, phyaddr, value);

  return 0;
}

int check_if_in_freerg_list(struct pcb_t *caller, int vmaid, struct vm_rg_struct *currg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
  if (rgit == NULL)
    return 1;
  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    // printf("[Debug] %ld %ld %ld %ld\n", rgit->rg_start, rgit->rg_end, currg->rg_start, currg->rg_end);
    if (rgit->rg_start == currg->rg_start && rgit->rg_end == currg->rg_end)
    {
      return -1;
    }
    rgit = rgit->rg_next; // Traverse next rg
  }
  return 1;
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
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (check_if_in_freerg_list(caller, vmaid, currg) == -1)
  {
    printf("Read in free area.\n");
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  pg_getval(caller->mm, currg->rg_start + offset, data, caller);
  pthread_mutex_unlock(&mmvm_lock);
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
  destination = (uint32_t)data;
  if (val == -1)
  {
    // print_pgtbl(proc, 0, -1);
    return -1;
  }
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  // print_pgtbl(proc, 0, -1); // print max TBL
#endif
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
  pthread_mutex_lock(&mmvm_lock);
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
  {
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  if (check_if_in_freerg_list(caller, vmaid, currg) == -1)
  {
    printf("Write in free area.\n");
    pthread_mutex_unlock(&mmvm_lock);
    return -1;
  }
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  pthread_mutex_unlock(&mmvm_lock);
  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
  int num = __write(proc, 0, destination, offset, data);
#ifdef IODUMP
  if (num != -1)
  {
    printf("write region=%d offset=%d value=%d\n", destination, offset, data);
  }
#ifdef PAGETBL_DUMP
  // print_pgtbl(proc, 0, -1); // print max TBL
#endif

  MEMPHY_dump(proc->mram);
#endif
  return num;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
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
  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TODO validate the planned memory area is not overlapped */

  struct vm_area_struct *cur_area = get_vma_by_num(caller->mm, vmaid);
  if (cur_area == NULL)
  {
    return -1;
  }

  while (vma != NULL)
  {
    if (vma != cur_area && OVERLAP(cur_area->vm_start, cur_area->vm_end, vma->vm_start, vma->vm_end))
    {
      return -1;
    }
    vma = vma->vm_next;
  }
  /* Because only using vmaid = 0, you can return 0 immediately in this function =))*/
  return 0;
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
  {
    return -1; /*Overlap and failed allocation */
  }
  /* The obtained vm area (only)
   * now will be alloc real ram region */
  cur_vma->vm_end += inc_sz;
  cur_vma->sbrk += inc_sz;

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
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (pg == NULL)
  {
    return -1;
  }
  struct pgn_t *pre = NULL;
  while (pg->pg_next)
  {
    pre = pg;
    pg = pg->pg_next;
  }
  *retpgn = pg->pgn;
  pre->pg_next = NULL;

  free(pg);

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
  if (rgit == NULL)
    return -1;
  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;
  int i = 0;
  // int rg_end = PAGING_PAGE_ALIGNSZ(rgit->rg_end);
  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    // printf("mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm%d %d\n", rgit->rg_start, rgit->rg_end);
    if ((rgit->rg_start + size <= rgit->rg_end))
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;
      // printf("bbbbbbbbbbbbbbbbbbbbbbbbbbbb%d %d\n", newrg->rg_start, newrg->rg_end);
      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        // printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx%d %d\n", newrg->rg_start, newrg->rg_end);
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        {                                /*End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
    }
    else
    {
      i++;
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
    return -1;

  return 0;
}

// #endif
