/*----------------------------------------------------------------------------*
 *  Copyright (c) 2009        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: David Abbott                                                   *
 *             abbottd@jlab.org                  Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-7190             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *             Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Library for a memory allocation system
 *
 *----------------------------------------------------------------------------*/
#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <search.h>
#include "dmaPList.h"
#include "jlabgef.h"
#include "gef/gefcmn_vme_framework.h"


/* #define DEBUG_TSEARCH */

/*! GE-VME Driver handle (jlabgef.c) */
extern GEF_VME_BUS_HDL vmeHdl;

#define DOMUTEX
/* Maximum size that can be allocated for one DMA partition in Linux = 4 MB */
#define LINUX_MAX_PARTSIZE 0x400000

pthread_mutex_t   partMutex = PTHREAD_MUTEX_INITIALIZER;
#ifdef DOMUTEX
#define PARTLOCK     if(pthread_mutex_lock(&partMutex)<0) perror("pthread_mutex_lock");
#define PARTUNLOCK   if(pthread_mutex_unlock(&partMutex)<0) perror("pthread_mutex_unlock");
#else
#define PARTLOCK
#define PARTUNLOCK
#endif


#define maximum(a,b) (a<b ? b : a)

/* global data */
static DMALIST  dmaPList;     /* global part list */
static int useSlaveWindow=0;  /* decision to use (1) a Slave VME window (useful for SFI) */
extern void *a32slave_window; /* global variable from jlabgef.c */
extern int a32slave_physmembase;
/** Flag for turning off or on driver verbosity
    \see vmeSetQuietFlag()
*/
extern unsigned int vmeQuietFlag;

/*! Buffer node pointer */
DMANODE *the_event;
/*! Data pointer */
unsigned int *dma_dabufp;

/*!
  Structure to hold information on Physical Memory allocated regions
*/
typedef struct
{
  unsigned long base;
  unsigned long width;
} DMA_PHYSMEM_INFO;

/*! Pointer to array of info for allocated regions */
static DMA_PHYSMEM_INFO *partPhysMem = NULL;
/*! Size of @partPhysMem */
static int npartPhysMem = 0;

/*!
  Pointer to hold the Allocated Physical Memory region tree
 */
#ifdef USE_TSEARCH
static void *dmaPhysMemRoot = NULL;

static int dmaMemCompare(const void *pa, const void *pb);
static int dmaMemAdd(unsigned long base, unsigned long width);
#endif

/*!
 Routine to obtain the physical address from a GE-VME DMA handle

 @param inpDmaHdl GE-VME DMA handle

 @return The 32bit physical address
*/
unsigned long
dmaHdl_to_PhysAddr(GEF_VME_DMA_HDL inpDmaHdl)
{
  return (unsigned long)inpDmaHdl->phys_addr;
}

/*!
  Routine to allow for using a Slave VME window (opened with vmeOpenSlaveA32)
  for the DMA buffer.
  This routine MUST be called before initializing DMA buffers, if the Slave
  VME Window physical memory is to be used.

  @param iFlag
  - 0 to use regular physical memory
  - 1 to use physical memory already mapped to a VME Slave Window

  @return OK if successful, ERROR on error
*/
int
dmaPUseSlaveWindow (int iFlag)
{
  /* Check argument */
  if((iFlag < 0) || (iFlag > 1))
    {
      printf("%s: ERROR: Invalid iFlag (%d).  Must be 0 or 1.\n",
	     __func__,iFlag);
      return ERROR;
    }

  if(iFlag == 0) /* Don't use Slave Window.. */
    {
      useSlaveWindow=0;
      return OK;
    }

  if(iFlag == 1) /* Use Slave Window */
    {
      /* Check if window was already opened and initialized */
      if(a32slave_window==NULL)
	{
	  useSlaveWindow=0;
	  printf("%s: ERROR: Slave Window has not been initialized.\n",
		 __func__);
	  return ERROR;
	}

      useSlaveWindow=1;
      return OK;
    }

  return ERROR; /* Shouldn't get here, anyway */
}


void
dmaPartInit()
{
  dmalistInit(&dmaPList);
}

