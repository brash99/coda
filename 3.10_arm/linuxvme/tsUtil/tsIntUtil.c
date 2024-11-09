/* Module: tsIntUtil.c
 *
 * Description: Trigger Supervisor Program
 *              Utility Library - Interrupt Specific functions.
 *
 * Author:
 *	David Abbott
 *	CEBAF Data Acquisition Group
 *
 * Revision History:
 *	  Revision 1.0  2008/20/3   abbottd
 *
 *        Revision 2.1  2010/02     moffit
 *           - Added Linux compatibility
 *
 */

#ifdef VXWORKS
#include <intLib.h>
#include <iv.h>
#endif

/* Define default Interrupt Level and vector for the TS source -
   Note that the Interrupt level is assigned by a hardware switch but is not
   determinable via software */
#define TS_INT_LEVEL    3
#define TS_INT_VEC      0xec


LOCAL VOIDFUNCPTR tsIntRoutine  = NULL;	            /* user interrupt service routine */
LOCAL int         tsIntArg      = 0;	            /* arg to user routine */
LOCAL UINT32      tsIntLevel    = TS_INT_LEVEL;     /* VME Interrupt Level 1-7 valid*/
LOCAL UINT32      tsIntVec      = TS_INT_VEC;       /* default interrupt Vector */
LOCAL int         tsDoAck       = 1;                /* Default - Acknowledge trigger */
LOCAL int         tsNeedAck     = 0;

/* Globals */
unsigned int      tsIntPoll     = 0;
unsigned int      tsIntCount    = 0;

/* polling thread pthread and pthread_attr */
pthread_attr_t tspollthread_attr;
pthread_t tspollthread;


/*******************************************************************************
 *
 * tsInt - default interrupt handler
 *
 * This rountine handles the ts interrupt.  A user routine is
 * called, if one was specified by tsIntConnect().
 *
 * RETURNS: N/A
 *
 * SEE ALSO: tsIntConnect()
 */
static void
tsInt (
       void
       )
{
  tsIntCount++;

  TSINTLOCK;

  if (tsIntRoutine != NULL)	/* call user routine */
    (*tsIntRoutine) (tsIntArg);


  /* Acknowledge trigger */
  if(tsDoAck)
    tsIntAck();

  TSINTUNLOCK;
}


/*******************************************************************
 *   Function : tsPoll
 *
 *   Function : TS Polling Thread
 *              Continuously checks for a valid trigger.  When a
 *              valid trigger is present, execute the Service
 *              Routine.  The thread is cancelled with a call
 *              to tsIntDisconnect().
 *
 *   Parameters : none
 *
 *   Returns : void
 *
 */
void
tsPoll()
{
  int tsdata;


  printf("tsPoll: TS Polling Thread waiting for valid triggers\n");

  while(1) {

    pthread_testcancel();

    // If still need Ack, don't test the Trigger Status
    if(tsNeedAck) continue;

    tsdata = 0;
    tsdata = tsIntTest();

    if(tsdata == ERROR) break;

    if(tsdata) {
      tsIntCount++;

      TSINTLOCK;

      if (tsIntRoutine != NULL)	/* call user routine */
	(*tsIntRoutine) (tsIntArg);

      /* Write to TS to Acknowledge Interrupt */
      if(tsDoAck==1)
	tsIntAck();

      TSINTUNLOCK;
    }

  }
  printf("tsPoll: Read Error: Exiting Thread\n");
  pthread_exit(0);
}

/*******************************************************************
 *   Function : startTSPollThread
 *
 *   Function : Spawns tsPoll() - TS Polling Thread
 *
 *   Parameters : none
 *
 *   Returns : void
 *
 */
void
startTSPollThread()
{
  int pts_status;

  pts_status = pthread_create(&tspollthread,
			       NULL,
			       (void*(*)(void *)) tsPoll,
			       (void *)NULL);
  if(pts_status!=0) {
    printf("Error: TS Polling Thread could not be started.\n");
    printf("\t ... returned: %d\n",pts_status);
  }

}

/* Example User routine */
void
tsIntUser(int arg)
{
  int tsdata;
  unsigned int sync, latefail, code;

  tsdata = tsIntTest();
  sync     = tsdata&TS_LROC_SYNC;
  latefail = (tsdata&TS_LROC_LATEFAIL)>>1;
  code     = (tsdata&TS_LROC_CODE_MASK)>>2;
  logMsg("tsIntUser: Got Trigger (0x%x): Event Type = %d (sync = %d, latefail = %d)\n",
	 tsdata,code,sync,latefail,0,0);

  return;
}


/*******************************************************************************
 *
 * tsIntConnect - connect a user routine to the TS interrupt
 *
 * This routine specifies the user interrupt routine to be called at each
 * interrupt
 *
 * RETURNS: OK, or ERROR .
 */
