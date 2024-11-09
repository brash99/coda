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
 *     Primitive trigger control for VME CPUs using the TJNAF Trigger
 *     Supervisor interface card
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#define _GNU_SOURCE


#ifdef VXWORKS
#include <vxWorks.h>
#else
#include <stdlib.h>
#include <stdarg.h>
#include <sched.h>
#endif
#include <stdio.h>
#include <string.h>

#ifdef VXWORKS
#include <logLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <iv.h>
#include <vxLib.h>
#endif

#include "jvme.h"
#include "tirLib.h"


/* Define variable for TIR board revision */
static int tirVersion = 0; /* Default is an invalid Version ID */
struct vme_tir *tirPtr;

// Global Variables
unsigned int tirNeedAck=0;

#ifdef VXWORKS
extern  int sysBusToLocalAdrs(int, char *, char **);
extern  int intDisconnect(int);
extern  int sysIntEnable(int);
IMPORT  STATUS sysIntDisable(int);
#else

int
intLock()
{
  return 0;
}

void
intUnlock(int arg)
{
  return;
}

int
sysIntEnable(int arg)
{
  return 0;
}
#endif

static BOOL         tirIntRunning  = FALSE;	      /* running flag */
static VOIDFUNCPTR  tirIntRoutine  = NULL;	      /* user interrupt service routine */
static int          tirIntArg      = 0;	              /* arg to user routine */
static unsigned int tirIntLevel    = 0;               /* VME Interrupt Level 1-7 */
static unsigned int tirIntVec      = TIR_INT_VEC;     /* default interrupt Vector */

static VOIDFUNCPTR  tirAckRoutine  = NULL;            /* user trigger acknowledge routine */
static int          tirAckArg      = 0;               /* arg to user trigger ack routine */

/* Globals */
unsigned int      tirIntMode     = 0;
unsigned int      tirIntCount    = 0;
unsigned int      tirSyncFlag    = 0;
unsigned int      tirLateFail    = 0;
int               tirDoAck       = 0;               /* Acknowledge trigger */
pthread_mutex_t   tirMutex = PTHREAD_MUTEX_INITIALIZER;
int               tirDoIntPolling= 1;               /* Decision to use library polling thread routine */

#define TLOCK     if(pthread_mutex_lock(&tirMutex)<0) perror("pthread_mutex_lock");
#define TUNLOCK   if(pthread_mutex_unlock(&tirMutex)<0) perror("pthread_mutex_unlock");

#define TRYLOCK   pthread_mutex_trylock(&tirMutex)

// Routine to start up the TIR polling thread
void startTirPollThread();

// polling thread pthread and pthread_attr
pthread_attr_t tirpollthread_attr;
pthread_t tirpollthread;

BOOL
tirIntIsRunning()
{
  return tirIntRunning;
}


/*******************************************************************************
 *
 * tirInt - default interrupt handler
 *
 * This rountine handles the tir interrupt.  A user routine is
 * called, if one was connected by tirIntConnect().
 *
 * RETURNS: N/A
 *
 * SEE ALSO: tirIntConnect()
 */

static void
tirInt (void)
{
  tirIntCount++;

/*   INTLOCK; */

  if (tirIntRoutine != NULL)	/* call user routine */
    (*tirIntRoutine) (tirIntArg);

  /* Acknowledge trigger */
  if(tirDoAck==1)
    {
      tirIntAck();
    }
/*   INTUNLOCK; */
}


/*******************************************************************************
 *
 * tirPoll - Polling Service Thread
 *
 * This thread handles the polling of the tir interrupt.  A user routine is
 * called, if one was connected by tirIntConnect().
 *
 * RETURNS: N/A
 *
 * SEE ALSO: tirIntConnect()
 */