/*!
  Create and initialize a memory partition

  @param *name Name of the new partition
  @param size  Size of a single item
  @param c     Initial number if items
  @param incr  Number of items to add when enlarging

  @return Created memory partition
*/
DMA_MEM_ID
dmaPCreate(char *name, int size, int c, int incr)
{
  DMA_MEM_ID pPart;
  /*   int c_alloc = 0; */
  GEF_STATUS status;
  GEF_VME_DMA_HDL dma_hdl;
  GEF_MAP_PTR mapPtr;
  int size_incr = 0;

  if(vmeHdl==NULL)
    {
      printf("%s: ERROR: vmeHdl undefined.\n",
	     __func__);
      return 0;
    }

  /* Allocate memory to hold partition info */
  status = gefVmeAllocDmaBuf (vmeHdl,sizeof(DMA_MEM_PART),
			      &dma_hdl,&mapPtr);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeAllocDmaBuf", status);
      return 0;
    }

  pPart = (DMA_MEM_ID) mapPtr;
  memset ((char *)pPart,0,sizeof(DMA_MEM_PART));

  /* Initialize holder of physical memory regions */
  if(c > 0)
    {
      partPhysMem  = (DMA_PHYSMEM_INFO *)malloc(c*sizeof(DMA_PHYSMEM_INFO));
      npartPhysMem = 0;
    }

  if (pPart != NULL)
    {
      dmalistInit (&(pPart->list));

      /* Increase size by 2KB (0x800) to allow extra space for DMA Flush */
      size += 0x800;

      /* Check if the size needs to be increased to put the partition on
	 an 8 byte boundary */
      if((size + sizeof(DMANODE))%8 != 0)
	size_incr = 8-(size + sizeof(DMANODE))%8;

      pPart->size = size + sizeof(DMANODE) + size_incr;
      pPart->incr = 0;
      pPart->total = 0;

      pPart->dmaHdl = dma_hdl;

      strcpy(pPart->name, name);
      if (name && strlen(name) == 0)
	pPart->name[0] = 0;
      dmalistAdd (&dmaPList, (DMANODE *)pPart);
      if((dmaPIncr (pPart, c)) != c)
	return (0);
    }
  return pPart;
}

/*!
  Routine to find a memory partition based on its name

  @param *name Name of the partition to find

  @return Pointer to found memory partition, if successful.
*/
DMA_MEM_ID
dmaPFindByName(char *name)
{
  DMA_MEM_ID	pPart;

  pPart = (DMA_MEM_ID) dmalistFirst (&dmaPList);

  while (pPart != NULL)
    {
      if (pPart->name && strcmp(pPart->name, name) == 0)break;
      pPart = (DMA_MEM_ID) dmalistNext ((DMANODE *)pPart);
    }
  return (pPart);
}


/*!
  Frees all nodes for a given memory part and removes part from global
  part list.

  @param pPart Memory parition
*/
void
dmaPFree(DMA_MEM_ID pPart)
{
  DMANODE *the_node;
  GEF_STATUS status;

  if(pPart == NULL) return;

  if(!vmeQuietFlag)
    printf("%s: free list %s\n",
	   __func__, pPart->name);

  if (pPart->incr == 1)
    {
      /* Free all buffers in the partition individually */
      while (pPart->list.c)
	{
	  dmalistGet(&(pPart->list),the_node);
#ifdef DEBUG
	  printf("%s: DEBUG: the_node->dmaHdl : 0x%x\n",
		 __func__,
		 (unsigned int)the_node->dmaHdl);
#endif
	  status = gefVmeFreeDmaBuf(the_node->dmaHdl);
	  if(status != GEF_STATUS_SUCCESS)
	    {
	      jlabgefPrintGefError((char *)__func__, "gefVmeFreeDmaBuf", status);
	    }

	  the_node = (DMANODE *)0;
	}
      dmalistSnip (&dmaPList, (DMANODE *)pPart);
    }
  else
    {
      /* Just need to Free the one contiguous block of data */
      dmalistSnip (&dmaPList, (DMANODE *)pPart);
      dmalistGet(&(pPart->list),the_node);
      if(the_node)
	{
#ifdef DEBUG
	  printf("%s: DEBUG: the_node->dmaHdl : 0x%x\n",
		 __func__,
		 (unsigned int)the_node->dmaHdl );
#endif
	  if(!useSlaveWindow)
	    {
	      status = gefVmeFreeDmaBuf(the_node->dmaHdl );
	      if(status != GEF_STATUS_SUCCESS)
		{
		  jlabgefPrintGefError((char *)__func__, "gefVmeFreeDmaBuf", status);
		}
	    }
	}

    }
#ifdef DEBUG
  printf("%s: DEBUG: pPart->dmaHdl: 0x%x\n",
	 __func__,
	 (unsigned int)pPart->dmaHdl);
#endif
  status = gefVmeFreeDmaBuf(pPart->dmaHdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeFreeDmaBuf", status);
    }

  free(partPhysMem);
  partPhysMem = NULL;
  npartPhysMem = 0;

  pPart = 0;
}


