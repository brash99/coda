/* universeDma.c - Tundra Universe chip DMA Interface Library */

/* Copyright 1984-1997 Wind River Systems, Inc. */
/* Copyright 1996-1997 Motorola, Inc. */

/*
modification history
--------------------
01a,10mar97,rcp   written.
*/

#include "vxWorks.h"
#include "stdio.h"
#include "semaphore.h"
#include "semLib.h"
#include "logLib.h"
#include "stdlib.h"
#include "universe.h"
#include "universeDma.h"

/* Global Variables */
struct stat_struct univDmaStat;      /*  Error Status Structure */
SEM_ID univDmaSem;                   /*  Global DMA Semaphore */

/* Command packet structures for Link-List operation */
volatile struct univDma_ll ull[UNIV_DMA_MAX_LL];



/* External Functions */
IMPORT STATUS sysUnivIntConnect(int, VOIDFUNCPTR, int);
IMPORT STATUS sysUnivIntEnable(int);
IMPORT STATUS sysUnivIntDisable(int);
IMPORT STATUS intDisconnect(int);

/*  Added a new utility to display the current value of all DMA registers */
void
sysVmeDmaShow ()
{

    printf ("CURRENT STATE OF UNIVERSE DMA REGISTERS\n");
    printf ("   DCTL   => 0x%x\n", LONGSWAP (*UNIVERSE_DCTL));
    printf ("   DTBC   => 0x%x\n", LONGSWAP (*UNIVERSE_DTBC));
    printf ("   DLA    => 0x%x\n", LONGSWAP (*UNIVERSE_DLA));
    printf ("   DVA    => 0x%x\n", LONGSWAP (*UNIVERSE_DVA));
    printf ("   DCPP   => 0x%x\n", LONGSWAP (*UNIVERSE_DCPP));
    printf ("   DGCS   => 0x%x\n", LONGSWAP (*UNIVERSE_DGCS));
    printf (" D_LLUE   => 0x%x\n", LONGSWAP (*UNIVERSE_D_LLUE));
    printf ("LINT_EN   => 0x%x\n", LONGSWAP (*UNIVERSE_LINT_EN));
    printf ("LINT_STAT => 0x%x\n", LONGSWAP (*UNIVERSE_LINT_STAT));
    printf ("LINT_MAP0 => 0x%x\n", LONGSWAP (*UNIVERSE_LINT_MAP0));
    printf ("LINT_MAP1 => 0x%x\n", LONGSWAP (*UNIVERSE_LINT_MAP1));

    printf("DMA Status=0x%x, Local Addr=0x%x, VME Addr=0x%x, Bytes Remaining=0x%x\n",
	   univDmaStat.finalStatus,
	   (int) univDmaStat.finalLadr,
	   (int) univDmaStat.finalVadr,
	   univDmaStat.finalCount);

}

/*  The next two variables are set while the DMA is occuring and
    contain the value for the next cycle */
UINT32   globalDCTL,         /*  The next value of the DCTL Register */
         globalDGCS;         /*  The next value of the DGCS Register */

/***************************************************************************
 *                                                                         *
 *  sysVmeDmaInt - Interrupt handler for the DMA channel on of the         *
 *  Universe Chip.  This will a very simplistic ISR for the current        *
 *  DMA interface.  This ISR only handles Direct DMA transactions for      *
 *  a single request at a time.  A semaphore shared between the system     *
 *  call which initiates the DMA transaction will be released so that any  *
 *  other pending DMAs can be continued.  If an error occurs, a global     *
 *  variable will be set with the type of error so that the application    *
 *  programmers can be notified of a problem with the previous DMA.  It    *
 *  could also be used to stop the next DMA transaction.                   *
 *                                                                         *
 ***************************************************************************/