void
tirPoll()
{
  int tirdata;
  int policy=0;
  struct sched_param sp;
#ifdef DO_CPUAFFINITY
  cpu_set_t testCPU;

  if (pthread_getaffinity_np(pthread_self(), sizeof(testCPU), &testCPU) <0)
    {
      perror("pthread_getaffinity_np");
    }
  printf("tirPoll: CPUset = %d\n",testCPU);

  CPU_ZERO(&testCPU);
  CPU_SET(2,&testCPU); // set it to be the same as the dmaPollLib polling thread
  if (pthread_setaffinity_np(pthread_self(),sizeof(testCPU), &testCPU) <0)
    {
      perror("pthread_setaffinity_np");
    }
  if (pthread_getaffinity_np(pthread_self(), sizeof(testCPU), &testCPU) <0)
    {
      perror("pthread_getaffinity_np");
    }
  printf("tirPoll: CPUset = %d\n",testCPU);
#endif

  policy=SCHED_FIFO;
  sp.sched_priority=40;
  printf("tirPoll: Entering polling loop...\n");
  pthread_setschedparam(pthread_self(),policy,&sp);
  pthread_getschedparam(pthread_self(),&policy,&sp);
  printf ("tirPoll: INFO: Running at %s/%d\n",
	  (policy == SCHED_FIFO ? "FIFO"
	   : (policy == SCHED_RR ? "RR"
	      : (policy == SCHED_OTHER ? "OTHER"
		 : "unknown"))), sp.sched_priority);


  while(1)
    {

      pthread_testcancel();

      // If still need Ack, don't test the Trigger Status
      if(tirNeedAck)
	{
	  continue;
	}

      tirdata = 0;

      tirdata = tirIntPoll();
      if(tirdata == ERROR)
	{
	  printf("%s: ERROR: tirIntPoll returned ERROR.\n",__FUNCTION__);
	  break;
	}

      if(tirdata)
	{
	  INTLOCK;

	  if (tirIntRoutine != NULL)	/* call user routine */
	    (*tirIntRoutine) (tirIntArg);

	  /* Write to TIR to Acknowledge Interrupt */
	  if(tirDoAck==1)
	    {
	      tirIntAck();
	    }
	  INTUNLOCK;
	}

    }
  printf("tirPoll: Read Error: Exiting Thread\n");
  pthread_exit(0);
}

void
startTirPollThread()
{
  int ptir_status;
#ifdef DO_CPUAFFINITY
  cpu_set_t pollCPU;
#endif

  ptir_status = pthread_create(&tirpollthread,
			       NULL,
			       (void*(*)(void *)) tirPoll,
			       (void *)NULL);
  if(ptir_status!=0)
    {
      printf("Error: TIR Polling Thread could not be started.\n");
      printf("\t ... returned: %d\n",ptir_status);
    }

#ifdef DO_CPUAFFINITY
  CPU_ZERO(&pollCPU);
  CPU_SET(2,&pollCPU);
  if (pthread_setaffinity_np(tirpollthread,sizeof(pollCPU), &pollCPU) <0)
    {
      perror("pthread_setaffinity_np");
    }
  if (pthread_getaffinity_np(tirpollthread, sizeof(pollCPU), &pollCPU) <0)
    {
      perror("pthread_getaffinity_np");
    }
  printf("startTirPollThread: CPUset = %d\n",pollCPU);
#endif

}

/* Example User routines */
void
tirIntUser(int arg)
{

  vmeWrite16(&tirPtr->tir_oport,0x0001);
  vmeRead16(&tirPtr->tir_oport);
  vmeWrite16(&tirPtr->tir_oport, 0x0000);

  return;
}

void
tirPollUser(int arg)
{
  int tirdata;

  printf("tirPollUser: Entering polling loop...\n");

  while(1)
    {

      tirdata = 0;
      tirdata = tirIntPoll();
      if(tirdata == ERROR) break;

      if(tirdata)
	{

	  /* Strobe output Register */
	  tirIntOutput(1);
	  tirIntOutput(0);

	  /* Write to TIR to Acknowledge Interrupt */
	  tirIntAck();

	}

    }
  printf("tirPollUser: Read Error: Exiting Loop\n");
}