/*!
  Frees all memory parts in global part list and frees all nodes for a
  given list.
*/
void
dmaPFreeAll()
{
  DMA_MEM_ID	pPart = (DMA_MEM_ID) 0;

  if (dmalistCount(&dmaPList))
    {
      pPart = (DMA_MEM_ID) dmalistFirst (&dmaPList);
      while (pPart != NULL)
	{
	  dmaPFree(pPart);
	  pPart = (DMA_MEM_ID) dmalistFirst (&dmaPList);
	}
    }
}



/*!
  Routine to increase a partition size.

  @param pPart Memory parition to increase
  @param c     Minimum number of items to add.

  @return Number of items added, if successful, -1, otherwise.
*/
int
dmaPIncr(DMA_MEM_ID pPart, int c)
{

  register char *node;
  register long *block;
  /*   unsigned bytes; */
  int total_bytes;
  int actual = c;		/* actual # of items added */
  GEF_STATUS status;
  GEF_MAP_PTR mapPtr;
  GEF_VME_DMA_HDL dma_hdl;
  long physMemBase=0;

  pPart->total += c;

  if ((pPart == NULL)||(c == 0)) return (0);

  total_bytes =  c * pPart->size;

  if(LINUX_MAX_PARTSIZE <= total_bytes)
    {
      if(useSlaveWindow)
	{
	  printf("%s: ERROR:  Unable to create memory partition for Slave Window.\n",
		 __func__);
	  printf("  Requested partition size (%d) is larger than max allowed (%d)\n",
		 total_bytes,LINUX_MAX_PARTSIZE);
	  return ERROR;
	}

      if(!vmeQuietFlag)
	printf("%s: Creating a fragmented memory partition.\n",__func__);

      if(LINUX_MAX_PARTSIZE < pPart->size)
	{
	  printf("%s: ERROR: Requested partition size (%d) is larger than max allowed (%d)\n",
		 __func__,pPart->size,LINUX_MAX_PARTSIZE);
	  return (-1);
	}
      else
	{
	  pPart->incr = 1; /* Create a Fragmented memory Partition */

	  while (actual--)
	    {
	      /* Allocate memory for individual buffer */
	      status = gefVmeAllocDmaBuf (vmeHdl,pPart->size,
					  &dma_hdl,&mapPtr);
	      if(status != GEF_STATUS_SUCCESS)
		{
		  jlabgefPrintGefError((char *)__func__, "gefVmeAllocDmaBuf", status);
		  printf("                bytes requested = %d\n",pPart->size);
		  return -1;
		}
	      block = (long *) mapPtr;
#ifdef DEBUG
	      printf("%s: DEBUG: block = 0x%x\t dma_hdl = 0x%x\n",
		     __func__,
		     block,dma_hdl);
#endif
	      if (block == NULL)
		{
		  return (-1);
		}

	      memset((char *) block, 0, pPart->size);

	      ((DMANODE *)block)->part = pPart; /* remember where we came from... */
	      ((DMANODE *)block)->dmaHdl = dma_hdl;
	      ((DMANODE *)block)->partBaseAdr = (unsigned long)block;
	      ((DMANODE *)block)->physMemBase = dmaHdl_to_PhysAddr(dma_hdl);

#ifdef USE_TSEARCH
	      /* Add Physical Memory region info to Tree */
	      dmaMemAdd( ((DMANODE *)block)->physMemBase +
			 (unsigned long)&(((DMANODE *)block)->data[0]) -
			 ((DMANODE *)block)->partBaseAdr,
			 pPart->size);
#endif /* USE_TSEARCH */
	      dmalistAdd (&pPart->list,(DMANODE *)block);
	    }
	  return (c);
	}
    }
  else /* Single memory block for data */
    {
      if(useSlaveWindow)
	{
	  dma_hdl=NULL;
	  block = (long *)a32slave_window;
	  physMemBase = a32slave_physmembase;
	}
      else
	{
	  /* Allocate memory for all buffers */
	  status = gefVmeAllocDmaBuf (vmeHdl,total_bytes,
				      &dma_hdl,&mapPtr);
	  if(status != GEF_STATUS_SUCCESS)
	    {
	      jlabgefPrintGefError((char *)__func__, "gefVmeAllocDmaBuf", status);
	      printf("          total_bytes requested = %d\n",total_bytes);
	      return -1;
	    }
	  block = (long *) mapPtr;
	  physMemBase = dmaHdl_to_PhysAddr(dma_hdl);
#ifdef DEBUG
	  printf("%s: DEBUG: block = 0x%x\t dma_hdl = 0x%x\n",
		 __func__,
		 block,dma_hdl);
#endif
	}

      if (block == NULL)
	{
	  printf("%s: ERROR: Memory Allocator returned NULL\n",
		 __func__);
	  return (-1);
	}

      pPart->incr = 0;
      memset((char *) block, 0, c * pPart->size);

      node = (char *) block;
      *((char **) &pPart->part[0]) = node;

      /* Split large allocation into the individual buffers */
      while (actual--)
	{
	  ((DMANODE *)node)->part = pPart; /* remember where we came from... */
	  ((DMANODE *)node)->dmaHdl = dma_hdl;
	  ((DMANODE *)node)->partBaseAdr = (pPart->part[0]);
	  ((DMANODE *)node)->physMemBase = physMemBase;

#ifdef USE_TSEARCH
	  /* Add Physical Memory region info to Tree */
	  dmaMemAdd( ((DMANODE *)node)->physMemBase +
		     (unsigned long)&(((DMANODE *)node)->data[0]) -
		     ((DMANODE *)node)->partBaseAdr,
		     pPart->size);
#endif /* USE_TSEARCH */


	  dmalistAdd (&pPart->list,(DMANODE *)node);
	  node += pPart->size;
	}

      return (c);
    }

}



