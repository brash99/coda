/*****************************************************************************
 *
 * tsprimary_list.c - "Primary" Readout list routines for Trigger Supervisor
 *
 * Usage:
 *
 *    #include "tsprimary_list.c"
 *
 *  then define the following routines:
 *
 *    void rocDownload();
 *    void rocPrestart();
 *    void rocGo();
 *    void rocEnd();
 *    void rocTrigger(int arg);
 *    void rocCleanup()
 */

#define ROL_NAME__ "TSPRIMARY"

#define POLLING___
/* INIT_NAME should be defined with readlist filename base at compilation time */
#ifndef INIT_NAME
#error "INIT_NAME undefined. Set to readout list filename base with gcc flag -DINIT_NAME"
#endif
#ifndef INIT_NAME_POLL
#error "INIT_NAME_POLL undefined. Set to readout list filename base with gcc flag -DINIT_NAME_POLL"
#endif

#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <rol.h>
#include "jvme.h"
#include "tsLib.h"
#include "dmaBankTools.h"
#include <TSPRIMARY_source.h>
extern void daLogMsg(char *severity, char *fmt,...);

extern int bigendian_out;

extern int tsNeedAck; /* Request for acknowledge, declared in tsLib */
extern DMANODE *the_event; /* node pointer for event buffer obtained from GETEVENT, declared in dmaPList */
extern unsigned int *dma_dabufp; /* event buffer pointer obtained from GETEVENT, declared in dmaPList */

int emptyCount = 0;   /* Count the number of times event buffers are empty */
int errCount = 0;     /* Count the number of times no buffer available from vmeIN */

#define ISR_INTLOCK INTLOCK
#define ISR_INTUNLOCK INTUNLOCK

/* Readout Acknowledge condition variable and mutex */
extern int tsDoAck;
extern unsigned int tsIntCount;
extern int tsNeedAck;

pthread_mutex_t ack_mutex=PTHREAD_MUTEX_INITIALIZER;
#define ACKLOCK {				\
    if(pthread_mutex_lock(&ack_mutex)<0)	\
      perror("pthread_mutex_lock");		\
  }
#define ACKUNLOCK {				\
    if(pthread_mutex_unlock(&ack_mutex)<0)	\
      perror("pthread_mutex_unlock");		\
  }
pthread_cond_t ack_cv = PTHREAD_COND_INITIALIZER;
#define ACKWAIT {					\
    if(pthread_cond_wait(&ack_cv, &ack_mutex)<0)	\
      perror("pthread_cond_wait");			\
  }
#define ACKSIGNAL {					\
    if(pthread_cond_signal(&ack_cv)<0)			\
      perror("pthread_cond_signal");			\
  }
int ack_runend=0;

/* End of run condition variable */
pthread_cond_t endrun_cv = PTHREAD_COND_INITIALIZER;
struct timespec endrun_waittime;
int endrun_timedwait_ret=0;
#define ENDRUN_TIMEDWAIT(__x) {						\
    clock_gettime(CLOCK_REALTIME, &endrun_waittime);			\
    endrun_waittime.tv_sec += __x;					\
    endrun_timedwait_ret = pthread_cond_timedwait(&endrun_cv, &ack_mutex, &endrun_waittime); \
    if(endrun_timedwait_ret<0)						\
      perror("pthread_cond_timedwait");					\
  }
#define ENDRUN_SIGNAL {					\
    if(pthread_cond_signal(&endrun_cv)<0)		\
      perror("pthread_cond_signal");			\
  }

/* ROC Function prototypes defined by the user */
void rocDownload();
void rocPrestart();
void rocGo();
void rocEnd();
void rocTrigger(int arg);
void rocCleanup();

/* Routines to get in/out queue counts */
int  getOutQueueCount();
int  getInQueueCount();

/* Asynchronous (to tsprimary rol) trigger routine, connects to rocTrigger */
void asyncTrigger();