int
tirIntInit(unsigned int tAddr, unsigned int mode, int force)
{
  /*   int stat = 0; */
  unsigned long laddr;
  unsigned short rval;
  int stat;

  if (tAddr == 0)
    {
      tAddr = TIR_DEFAULT_ADDR;
    }

  tirIntCount = 0;
  tirDoAck = 1;

#ifdef VXWORKS
  stat = sysBusToLocalAdrs(0x29,(char *)tAddr,(char **)&laddr);
  if (stat != 0)
    {
      printf("tirInit: ERROR: Error in sysBusToLocalAdrs res=%d \n",stat);
    }
  else
    {
      printf("TIR address = 0x%lx\n",laddr);
    }
#else
  stat = vmeBusToLocalAdrs(0x29,(char *)(unsigned long)tAddr,(char **)&laddr);
  if (stat != 0)
    {
      printf("tirInit: ERROR: Error in vmeBusToLocalAdrs res=%d \n",stat);
    }
  else
    {
      printf("TIR VME (USER) address = 0x%.8x (0x%lx)\n",tAddr,laddr);
    }
#endif

  /* Check if TIR board is readable */
#ifdef VXWORKS
  stat = vxMemProbe((char *)laddr,0,2,(char *)&rval);
#else
  stat = vmeMemProbe((char *)laddr,2,(char *)&rval);
#endif
  if (stat != 0)
    {
      printf("tirInit: ERROR: TIR card not addressable\n");
      return(-1);
    }

  if(force == 0)  /* Check if the TIR is active */
    {
      if(rval&TIR_ENABLED)
	{
	  printf("WARN: TIR is currently enabled (set the Force Reset Flag to override)\n");
	  return(-1);
	}
    }

  /* Set Up pointer */
  tirPtr = (struct vme_tir *)laddr;

  tirIntRunning = 0;
  vmeWrite16(&tirPtr->tir_csr,0x80); /* Reset the board */
  rval = vmeRead16(&tirPtr->tir_csr)&TIR_VERSION_MASK;
  if(rval == TIR_VERSION_1)
    {
      tirVersion = 1;
    }
  else if(rval == TIR_VERSION_2)
    {
      tirVersion = 2;
    }
  else
    {
      tirVersion = 0;
      printf("tirInit: ERROR: Invalid TIR Version register= 0x%04x\n",rval);
      return(-1);
    }


  /* Check on Mode of operations */
  if(mode > 3)
    {
      printf("tirInit: WARN: Invalid mode id (%d) - Default to External Interrupts\n",mode);
      tirIntMode = TIR_EXT_INT;
    }
  else
    {
      tirIntMode = mode;
      switch(tirIntMode)
	{
	case TIR_EXT_INT:
	  printf("TIR (version %d) setup for External Interrupts\n",tirVersion);
	  break;
	case TIR_TS_INT:
	  printf("TIR (version %d) setup for TS Interrupts\n",tirVersion);
	  break;
	case TIR_EXT_POLL:
	  printf("TIR (version %d) setup for External polling\n",tirVersion);
	  break;
	case TIR_TS_POLL:
	  printf("TIR (version %d) setup for TS polling\n",tirVersion);

	}
    }


  return(tirVersion);

}

/*******************************************************************************
 *
 * tirDoLibraryPollingThread - Set the decision on whether or not the
 *      TIR library should perform the trigger polling via thread.
 *
 *   choice:   0 - No Thread Polling
 *             1 - Library Thread Polling (started with tirIntEnable)
 *
 *
 * RETURNS: OK, or ERROR .
 */

int
tirDoLibraryPollingThread(int choice)
{
  if(choice)
    tirDoIntPolling=1;
  else
    tirDoIntPolling=0;

  return tirDoIntPolling;
}