/*****************************************************************
 *
 *  Wrapper routines for DMA Memory Partition list manipulation
 *
 *   dmaPFreeItem:  Free a buffer(node) back to its owner partition
 *   dmaPEmpty   :  Check if a Partition has available nodes
 *   dmaPGetItem :  Get (reserve) the first available  node from a partition
 *   dmaAddItem  :  Add node to a specified partition's list.
 *
 */

/*!
  Free a buffer(node) back to its owner partition

  @param *pItem Buffer(node) to free
*/
void
dmaPFreeItem(DMANODE *pItem)
{
  GEF_STATUS status;

  PARTLOCK;
  /* if the node does not have an owner then delete it - otherwise add it back to
     the owner list - lock out interrupts to be safe */
  if ((pItem)->part == 0)
    {
      if(useSlaveWindow)
	{
	  printf("%s: I dont think I should be here... useSlaveWindow==%d",
		 __func__,useSlaveWindow);
	}
      status = gefVmeFreeDmaBuf(pItem->dmaHdl);
      if(status != GEF_STATUS_SUCCESS)
	{
	  jlabgefPrintGefError((char *)__func__, "gefVmeFreeDmaBuf", status);
	}
      pItem = 0;
    }
  else
    {
      pItem->length=0;
      dmalistAdd (&pItem->part->list, pItem);
    }

  /* execute any command accociated with freeing the buffer */
  if(pItem->part->free_cmd != NULL)
    (*(pItem->part->free_cmd)) (pItem->part->clientData);

  PARTUNLOCK;
}

