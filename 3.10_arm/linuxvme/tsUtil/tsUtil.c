/*----------------------------------------------------------------------------*
 *  Copyright (c) 1991, 1992  Southeastern Universities Research Association, *
 *                            Continuous Electron Beam Accelerator Facility   *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 * CEBAF Data Acquisition Group, 12000 Jefferson Ave., Newport News, VA 23606 *
 *      heyes@cebaf.gov   Tel: (804) 249-7030    Fax: (804) 249-7363          *
 *----------------------------------------------------------------------------*
 * Module: tsUtil.c
 *
 * Description: Trigger Supervisor Program
 *              Utility Library.
 *
 * Author:
 *	David Abbott
 *	CEBAF Data Acquisition Group
 *
 * Revision History:
 *	  Revision 1.0  1996/05/1  17:38:00  abbottd
 *
 *        Revision 2.0  2007/07              abbottd
 *
 *        Revision 2.1  2010/01              moffit
 *           - Added Linux compatibility
 *
 */


/* Include Files */
#ifdef VXWORKS
#include <vxWorks.h>
#include <logLib.h>
#else
#include <stdlib.h>
#include "jvme.h"
#endif
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "tsUtil.h"

/* Global Definitions  */
volatile struct vme_ts2 *tsP = 0;
volatile unsigned int *tsMemP = 0;


/* external definitions */
#ifdef VXWORKS
extern STATUS sysBusToLocalAdrs();
#endif

/* Trigger Supervisor Macros */
#define TSROC(b4,b3,b2,b1)      {vmeWrite32(&(tsP->roc), (b4<<24)|(b3<<16)|(b2<<8)|(b1));}

#define TSENABLED              (vmeRead32(&(tsP->csr))&TS_CSR_GO)
#define L1ENABLED             (vmeRead32(&(tsP->csr))&TS_CSR_ENABLE_L1)

#define TSPRESCALE(p,c)        {vmeWrite32(&(tsP->prescale[p]), c);}
#define TSTIMER(t,c)           {vmeWrite32(&(tsP->timer[t]), c);}


pthread_mutex_t   tsMutex = PTHREAD_MUTEX_INITIALIZER;
#define TSLOCK     if(pthread_mutex_lock(&tsMutex)<0) perror("pthread_mutex_lock");
#define TSUNLOCK   if(pthread_mutex_unlock(&tsMutex)<0) perror("pthread_mutex_unlock");

/* Local ROC - Branch 5 Routines */
#include "tsIntUtil.c"

/*******************************************************************
 *   Function : tsInit
 *
 *   Function : Initialize Trigger Supervisor
 *
 *   Parameters :  int laddr - Local Address of TS
 *                             if 0 then use default
 *                 int iflag - if non-zero then do not
 *                             initialize TS and Memory.
 *                                0 - Initialize TS and MLU
 *                                1 - No Initialization of MLU
 *                               >1 - No Initialization of TS or MLU
 *
 *   returns -1 if error, 0 if ok.
 *
 *******************************************************************/

int
tsInit (unsigned int addr, int iflag)
{
  unsigned long laddr;
  unsigned int tmpadr, rdata;
  int res;

  /* Check for valid address */
  if(addr==0) {
    tmpadr = TS_BASE_ADDR;
#ifdef VXWORKS
    res = sysBusToLocalAdrs(0x39,(char *)tmpadr,(char **)&laddr);
    if (res != 0) {
      printf("tsInit: ERROR in sysBusToLocalAdrs(0x39,0x%x,&laddr) \n",tmpadr);
      return(ERROR);
    }
#else
    res = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)tmpadr,(char **)&laddr);
    if (res != 0) {
      printf("tsInit: ERROR in vmeBusToLocalAdrs(0x39,0x%x,&laddr) \n",tmpadr);
      return(ERROR);
    }
#endif
  }else if(addr > 0x00ffffff) { /* A32 Addressing */
    printf("tsInit: ERROR: A32 Addressing not supported for TS (use A24 address only)\n");
    return(ERROR);
  }else{                       /* A24 Addressing */
    /* get the CPU Based TS address */
#ifdef VXWORKS
    res = sysBusToLocalAdrs(0x39,(char *)addr,(char **)&laddr);
    if (res != 0) {
      printf("tsInit: ERROR in sysBusToLocalAdrs(0x39,0x%x,&laddr) \n",addr);
      return(ERROR);
    }
#else
    res = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)addr,(char **)&laddr);
    if (res != 0) {
      printf("tsInit: ERROR in vmeBusToLocalAdrs(0x39,0x%x,&laddr) \n",addr);
      return(ERROR);
    }