/*******************************************************************************
 *
 * tirIntConnect - connect a user routine to the TIR interrupt or
 *                 latched trigger, if polling
 *
 * This routine specifies the user interrupt routine to be called at each
 * interrupt or latched trigger (if using polling).
 *
 * RETURNS: OK, or ERROR .
 */

int
tirIntConnect(unsigned int vector, VOIDFUNCPTR routine, unsigned int arg)
{

#ifndef VXWORKS
  int status;
#endif

  if(tirPtr == NULL)
    {
      printf("tirIntConnect: ERROR: TIR not initialized\n");
      return(ERROR);
    }


#ifdef VXWORKS
  /* Disconnect any current interrupts */
  if((intDisconnect(tirIntVec) !=0))
    printf("tirIntConnect: Error disconnecting Interrupt\n");
#endif

  tirIntCount = 0;
  tirDoAck = 1;

  /* Set Vector and Level */
  TLOCK;
  if((vector < 255)&&(vector > 64))
    {
      tirIntVec = vector;
    }
  else
    {
      tirIntVec = TIR_INT_VEC;
    }
  tirIntLevel = (vmeRead16(&tirPtr->tir_csr)&TIR_LEVEL_MASK)>>8;
  printf("tirIntConnect: INFO: Int Vector = 0x%x  Level = %d\n",tirIntVec,tirIntLevel);

  switch (tirIntMode)
    {
    case TIR_TS_POLL:
    case TIR_EXT_POLL:

      vmeWrite16(&tirPtr->tir_csr,0x80);
      vmeWrite16(&tirPtr->tir_vec,tirIntVec);

      break;
    case TIR_TS_INT:
    case TIR_EXT_INT:

      vmeWrite16(&tirPtr->tir_csr,0x80);
      vmeWrite16(&tirPtr->tir_vec,tirIntVec);
#ifdef VXWORKS
      intConnect(INUM_TO_IVEC(tirIntVec),tirInt,arg);
#else
      status = vmeIntConnect (tirIntVec, tirIntLevel,
			      tirInt,arg);
      if (status != OK)
	{
	  printf("vmeIntConnect failed\n");
	  TUNLOCK;
	  return(ERROR);
	}
#endif

      break;

    default:
      printf(" tirIntConnect: ERROR: TIR Mode not defined %d\n",tirIntMode);
      TUNLOCK;
      return(ERROR);
    }

  if(routine)
    {
      tirIntRoutine = routine;
      tirIntArg = arg;
    }
  else
    {
      tirIntRoutine = NULL;
      tirIntArg = 0;
    }

  TUNLOCK;
  return(OK);
}


void
tirIntDisconnect()
{
#ifndef VXWORKS
  int status;
#endif
  void *res;

  if(tirPtr == NULL)
    {
      printf("tirIntDisconnect: ERROR: TIR not initialized\n");
      return;
    }

  if(tirIntRunning)
    {
      printf("tirIntDisconnect: ERROR: TIR is Enabled - Call tirIntDisable() first\n");
      return;
    }

  /* INTLOCK; */

  /* Reset TIR */
  TLOCK;
  vmeWrite16(&tirPtr->tir_csr,0x80);
  TUNLOCK;

  switch (tirIntMode)
    {
    case TIR_TS_INT:
    case TIR_EXT_INT:

#ifdef VXWORKS
      /* Disconnect any current interrupts */
      sysIntDisable(tirIntLevel);
      if((intDisconnect(tirIntVec) !=0))
	printf("tirIntConnect: Error disconnecting Interrupt\n");
#else
      status = vmeIntDisconnect(tirIntLevel);
      if (status != OK)
	{
	  printf("vmeIntDisconnect failed\n");
	}
#endif
      break;
    case TIR_TS_POLL:
    case TIR_EXT_POLL:
      if(tirDoIntPolling)
	{
	  if(tirpollthread)
	    {
	      if(pthread_cancel(tirpollthread)<0)
		perror("pthread_cancel");
	      if(pthread_join(tirpollthread,&res)<0)
		perror("pthread_join");
	      if (res == PTHREAD_CANCELED)
		printf("%s: Polling thread canceled\n",__FUNCTION__);
	      else
		printf("%s: ERROR: Polling thread NOT canceled\n",__FUNCTION__);
	    }
	}
      break;
    default:
      break;
    }

  /* INTUNLOCK; */

  printf("tirIntDisconnect: Disconnected\n");

  return;
}