/*!
  Check if a Partition has available nodes

  @param pPart Partition to check

  @return 1 if empty, otherwise 0.
*/
int
dmaPEmpty(DMA_MEM_ID pPart)
{
  int rval;
  PARTLOCK;
  rval = (pPart->list.c == 0);
  PARTUNLOCK;
  return rval;
}

/*!
  Return the number of available nodes in specified Partition

  @param pPart Partition to check

  @return Number of nodes available.
*/
int
dmaPNodeCount(DMA_MEM_ID pPart)
{
  int rval;
  PARTLOCK;
  rval = pPart->list.c;
  PARTUNLOCK;
  return rval;
}


/*!
  Get (reserve) the first available node from a parititon

  @param pPart Partition to obtain a node

  @return First available node, if successful. 0, otherwise.
*/
DMANODE *
dmaPGetItem(DMA_MEM_ID pPart)
{
  DMANODE *theNode;

  PARTLOCK;
  dmalistGet(&(pPart->list),theNode);
  if(!theNode)
    {
      PARTUNLOCK;
      return 0;
    }

  if(theNode->length > theNode->part->size)
    {
      printf("%s: ERROR:", __func__);
      printf("  Event length (%d) is larger than the Event buffer size (%d).  (Event %d)\n",
	     (int)theNode->length,theNode->part->size,
	     (int)theNode->nevent);
    }
  PARTUNLOCK;
  return(theNode);
}

/*!
  Add node to a specified partition's list

  @param pPart  Partition to add to
  @param *pItem Item to add to partition's list
*/
void
dmaPAddItem(DMA_MEM_ID pPart, DMANODE *pItem)
{

  PARTLOCK;
  dmalistAdd(&(pPart->list),pItem);
  if(pItem->length > pItem->part->size)
    {
      printf("%s: ERROR:", __func__);
      printf("  Event length (%d) is larger than the Event buffer size (%d).  (Event %d)\n",
	     (int)pItem->length,pItem->part->size,
	     (int)pItem->nevent);
    }
  PARTUNLOCK;
}



/*!
  Initialize an existing partition

  @param pPart Parition to initialize

  @return 0, if successful.  -1, otherwise.
*/
int
dmaPReInit(DMA_MEM_ID pPart)
{
  register char *node;
  register DMANODE *theNode;
  int actual;
  long oldPhysMemBase=0;
  unsigned long oldPartBaseAdr=0;
  GEF_VME_DMA_HDL oldPartDMAHdl,oldNodeDMAHdl;

  if (pPart == NULL) return -1;

  if (pPart->incr == 1)
    {   /* Does this partition have a Fragmented buffer list */
      /* Check if partition has buffers that do not belong to it
	 and return them to their rightful owners */
      if ((pPart->total == 0) && (dmalistCount(&pPart->list) > 0))
	{
	  while (dmalistCount(&pPart->list) > 0)
	    {
	      dmalistGet(&pPart->list,theNode);
	      dmaPFreeItem(theNode);
	    }
	}

    }
  else
    {
      /* Cheat to initialize memory partition assuming buffers in one
	 contiguous memory bloack */

      /* Get the dma handles (if they exist) before they're erased */
      dmalistGet(&pPart->list,theNode);
      if(theNode)
	{
	  oldNodeDMAHdl = theNode->dmaHdl;
	  oldPhysMemBase = theNode->physMemBase;
	  oldPartBaseAdr = theNode->partBaseAdr;
	}
      else
	{
	  oldNodeDMAHdl = 0;
	}
      oldPartDMAHdl = pPart->dmaHdl;
      memset(*((char **) &pPart->part[0]), 0, pPart->total * pPart->size);

      node = *((char **) &pPart->part[0]);

      actual = pPart->total;

      pPart->dmaHdl = oldPartDMAHdl;
      pPart->list.f = pPart->list.l = (DMANODE *) (pPart->list.c = 0);

      while (actual--)
	{
	  ((DMANODE *)node)->part = pPart; /* remember where we came from... */
	  ((DMANODE *)node)->dmaHdl = oldNodeDMAHdl;
	  ((DMANODE *)node)->physMemBase = oldPhysMemBase;
	  ((DMANODE *)node)->partBaseAdr = oldPartBaseAdr;
	  dmalistAdd (&pPart->list,(DMANODE *)node);
	  node += pPart->size;
	}
    }
  return 0;
}