void
sysVmeDmaInt (void)
{

UINT32 univTemp;

/*  Get the results of the DMA  */

   univTemp= LONGSWAP (*UNIVERSE_DGCS);
   if ((univTemp & DGCS_DONE) == DGCS_DONE)
      univDmaStat.finalStatus = 0;
   else
      univDmaStat.finalStatus = univTemp & DGCS_ERROR_MASK;

/*  If the DMA terminated with an error, save the addresses and byte count */

   if (univDmaStat.finalStatus) {
      univDmaStat.finalLadr = (char *)(LONGSWAP (*UNIVERSE_DLA));
      univDmaStat.finalVadr = (char *)(LONGSWAP (*UNIVERSE_DVA));
      univDmaStat.finalCount = LONGSWAP (*UNIVERSE_DTBC);
      logMsg("ERROR: sysVmeDmaInt: DMA Status=0x%x, Local Addr=0x%x, VME Addr=0x%x, Bytes Remaining=0x%x\n",
	     univDmaStat.finalStatus,
	     (int) univDmaStat.finalLadr,
	     (int) univDmaStat.finalVadr,
	     univDmaStat.finalCount,
	     0,
	     0);
   }

/*  Clear all interrupts  */

   *UNIVERSE_LINT_STAT |= LONGSWAP (LINT_STAT_DMA);
   *UNIVERSE_VINT_STAT |= LONGSWAP (VINT_STAT_DMA);

/* Release the DMA Semaphore */
   semGive (univDmaSem);

}


/**********************************************************************************
*
*  Check for DMA in an active state
*
*   Return:  0  : Idle
*            1  : Active
*/
int
sysVmeDmaActive()
{
  UINT32 univTemp=0;

  univTemp= LONGSWAP (*UNIVERSE_DGCS);

  if(univTemp & DGCS_ACT) {
    return(1);
  } else {
    return(0);
  }

}



/**********************************************************************************
*
*   Polling routine that returns when the DMA is Done or an error occurs
*
*      pcnt : Number of times to check status (~1microsec/count)
*
*      pflag: if >0 then return byte count (rather than status)
*
*/
int
sysVmeDmaDone (int pcnt, int pflag)
{

  int ii=0;
  UINT32 univTemp, dMask;

/*  Wait for the results of the DMA  Done or Error */
    dMask = DGCS_DONE | DGCS_LERR | DGCS_VERR | DGCS_P_ERR;

  /* Wait forever */
  if (pcnt <= 0) {

    while (1) {
      univTemp= LONGSWAP (*UNIVERSE_DGCS);
      if(univTemp & dMask) {
	if ((univTemp & DGCS_DONE) == DGCS_DONE) {
	  univDmaStat.finalStatus = 0;
	} else {
	  univDmaStat.finalStatus = univTemp & DGCS_ERROR_MASK;
	}
	break;
      }else{
	if(univTemp&DGCS_ACT) {
	  continue;
	}else{
	  logMsg("sysVmeDmaDone: ERROR: DMA is not Active (0x%08x)\n",univTemp,0,0,0,0,0);
	  return(ERROR);
	}
      }
    }

  } else {

    while ((ii<pcnt)) {
      univTemp= LONGSWAP (*UNIVERSE_DGCS);
      if(univTemp & dMask) {
	if ((univTemp & DGCS_DONE) == DGCS_DONE) {
	  univDmaStat.finalStatus = 0;
	} else {
	  univDmaStat.finalStatus = univTemp & DGCS_ERROR_MASK;
	}
	break;
      }else{
	/* Check if the DMA engine is even active */
	if(univTemp&DGCS_ACT) {
	  ii++;
	}else{
	  /* Ignore missing ACT bit, if Linked List bit is set */
	  if((univTemp & DGCS_CHAIN) == 0)
	    {
	      logMsg("sysVmeDmaDone: ERROR: DMA is not Active (0x%08x)\n",univTemp,0,0,0,0,0);
	      return(ERROR);
	    }
	}
      }
    }

  }

  /* If we timed out then something is wrong - reset - return an error */
  if((pcnt>0)&&(ii>=pcnt)) {
    logMsg("sysVmeDmaDone: timed out. Universe Status = 0x%x\n",univTemp,0,0,0,0,0);
    sysVmeDmaReset();
    return(ERROR);
  }


/*  If the DMA terminated with an error, save the addresses and byte count */

  switch (univDmaStat.finalStatus) {
  case 0:
    univDmaStat.finalLadr = (char *)(LONGSWAP (*UNIVERSE_DLA));
    univDmaStat.finalVadr = (char *)(LONGSWAP (*UNIVERSE_DVA));
    univDmaStat.finalCount = LONGSWAP (*UNIVERSE_DTBC);
    /*logMsg("sysVmeDmaDone: INFO: DMA Complete: poll count %d\n",ii,0,0,0,0,0); */
    break;
  case DGCS_VERR:
    univDmaStat.finalLadr = (char *)(LONGSWAP (*UNIVERSE_DLA));
    univDmaStat.finalVadr = (char *)(LONGSWAP (*UNIVERSE_DVA));
    univDmaStat.finalCount = LONGSWAP (*UNIVERSE_DTBC);
    if (pflag) {
      return ( (int) univDmaStat.finalCount);
    }else{
      logMsg("sysVmeDmaDone: WARN: DMA terminated by VMEBus Error\n",0,0,0,0,0,0);
      return( (int) univDmaStat.finalStatus);
    }
  default:
    univDmaStat.finalLadr = (char *)(LONGSWAP (*UNIVERSE_DLA));
    univDmaStat.finalVadr = (char *)(LONGSWAP (*UNIVERSE_DVA));
    univDmaStat.finalCount = LONGSWAP (*UNIVERSE_DTBC);
    logMsg("sysVmeDmaDone: ERROR: DMA Status=0x%x, Local Addr=0x%x, VME Addr=0x%x, Bytes Remaining=0x%x\n",
	   univDmaStat.finalStatus,
	   (int) univDmaStat.finalLadr,
	   (int) univDmaStat.finalVadr,
	   univDmaStat.finalCount,
	   0,
	   0);
    return(-1);
  }

  return(0);

}