int
tirIntEnable(int iflag)
{
  int lock_key=0;

  if(tirPtr == NULL)
    {
      printf("tirIntEnable: ERROR: TIR not initialized\n");
      return(-1);
    }

  TLOCK;
  if(iflag == TIR_CLEAR_COUNT)
    tirIntCount = 0;

  tirIntRunning = 1;
  tirDoAck      = 1;
  tirNeedAck    = 0;

  switch (tirIntMode)
    {
    case TIR_TS_POLL:

      if(tirDoIntPolling)
	startTirPollThread();
      vmeWrite16(&tirPtr->tir_csr,TIR_ENABLED);

      break;
    case TIR_EXT_POLL:

      if(tirDoIntPolling)
	startTirPollThread();
      vmeWrite16(&tirPtr->tir_csr,(TIR_ENABLED | TIR_EXTERNAL));

      break;
    case TIR_TS_INT:

      lock_key = intLock();
      sysIntEnable(tirIntLevel);
      vmeWrite16(&tirPtr->tir_csr,(TIR_ENABLED | TIR_INTERRUPT));

      break;
    case TIR_EXT_INT:

      lock_key = intLock();
      sysIntEnable(tirIntLevel);
      vmeWrite16(&tirPtr->tir_csr,(TIR_ENABLED | TIR_EXTERNAL | TIR_INTERRUPT));

      break;
    default:
      tirIntRunning = 0;
      if(lock_key)
	intUnlock(lock_key);
      printf(" tirIntError: ERROR: TIR Mode not defined %d\n",tirIntMode);
      TUNLOCK;
      return(ERROR);
    }

  TUNLOCK;
  if(lock_key)
    intUnlock(lock_key);

  return(OK);

}

void
tirIntDisable()
{

  if(tirPtr == NULL)
    {
      printf("tirIntDisable: ERROR: TIR not initialized\n");
      return;
    }

  TLOCK;
  vmeWrite16(&tirPtr->tir_csr,0);
  tirIntRunning = 0;
  TUNLOCK;
}

void
tirIntReset()
{
  if(tirPtr == NULL)
    {
      printf("tirIntReset: ERROR: TIR not initialized\n");
      return;
    }

  TLOCK;
  vmeWrite16(&tirPtr->tir_csr,0x80);
  tirIntRunning = 0;
  TUNLOCK;

}

int
tirIntAckConnect(VOIDFUNCPTR routine, unsigned int arg)
{
  if(routine)
    {
      tirAckRoutine = routine;
      tirAckArg = arg;
    }
  else
    {
      printf("%s: WARN: routine undefined.\n",__FUNCTION__);
      tirAckRoutine = NULL;
      tirAckArg = 0;
      return ERROR;
    }
  return OK;
}

void
tirIntAck()
{
  if(tirPtr == NULL)
    {
      logMsg("tirIntAck: ERROR: TIR not initialized\n",0,0,0,0,0,0);
      return;
    }

  if (tirAckRoutine != NULL)
    {
      /* Execute user defined Acknowlege, if it was defined */
      TLOCK;
      (*tirAckRoutine) (tirAckArg);
      TUNLOCK;
    }
  else
    {
      TLOCK;
/*       tirNeedAck = 0; */
      tirDoAck = 1;
      vmeWrite16(&tirPtr->tir_dat,0x8000);
      TUNLOCK;
    }
}

void
tirIntPause()
{
  tirDoAck = 0;
}

void
tirIntResume()
{
  tirDoAck = 1;
}


