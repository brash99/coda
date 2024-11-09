/*****************************************************************
 *
 * linuxvme_list.c - "Primary" Readout list routines for
 *                   JLab legacy trigger system
 *
 * Usage:
 *
 *    #include "linuxvme_list.c"
 *
 *  then define the following routines:
 *
 *    void rocDownload();
 *    void rocPrestart();
 *    void rocGo();
 *    void rocEnd();
 *    void rocTrigger(int evnum, int ev_type, int sync_flag);
 *    void rocCleanup()
 *
 */


#define ROL_NAME__ "LINUXVME"

/* POLLING_MODE */
#define POLLING___
#define POLLING_MODE

/* INIT_NAME, INIT_NAME_POLL must be defined at compilation.
   Check readout list Makefile */
#ifndef INIT_NAME
#warning "INIT_NAME undefined. Setting to linuxvme_list__init"
#endif
#ifndef INIT_NAME_POLL
#warning "INIT_NAME_POLL undefined. Set to readout list filename base with gcc flag -DINIT_NAME_POLL"
#endif

#include <stdio.h>
#include <rol.h>
#include "jvme.h"
#include <LINUXVME_source.h>
#include "dmaBankTools.h"
#ifdef TIR_SOURCE
#include "tirLib.h"
#elif defined(TS_SOURCE)
#include "tsUtil.h"
#endif
extern int bigendian_out;

int errCount = 0;     /* Count the number of times no buffer available from vmeIN */

#ifdef TIR_SOURCE
extern unsigned int tirDoAck;
extern unsigned int tirNeedAck;
#elif defined(TS_SOURCE)
extern unsigned int tsDoAck;
extern unsigned int tsNeedAck;
#endif

/* At the moment, they point to the same mutex.. should probably consolidate */
#ifdef TIR_SOURCE
#define ISR_INTLOCK INTLOCK
#define ISR_INTUNLOCK INTUNLOCK
#elif defined(TS_SOURCE)
#define ISR_INTLOCK TSINTLOCK
#define ISR_INTUNLOCK TSINTUNLOCK
#endif

extern DMANODE *the_event; /* node pointer for event buffer obtained from GETEVENT, declared in dmaPList */
extern unsigned int *dma_dabufp; /* event buffer pointer obtained from GETEVENT, declared in dmaPList */

/* ROC Function prototypes defined by the user */
void rocDownload();
void rocPrestart();
void rocGo();
void rocEnd();
void rocTrigger(int evnum, int ev_type, int sync_flag);
void rocCleanup();

int  getOutQueueCount();

/* Asynchronous (to linuxvme rol) trigger routine, connects to rocTrigger */
void asyncTrigger();

/* Input and Output Partitions for VME Readout */
DMA_MEM_ID vmeIN, vmeOUT;


static void __download()
{
  int status;

  daLogMsg("INFO","Readout list compiled %s", DAYTIME);
#ifdef POLLING___
  rol->poll = 1;
#endif
  *(rol->async_roc) = 0; /* Normal ROC */

  bigendian_out=1;

  /* Initialize memory partition library */
  dmaPartInit();

  /* Setup Buffer memory to store events */
  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",MAX_EVENT_LENGTH,MAX_EVENT_POOL,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);

  if(vmeIN == 0)
    daLogMsg("ERROR", "Unable to allocate memory for event buffers");

  /* Reinitialize the Buffer memory */
  dmaPReInitAll();

  /* Initialize VME Interrupt interface - use defaults */
#ifdef TIR_SOURCE
  tirIntInit(TIR_ADDR,TIR_MODE,1);
#endif
#ifdef TS_SOURCE
  tsInit(TS_ADDR,0);
#endif

  /* Execute User defined download */
  rocDownload();

  daLogMsg("INFO","Download Executed");


} /*end download */