/***************************************************************************
 *                                                                         *
 *  sysVmeDmaInit - System call which initializes the current state of     *
 *  all of the VME DMA registers and initial global symbols used in        *
 *  configuring and controlling the DMA cycles.                            *
 *                                                                         *
 *    iflag > 0  Does not enable Interrupts for DMA Done                   *
 *                                                                         *
 ***************************************************************************/

STATUS
sysVmeDmaInit (int iflag)
{
  char *univTemp;

/* Check to see if the DMA was previously created */

  if (univDmaSem > 0) {
    semDelete(univDmaSem);
    univDmaSem = 0;
    intDisconnect(UNIV_DMA_INT_VEC);
  }

/* Create the Global DMA Semaphore and store the value.  */

  univDmaSem = semBCreate (SEM_Q_FIFO, SEM_FULL);

/* Initialize the states of the DMA control registers  */

  univTemp = (char *)UNIVERSE_DCTL;
  globalDCTL = LONGSWAP (*UNIVERSE_DCTL);
  univTemp = (char *)UNIVERSE_DGCS;
  globalDGCS = LONGSWAP (*UNIVERSE_DGCS);

/* Enable the specific Interrupt of the DMA channel */

  if(iflag == 0) {
    globalDGCS |= DMA_INTERRUPTS;

/* Assign the sysVmeDmaInt to the PCI interrupt associated with the
   DMA Channel  */

    if (sysUnivIntConnect (UNIVERSE_DMA_INT, (VOIDFUNCPTR) sysVmeDmaInt, 0) == ERROR) {
      printf("Error in sysUnivIntConnect()\n");
      return (ERROR);
    }

  /* Enable the DMA interrupt for the Universe chip  */

    if (sysUnivIntEnable (UNIVERSE_DMA_INT) == ERROR) {
      printf("Error in sysUnivIntEnble()\n");
      return (ERROR);
    }
  }

  return(OK);
}


/***************************************************************************
 *                                                                         *
 *  sysVmeDmaGet - System call which gets the current state of the         *
 *  DMA configuration register.  This will be used with the                *
 *  sysVmeDmaSet to set characteristics of the DMA transfers.              *
 *                                                                         *
 ***************************************************************************/