int
tsIntConnect (unsigned int vector, VOIDFUNCPTR routine, int arg, int poll)
{

#ifndef VXWORKS
  int status;
#endif

  if(tsP == NULL) {
    printf("tsIntConnect: ERROR: TS not initialized\n");
    return(-1);
  }

  TSLOCK;
  if(TSENABLED) {
    printf("tsIntConnect: ERROR : Cannot connect interrupts while TS is enabled\n");
    TSUNLOCK;
    return(-1);
  }


  /* Disconnect any current interrupts */
#ifdef VXWORKS
  if((intDisconnect(tsIntVec) !=0))
    printf("tsIntConnect: Error disconnecting Interrupt\n");
#else
  status = vmeIntDisconnect(tsIntLevel);
    if (status != OK) {
      printf("tsIntConnect: Error disconnecting Interrupt");
    }
#endif

  tsIntPoll = poll;
  tsIntCount = 0;
  tsDoAck = 1;

  /* Set Vector and Level */
  if((vector < 255)&&(vector > 64)) {
    tsIntVec = vector;
  }else{
    tsIntVec = TS_INT_VEC;
  }
  tsIntLevel = TS_INT_LEVEL;
  printf("tsIntConnect: INFO: Interrupt Vector = 0x%x  Level = %d\n",tsIntVec,tsIntLevel);

  vmeWrite32(&(tsP->intVec), tsIntVec);

  if (!tsIntPoll)
    {
#ifdef VXWORKS
      intConnect(INUM_TO_IVEC(tsIntVec),tsInt,0);
#else
      status = vmeIntConnect (tsIntVec, tsIntLevel,
			      tsInt,arg);
      if (status != OK) {
	printf("vmeIntConnect failed\n");
      }
#endif
    }
  TSUNLOCK;

  if(routine) {
    tsIntRoutine = routine;
    tsIntArg = arg;
  }else{
    tsIntRoutine = NULL;
    tsIntArg = 0;
  }

  return(0);
}

/*******************************************************************
 *   Function : tsIntDisconnect
 *
 *   Function : Disconnect the Interrupt or Polling Service routine
 *              and disable the Local ROC
 *
 *   Parameters : None
 *
 *   Returns : void
 *
 */

void
tsIntDisconnect()
{
#ifndef VXWORKS
  int status;
#endif

  if(tsP == NULL) {
    printf("tsIntDisconnect: ERROR: TS not initialized\n");
    return;
  }

  TSINTLOCK;
  TSLOCK;
  if(TSENABLED) {
    printf("tsIntDisconnect: ERROR: TS (GO) is Enabled - Call tsStop() first\n");
    TSUNLOCK;
    TSINTUNLOCK;
    return;
  }


  /* Reset TS Branch 5 Interrupts*/
  vmeWrite32(&(tsP->csr2),
	  vmeRead32(&(tsP->csr2)) & ~(TS_CSR2_ENABLE_LROC | TS_CSR2_ENABLE_INT));

  if(tsIntPoll)
    {
      if(tspollthread) {
	printf("tsIntDisconnect: Cancelling polling thread\n");
	if(pthread_cancel(tspollthread)<0)
	  perror("pthread_cancel");
      }
    }
  else
    {
  /* Disconnect any current interrupts */
#ifdef VXWORKS
  sysIntDisable(tsIntLevel);
  if((intDisconnect(tsIntVec) !=0))
    printf("tsIntDisconnect: Error disconnecting Interrupt (vector=%d)\n",tsIntVec);
#else
    status = vmeIntDisconnect(tsIntLevel);
    if (status != OK) {
      printf("vmeIntDisconnect failed\n");
    }
#endif
    }

  TSUNLOCK;
  TSINTUNLOCK;

  return;
}


/*******************************************************************
 *   Function : tsIntEnable
 *
 *   Function : Enable TS Interrupts or Start Polling thread
 *              and enable the Local ROC
 *
 *   Parameters : iflag - not used
 *
 *   Returns : OK if successful, -1 otherwise
 *
 */

int
tsIntEnable(int iflag)
{
  if(tsP == NULL) {
    printf("tsIntEnable: ERROR: TS not initialized\n");
    return(-1);
  }

  TSLOCK;
  if(TSENABLED) {
    printf("tsIntEnable: ERROR : Cannot Enable Interrupts while TS (GO) is enabled\n");
    TSUNLOCK;
    return(-1);
  }

  if(iflag)
    tsIntCount = 0;

  tsDoAck      = 1;

  if (tsIntPoll) {
    startTSPollThread();
    vmeWrite32(&(tsP->csr2),
	    vmeRead32(&(tsP->csr2)) | TS_CSR2_ENABLE_LROC);
  }else{
#ifdef VXWORKS
    sysIntEnable(tsIntLevel);
#endif
    vmeWrite32(&(tsP->csr2),
	    vmeRead32(&(tsP->csr2)) | (TS_CSR2_ENABLE_LROC | TS_CSR2_ENABLE_INT));
  }

  TSUNLOCK;
  return(OK);

}

/*******************************************************************
 *   Function : tsIntDisable
 *
 *   Function : Disable TS Interrupts and Local ROC
 *
 *   Parameters : none
 *
 *   Returns : void
 *
 */