static void __prestart()
{
  CTRIGINIT;
  *(rol->nevents) = 0;
  unsigned long jj, adc_id;
  daLogMsg("INFO","Entering Prestart");

  LINUXVME_INIT;
  CTRIGRSS(LINUXVME,1,usrtrig,usrtrig_done);
  CRTTYPE(1,LINUXVME,1);

  /* Check the health of the vmeBus Mutex.. re-init if necessary */
  vmeCheckMutexHealth(10);

  /* Execute User defined prestart */
  rocPrestart();

  /* Initialize VME Interrupt variables */
#ifdef TIR_SOURCE
  tirClearIntCount();
#endif

  /* Connect User Trigger Routine */
#ifdef TIR_SOURCE
  tirIntConnect(TIR_INT_VEC,asyncTrigger,0);
#elif defined(TS_SOURCE)
  tsIntConnect(0,asyncTrigger,0,TS_MODE);
#endif

  daLogMsg("INFO","Prestart Executed");

  if (__the_event__) WRITE_EVENT_;
  *(rol->nevents) = 0;
  rol->recNb = 0;
} /*end prestart */

static void __end()
{
  int ii, ievt, rem_count;
  int len, type, lock_key;
  DMANODE *outEvent;
  int oldnumber;

  /* Disable Interrupts */
#ifdef TIR_SOURCE
  tirIntDisable();
  tirIntDisconnect();
  tirIntStatus(1);
#elif defined(TS_SOURCE)
  tsStop(1);
  tsIntDisable();
  tsIntDisconnect();
#endif

  /* Execute User defined end */
  rocEnd();

  CDODISABLE(LINUXVME,1,0);

  // FIXME: I dont think we need this for CODA 3
#ifdef CLEAR_QUEUE
  /* we need to make sure all events taken by the
     VME are collected from the vmeOUT queue */

  rem_count = getOutQueueCount();
  if (rem_count > 0)
    {
      printf("linuxvme_list End: %d events left on vmeOUT queue (will now de-queue them)\n",rem_count);
      /* This wont work without a secondary readout list (will crash EB or hang the ROC) */
      for(ii=0;ii<rem_count;ii++)
	{
	  __poll();
	}
    }
  else
    {
      printf("linuxvme_list End: vmeOUT queue Empty\n");
    }
#endif

  daLogMsg("INFO","End Executed");

  if (__the_event__) WRITE_EVENT_;
} /* end end block */

static void __pause()
{
  CDODISABLE(LINUXVME,1,0);
  daLogMsg("INFO","Pause Executed");

  if (__the_event__) WRITE_EVENT_;
} /*end pause */

static void __go()
{
  daLogMsg("INFO","Entering Go");

  errCount=0;

  CDOENABLE(LINUXVME,1,1);
  rocGo();

#ifdef TIR_SOURCE
  tirIntEnable(TIR_CLEAR_COUNT);
#elif defined(TS_SOURCE)
  tsIntEnable(1);
  tsGo(1);
#endif

  if (__the_event__) WRITE_EVENT_;
}

void usrtrig(unsigned long EVTYPE,unsigned long EVSOURCE)
{
  long EVENT_LENGTH;
  int ii, len, data, type, lock_key;
  DMANODE *outEvent;
  int syncFlag = 0;

  outEvent = dmaPGetItem(vmeOUT);
  if(outEvent != NULL)
    {
      len = outEvent->length;
      syncFlag = the_event->type;

      CEOPEN(ROCID, BT_BANK, blockLevel);

      if(rol->dabufp != NULL)
	{
	  for(ii=0;ii<len;ii++)
	    {
	      *rol->dabufp++ = outEvent->data[ii];
	    }
	}
      else
	{
	  printf("linuxvme_list: ERROR rol->dabufp is NULL -- Event lost\n");
	}

      /* Free up the trigger after freeing up the buffer - do it
	 this way to prevent triggers from coming in after freeing
	 the buffer and before the Ack */
#ifdef TIR_SOURCE
      if(tirNeedAck > 0)
	{
	  outEvent->part->free_cmd = *tirIntAck;
	}
#elif defined(TS_SOURCE)
      if(tsNeedAck > 0)
	{
	  outEvent->part->free_cmd = *tsIntAck;
	}
#endif
      else
	{
	  outEvent->part->free_cmd = NULL;
	}

      CECLOSE;
      dmaPFreeItem(outEvent); /* IntAck performed in here, if NeedAck */
    }
  else
    {
      logMsg("Error: no Event in vmeOUT queue\n",0,0,0,0,0,0);
    }




} /*end trigger */