STATUS
sysVmeDmaGet
   (
   UINT32   type,   /* type of parameter */
   UINT32   *pval   /* pointer to parameter */
   )
{

   STATUS retVal = 0;

/* Decode the desired DMA control parameter */

   switch (type) {

/*  Return the Total Value of the DCTL */
   case FRC_VME_DMA_LOCAL_DCTL:
      *pval=globalDCTL;
   break;
/*  Return the Total Value of the DGCS */
   case FRC_VME_DMA_LOCAL_DGCS:
      *pval=globalDGCS;
   break;
/*  PCI Local Mode (undefined) */
   case FRC_VME_DMA_LOCAL_MODE:
      retVal=ERROR;
   break;
/*  PCI Local Address Capabilities (undefined) */
   case FRC_VME_DMA_LOCAL_ACAP:
      retVal=ERROR;
   break;
/*  PCI Local Data Capabilities (undefined) */
   case FRC_VME_DMA_LOCAL_DCAP:
      retVal=ERROR;
   break;
/* PCI Local Bus Maximum Band Width (64bit) */
   case FRC_VME_DMA_LOCAL_BSIZE:
      *pval = (globalDCTL & DCTL_LD64EN)>>7;
   break;
/* VME Bus Mode Cycle Type */
   case FRC_VME_DMA_VME_MODE:
      retVal=ERROR;
   break;
/* VME Bus Maximum Block Size Transfered (VON)*/
   case FRC_VME_DMA_VME_BSIZE:
      *pval = (globalDGCS >> 20) & 0x0000000f;
   break;
/* VME Bus Time off Bus (VOFF)*/
   case FRC_VME_DMA_VME_DELAY:
      *pval = (globalDGCS >> 16) & 0x0000000f;
   break;
/* VME Bus Address Capability */
   case FRC_VME_DMA_VME_ACAP:
      *pval = (globalDCTL >> 16) & 0x00000003;
   break;
/* VME Bus Data Capability */
   case FRC_VME_DMA_VME_DCAP:
      switch ((globalDCTL >> 22) & 0x00000003) {
/*  Is it D8 Mode */
      case 0:
         *pval = DCAP_D8;
      break;
/*  Is it D16 Mode */
      case 1:
         *pval = DCAP_D16;
      break;
/*  Is it D32 Mode */
      case 2:
         if ((globalDCTL & DCTL_VCT_EN)>>8)
/*  BLT Mode */
            *pval = DCAP_BLT;
         else
/*  Not BLT Mode */
            *pval = DCAP_D32;
      break;
/*  Is it D64 Mode */
      case 3:
         if ((globalDCTL & DCTL_VCT_EN)>>8)
/*  BLT Mode */
            *pval = DCAP_MBLT;
         else
/*  Not BLT Mode */
            *pval = DCAP_D64;
      break;
   }
   break;

/* VME Bus User/Supervisor Capabilities */
   case FRC_VME_DMA_VME_UCAP:
      *pval = (globalDCTL >> 12) & 0x00000003;
   break;
/* VME Bus Program/Data Space Capabilities */
   case FRC_VME_DMA_VME_PCAP:
      *pval = (globalDCTL >> 14) & 0x00000003;
   break;
   default:
      retVal=ERROR;
   break;
   }
   return (retVal);
}


/***************************************************************************
 *                                                                         *
 *  sysVmeDmaSet - System call which sets the current state of the         *
 *  DMA configuration register.  This will be used with the                *
 *  sysVmeDmaGet to set characteristics of the DMA transfers.              *
 *                                                                         *
 ***************************************************************************/