unsigned int
tirIntType()
{
  unsigned short reg;
  unsigned int tt=0;

  if(tirPtr == NULL)
    {
      logMsg("tirIntType: ERROR: TIR not initialized\n",0,0,0,0,0,0);
      return(0);
    }

  TLOCK;
  reg = vmeRead16(&tirPtr->tir_dat);
  TUNLOCK;

  if((tirIntMode == TIR_EXT_POLL)||(tirIntMode == TIR_EXT_INT))
    {
      if(tirVersion == 1)
	tt = (reg&0x3f);
      else
	tt = (reg&0xfff);

      tirSyncFlag = 0;
      tirLateFail = 0;

    }
  else if((tirIntMode == TIR_TS_POLL)||(tirIntMode == TIR_TS_INT))
    {
      if(tirVersion == 1)
	tt = ((reg&0x3c)>>2);
      else
	tt = ((reg&0xfc)>>2);

      tirSyncFlag = reg&0x01;
      tirLateFail = (reg&0x02)>>1;

    }

  return(tt);
}

int
tirIntTrigData(unsigned int *itype, unsigned int *isyncFlag, unsigned int *ilateFail)
{
  unsigned short reg;
  if(tirPtr == NULL)
    {
      logMsg("tirIntType: ERROR: TIR not initialized\n",0,0,0,0,0,0);
      return(ERROR);
    }

  TLOCK;
  reg = vmeRead16(&tirPtr->tir_dat);

  if((tirIntMode == TIR_EXT_POLL)||(tirIntMode == TIR_EXT_INT))
    {
      if(tirVersion == 1)
	*itype = (reg&0x3f);
      else
	*itype = (reg&0xfff);

      *isyncFlag   = 0;
      *ilateFail   = 0;
      tirSyncFlag = 0;
      tirLateFail = 0;

    }
  else if((tirIntMode == TIR_TS_POLL)||(tirIntMode == TIR_TS_INT))
    {
      if(tirVersion == 1)
	*itype = ((reg&0x3c)>>2);
      else
	*itype = ((reg&0xfc)>>2);

      *isyncFlag   = reg&TIR_SYNC_FLAG;
      *ilateFail   = (reg&TIR_LATE_FAIL)>>1;
      tirSyncFlag = reg&TIR_SYNC_FLAG;
      tirLateFail = (reg&TIR_LATE_FAIL)>>1;
    }
  TUNLOCK;

  return OK;
}

int
tirDecodeTrigData(unsigned int idata, unsigned int *itype,
		  unsigned int *isyncFlag, unsigned int *ilateFail)
{

  if((tirIntMode == TIR_EXT_POLL)||(tirIntMode == TIR_EXT_INT))
    {
      if(tirVersion == 1)
	*itype = (idata&0x3f);
      else
	*itype = (idata&0xfff);

      *isyncFlag   = 0;
      *ilateFail   = 0;

    }
  else if((tirIntMode == TIR_TS_POLL)||(tirIntMode == TIR_TS_INT))
    {
      if(tirVersion == 1)
	*itype = ((idata&0x3c)>>2);
      else
	*itype = ((idata&0xfc)>>2);

      *isyncFlag   = idata&TIR_SYNC_FLAG;
      *ilateFail   = (idata&TIR_LATE_FAIL)>>1;
    }

  return OK;
}

int
tirIntPoll()
{
  unsigned short sval=0;

  TLOCK;
  sval = vmeRead16(&tirPtr->tir_csr);
  if( (sval != 0xffff) && ((sval&0x8000) != 0) )
    {
      tirIntCount++;
      TUNLOCK;
      return (1);
    }
  else
    {
      TUNLOCK;
      return (0);
    }

}

void
tirIntOutput(unsigned short out)
{
  if(tirPtr == NULL)
    {
      logMsg("tirIntOutput: ERROR: TIR not initialized\n",0,0,0,0,0,0);
      return;
    }

  TLOCK;
  vmeWrite16(&tirPtr->tir_oport,out);
  TUNLOCK;

}