#endif
  }

#ifdef VXWORKS
  printf("tsInit: Checking for Trigger Supervisor at address (0x%x)\n",laddr);
#else
  printf("tsInit: Checking for Trigger Supervisor at VME (USER) address 0x%x (0x%lx)\n",
	 addr,laddr);
#endif
  tsP = (struct vme_ts2 *)(laddr);
  /* Check if Board exists at that address */
#ifdef VXWORKS
  res = vxMemProbe((char *) &(tsP->id),VX_READ,4,(char *)&rdata);
#else
  res = vmeMemProbe((char *) &(tsP->id),4,(char *)&rdata);
#endif
  if(res < 0) {
    printf("tsInit: ERROR: No addressable board at addr=0x%lx\n",(unsigned long) tsP);
    return(ERROR);
  } else {
    /* Check that it is an TS board */
    if((rdata&TS_ID_MASK) != TS_BOARD_ID) {
      printf("tsInit: ERROR: Invalid Board ID: 0x%x\n",rdata);
      return(ERROR);
    }
  }

  /* Setup pointer to Trigger Supervisor Memory Lookup table */
  tsMemP = (unsigned int *)((unsigned long)tsP + TS_MEM_OFFSET);
  printf("tsInit: Trigger Supervisor MLU Pointer: tsMemP = 0x%lx\n",
	 (unsigned long)tsMemP);

  /* Initialize Trigger Supervisor if flag is not set */
  if(iflag == 0) {
    vmeWrite32(&(tsP->csr),TS_CSR_INIT);
    printf("tsInit: Trigger Supervisor -- Initialized\n");
    tsMemInit();
  } else if(iflag == 1) {
    vmeWrite32(&(tsP->csr),TS_CSR_INIT);
    printf("tsInit: WARN: Trigger Supervisor MLU -- Not Initalized\n");
  }else if(iflag >= 2) {
    printf("tsInit: WARN: Trigger Supervisor and MLU -- Not Initalized\n");
  }

  return(OK);
}


/*********************************************************************
 *   Function : tsMemInit
 *
 *   Function : Initialize Trigger Supervisor Memory to it Default
 *              configuration:
 *                     For any single trigger input 1-12 latched the
 *                     corresponding trigger type is 1-12. For any
 *                     multiple trigger input latch the trigger type
 *                     will be 14. Trigger types 0,13,15 are not used.
 *                     For any trigger all level 1 Accept outputs fire
 *                     and all triggers are defined as class 1.
 *
 *   Parameters :  None
 *
 *
 *   returns -1 if error, 0 if ok.
 *
 *********************************************************************/
int
tsMemInit ()
{
  unsigned int laddr, mem_value;
  unsigned int *ts_memory = NULL;
  int jj;
  int tserror=0;

  if(tsMemP == 0) {
    printf("tsMemInit: ERROR : Trigger Supervisor MLU Pointer not Initialized\n");
    return(-1);
  }

  TSLOCK;
  if(TSENABLED) {
    printf("tsMemInit: ERROR : Cannot Write to MLU while TS is enabled\n");
    TSUNLOCK;
    return(-1);
  }

  ts_memory = (unsigned int *) malloc(4096*4);
  if(ts_memory == NULL) {
    printf("tsMemInit: ERROR : cannot malloc array to store MLU Programming\n");
    return(-1);
  } else {
    bzero((char *)ts_memory,(4096*4));
  }

  for( laddr = 1; laddr <= 4095;  laddr++ )    /* Assign default data to all memory addresses  */
    ts_memory[laddr] = 0xEFF03;             /* All L1A fire, Class 1, Type = 14             */

  jj = 0;
  for( laddr = 1; laddr <= 4095;  laddr = 2*laddr )   /*  Fix ROC code for single hit patterns  */
    {
      jj++;
      ts_memory[laddr] = 0xFF03 + (0x10000)*jj;  /* Trigger type = input number */
    }

  /* Load and readback TS memory */
  for( laddr = 0; laddr <= 4095;  laddr++ )
    {
      vmeWrite32(&(tsMemP[laddr]),ts_memory[laddr]);
      mem_value = vmeRead32(&(tsMemP[laddr]));
      if( ts_memory[laddr] != ( TS_MEM_MASK & mem_value ) ) {
	printf("tsMemInit: ***** TS memory error ***** Program Value = 0x%x, Read Back = 0x%x\n",
	       ts_memory[laddr],(TS_MEM_MASK & mem_value));
	tserror++;
      }
    }
  TSUNLOCK;

  free(ts_memory);

  if(tserror) {
    return(-1);
  }else{
    printf("tsMemInit: Trigger Supervisor Memory Lookup Table -- Initialized\n");
    return(OK);
  }
}