STATUS
sysVmeDmaSet
   (
   UINT32   type,   /* type of parameter */
   UINT32   pval   /* pointer to parameter */
   )
{

   STATUS retVal = 0;

   switch (type) {

/*  Set the Total Value of the DCTL */
   case FRC_VME_DMA_LOCAL_DCTL:
      globalDCTL = pval;
   break;
/*  Set the Total Value of the DGCS */
   case FRC_VME_DMA_LOCAL_DGCS:
      globalDGCS = pval;
   break;
/*  PCI Local Mode (undefined) */
   case FRC_VME_DMA_LOCAL_MODE:
      retVal=ERROR;
   break;
/*  PCI Local Address Capabilities (undefined) */
   case FRC_VME_DMA_LOCAL_ACAP:
      retVal=ERROR;
   break;
/*  PCI Local Data Capabilities (undefined) */
   case FRC_VME_DMA_LOCAL_DCAP:
      retVal=ERROR;
   break;
/* PCI Local Bus Maximum Band Width (64bit) */
   case FRC_VME_DMA_LOCAL_BSIZE:
      if (pval == 0)
         globalDCTL = globalDCTL & DCTL_64EN_CLEAR;
      else
         globalDCTL |= DCTL_LD64EN;
   break;
/* VME Bus Mode Cycle Type */
   case FRC_VME_DMA_VME_MODE:
      retVal=ERROR;
   break;
/* VME Bus Maximum Block Size Transfered (VON)*/
   case FRC_VME_DMA_VME_BSIZE:
      globalDGCS = (globalDGCS & DGCS_VON_CLEAR) | (pval & 0x0000000f) << 20;
   break;
/* VME Bus Time off Bus (VOFF)*/
   case FRC_VME_DMA_VME_DELAY:
      globalDGCS = (globalDGCS & DGCS_VOFF_CLEAR) | (pval & 0x0000000f) << 16;
   break;
/* VME Bus Address Capability */
   case FRC_VME_DMA_VME_ACAP:
      if (pval > 2)
         retVal = ERROR;
      else
         globalDCTL = (globalDCTL & DCTL_VAS_CLEAR) |
                      (pval & 0x00000003) << 16;
   break;
/* VME Bus Data Capability */
   case FRC_VME_DMA_VME_DCAP:

/* Make sure the value is within range */
      if (pval >6)
         retVal=ERROR;
      else
      {
	 globalDCTL &= (DCTL_VDW_CLEAR & DCTL_VCT_CLEAR);
/*  Determine whether Block mode transfers are desired */
         switch (pval) {
         case 0:
         break;
/* Set the VMEbus Data Size to 8 bit */
         case DCAP_D8:
            globalDCTL |= DCTL_VDW_8;
         break;
/* Set the VMEbus Data Size to 16 bit */
         case DCAP_D16:
            globalDCTL |= DCTL_VDW_16;
         break;
/* Set the VMEbus Data Size to 32 bit */
         case DCAP_D32:
            globalDCTL |= DCTL_VDW_32;
         break;
/* Set the VMEbus to B32 Mode */
         case DCAP_BLT:
	   globalDCTL |= DCTL_VDW_32 | DCTL_VCT_EN; /* BLK Mode 32 */
         break;
/* Set the VMEbus to MBLT Mode */
         case DCAP_MBLT:
	   globalDCTL |= DCTL_VDW_64 | DCTL_VCT_EN; /* MBLT Mode 64 */
         break;
/* Set the VMEbus to D64 Mode */
         case DCAP_D64:
	   globalDCTL |= DCTL_VDW_64; /* D64 no MBLT */
         break;
/* Else return error */
      }
   }
   break;

/* VME Bus User/Supervisor Capabilities */
   case FRC_VME_DMA_VME_UCAP:
      globalDCTL = (globalDCTL & DCTL_SUPER_CLEAR) | (pval & 0x00000003) << 12;
   break;
/* VME Bus Program/Data Space Capabilities */
   case FRC_VME_DMA_VME_PCAP:
      globalDCTL = (globalDCTL & DCTL_PGM_CLEAR) | (pval & 0x00000003) << 14;
   break;
/* Collect all remaining cases */
   default:
      retVal=ERROR;
   break;
   }

   return (retVal);
}


/***************************************************************************
 *                                                                         *
 *  sysVmeDmaCopy - System call which checks the status of the global      *
 *  DMA semaphore and if it is free, callse sysVmeDmaSend to start the     *
 *  DMA transaction.                                                       *
 *                                                                         *
 ***************************************************************************/

STATUS
sysVmeDmaCopy
   (
    char *locAdrs,   /* PCI Bus Local Address */
    char *vmeAdrs,   /* VME Bus External Address */
    int size,       /* Size of the DMA Transfer */
    BOOL toVme   /* Direction of the DMA  */
   )
{
   STATUS  retVal;

/* Grab the DMA semaphore. If the Semaphore is unavailable, wait for the
   previous DMA cycle ot complete.  */

   semTake (univDmaSem, WAIT_FOREVER);
   if ((retVal=sysVmeDmaSend ((UINT32)locAdrs,
               (UINT32)vmeAdrs, size, toVme)) != OK)
   {
/*  If call falled, release the semaphore */
      semGive (univDmaSem);
      return (retVal);
   }

   return (OK);
}

/***************************************************************************
 *                                                                         *
 *  async_sysVmeDmaCopy - System call which checks the status of the       *
 *  global DMA semaphore and if it is free, calls sysVmeDmaSend to start   *
 *  the DMA transaction.  If it is not, the system call will return        *
 *  statu indicating the DMA is blocked.                                   *
 *                                                                         *
 ***************************************************************************/