/*!
  Initialize all existing memory partitions

  @return 0
*/
int
dmaPReInitAll()
{
  DMA_MEM_ID	pPart = (DMA_MEM_ID) 0;

  if (dmalistCount(&dmaPList))
    {
      pPart = (DMA_MEM_ID) dmalistFirst (&dmaPList);
      while (pPart != NULL)
	{
	  dmaPReInit(pPart);
	  pPart = (DMA_MEM_ID) dmalistNext ((DMANODE *) pPart);
	}
    }
  return 0;
}

/***************************************************************
 * dmaPHdr - Print headings for part statitistics printout
 */

static void
dmaPHdr()
{
  printf("Address    ");
#ifdef ARCH_x86_64
  printf("    ");
#endif
  printf("total   free   busy     size  incr  (KBytes)  Name\n");

  printf("----------");
#ifdef ARCH_x86_64
  printf("----");
#endif
  printf(" -----  -----  -----  -------  ----  --------  ----------\n");
}


/***************************************************************
 * dmaPPrint - Print statitistics for a single part
 */

static void
dmaPPrint(DMA_MEM_ID pPart)
{
  int freen;

#ifdef ARCH_x86_64
  printf("0x%012lx ",(unsigned long)pPart);
#else
  printf("0x%08lx ",(unsigned long)pPart);
#endif

  if (pPart != NULL)
    {
      freen = dmalistCount (&pPart->list);
      printf("%5d  %5d  %5d  %7d     %1d  (%6d)  %s\n",
	     pPart->total,
	     freen,
	     pPart->total - freen,
	     pPart->size,
	     pPart->incr,
	     (((pPart->total * pPart->size) + 1023) / 1024),
	     pPart->name
	     );
    }
}


/*!
  Print statistics on a memory part

  @param pPart Memory paritition

  @return 0
*/
int
dmaPStats(DMA_MEM_ID pPart)
{
  dmaPHdr ();
  dmaPPrint (pPart);
  return (0);
}


/*!
  Print statistics on all partitions
*/
int
dmaPStatsAll()
{
  DMA_MEM_ID  pPart;

  dmaPHdr ();
  pPart = (DMA_MEM_ID) dmalistFirst (&dmaPList);
  while (pPart != NULL)
    {
      dmaPPrint (pPart);
      pPart = (DMA_MEM_ID) dmalistNext ((DMANODE *)pPart);
    }
  return (0);
}


/*!
  Prints statistics for a given list structure

  @param *admalist List of nodes

  @return 0
*/
int
dmaPPrintList(DMALIST *admalist)
{
  DMANODE *theNode;

  printf("dalist->f         %lx\n",(unsigned long)admalist->f);
  printf("dalist->l         %lx\n",(unsigned long)admalist->l);
  printf("dalist->c         %ld\n",(unsigned long)admalist->c);

  theNode = dmalistFirst(admalist);
  while (theNode)
    {
      printf ("part %lx prev %lx self %lx next %lx left %d fd %d\n",
	      (unsigned long)theNode->part,
	      (unsigned long)theNode->p,
	      (unsigned long)theNode,
	      (unsigned long)theNode->n,
	      (int)theNode->left,
	      theNode->fd);
      theNode = dmalistNext(theNode);
    }
  return(0);
}

