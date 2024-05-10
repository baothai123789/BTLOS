/*
 * Copyright (C) 2024 pdnguyen of the HCMC University of Technology
 */
/*
 * Source Code License Grant: Authors hereby grants to Licensee 
 * a personal to use and modify the Licensed Source Code for 
 * the sole purpose of studying during attending the course CO2018.
 */
//#ifdef MM_TLB
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
#define GET_TAG(tlb_page) GETVAL(tlb_page, GENMASK(8, 0), 0)
#define GET_PID(tlb_page) GETVAL(tlb_page, GENMASK(29, 9), 9)
#define GET_VALID(tlb_page) GETVAL(tlb_page, BIT(30), 30)
#define init_tlbcache(mp, sz, ...) init_memphy(mp, sz, (1, ##__VA_ARGS__))
#define SET_PID(tlb_page, pid) SETVAL(tlb_page, pid, GENMASK(29, 9), 9)
#define SET_TAG(tlb_page, tag) SETVAL(tlb_page, tag, GENMASK(8, 0), 0)
#define init_tlbcache(mp,sz,...) init_memphy(mp, sz, (1, ##__VA_ARGS__))
/*
bit 31: TAG USED
bit 30: VALID
bit 29-9: PID
bit 8-0: TAG

*/
#define MAX_TLB 8
static uint32_t *tlb[MAX_TLB];
static pthread_mutex_t tlb_lock;

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */

/*
 *  tlb_cache_read read TLB cache device
 *  @mp: memphy struct
 *  @pid: process id
 *  @pgnum: page number
 *  @value: obtained value
 */


int tlb_clear_tlb_entry(struct memphy_struct *mp, int pid, int pgnum)
{
   int index = pgnum % 8;
   if (GET_PID(tlb[index][0]) == pid)
   {
      CLRBIT(tlb[index][0], BIT(30));
      SET_PID(tlb[index][0], 0);
      tlb[index][1] = 0;
   }
   return 0;
}
int tlb_cache_read(struct memphy_struct * mp, int pid, int pgnum, BYTE *value)
{
   /* TODO: the identify info is mapped to 
    *      cache line by employing:
    *      direct mapped, associated mapping etc.
    */
   int index = pgnum%8;
   int tag = pgnum/8;
   uint32_t tlb_pg = tlb[index][0];
   if(tlb_pg&&BIT(30)){
      int tag_pg = GET_TAG(tlb_pg);
      int pid_pg = GET_PID(tlb_pg);
      if(tag==tag_pg&&pid==pid_pg){
         *value = (int)tlb[index][1];
         return 0 ;
      }
      return -1;
   }
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

   int index = pgnum % 8;
   int tag = pgnum / 8;
   tlb[index][1] = value;
   SET_TAG(tlb[index][0], tag);
   SET_PID(tlb[index][0], pid);
   SETBIT(tlb[index][0], BIT(30));

   return -1;
}

/*
 *  TLBMEMPHY_read natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int TLBMEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
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
int TLBMEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{
   if (mp == NULL)
     return -1;

   /* TLB cached is random access by native */
   mp->storage[addr] = data;

   return 0;
}

/*
 *  TLBMEMPHY_format natively supports MEMPHY device interfaces
 *  @mp: memphy struct
 */


int TLBMEMPHY_dump(struct memphy_struct * mp)
{
   /*TODO dump memphy contnt mp->storage 
    *     for tracing the memory content
    */

  pthread_mutex_lock(&tlb_lock);
   printf("-----------------------Dump TLB------------------------\n");
   for (int i = 0; i < MAX_TLB; i++)
   {
      int valid = GET_VALID(tlb[i][0]);
      int pid = GET_PID(tlb[i][0]);
      int frame = tlb[i][1];
      printf("%02d %d %08d %08d\n", i, valid, pid, frame);
   }
   printf("-----------------------Dump TLB------------------------\n");
   pthread_mutex_unlock(&tlb_lock);
   return 0;
}


/*
 *  Init TLBMEMPHY struct
 */
int init_tlbmemphy(struct memphy_struct *mp, int max_size)
{
   mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
   for (int i = 0; i < MAX_TLB; i++)
   {
      tlb[i] = (uint32_t *)(mp->storage + i * 8);
      tlb[i][0] = 0; 
      tlb[i][1] = 0;
   }
   mp->maxsz = max_size;

   mp->rdmflg = 1;
   pthread_mutex_init(&tlb_lock, NULL);
   return 0;
}

//#endif