STATUS
async_sysVmeDmaCopy
   (
    char *locAdrs,   /* PCI Bus Local Address */
    char *vmeAdrs,   /* VME Bus External Address */
    int size,       /* Size of the DMA Transfer */
    BOOL toVme    /* Direction of the DMA  */
   )
{

/* Grab the DMA semaphore. If the Semaphore is unavailable, return to the
   calling routing.  */

   if (semTake (univDmaSem, NO_WAIT) == ERROR)
      return (DMA_BUSY);

   return (sysVmeDmaSend ((UINT32)locAdrs, (UINT32)vmeAdrs, size, toVme));
}


/***************************************************************************
 *                                                                         *
 *  sysVmeDmaSend - System call which starts the DMA cycle.  The range of  *
 *  values passed into the subroutine will be check for the correct range. *
 *  If they are all within range, the DMA control registers will be set    *
 *  to start the DMA transaction.                                          *
 *                                                                         *
 ***************************************************************************/


STATUS
sysVmeDmaSend
   (
    UINT32 locAdrs,   /* PCI Bus Local Address */
    UINT32 vmeAdrs,   /* VME Bus External Address */
    int size,       /* Size of the DMA Transfer */
    BOOL toVme    /* Direction of the DMA  */
   )
{

/*  Evaluate the range of each argument */

/*  The local and vmebus address must be on an even 8 byte offset */
/*   if ((locAdrs % 8) > 0)
      return (DMA_LADR_ERROR);
   if ((vmeAdrs % 8) > 0)
      return (DMA_VADR_ERROR);
*/
/* Correction : they must be on the same byte boundary  DJA */
  if((locAdrs&7) != (vmeAdrs&7))
    return (DMA_VADR_ERROR);

/* Check the range of the count */
   if ((size>=0x1000000) || (size < 0))
      return ((STATUS)DMA_SIZE_ERROR);

/* Store the values of the DCTL */
   *UNIVERSE_DCTL = LONGSWAP (globalDCTL);

/* Set the direction of the transfer */
   if (toVme)
/* Set the direction bit for a Local to DMA Transfer */
      *UNIVERSE_DCTL = LONGSWAP(LONGSWAP(*UNIVERSE_DCTL) | DCTL_L2V);
   else
/* Reset the direction bit for a VME to Local DMA Transfer */
      *UNIVERSE_DCTL = LONGSWAP(LONGSWAP(*UNIVERSE_DCTL) & DCTL_L2V_CLEAR);

/* Initialize the DMA registers for the transaction */
   *UNIVERSE_DTBC = LONGSWAP(size);   /*  Load the DMA transfer size */
/*  Load the DMA transfer local address if */
/*  Check the range of the memory location.  If it is < 0x80000000,
    then the memory is DRAM which will be offset to the PCI bus range
    for the local RAM.  If not, then the literal address will be used
    assuming the developer is trying to do a DMA address to a device */
   if (locAdrs < 0x80000000) /*  Local DRAM, add offset */
      *UNIVERSE_DLA  = LONGSWAP(locAdrs+PCI2DRAM_BASE_ADRS);
   else
      *UNIVERSE_DLA  = LONGSWAP(locAdrs);

/*  Load the DMA transfer vmebus address  */
   *UNIVERSE_DVA  = LONGSWAP(vmeAdrs);

   /* Set the DGCS to start the DMA */
   globalDGCS = globalDGCS & ~DGCS_CHAIN; /* Direct mode */

   *UNIVERSE_DGCS = LONGSWAP (DGCS_RESET_STAT);
   *UNIVERSE_DGCS = LONGSWAP (globalDGCS | DGCS_GO );

   return (OK);
}