unsigned int
tirGetIntCount()
{
  return(tirIntCount);
}

void
tirClearIntCount()
{
  tirIntCount=0;
}


unsigned short
tirReadCsr()
{
  unsigned short rval;
  TLOCK;
  rval = vmeRead16(&tirPtr->tir_csr);
  TUNLOCK;

  return rval;
}
unsigned short
tirReadVec()
{
  unsigned short rval;
  TLOCK;
  rval = vmeRead16(&tirPtr->tir_vec);
  TUNLOCK;

  return rval;
}
unsigned short
tirReadData()
{
  unsigned short rval;
  TLOCK;
  rval = vmeRead16(&tirPtr->tir_dat);
  TUNLOCK;

  return rval;
}
unsigned short
tirReadOutput()
{
  unsigned short rval;
  TLOCK;
  rval = vmeRead16(&tirPtr->tir_oport);
  TUNLOCK;

  return rval;
}
unsigned short
tirReadInput()
{
  unsigned short rval;
  TLOCK;
  rval = vmeRead16(&tirPtr->tir_iport);
  TUNLOCK;

  return rval;
}
void
tirWriteCsr(unsigned short val)
{
  TLOCK;
  vmeWrite16(&tirPtr->tir_csr, val);
  TUNLOCK;
}
void
tirWriteData(unsigned short val)
{
  /* Mask the write-only bits */
  val &= (TIR_TEST_INTERRUPT | TIR_ACK_TRIGGER);

  TLOCK;
  vmeWrite16(&tirPtr->tir_dat, val);
  TUNLOCK;
}


int
tirIntStatus(int pflag)
{
  unsigned short csr, ivec, data, out, inp;
  int enabled, int_level, ext, test, latch, int_pending, int_mode;

  if(tirPtr == NULL)
    {
      printf("tirIntStatus: ERROR: TIR not initialized\n");
      return -1;
    }

  /* Read TIR Registers */
  TLOCK;
  csr  = vmeRead16(&tirPtr->tir_csr);
  ivec = vmeRead16(&tirPtr->tir_vec);
  data = vmeRead16(&tirPtr->tir_dat);
  out  = vmeRead16(&tirPtr->tir_oport);
  inp  = vmeRead16(&tirPtr->tir_iport);
  TUNLOCK;

  enabled     = csr&TIR_ENABLED;
  int_level   = (csr&TIR_LEVEL_MASK)>>8;
  ext         = csr&TIR_EXTERNAL;
  int_mode    = csr&TIR_INTERRUPT;
  test        = csr&TIR_TEST_MODE;
  latch       = csr&TIR_TRIG_STATUS;
  int_pending = csr&TIR_INT_PENDING;

  if(pflag == 1)
    {
      /* print out status info */

      printf("STATUS for TIR at base address 0x%lx \n",(unsigned long) tirPtr);
      printf("----------------------------------------- \n");
      printf("Trigger Count: %d\n",tirIntCount);
      printf("Registers: \n");
      printf("  CSR    = 0x%04x (VME Int Level: %d)\n",csr,int_level);
      printf("  VEC    = 0x%04x \n",ivec);
      printf("  DATA   = 0x%04x \n",data);
      printf("  OUTPUT = 0x%04x \n",out);
      printf("  INPUT  = 0x%04x \n",inp);
      printf("\n");
      if(test)
	printf("Test Mode Enabled\n");

      if(enabled)
	{
	  if(latch)
	    printf("State     : Enabled (trigger latched!)\n");
	  else
	    printf("State     : Enabled\n");

	  if(ext)
	    printf("Source    : External\n");
	  else
	    printf("Source    : TS\n");

	  if(int_mode)
	    {
	      if(int_pending)
		printf("Interrupts: Enabled (pending)\n");
	      else
		printf("Interrupts: Enabled\n");
	    }
	}
      else
	{
	  printf("State : Disabled\n");
	}
      printf("\n");
    }

  return enabled;

}
