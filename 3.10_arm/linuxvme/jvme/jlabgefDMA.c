/*----------------------------------------------------------------------------*
 *  Copyright (c) 2009,2010   Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Author:  Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     JLab DMA routines to compliment the GEFanuc API
 *
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <pthread.h>
#include "jvme.h"
#include "jlabgef.h"
#include "jlabgefDMA.h"
#include "jlabTsi148.h"
#include "gef/gefcmn_vme_tempe.h"
#include "tsi148.h"
#include "dmaPList.h"

#define DMA_MAX_LL 21

/* Timers to measure DMA */
unsigned long long dma_timer[10];


/*! GE-VME Driver handle (jlabgef.c) */
extern GEF_VME_BUS_HDL vmeHdl;

/* Some DMA Global Variables */
/*! GEF HDL for DMA Linked List buffer */
static GEF_VME_DMA_HDL dmaLL_hdl;

extern pthread_mutex_t tsi_mutex;

extern GEF_VME_ADDR addr_A32slave;
extern void *a32slave_window;

/*! Buffer node pointer */
extern DMANODE *the_event;
/* /\*! Data pointer *\/ */
/* extern unsigned int *dma_dabufp; */

volatile unsigned int *dmaListMap = NULL;
unsigned long dmaListAdr = 0;

/*!
  Routine to allocate memory for a linked list buffer.

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
jlabgefDmaAllocLLBuffer()
{
  GEF_STATUS status;
  void *mapPtr;
  int listsize = DMA_MAX_LL*sizeof(tsi148DmaDescriptor_t); /* Largest of the two descriptors */

  /* Allocate some system memory - mapped to userspace*/
  status = gefVmeAllocDmaBuf (vmeHdl, listsize, &dmaLL_hdl, (GEF_MAP_PTR *) &mapPtr);
  if (status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeAllocDmaBuf", status);
      return ERROR;
    }

  dmaListMap = (unsigned int *)mapPtr;

  dmaListAdr = dmaHdl_to_PhysAddr(dmaLL_hdl);

  return OK;
}

/*!
  Routine to free the memory allocated with vmeDmaAllocLLBuffer()

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
jlabgefDmaFreeLLBuffer()
{
  GEF_STATUS status;
  int retval = OK;

  status = gefVmeFreeDmaBuf(dmaLL_hdl);
  if (status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeFreeDmaBuf", status);
      retval = ERROR;
    }

  return retval;
}

/*!
  Routine to convert the current data buffer pointer position in userspace
  to Physical Memory space

  @param locAdrs
  Pointer to current position in data buffer in userspace.

  @return Physical Memory address of the current position in the data buffer.
*/
unsigned long
jlabgefDmaLocalToPhysAdrs(unsigned long locAdrs)
{
  unsigned long offset=0;
  unsigned long rval=0;

  if(!the_event)
    {
      printf("%s: ERROR:  the_event pointer is invalid!\n",__func__);
      return ERROR;
    }

  if(!the_event->physMemBase)
    {
      printf("%s: ERROR: DMA Physical Memory has an invalid base address (0x%lx)",
	     __func__,
	     the_event->physMemBase);
      return ERROR;
    }


  offset = locAdrs - the_event->partBaseAdr;
  rval = the_event->physMemBase + offset;

  return rval;
}

/*!
  Routine to convert the current data buffer pointer position in userspace
  to a VME Address.

  The VME Slave window must be initialized, prior to a call to this routine.

  @param locAdrs
  Pointer to current position in data buffer in userspace.

  @return VME Slave address of the current position in the data buffer.
*/
unsigned int
jlabgefDmaLocalToVmeAdrs(unsigned long locAdrs)
{
  unsigned long offset=0;
  int rval;
  unsigned int vme_base=addr_A32slave.lower;

  if(a32slave_window==NULL)
    {
      printf("%s: ERROR: Slave Window has not been initialized.\n",
	     __func__);
      return ERROR;
    }
  if(!the_event)
    {
      printf("%s: ERROR:  the_event pointer is invalid!\n",__func__);
      return ERROR;
    }

  if(!the_event->physMemBase)
    {
      printf("%s: ERROR: DMA Physical Memory has an invalid base address (0x%lx)",
	     __func__,
	     the_event->physMemBase);
      return ERROR;
    }


  offset = locAdrs - the_event->partBaseAdr;
  rval = vme_base + offset;

  return rval;
}

GEF_VME_DMA_HDL dmaTrash_hdl;
GEF_MAP_PTR     dmaTrashp = NULL;
unsigned long   dmaTrashPhysBase = -1;
int             dmaTrashSize = -1;

/*!
  Routine to allocate DMA Trash Can
  @param size Size (in bytes) of trashcan
     Must be less than 4Mb.  If 0, will set to maximum.
*/

int
jlabgefAllocDMATrash(int size)
{
  GEF_STATUS status;

  status = gefVmeAllocDmaBuf (vmeHdl,sizeof(DMA_MEM_PART),
			      &dmaTrash_hdl,&dmaTrashp);

  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeAllocDmaBuf", status);
      return ERROR;
    }

  dmaTrashPhysBase = dmaHdl_to_PhysAddr(dmaTrash_hdl);
  dmaTrashSize     = size;

  return OK;
}

/*!
  Routine to Free memory allocated for the DMA Trashcan
*/

int
jlabgefFreeDMATrash()
{
  GEF_STATUS status;

  status = gefVmeFreeDmaBuf(dmaTrash_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeFreeDmaBuf", status);
      return ERROR;
    }

  dmaTrashp        = NULL;
  dmaTrashPhysBase = -1;
  dmaTrashSize     = -1;

  return OK;
}

/*!
  Routine to DMA rest of transaction to trash can
  @param vmeAddr VME address to continue DMA
  @returns number of bytes transferred.
*/

int
jlabgefDmaFlush(unsigned int vmeaddr)
{
  int stat = OK;
  int rval = 0;

  stat = vmeDmaSendPhys(dmaTrashPhysBase, vmeaddr, dmaTrashSize);
  if(stat == ERROR)
    {
      return ERROR;
    }

  rval = vmeDmaDone();

  return rval;
}