/*************************************************************************
*
* sysVmeDmaLLCreate - Setup a Linked List DMA Transfer
*
*     locAdrs - Array of Local (DRAM) addresses
*     vmeAdrs - Array of VME Addresses
*     size    - Array of Transfer Sizes (bytes)
*     numt    - Number of array entries
*     toVme   - Transfer directon 0 (from VMEBus) 1 (to VMEBus)
*
**************************************************************************/
STATUS
sysVmeDmaLLCreate(UINT32 *locAdrs,
                  UINT32 *vmeAdrs,
                  int    *size,    int numt, BOOL toVme)
{
  int ii;
  UINT32 temp1;

/* Store the values of the DCTL */
/* Set the direction of the transfer */
   if (toVme)
/* Set the direction bit for a Local to DMA Transfer */
      *UNIVERSE_DCTL = LONGSWAP(globalDCTL | DCTL_L2V);
   else
/* Reset the direction bit for a VME to Local DMA Transfer */
      *UNIVERSE_DCTL = LONGSWAP(globalDCTL & DCTL_L2V_CLEAR);

/* Set initial Command Packet pointer to start of linked list*/
   if ((UINT32)(&(ull[0].dctl)) < 0x80000000) /*  Local DRAM, add offset */
      *UNIVERSE_DCPP  = LONGSWAP((UINT32)(&(ull[0].dctl))+PCI2DRAM_BASE_ADRS);
   else
      *UNIVERSE_DCPP  = LONGSWAP((UINT32)(&(ull[0].dctl)));

/* Set DMA Transfer byte count to Zero. */
   *UNIVERSE_DTBC = LONGSWAP(0);


  if ((numt > 0)&&(numt <= UNIV_DMA_MAX_LL)) {

    for(ii=0; ii<numt ;ii++) {
      /* Check that source/desination addresses are on same byte boundary */
      if((locAdrs[ii]&7) != (vmeAdrs[ii]&7)) {
 	logMsg("sysVmeDmaLLCreate: ERROR: source/destination address: [%d] 0x%x 0x%x",
	       ii,locAdrs[ii],vmeAdrs[ii],0,0,0);
	return(ERROR);
      }

      /* Check the range of the count */
      if ((size[ii]>=0x1000000) && (size[ii] < 0)) {
 	logMsg("sysVmeDmaLLCreate: ERROR: Transfer size to large: [%d] %d\n",
	       ii,size[ii],0,0,0,0);
	return (ERROR);
      }

      ull[ii].dctl = LONGSWAP(*UNIVERSE_DCTL);
      ull[ii].dtbc = size[ii];
      if (locAdrs[ii] < 0x80000000) /*  Local DRAM, add offset */
	ull[ii].dla = (locAdrs[ii]+PCI2DRAM_BASE_ADRS);
      else
	ull[ii].dla = locAdrs[ii];
      ull[ii].dva = vmeAdrs[ii];
      if(ii == (numt-1)) {
	ull[ii].dcpp = 1;
      } else {
	temp1 = (UINT32)(&(ull[ii+1].dctl));
	ull[ii].dcpp = (temp1+PCI2DRAM_BASE_ADRS);
      }
    }

  }else{
    logMsg("sysVmeDmaLLCreate: ERROR: Number of command packets out of range (numt = %d)\n"
	   ,numt,0,0,0,0,0);
    return(ERROR);
  }


  /* logMsg("Link-List DMA setup for %d transfers\n",numt,2,3,4,5,6); */
  /* logMsg("        Local Address     VME Address       Nbytes\n",1,2,3,4,5,6); */

  for(ii=0; ii<numt;ii++) {

    /* logMsg("  %d     0x%08x            0x%08x            %d\n", */
    /* 	   ii,ull[ii].dla,ull[ii].dva,ull[ii].dtbc,5,6); */

    /* Swap the bytes in the structure */
    ull[ii].dctl = LONGSWAP(ull[ii].dctl);
    ull[ii].dtbc = LONGSWAP(ull[ii].dtbc);
    ull[ii].dla  = LONGSWAP(ull[ii].dla);
    ull[ii].dva  = LONGSWAP(ull[ii].dva);
    ull[ii].dcpp = LONGSWAP(ull[ii].dcpp);
  }

  return(OK);
}

void
sysVmeDmaLLGo()
{
  /* Set the DGCS to start the DMA */

  *UNIVERSE_DGCS = LONGSWAP (DGCS_RESET_STAT);
  *UNIVERSE_DGCS = LONGSWAP (globalDGCS | DGCS_CHAIN | DGCS_GO );

  return;
}

void
sysVmeDmaReset()
{

/* Set the DGCS to stop the DMA engine - then reset status */

   *UNIVERSE_DGCS = LONGSWAP (DGCS_STOP_REQ);

   while (1) {
     if(sysVmeDmaActive() == 0)
       break;
   }

   *UNIVERSE_DGCS = LONGSWAP (globalDGCS | DGCS_RESET_STAT);

}