#ifdef USE_TSEARCH
static int
dmaMemCompare(const void *pa, const void *pb)
{
  unsigned long mema, memb;
  unsigned long widtha, widthb;

  mema = ((DMA_PHYSMEM_INFO *) pa)->base;
  memb = ((DMA_PHYSMEM_INFO *) pb)->base;

  widtha = ((DMA_PHYSMEM_INFO *) pa)->width;
  widthb = ((DMA_PHYSMEM_INFO *) pb)->width;

#ifdef DEBUG_TSEARCH
  printf("%s: DEBUG: \n",
	 __func__);
  printf("  mema = 0x%lx  widtha = 0x%lx\n",
	 mema, widtha);
  printf("  memb = 0x%lx  widthb = 0x%lx  ",
	 memb, widthb);
#endif

  if ((mema + widtha) < memb)
    {
#ifdef DEBUG_TSEARCH
      printf("   -1\n");
#endif
      return -1;
    }
  if (mema >= (memb + widthb))
    {
#ifdef DEBUG_TSEARCH
      printf("    1\n");
#endif
      return 1;
    }

#ifdef DEBUG_TSEARCH
  printf("    0\n");
#endif
  return 0;
}

static int
dmaMemAdd(unsigned long base, unsigned long width)
{
  int rval = 0;
  DMA_PHYSMEM_INFO *val;

  partPhysMem[npartPhysMem].base  = base;
  partPhysMem[npartPhysMem].width  = width;

  val = (DMA_PHYSMEM_INFO *)tsearch((void *) &partPhysMem[npartPhysMem],
				    &dmaPhysMemRoot, dmaMemCompare);
  if (val == NULL)
    {
      printf("%s: ERROR: Out of space for Tree.  Number of current nodes: %d\n",
	     __func__,
	     npartPhysMem);
      rval = ERROR;
    }
  else
    {
      /* Check if returned pointer DMA_PHYSMEM_INFO is the
	 same as what was tsearch'd */
      if((*(DMA_PHYSMEM_INFO **) val)->base ==
	 partPhysMem[npartPhysMem].base)
	{
#ifdef DEBUG_TSEARCH
	  printf("%s: DEBUG: Added to tree[%d]: 0x%0lx 0x%0lx\n",
		 __func__, npartPhysMem,
		 partPhysMem[npartPhysMem].base,
		 partPhysMem[npartPhysMem].width);
#endif
	  npartPhysMem++;
	  rval = OK;
	}
      else
	{
	  printf("%s: WARN: Item to add (0x%0lx, 0x%0lx)"
		 " already in tree item (0x%0lx,  0x%0lx)\n",
		 __func__,
		 partPhysMem[npartPhysMem].base,
		 partPhysMem[npartPhysMem].width,
		 (*(DMA_PHYSMEM_INFO **) val)->base,
		 (*(DMA_PHYSMEM_INFO **) val)->width);
	  rval = ERROR;

	}

    }

  return rval;
}
#endif /* USE_TSEARCH */

/*!
  Test specified address for inclusion in allocated physical memory regions.

  @param physMem Physical Memory Address to test.

  @return 1 if address is valid, -1 if test not initialized, otherwise 0
*/
int
dmaPMemIsValid(unsigned long physMem)
{
#ifdef USE_TSEARCH
  int rval = 0;
  DMA_PHYSMEM_INFO testMem;
  void *val;
  static int fWarned = 0;

  testMem.base = physMem;
  testMem.width = 0;

#ifdef DEBUG_TSEARCH
  printf("%s: DEBUG: testMem.base = 0x%lx\ttestMem.width = 0x%lx\n",
	 __func__, testMem.base, testMem.width);
#endif

  if(dmaPhysMemRoot == NULL)
    {
      if(fWarned == 0)
	{
	  printf("%s: WARN: Physical Memory address check is not initialized.\n",
		 __func__);
	  fWarned = 1;
	}
      return -1;
    }

  val = (DMA_PHYSMEM_INFO *)tfind((void *) &testMem,
				  &dmaPhysMemRoot, dmaMemCompare);
  if(val == NULL)
    {
#ifdef DEBUG_TSEARCH
      printf(" 0x%lx  Not in range\n",
	     testMem.base);
#endif
      rval = 0;
    }
  else
    {
#ifdef DEBUG_TSEARCH
      unsigned long mem_base = (*(DMA_PHYSMEM_INFO **) val)->base;
      printf(" 0x%lx  in range of   0x%lx\n",
	     testMem.base, mem_base);
#endif
      rval = 1;
    }

  return rval;
#else /* USE_TSEARCH */
  return 1;
#endif /* USE_TSEARCH */
}