void asyncTrigger()
{
  int intCount = 0, event_type = 0, sync_flag = 0, late_fail = 0;
  int length,size;
  unsigned int rval = 0;

  /* Read out event number, event type, syncFlag, and lateFail */
#ifdef TIR_SOURCE
  intCount = tirGetIntCount();

  rval = tirReadData();
  tirDecodeTrigData(rval, (unsigned int *)&event_type, (unsigned int *)&sync_flag, (unsigned int *)&late_fail);

#elif defined(TS_SOURCE)
  intCount = tsGetIntCount();

  rval = tsIntLRocData();
  tsDecodeTrigData(rval, &event_type, &sync_flag, &late_fail);
#endif

  /* grap a buffer from the queue */
  GETEVENT(vmeIN,intCount);
  if(the_event == NULL)
    {
      if(errCount == 0)
	daLogMsg("ERROR","asyncTrigger: No DMA Buffer Available. Events could be out of sync!");
      printf("asyncTrigger:ERROR: No buffer available!\n");
      errCount++;
      return;
    }
  if(the_event->length!=0)
    {
      printf("asyncTrigger: ERROR: Interrupt Count = %d the_event->length = %ld\t",intCount, the_event->length);
    }

  /* Store Sync Flag status for this event */
  the_event->type = sync_flag;

  /* Execute user defined Trigger Routine */
  rocTrigger(intCount, event_type, sync_flag);

  /* Put this event's buffer into the OUT queue. */
  PUTEVENT(vmeOUT);

  /* Check if the event length is larger than expected */
  length = (((unsigned long)(dma_dabufp) - (unsigned long)(&the_event->length))) - 4;
  size = the_event->part->size - sizeof(DMANODE);

  if(length>size)
    {
      printf("rocLib: ERROR: Event length > Buffer size (%d > %d).  Event %ld\n",
	     length, size, the_event->nevent);
      daLogMsg("WARN", "Event buffer overflow");
    }
  if(dmaPEmpty(vmeIN))
    {
      printf("WARN: vmeIN out of event buffers.\n");
#ifdef TIR_SOURCE
      tirDoAck = 0;
      tirNeedAck = 1;
#elif defined(TS_SOURCE)
      tsDoAck = 0;
      tsNeedAck = 1;
#endif
    }


}

void usrtrig_done()
{
} /*end done */

void __done()
{
  poolEmpty = 0; /* global Done, Buffers have been freed */
} /*end done */

static void __status()
{
} /* end status */

static void __reset()
{
  int iemp=0;

  /* Disable Interrupts */
#ifdef TIR_SOURCE
  tirIntDisable();
  tirIntDisconnect();
#elif defined(TS_SOURCE)
  tsStop(1);
  tsIntDisable();
  tsIntDisconnect();
#endif

  /* Empty the vmeOUT queue */
  while(!dmaPEmpty(vmeOUT))
    {
      iemp++;
      dmaPFreeItem(dmaPGetItem(vmeOUT));
      if(iemp>=MAX_EVENT_POOL) break;
    }

  printf(" **Reset Called** \n");

} /* end reset */

int
getOutQueueCount()
{
  if(vmeOUT)
    return(dmaPNodeCount(vmeOUT));
  else
    return(0);
}

void
InsertDummyTriggerBank(int trigType, int evNum, int evType, int evCnt)
{
  int ii;

  BANKOPEN(trigType, BT_SEG, evCnt);
  for (ii = 0; ii < evCnt; ii++)
    {
      if (trigType == 0xff11)
	{
	  *dma_dabufp++ = (evType << 24) | (0x01 << 16) | (3);
	}
      else
	{
	  *dma_dabufp++ = (evType << 24) | (0x01 << 16) | (1);
	}
      *dma_dabufp++ = (evCnt * (evNum - 1) + (ii + 1));
      if (trigType == 0xff11)
	{
	  *dma_dabufp++ = 0x12345678;
	  *dma_dabufp++ = 0;
	}
    }
  BANKCLOSE;

}

/* This routine is automatically executed just before the shared libary
   is unloaded.

   Clean up memory that was allocated
*/
__attribute__((destructor)) void end (void)
{
  printf("ROC Cleanup\n");
  dmaPFreeAll();
}