/* Input and Output Partitions for VME Readout */
DMA_MEM_ID vmeIN, vmeOUT;

/**
 *  DOWNLOAD
 */
static void __download()
{
  int status;

  daLogMsg("INFO","Readout list compiled %s", DAYTIME);
  rol->poll = 1;
  *(rol->async_roc) = 0; /* Normal ROC */

  bigendian_out=1;

  pthread_mutex_init(&ack_mutex, NULL);
  pthread_cond_init(&ack_cv,NULL);
  pthread_cond_init(&endrun_cv,NULL);

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
  dmaPStatsAll();

  /* Set crate ID */
  tsSetCrateID_preInit(ROCID);

#ifndef TS_ADDR
#define TS_ADDR 0
#endif
#ifndef TS_READOUT
#warning "TS_READOUT undefined.  Using TS_REAODUT_EXT_POLL"
#define TS_READOUT TS_READOUT_EXT_POLL
#endif
#ifndef TS_FLAG
#define TS_FLAG 0
#endif

  status = tsInit(TS_ADDR,TS_READOUT,TS_FLAG);
  if(status == -1) daLogMsg("ERROR","Unable to initialize TS Board\n");

  /* Set timestamp format 48 bits */
  tsSetEventFormat(3);

  /* Execute User defined download */
  rocDownload();

  daLogMsg("INFO","Download Executed");

  tsClockReset();
  taskDelay(2);
  tsTrigLinkReset();
  taskDelay(2);


} /*end download */

/**
 *  PRESTART
 */
static void __prestart()
{
  ACKLOCK;
  ack_runend=0;
  ACKUNLOCK;
  CTRIGINIT;
  *(rol->nevents) = 0;

  daLogMsg("INFO","Entering Prestart");

  TSPRIMARY_INIT;
  CTRIGRSS(TSPRIMARY,1,usrtrig,usrtrig_done);
  CRTTYPE(1,TSPRIMARY,1);

  /* Check the health of the vmeBus Mutex.. re-init if necessary */
  vmeCheckMutexHealth(10);

  /* Execute User defined prestart */
  rocPrestart();

  /* Send a Sync Reset */
  printf("%s: Sending sync\n",__FUNCTION__);
  taskDelay(2);
  tsSyncReset(1); /* Set Block level as well */
  taskDelay(2);
  tsSetBlockLevel(blockLevel);

  /* Connect User Trigger Routine */
  tsIntConnect(TS_INT_VEC,asyncTrigger,0);

  daLogMsg("INFO","Prestart Executed");

  if (__the_event__) WRITE_EVENT_;
  *(rol->nevents) = 0;
  rol->recNb = 0;
} /*end prestart */

/**
 *  PAUSE
 */
static void __pause()
{
  CDODISABLE(TSPRIMARY,1,0);
  daLogMsg("INFO","Pause Executed");

  if (__the_event__) WRITE_EVENT_;
} /*end pause */

/**
 *  GO
 */
static void __go()
{
  daLogMsg("INFO","Entering Go");
  ACKLOCK;
  ack_runend=0;
  ACKUNLOCK;

  emptyCount=0;
  errCount=0;

  CDOENABLE(TSPRIMARY,1,1);
  rocGo();

  tsIntEnable(1);

  if (__the_event__) WRITE_EVENT_;
}

/**
 *  END
 */
static void __end()
{
  unsigned int blockstatus=0;

  tsDisableTriggerSource(1);

  blockstatus = tsBlockStatus(0,0);

  ACKLOCK;
  ack_runend=1;
  if(blockstatus)
    {
      printf("%s: Clearing data from TS (blockstatus = 0x%x)\n",__FUNCTION__, blockstatus);
      ENDRUN_TIMEDWAIT(30);
      printf("%s: endrun_timedwait_ret = %d   blockstatus = 0x%x\n",
	     __FUNCTION__,endrun_timedwait_ret,tsBlockStatus(0,0));
    }
  ACKUNLOCK;

  INTLOCK;
  INTUNLOCK;

  tsIntDisable();
  tsIntDisconnect();

  /* Execute User defined end */
  rocEnd();

  CDODISABLE(TSPRIMARY,1,0);

  dmaPStatsAll();

  daLogMsg("INFO","End Executed");

  if (__the_event__) WRITE_EVENT_;
} /* end end block */