/*********************************************************************
 *   Function : tsLive
 *
 *   Function   : Calc Live time from TS Live scalers
 *   Parameters : sflag if > 0 then return integrated live time
 *
 *   Returns    : live time as a 3 digit ineger %  (eg  987 = 98.7%)
 *
 *********************************************************************/

static unsigned int oldLive1, oldLive2;

int
tsLive(int sflag)
{
  float lt=0.0;
  int ilt=0;
  unsigned int val1, val2, newLive1, newLive2;

  if(tsP == 0) {
    logMsg("tsLive: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  TSLOCK;
  /* 2 scalers are latched upon reading the first */
  newLive1 = vmeRead32(&(tsP->scalLive[0]));
  newLive2 = vmeRead32(&(tsP->scalLive[1]));

  /* Get Differential live time */
  if((sflag==0)&&(oldLive2<newLive2)) {
    val1 = newLive1 - oldLive1;
    val2 = newLive2 - oldLive2;
  }else{ /* Integrated */
    val1 = newLive1;
    val2 = newLive2;
  }
  oldLive1 = newLive1;
  oldLive2 = newLive2;
  TSUNLOCK;

  if (val2 >0)
    lt = 1000*(((float) val1)/((float) val2));

  ilt = (int) lt;

  return(ilt);
}

void
tsLiveClear()
{

  if(tsP == 0) {
    logMsg("tsLiveClear: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }

  /* Clear Live time scalers */
  TSLOCK;
  vmeWrite32(&(tsP->scalControl),0x80000);
  TSUNLOCK;
}


/*********************************************************************
 *   Function : tsCsr
 *
 *   Function   : Write to TS CSR Register
 *   Parameters : Val - value to write into CSR
 *                      if 0 then readback CSR only
 *
 *   Returns    : Readback of CSR register
 *
 *********************************************************************/
unsigned int
tsCsr(unsigned int Val)
{
  unsigned int csr=0;

  if(tsP == 0) {
    logMsg("tsCsr: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  TSLOCK;
  if(Val) {
    vmeWrite32(&(tsP->csr), Val);
  }

  csr = vmeRead32(&(tsP->csr))&TS_CSR_MASK;
  TSUNLOCK;

  return(csr);
}

unsigned int
tsCsr2Set(unsigned int cval)
{
  unsigned int val;

  if(tsP == 0) {
    logMsg("tsCsr2Set: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  TSLOCK;
  vmeWrite32(&(tsP->csr2),
	  vmeRead32(&(tsP->csr2)) | (cval&TS_CSR2_MASK));
  val = vmeRead32(&(tsP->csr2));
  TSUNLOCK;

  return(val);
}

unsigned int
tsCsr2Clear(unsigned int cval)
{
  unsigned int val;

  if(tsP == 0) {
    logMsg("tsCsr2Clear: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  TSLOCK;
  vmeWrite32(&(tsP->csr2),
	  vmeRead32(&(tsP->csr2)) & ~(cval&TS_CSR2_MASK));
  val = vmeRead32(&(tsP->csr2));
  TSUNLOCK;

  return(val);
}

void
tsClearStatus()
{

  if(tsP == 0) {
    logMsg("tsClearStatus: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }

  TSLOCK;
  vmeWrite32(&(tsP->csr), TS_CSR_CLEAR_STATUS);
  TSUNLOCK;

}



/*********************************************************************
 *   Function : tsEnableInput
 *
 *   Function   : Write to TS TRIG Register
 *   Parameters : Val - bitmask of inputs to enable 0-11
 *                      bit 0 (Non-strobe) is set automatically
 *                tflag - 4 bit flag for other options
 *                        0000 - default - NonCommon Strobe mode
 *                        0001 - Common Strobe req
 *                        0010 - No Pulse Regen Input  9-10
 *                        0100 - No Pulse Regen Input 11-12
 *                        1000 - Open Prescales
 *
 *   Returns    : Readback of TRIG register
 *
 *********************************************************************/
unsigned int
tsEnableInput(unsigned int Val, int tflag)
{
  unsigned int trigval=0;
  unsigned int readval=0;

  if(tsP == 0) {
    logMsg("tsEnableInput: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return ERROR;
  }

  trigval = (Val&0xfff)<<1;
  if((tflag&0x1)==0) trigval |= 0x0001;
  trigval |= ((tflag&0xe)<<12); /* write Upper bits */

  TSLOCK;
  vmeWrite32(&(tsP->trig), trigval);
  readval = vmeRead32(&(tsP->trig))&TS_TRIG_MASK;
  TSUNLOCK;

  return(readval);
}

/*********************************************************************
 *   Function : tsRoc
 *
 *   Function   : Write to TS ROC enable Register
 *   Parameters : b4,b3,b2,b1 - bitmask of enabled ROCs for each branch
 *
 *   Returns    : Readback of ROC register
 *
 *********************************************************************/
unsigned int
tsRoc(unsigned char b4,unsigned char b3,unsigned char b2,unsigned char b1)
{
  unsigned int rocval=0;

  if(tsP == 0) {
    logMsg("tsRoc: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  TSLOCK;
  TSROC(b4,b3,b2,b1);
  rocval = vmeRead32(&(tsP->roc))&TS_ROC_MASK;
  TSUNLOCK;

  return(rocval);
}

/*********************************************************************
 *   Function : tsSync
 *
 *   Function   : Write to TS SYNC Register
 *   Parameters : Val - Value of syncronization event interval
 *
 *   Returns    : Readback of SYNC register
 *
 *********************************************************************/
unsigned int
tsSync(unsigned int Val)
{
  unsigned int sval=0;
  unsigned int syncval=0;

  if(tsP == 0) {
    logMsg("tsSync: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return ERROR;
  }

  sval = (Val > TS_SYNC_MASK) ? TS_SYNC_MASK : Val;

  TSLOCK;
  if (sval>0)
    vmeWrite32(&(tsP->sync), (sval-1)); /* Sync Interval = register val + 1 */

  syncval = vmeRead32(&(tsP->sync))&TS_SYNC_MASK;
  TSUNLOCK;

  return(syncval);
}


/*********************************************************************
 *   Function : tsPrescale
 *
 *   Function   : Write to TS Prescale Registers
 *   Parameters : Num - number of prescale input to set 1-8
 *                Val - Value to set the prescale register
 *                      if 0 then readback PRESCALE only
 *
 *   Returns    : Readback of PRESCALE register
 *
 *********************************************************************/
unsigned int
tsPrescale(int Num, unsigned int Val)
{
  int pnum;
  unsigned int pval,pmask,readback;


  if(tsP == 0) {
    logMsg("tsPrescale: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  if((Num<=0)||(Num>12)) {
    logMsg("tsPrescale: ERROR : Invalid Input number %d  (1-12 only)\n",Num,0,0,0,0,0);
    return(0);
  }
  if((Num>8)&&(Num<=12)) {
    logMsg("tsPrescale: WARN : Input %d not Prescalable (set to 1)\n",Num,0,0,0,0,0);
    return(1);
  }

  pnum  = Num-1;
  pmask = (Num < 5) ? TS_PRESCALE1_4MASK : TS_PRESCALE5_8MASK;
  pval  = (Val >= (pmask+1)) ? pmask : (Val-1);

  TSLOCK;
  if(Val > 0) TSPRESCALE(pnum,pval);   /* Only Write to Prescale register if Val > 0 */
  readback = (vmeRead32(&(tsP->prescale[pnum]))&pmask)+1;
  TSUNLOCK;

  return(readback);
}

/*********************************************************************
 *   Function : tsTimerWrite
 *
 *   Function   : Write to TS Timer Registers
 *   Parameters : Num - number of timer to set 1-5
 *                Val - timer value to set
 *
 *   Returns    : Readback of Timer register
 *
 *********************************************************************/
unsigned int
tsTimerWrite(int Num, unsigned int Val)
{
  int tnum;
  unsigned int tval,tmask,readback;

  if(tsP == 0) {
    logMsg("tsTimerWrite: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  if((Num<=0)||(Num>5)) {
    logMsg("tsTimerWrite: ERROR : Timer Number %d not Valid (1-5 only)\n",Num,0,0,0,0,0);
    return(0);
  }

  tnum  = Num-1;
  tmask = (Num < 5) ? TS_TIMER1_4MASK : TS_TIMER5_MASK;
  tval  = (Val >= tmask) ? tmask : Val;

  TSLOCK;
  TSTIMER(tnum,tval);
  readback = vmeRead32(&(tsP->timer[tnum]))&tmask;
  TSUNLOCK;

  return(readback);
}

/*********************************************************************
 *   Function : tsMemWrite
 *
 *   Function   : Write to TS Memory Lookup table
 *   Parameters : Num - memory address 0-4095
 *                Val - Value to write into memory
 *
 *   Returns    : Readback of Memory register specified
 *
 *********************************************************************/
unsigned int
tsMemWrite(int Num, unsigned int Val)
{
  unsigned int readback=0;

  if(tsP == 0) {
    logMsg("tsMemWrite: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(0);
  }

  TSLOCK;
  if(TSENABLED) {
    logMsg("tsMemWrite: ERROR : Cannot Access MLU while TS is enabled\n",0,0,0,0,0,0);
    TSUNLOCK;
    return(0);
  }
  if((Num<0)||(Num>4095)) {
    logMsg("tsMemWrite: ERROR : TS MLU address %d not Valid\n",Num,0,0,0,0,0);
    return(0);
  }

  vmeWrite32(&(tsMemP[Num]), Val);
  readback = vmeRead32(&(tsMemP[Num]))&TS_MEM_MASK;
  TSUNLOCK;

  return(readback);
}

/*********************************************************************
 *   Functions : tsGo , tsStop, tsReset
 *
 *   Function   : Utilities to Enable, Disable and Initialize the TS
 *   Parameters : iflag -
 *                      for tsGo if 1 then set L1 Enable bit as well
 *                      for tsStop if 1 then clear L1 Enable bit as well
 *                      for tsInit if 1 then Reset only (No Initialize)
 *
 *   Returns    : None
 *
 *********************************************************************/
void
tsGo(int iflag)
{

  if(tsP == 0) {
    logMsg("tsGo: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }

  TSLOCK;
  if(iflag)
    vmeWrite32(&(tsP->csr), (TS_CSR_GO | TS_CSR_ENABLE_L1) );  /* Enable Go and L1 Hardware */
  else
    vmeWrite32(&(tsP->csr), TS_CSR_GO);  /* Enable Go only */
  TSUNLOCK;

  return;
}

void
tsStop(int iflag)
{

  if(tsP == 0) {
    logMsg("tsStop: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }

  TSLOCK;
  if(iflag)
    vmeWrite32(&(tsP->csr), ((TS_CSR_GO | TS_CSR_ENABLE_L1)<<16) ); /* Disable Go and L1 Hardware */
  else
    vmeWrite32(&(tsP->csr), (TS_CSR_GO<<16));  /* Disable Go only */
  TSUNLOCK;

  return;
}

void
tsReset(int iflag)
{

  if(tsP == 0) {
    logMsg("tsReset: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }

  TSLOCK;
  if(iflag)
    vmeWrite32(&(tsP->csr), TS_CSR_RESET); /* Reset Latched status only*/
  else
    vmeWrite32(&(tsP->csr), TS_CSR_INIT);  /* Initialize TS to PowerUP*/
  TSUNLOCK;

  return;
}


/*********************************************************************
 *   Function : tsStatus
 *
 *   Function   : General status info on TS
 *   Parameters : iflag - General level of retruned status info
 *                      = 0 then return value of CSR registers only
 *                      > 0 then print general CSR info
 *
 *   Returns    : Readback of CSR1 and CSR2  Registers
 *
 *********************************************************************/
unsigned int
tsStatus (int iflag)
{
  int lockmode[5];
  unsigned int csr1, csr2, state, roc;
  unsigned int retval = 0;

  if(tsP == 0) {
    printf("tsStatus: ERROR: TS Library Not Initialized -- Call tsInit() \n");
    return(0);
  }else{
    TSLOCK;
    csr1  = vmeRead32(&(tsP->csr));
    csr2  = vmeRead32(&(tsP->csr2));
    roc   = vmeRead32(&(tsP->roc));
    state = vmeRead32(&(tsP->state))&TS_STATE_MASK;
    TSUNLOCK;
  }

  if (iflag) {
    printf("*** Trigger Supervisor Status ***\n");
    if (csr1&TS_CSR_GO) {
      printf("  TS Go                : Enabled\n");
    }else{
      printf("  TS Go                : Disabled\n");
    }

    if (csr1&TS_CSR_ENABLE_L1) {
      printf("  TS Level 1 Hardware  : Enabled\n");
    }else{
      printf("  TS Level 1 Hardware  : Disabled\n");
    }

    if (csr2&TS_CSR2_ENABLE_SYNC) {
      printf("  TS Sync Events       : Enabled\n");
    }else{
      printf("  TS Sync Events       : Disabled\n");
    }

    lockmode[0] = (csr2&TS_CSR2_LOCK_BRANCH1)>>5;
    lockmode[1] = (csr2&TS_CSR2_LOCK_BRANCH2)>>6;
    lockmode[2] = (csr2&TS_CSR2_LOCK_BRANCH3)>>7;
    lockmode[3] = (csr2&TS_CSR2_LOCK_BRANCH4)>>8;
    lockmode[4] = (csr2&TS_CSR2_LOCK_BRANCH5)>>9;

    printf("  ROC LOCK Branches    : %d,%d,%d,%d,%d\n",
	   lockmode[0],lockmode[1],lockmode[2],lockmode[3],lockmode[4]);

    printf("  ROC Enable Register  : 0x%08x\n",roc);

    if (state&TS_STATE_LATCHED) {
      printf("  TS STATE Register    : 0x%08x (Trigger Latched)\n",state);
    }else{
      printf("  TS STATE Register    : 0x%08x \n",state);
    }

    printf("  Latched Status Bits (csr1 = 0x%08x):\n",(csr1&TS_CSR_MASK));
    if(csr1&TS_CSR_SYNC_OCCURED)      printf("     Sync Event       occurred\n");
    if(csr1&TS_CSR_P1_EVENT_OCCURED)  printf("     Prog 1 Event     occurred\n");
    if(csr1&TS_CSR_P2_EVENT_OCCURED)  printf("     Prog 2 Event     occurred\n");
    if(csr1&TS_CSR_LATE_FAIL_OCCURED) printf("     Late Fail        occurred\n");
    if(csr1&TS_CSR_INHIBIT_OCCURED)   printf("     Inhibit          occurred\n");
    if(csr1&TS_CSR_WFIFO_ERR_OCCURED) printf("     Write FIFO Error occurred\n");
    if(csr1&TS_CSR_RFIFO_ERR_OCCURED) printf("     Read FIFO Error  occurred\n");


    printf("\n");
  }

  retval = (csr2<<16)|(csr1&0xffff);

  return(retval);
}

/*********************************************************************
 *   Function : tsTimerStatus
 *
 *   Function   : Get status of TS timers
 *   Parameters : none
 *
 *   Returns    : none
 *
 *********************************************************************/
void
tsTimerStatus()
{
  int ii;
  unsigned int csr2;
  unsigned int tval[5];

  if(tsP == 0) {
    logMsg("%s: ERROR: TS Library Not Initialized\n",__FUNCTION__,0,0,0,0,0);
    return;
  }

  TSLOCK;
  csr2 = vmeRead32(&(tsP->csr2))&TS_CSR2_MASK;

  for(ii=0;ii<4;ii++) {
    tval[ii] = ((vmeRead32(&(tsP->timer[ii]))&TS_TIMER1_4MASK)*TS_TIMER_RES);
  }
  tval[4] = ((vmeRead32(&(tsP->timer[4]))&TS_TIMER5_MASK)*TS_TIMER_RES);
  TSUNLOCK;

  printf("\n");
  printf("  *** Trigger Supervisor Timers ***\n\n");

  printf("     Level 2          : Enabled    = %d ns\n",tval[1]);

  printf("     Level 3          : Enabled    = %d ns\n",tval[2]);

  if (csr2&TS_CSR2_ENABLE_FB_TIMER)
    printf("     Front-End Busy   : Enabled    = %d ns\n",tval[3]);
  else
    printf("     Front-End Busy   : Disabled   = %d ns\n",tval[3]);

  if (csr2&TS_CSR2_ENABLE_CP_TIMER)
    printf("     Clear Permit     : Enabled    = %d ns\n",tval[0]);
  else
    printf("     Clear Permit     : Disabled   = %d ns\n",tval[0]);

  if (csr2&TS_CSR2_ENABLE_CH_TIMER)
    printf("     Clear Hold       : Enabled    = %d ns\n",tval[4]);
  else
    printf("     Clear Hold       : Disabled   = %d ns\n",tval[4]);

}


/*********************************************************************
 *   Function : tsState
 *
 *   Function   : Current TS state info
 *   Parameters : iflag - General level of putput returned
 *                      = 0 state Register value only
 *                      > 0 print out useful diagnostics of state info
 *
 *   Returns    : Readback of STATE Register
 *
 *********************************************************************/
unsigned int
tsState(int iflag)
{
  unsigned int csr1, csr2, sval, roc, bufS, lbufS, ackS;
  struct ts_state tss;
  int ii, jj, kk;

  if(tsP == 0) {
    printf("tsState: ERROR: TS Library Not Initialized -- Call tsInit() \n");
    return(0);
  }else{
    TSLOCK;
    csr1  = vmeRead32(&(tsP->csr))&TS_CSR_MASK;
    csr2  = vmeRead32(&(tsP->csr2))&TS_CSR2_MASK;
    roc   = vmeRead32(&(tsP->roc))&TS_ROC_MASK;
    sval  = vmeRead32(&(tsP->state))&TS_STATE_MASK;
    bufS  = vmeRead32(&(tsP->rocBufStatus))&TS_ROC_STATUS_MASK;
    lbufS = vmeRead32(&(tsP->lrocBufStatus))&TS_LROC_STATUS_MASK;
    ackS  = vmeRead32(&(tsP->rocAckStatus))&TS_ACK_STATUS_MASK;
    TSUNLOCK;
  }

  if(csr1&TS_CSR_GO) {
    strncpy((char *) &tss.go[0],"Enabled \0",9);
  }else{
    strncpy((char *) &tss.go[0],"Disabled\0",9);
  }
  tss.l1e = (csr1&TS_CSR_ENABLE_L1) ? 1 : 0;

  /* Count # of ROCs per Branch */
  for (ii=0; ii<4; ii++) {
    kk = 0;
    for(jj=0; jj<8; jj++) {
      if(roc&(1<<(jj+(ii*8))))
	kk++;
    }
    tss.branch[ii] = kk;
    tss.strobe[ii] = ((bufS>>(ii*8))&0xf);
    if(tss.branch[ii] > 0) {
      tss.ack[ii] = (ackS>>(ii*8))&0xff;
    }else{
      tss.ack[ii] = 0;
    }
  }
  tss.branch[4] = (csr2&TS_CSR2_ENABLE_LROC) ? 1 : 0;
  tss.strobe[4] = (lbufS&0x8000)>>15;
  tss.ack[4]    = (lbufS&0x100)>>8;

  tss.feb     = (sval&TS_STATE_FE_BUSY) ? 1 : 0;
  tss.inhibit = (sval&TS_STATE_INHIBIT) ? 1 : 0;
  tss.clear   = (sval&TS_STATE_CLEAR) ? 1 : 0;
  tss.sync    = (sval&TS_STATE_SYNC_SEQ_ACTIVE) ? 1 : 0;
  tss.ready   = (sval&TS_STATE_READY) ? 1 : 0;
  tss.busy    = (sval&TS_STATE_BUSY) ? 1 : 0;
  tss.seq     = (sval&TS_STATE_MAIN_SEQ_ACTIVE) ? 1 : 0;

  if (tss.ready) {
    strncpy((char *) &tss.trigger[0],"Ready   \0",9);
    tss.feb = 0;  /* Front End Busy is Invalid when no Trigger is latched */
  }else if (sval&TS_STATE_LATCHED) {
    strncpy((char *) &tss.trigger[0],"Latched \0",9);
  }else{
    strncpy((char *) &tss.trigger[0],"Disabled\0",9);
  }


  /* Print out info in Useful format */
  if (iflag) {

    printf("\n      *******  Trigger Supervisor - Current State information  *******\n\n");
    printf("            TS Go  : %9s          TS Busy: %d\n",tss.go,tss.busy);
    printf("            TRIGGER: %9s          TS Sync: %d\n",tss.trigger,tss.sync);

    printf("\n            Front Panel:   Front-End Busy    : %d\n",tss.feb);
    printf("                           External Inhibit  : %d\n",tss.inhibit);
    printf("                           Enable L1 Hardware: %d\n",tss.l1e);
    printf("                           Clear             : %d\n",tss.clear);

    printf("\n      ******* Branch Status ********\n");
    printf("            Branch    # ROCs    Strobe    ACK( 8 bit mask)\n");
    printf("            ------    ------    ------    -----------------\n");
    for (ii=0;ii<4;ii++) {
      printf("                %d        %d        %d         0x%02x\n",
	     (ii+1),tss.branch[ii],tss.strobe[ii],tss.ack[ii]);
    }
    ii=4;
    printf("                %d        %d        %d           %d\n",
	   (ii+1),tss.branch[ii],tss.strobe[ii],tss.ack[ii]);
    printf("\n");
  }

  return(sval);
}

/*********************************************************************
 *   Function : tsFifoRead
 *
 *   Function   : Read TS Trigger Fifo
 *   Parameters : iflag (if >0  check empty - return error)
 *
 *   Returns    : oldest latched trigger word in FIFO
 *
 *********************************************************************/
unsigned int
tsFifoRead(int rflag)
{
  unsigned int wc = 0;
  unsigned int rval = 0;

  if(tsP == 0) {
    logMsg("tsFifoRead: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(ERROR);
  }

  TSLOCK;
  if (rflag)
    {                           /* check word count first*/
      wc = vmeRead32(&(tsP->trigCount))&TS_TRIG_COUNT_MASK;
      if(wc>0)
	rval = vmeRead32(&(tsP->trigData));
      else
	{
	  TSUNLOCK;
	  return(ERROR);
	}
    }
  else
    rval = vmeRead32(&(tsP->trigData));

  TSUNLOCK;

  return((rval&TS_TRIG_DATA_MASK));
}
/* Get word count in the trigger fifo */
unsigned int
tsFifoCount()
{

  unsigned int wc=0;

  if(tsP == 0) {
    logMsg("tsFifoCount: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(ERROR);
  }

  TSLOCK;
  wc = vmeRead32(&(tsP->trigCount))&TS_TRIG_COUNT_MASK;
  TSUNLOCK;

  return(wc);
}




/*********************************************************************
 *   Function : tsScalRead
 *
 *   Function   : Read/Clear TS Scalers
 *   Parameters : Num - Scaler #  (0 defines Event Scaler)
 *                sflag = if > 0 then Clear scaler after read
 *
 *   Returns    : Current Value of Scaler specified
 *
 *********************************************************************/
unsigned int
tsScalRead(int Num, int sflag)
{

  int ii=0;
  unsigned int rval = 0;

  if(tsP == 0) {
    logMsg("tsScalRead: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(ERROR);
  }

  if((Num<0)||(Num>18)) {
    logMsg("tsScalRead: ERROR : TS Scaler ID %d not Valid\n",Num,0,0,0,0,0);
    return(ERROR);
  }
  if(Num>0) ii = Num-1;  /* Define index into array */

  TSLOCK;
  if(Num == TS_SCALER_EVENT)
    rval = vmeRead32(&(tsP->scalEvent));
  else
    rval = vmeRead32(&(tsP->scaler[ii]));

  if (sflag) {                           /* clear the specified scaler */
    if (Num == TS_SCALER_EVENT)
      vmeWrite32(&(tsP->scalControl), TS_SCALER_EVENT_ID);
    else
      vmeWrite32(&(tsP->scalControl),(1<<ii));
  }
  TSUNLOCK;

  return(rval);
}

unsigned int
tsScalAssign(unsigned short u13, unsigned short u14, unsigned short u15,
	     unsigned short u16, unsigned short u17, unsigned short u18)
{

  unsigned int wval, rval;

  if(tsP == 0) {
    logMsg("tsScalAssign: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return(ERROR);
  }

  wval = ((u18&0xf)<<20)|((u17&0xf)<<16)|((u16&0xf)<<12)|
    ((u15&0xf)<<8)|((u14&0xf)<<4)|((u13&0xf));

  TSLOCK;
  vmeWrite32(&(tsP->scalAssign), wval);
  rval = vmeRead32(&(tsP->scalAssign))&TS_SCAL_ASSIGN_MASK;
  TSUNLOCK;

  return(rval);

}

void
tsScalClear(unsigned int mask)
{

  if(tsP == 0) {
    logMsg("tsScalClear: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }
  TSLOCK;
  vmeWrite32(&(tsP->scalControl), (mask&TS_SCALER_CONTROL_MASK));
  TSUNLOCK;

  return;
}

void
tsScalLatch()
{

  if(tsP == 0) {
    logMsg("tsScalLatch: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }
  TSLOCK;
  vmeWrite32(&(tsP->scalControl), TS_SCALER_LATCH);
  TSUNLOCK;

  return;
}

void
tsScalUnLatch()
{

  if(tsP == 0) {
    logMsg("tsScalUnLatch: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }
  TSLOCK;
  vmeWrite32(&(tsP->scalControl), 0);
  TSUNLOCK;

  return;
}


void
tsScalClearAll()
{

  if(tsP == 0) {
    logMsg("tsScalClearAll: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }
  TSLOCK;
  vmeWrite32(&(tsP->scalControl), TS_SCALER_CONTROL_MASK);
  TSUNLOCK;

  return;
}

void
tsTestLevel1TriggerPulse()
{
  if(tsP == 0) {
    logMsg("tsTestLevel1TriggerPulse: ERROR: TS Library Not Initialized\n",0,0,0,0,0,0);
    return;
  }
  TSLOCK;
  vmeWrite32(&(tsP->test), TS_TEST_LEVEL1_TRIGGER_PULSE);
  TSUNLOCK;

  return;
}