void
sysVmeReset()
{

  /* Set the software VME sys reset bit */

   *UNIVERSE_MISC_CTL = LONGSWAP (MISC_CTL_SW_SRST);

   return;

}


unsigned int
sysUnivSetUserAM(int amcode1, int amcode2)
{
  unsigned int reg=0;

  /* Set the User AM code register for User1 and User2 */

  if(amcode1) {
    reg = (LONGSWAP (*UNIVERSE_USER_AM)) & 0x00ff0000;
    *UNIVERSE_USER_AM = LONGSWAP (reg | ((amcode1&0x3f)<<26));
  }

  if(amcode2) {
    reg = (LONGSWAP (*UNIVERSE_USER_AM)) & 0xff000000;
    *UNIVERSE_USER_AM = LONGSWAP (reg | ((amcode2&0x3f)<<18));
  }

  reg = (LONGSWAP (*UNIVERSE_USER_AM));

  return(reg);

}

/* Set the VAS bits for the LSI window id *
   id = 0,1,2,3
   vas = 0:A16  1:A24  2:A32  3,4,5:Reserved  6:User1   7:User2
*/
unsigned int
sysUnivSetLSI(unsigned short id, unsigned short vas)
{
  int reg = 0;

  if((vas>=3)&&(vas<=5))
    return(0);

  switch(id) {
  case 0:
    reg = (LONGSWAP (*UNIVERSE_LSI0_CTL))&0xfff8ffff;
    *UNIVERSE_LSI0_CTL = LONGSWAP(reg | (vas<<16));
    reg = LONGSWAP (*UNIVERSE_LSI0_CTL);
    break;
  case 1:
    reg = (LONGSWAP (*UNIVERSE_LSI1_CTL))&0xfff8ffff;
    *UNIVERSE_LSI1_CTL = LONGSWAP(reg | (vas<<16));
    reg = LONGSWAP (*UNIVERSE_LSI1_CTL);
    break;
  case 2:
    reg = (LONGSWAP (*UNIVERSE_LSI2_CTL))&0xfff8ffff;
    *UNIVERSE_LSI2_CTL = LONGSWAP(reg | (vas<<16));
    reg = LONGSWAP (*UNIVERSE_LSI2_CTL);
    break;
  case 3:
    reg = (LONGSWAP (*UNIVERSE_LSI3_CTL))&0xfff8ffff;
    *UNIVERSE_LSI3_CTL = LONGSWAP(reg | (vas<<16));
    reg = LONGSWAP (*UNIVERSE_LSI3_CTL);
    break;
  default:
    return(0);
  }

  return(reg);
}

int
sysUnivDmaConfig(unsigned int addrType, unsigned int dataType)
{
  int retVal = OK;

  sysVmeDmaInit(1);
  sysVmeDmaSet (FRC_VME_DMA_LOCAL_BSIZE,1);      /* Set for 64bit PCI transfers */

  switch(addrType)
    {
    case 0: /* A16 */
      sysVmeDmaSet (FRC_VME_DMA_VME_ACAP, ACAP_A16);
      break;

    case 1: /* A24 */
      sysVmeDmaSet (FRC_VME_DMA_VME_ACAP, ACAP_A24);
      break;

    case 2: /* A32 */
      sysVmeDmaSet (FRC_VME_DMA_VME_ACAP, ACAP_A32);
      break;

    default:
      printf("%s: Invalid addrType (%d)\n",
	     __func__, dataType);
      retVal = ERROR;
    }

  switch(dataType)
    {
    case 0: /* D16 */
      sysVmeDmaSet (FRC_VME_DMA_VME_DCAP, DCAP_D16);
      break;

    case 1: /* D32 */
      sysVmeDmaSet (FRC_VME_DMA_VME_DCAP, DCAP_D32);
      break;

    case 2: /* BLT */
      sysVmeDmaSet (FRC_VME_DMA_VME_DCAP, DCAP_BLT);
      break;

    case 3: /* MBLT */
      sysVmeDmaSet (FRC_VME_DMA_VME_DCAP, DCAP_MBLT);
      break;

    default:
      printf("%s: Invalid dataType (%d)\n",
	     __func__, dataType);
      retVal = ERROR;
    }

  return retVal;
}