void usrtrig(unsigned long EVTYPE,unsigned long EVSOURCE)
{
  int ii, len;
  int syncFlag=0;
  unsigned int event_number=0;
  DMANODE *outEvent;
  unsigned int blockstatus = 0;
  int bready = 0;

  outEvent = dmaPGetItem(vmeOUT);
  if(outEvent != NULL)
    {
      len = outEvent->length;
      syncFlag = outEvent->type;
      event_number = outEvent->nevent;

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
	  printf("tsprimary_list: ERROR rol->dabufp is NULL -- Event lost\n");
	}

      CECLOSE;

      ACKLOCK;

      dmaPFreeItem(outEvent);

      if(tsNeedAck>0)
	{
	  tsNeedAck=0;
	  ACKSIGNAL;
	}

      if(ack_runend)
	{
	  /* Check for available blocks in the TI */
	  blockstatus = tsBlockStatus(0,0);
	  bready = tsBReady();
	  printf("tsBlockStatus = 0x%x   tsBReady() = %d\n",
		 blockstatus,bready);

	  if((blockstatus == 0) && (bready == 0))
	    ENDRUN_SIGNAL;
	}

      ACKUNLOCK;
    }
  else
    {
      logMsg("Error: no Event in vmeOUT queue\n",0,0,0,0,0,0);
    }

} /*end trigger */

void asyncTrigger()
{
  int intCount=0;
  int length,size;

  intCount = tsGetIntCount();
  syncFlag = tsGetSyncEventFlag();

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
  the_event->type = syncFlag;

  /* Execute user defined Trigger Routine */
  rocTrigger(intCount);

  /* Put this event's buffer into the OUT queue. */
  ACKLOCK;
  PUTEVENT(vmeOUT);

  /* Check if the event length is larger than expected */
  length = (((long)(dma_dabufp) - (long)(&the_event->length))) - 4;
  size = the_event->part->size - sizeof(DMANODE);

  if(length>size)
    {
      printf("rocLib: ERROR: Event length > Buffer size (%d > %d).  Event %ld\n",
	     length,size,the_event->nevent);
      daLogMsg("WARN", "Event buffer overflow");
    }

  if(dmaPEmpty(vmeIN))
    {
      emptyCount++;
      /*printf("WARN: vmeIN out of event buffers (intCount = %d).\n",intCount);*/

      if((ack_runend == 0) || (tsBReady() > 0))
	{
	  /* Set the NeedAck for Ack after a buffer is freed */
	  tsNeedAck = 1;

	  /* Wait for the signal indicating that a buffer has been freed */
	  ACKWAIT;
	}

    }

  ACKUNLOCK;

}

void usrtrig_done()
{
} /*end done */

void __done()
{
  poolEmpty = 0; /* global Done, Buffers have been freed */
} /*end done */

static void __reset()
{
  int iemp=0;

  tsIntDisable();
  tsIntDisconnect();

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

int
getInQueueCount()
{
  if(vmeIN)
    return(dmaPNodeCount(vmeIN));
  else
    return(0);
}

/* This routine is automatically executed just before the shared libary
   is unloaded.

   Clean up memory that was allocated
*/
__attribute__((destructor)) void end (void)
{
  static int ended=0;
  extern volatile struct TS_A24RegStruct  *TSp;

  if(ended==0)
    {
      printf("ROC Cleanup\n");

      rocCleanup();
      TSp = NULL;

      dmaPFreeAll();

      ended=1;
    }

}