void
tsIntDisable()
{

  if(tsP == NULL) {
    printf("tsIntDisable: ERROR: TS not initialized\n");
    return;
  }

  TSLOCK;
  if(TSENABLED) {
    printf("tsIntDisable: ERROR : Cannot Disable Interrupts while TS (GO) is enabled\n");
    TSUNLOCK;
    return;
  }

  vmeWrite32(&(tsP->csr2),
	  vmeRead32(&(tsP->csr2)) & ~(TS_CSR2_ENABLE_LROC | TS_CSR2_ENABLE_INT));
  TSUNLOCK;
}

/*******************************************************************
 *   Function : tsIntAck
 *
 *   Function : Acknowledge Trigger
 *
 *   Parameters : none
 *
 *   Returns : void
 *
 */
void
tsIntAck()
{
  if(tsP == NULL) {
    logMsg("tsIntAck: ERROR: TS not initialized\n",0,0,0,0,0,0);
    return;
  }

  TSLOCK;
  tsDoAck = 1;
  vmeWrite32(&(tsP->lrocBufStatus), TS_LROC_ACK);
  TSUNLOCK;
}

/*******************************************************************
 *   Function : tsIntType
 *
 *   Function : Get the event type
 *
 *   Parameters : none
 *
 *   Returns : Event type stored in Local ROC code
 *
 */
unsigned int
tsIntType()
{
  unsigned short reg;
  unsigned int tt=0;

  if(tsP == NULL) {
    logMsg("%s: ERROR: TS not initialized\n",__FUNCTION__,0,0,0,0,0);
    return ERROR;
  }

  TSLOCK;
  reg = vmeRead32(&(tsP->lrocData));
  tt = (reg&TS_LROC_CODE_MASK)>>2;
  TSUNLOCK;

  return tt;
}

/*******************************************************************
 *   Function : tsIntLRocData
 *
 *   Function : Get the Raw Local Roc Data
 *
 *   Parameters : none
 *
 *   Returns : Bits set in the Local Roc Data register
 *
 */
unsigned int
tsIntLRocData()
{
  unsigned short reg;

  if(tsP == NULL) {
    logMsg("%s: ERROR: TS not initialized\n",__FUNCTION__,0,0,0,0,0);
    return ERROR;
  }

  TSLOCK;
  reg = vmeRead32(&(tsP->lrocData));
  TSUNLOCK;

  return reg;
}



/*******************************************************************
 *   Function : tsIntTrigData
 *
 *   Function : Get trigger data from the local roc
 *
 *   Parameters :
 *        *itype     : Where to store the event type
 *        *isyncFlag : Where to store the syncFlag
 *        *ilateFail : Where to store the lateFail
 *
 *   Returns : OK if successful, otherwise ERROR
 *
 */
int
tsIntTrigData(unsigned int *itype, unsigned int *isyncFlag, unsigned int *ilateFail)
{
  unsigned short reg=0;

  if(tsP == NULL) {
    logMsg("%s: ERROR: TS not initialized\n",__FUNCTION__,0,0,0,0,0);
    return ERROR;
  }
  TSLOCK;
  reg      = vmeRead32(&(tsP->lrocData));

  *itype     = (reg&TS_LROC_CODE_MASK)>>2;
  *isyncFlag = (reg&TS_LROC_SYNC);
  *ilateFail = (reg&TS_LROC_LATEFAIL)>>1;
  TSUNLOCK;

  return OK;
}

/*******************************************************************
 *   Function : tsDecodeTrigData
 *
 *   Function : Decode trigger data from input "data" taken from local roc
 *              at some time in the past
 *
 *   Parameters :
 *        *itype     : Where to store the event type
 *        *isyncFlag : Where to store the syncFlag
 *        *ilateFail : Where to store the lateFail
 *
 *   Returns : OK
 *
 */
int
tsDecodeTrigData(unsigned int idata, unsigned int *itype,
		 unsigned int *isyncFlag, unsigned int *ilateFail)
{

  *itype     = (idata&TS_LROC_CODE_MASK)>>2;
  *isyncFlag = (idata&TS_LROC_SYNC);
  *ilateFail = (idata&TS_LROC_LATEFAIL)>>1;

  return OK;
}


/*******************************************************************
 *   Function : tsIntTest
 *
 *   Function : Test for valid trigger
 *
 *   Parameters : none
 *
 *   Returns : if Valid trigger - Local ROC Data
 *             if invalid status - -1
 *             if no trigger - 0
 *
 */
int
tsIntTest()
{
  unsigned int   lval=0;
  int   sval=0;

  TSLOCK;
  lval = vmeRead32(&(tsP->lrocBufStatus));
  if (lval == 0xffffffff)
    {
      return (-1);
      TSUNLOCK;
    }

  if(lval&TS_LROC_STROBE)
    {
      sval = (lval&TS_LROC_STROBE) | (vmeRead32(&(tsP->lrocData))&TS_LROC_DATA_MASK);
      TSUNLOCK;
      return (sval);
    }
  else
    {
      TSUNLOCK;
      return (0);
    }
}

/*******************************************************************
 *   Function : tsGetIntCount
 *
 *   Function : Get the interrupt count
 *
 *   Parameters : none
 *
 *   Returns : Interrupt Count
 *
 */
unsigned int
tsGetIntCount()
{
  return tsIntCount;
}
