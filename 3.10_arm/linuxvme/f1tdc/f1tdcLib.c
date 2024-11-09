/*----------------------------------------------------------------------------*/
/**
 * @mainpage
 * <pre>
 *
 *  f1tdcLib.c  -  Driver library for JLAB config and readout of JLAB F1
 *                 TDC v2/v3 using a VxWorks 5.4 or later, or Linux
 *                 based Single Board computer.
 *
 *  Authors: David Abbott 
 *           Jefferson Lab Data Acquisition Group
 *           August 2003
 *
 *           Bryan Moffit
 *           Jefferson Lab Data Acquisition Group
 *           July 2013
 *
 *  Revision  2.0 - Initial Revision for v2/v3
 *                    - Supports up to 20 F1 boards in a Crate
 *                    - Programmed I/O and Block reads
 *
 * </pre>
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#ifdef VXWORKS
#include <vxWorks.h>
#include <logLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <iv.h>
#include <semLib.h>
#include <vxLib.h>
#else
#include "jvme.h"
#include <unistd.h>
#endif

/* Include TDC definitions */
#include "f1tdcLib.h"

/* Mutex to guard flexio read/writes */
pthread_mutex_t   f1Mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t   f1sdcMutex = PTHREAD_MUTEX_INITIALIZER;
#define F1LOCK      if(pthread_mutex_lock(&f1Mutex)<0) perror("pthread_mutex_lock");
#define F1UNLOCK    if(pthread_mutex_unlock(&f1Mutex)<0) perror("pthread_mutex_unlock");

/* Define external Functions */
#ifdef VXWORKS
IMPORT  STATUS sysBusToLocalAdrs(int, char *, char **);
IMPORT  STATUS intDisconnect(int);
IMPORT  STATUS sysIntEnable(int);
IMPORT  STATUS sysIntDisable(int);
IMPORT  STATUS sysVmeDmaDone(int, int);
IMPORT  STATUS sysVmeDmaSend(UINT32, UINT32, int, BOOL);

#define EIEIO    __asm__ volatile ("eieio")
#define SYNC     __asm__ volatile ("sync")
#endif 

/* Define Interrupts variables */
BOOL              f1tdcIntRunning  = FALSE;                    /* running flag */
int               f1tdcIntID       = -1;                       /* id number of ADC generating interrupts */
LOCAL VOIDFUNCPTR f1tdcIntRoutine  = NULL;                     /* user interrupt service routine */
LOCAL int         f1tdcIntArg      = 0;                        /* arg to user routine */
LOCAL UINT32      f1tdcIntLevel    = F1_VME_INT_LEVEL;         /* default VME interrupt level */
LOCAL UINT32      f1tdcIntVec      = F1_VME_INT_VEC;           /* default interrupt Vector */

/* Define static default config data
   0: V2  Hi Rez      - 32 MHz Clock (Internal Clock)
   1: V2  Hi Rez      - 31.25 MHz Clock (SD Clock)
   2: V3  Normal Rez  - 32 MHz Clock (Internal Clock)
   3: V3  Normal Rez  - 31.25 MHz Clock (SD Clock)
   4: Not Initialized - Read data from a file
*/
LOCAL int f1ConfigData[5][16] = { 
  { 0x0180, 0x8000, 0x407F, 0x407F, 
    0x407F, 0x407F, 0x003F, 0x9CC0, 
    0x22E2, 0x68A6, 0x1FEB, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x000C},
  { 0x0180, 0x8000, 0x407F, 0x407F, 
    0x407F, 0x407F, 0x003F, 0x9C00, 
    0x22EF, 0x68CE, 0x1FF1, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x000C},
  { 0x0180, 0x0000, 0x4040, 0x4040, 
    0x4040, 0x4040, 0x003F, 0xBA00, 
    0x63A4, 0xCDEC, 0x1FEB, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x000C},
  { 0x0180, 0x0000, 0x4040, 0x4040, 
    0x4040, 0x4040, 0x003F, 0xB880,
    0x61D1, 0xCE1E, 0x1FF1, 0x0000,
    0x0000, 0x0000, 0x0000, 0x000C},
  { 0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000, 
    0x0000, 0x0000, 0x0000, 0x0000}
};

/* Define global variables */
int nf1tdc = 0;                                       /* Number of TDCs in Crate */
int f1tdcA32Base   = 0x08000000;                      /* Minimum VME A32 Address for use by TDCs */
int f1tdcA32Offset = 0x08000000;                      /* Difference in CPU A32 Base - VME A32 Base */
int f1tdcA24Offset = 0x0;                             /* Difference in CPU A24 Base - VME A24 Base */
int f1tdcA16Offset = 0x0;                             /* Difference in CPU A16 Base - VME A16 Base */
volatile struct f1tdc_struct *f1p[(F1_MAX_BOARDS+1)]; /* pointers to TDC memory map */
volatile unsigned int *f1pd[(F1_MAX_BOARDS+1)];       /* pointers to TDC FIFO memory */
volatile unsigned int *f1pmb;                         /* pointer to Multblock window */
int f1ID[F1_MAX_BOARDS];                           /* array of slot numbers for TDCs */
unsigned int f1AddrList[F1_MAX_BOARDS];            /* array of a24 addresses for TDCs */
int f1Rev[(F1_MAX_BOARDS+1)];                      /* Board Revision Info for each module */
int f1Nchips[(F1_MAX_BOARDS+1)];                   /* Number of f1 chips on each module */
int f1MaxSlot=0;                                   /* Highest Slot hold an F1 */
int f1MinSlot=0;                                   /* Lowest Slot holding an F1 */
int f1tdcIntCount = 0;                             /* Count of interrupts from TDC */
unsigned int f1LetraMode = 0;                      /* Mask of Letra Enabled TDCs. Rising edge only default */ 
unsigned long long int f1ChannelDisable[(F1_MAX_BOARDS+1)]; /* Mask of channels to disable for each module */
int f1ClockSource=0;                               /* Common clock source: 0=Internal, 1=External */
int f1BlockError=F1_BLOCKERROR_NO_ERROR; /* Whether (>0) or not (0) Block Transfer had an error */

/* Include Firmware Tools */
#include "f1FirmwareTools.c"

/* Some static function prototypes */
static int f1WriteChip(int id, int chip, int reg, unsigned short data);
static int f1WriteAllChips(int id, int reg, unsigned short data);
static unsigned short f1ReadChip(int id, int chip, int reg);

/**
 * @defgroup Config Initialization/Configuration
 * @defgroup PulserConfig Pulser Configuration (V3 only)
 *   @ingroup Config
 * @defgroup Status Status
 * @defgroup Readout Data Readout
 * @defgroup IntPoll Interrupt/Polling
 * @defgroup DebugSW Routines to test lines to Switch Slot Modules
 * @defgroup Deprec Deprecated - To be removed
 */


/**
 * @ingroup Config
 * @brief Initialize JLAB F1 TDC Library. 
 *
 * @param addr
 *  - A24 VME Address of the f1TDC
 * @param addr_inc
 *  - Amount to increment addr to find the next f1TDC
 * @param ntdc
 *  - Number of times to increment
 *
 * @param iFlag 
 *  - 18 bit integer for initialization flags
 *
 * <pre>
 *       Low 6 bits - Specifies the default Signal distribution (clock,trigger) 
 *                    sources for the board (Internal, FrontPanel, VXS, VME(Soft))
 *
 *       bit    0:  defines Sync Reset source
 *                     0  VME (Software Sync-Reset)
 *                     1  Front Panel/VXS/P2 (Depends on Clk/Trig source selection)
 *       bits 2-1:  defines Trigger source
 *                   0 0  VME (Software Triggers)
 *                   0 1  Front Panel Input
 *                   1 0  VXS (P0) 
 *                        (all others Undefined - default to Internal)
 *       bits 5-4:  defines Clock Source
 *                   0 0  32 MHz Clock
 *                   0 1  Front Panel 
 *                   1 0  VXS (P0)
 *
 *       bit 16:  Exit before board initialization
 *                     0  Initialize FADC (default behavior)
 *                     1  Skip initialization (just setup register map pointers)
 *
 *       bit 17:  Use f1AddrList instead of addr and addr_inc
 *                for VME addresses.
 *                     0  Initialize with addr and addr_inc
 *                     1  Use f1AddrList 
 *
 *       bit 18:  Skip firmware check.  Useful for firmware updating.
 *                     0  Perform firmware check
 *                     1  Skip firmware check
 * </pre>
 *
 * @return OK, or ERROR if the address is invalid or a board is not present.
 */
STATUS 
f1Init (UINT32 addr, UINT32 addr_inc, int ntdc, int iFlag)
{
  int ii, res, rdata, errFlag = 0;
  int boardID = 0;
  int maxSlot = 1;
  int minSlot = 21;
  unsigned int laddr, laddr_inc, a32addr;
  int rev=0, config=0;
  volatile struct f1tdc_struct *f1;
  int trigSrc=0, clkSrc=0, srSrc=0;
  int noBoardInit=0, useList=0, noFirmwareCheck=0;

  /* Parse some iFlag arguments, rest done later */
  /* Check if we are to exit when pointers are setup */
  noBoardInit=(iFlag&F1_IFLAG_NOINIT)? 1:0;

  /* Check if we're initializing using a list */
  useList=(iFlag&F1_IFLAG_USELIST)? 1:0;

  /* Skip the firmware check? */
  noFirmwareCheck=(iFlag&F1_IFLAG_NOFWCHECK)? 1:0;

  /* Check for valid address */
  if(addr==0) 
    {
      printf("f1Init: ERROR: Must specify a Bus (VME-based A24) address for TDC 0\n");
      return(ERROR);
    }
  else if(addr > 0x00ffffff) 
    { /* A24 Addressing */
      printf("f1Init: ERROR: A32 Addressing not allowed for the F1 TDC\n");
      return(ERROR);
    }
  else
    { /* A24 Addressing */
      if( ((addr_inc==0)||(ntdc==0)) && (useList==0) )
	ntdc = 1; /* assume only one TDC to initialize */

      /* get the TDC address */
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x39,(char *)addr,(char **)&laddr);
      if (res != 0) 
	{
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x39,0x%x,&laddr) \n",addr);
	  return(ERROR);
	}
#else
      res = vmeBusToLocalAdrs(0x39,(char *)addr,(char **)&laddr);
      if (res != 0) 
	{
	  printf("f1Init: ERROR in vmeBusToLocalAdrs(0x39,0x%x,&laddr) \n",addr);
	  return(ERROR);
	}
#endif
      f1tdcA24Offset = laddr - addr;
    }

  /* Init Some Global variables */
  nf1tdc = 0;
  memset((char *)f1ID, 0, sizeof(f1ID));
  memset((char *)f1ChannelDisable, 0 , sizeof(f1ChannelDisable));

  for (ii=0;ii<ntdc;ii++) 
    {
      if(useList==1)
	{
	  laddr_inc = f1AddrList[ii] + f1tdcA24Offset;
	}
      else
	{
	  laddr_inc = laddr + ii*addr_inc;
	}
      f1 = (struct f1tdc_struct *)laddr_inc;
      /* Check if Board exists at that address */
#ifdef VXWORKS
      res = vxMemProbe((char *) &(f1->version),VX_READ,4,(char *)&rdata);
#else
      res = vmeMemProbe((char *) &(f1->version),4,(char *)&rdata);
#endif
      if(res < 0) 
	{
#ifdef VXWORKS
	  printf("f1Init: WARN: No addressable board at addr=0x%x\n",(UINT32) f1);
#else
	  printf("f1Init: WARN: No addressable board at VME (Local) addr=0x%x (0x%x)\n",
		 (UINT32) laddr_inc-f1tdcA24Offset, (UINT32) f1);
#endif
	  errFlag = 1;
	  continue;
	} 
      else 
	{
	  /* Check that it is an F1 board */
	  if((rdata&F1_VERSION_BOARDTYPE_MASK) != F1_BOARD_ID) 
	    {
	      printf(" ERROR: For board at 0x%x, Invalid Board ID: 0x%x\n",
		     (UINT32) f1-f1tdcA24Offset,rdata);
	      errFlag = 1;
	      continue;
	    }
	  /* Check if this is board has a valid slot number */
	  boardID =  ((vmeRead32(&(f1->intr)))&F1_SLOT_ID_MASK)>>16;
	  if((boardID <= 0)||(boardID >21)) 
	    {
	      printf(" ERROR: Board Slot ID is not in range: %d\n",boardID);
	      errFlag = 1;
	      continue;
	    }

	  rev = (rdata&F1_VERSION_BOARDREV_MASK)>>8;
	  if((rev != 2) && (rev != 3))
	    {
	      printf("%s: ERROR: Module revision (%d) not compatible with this driver\n",
		     __FUNCTION__,rev);
	      errFlag = 1;
	      continue;
	    }

	  f1Rev[boardID] = rdata&(F1_VERSION_BOARDREV_MASK | F1_VERSION_FIRMWARE_MASK);

	  if(!noFirmwareCheck)
	    {
	      if( (f1Rev[boardID] & F1_VERSION_FIRMWARE_MASK) < F1_SUPPORTED_FIRMWARE(rev))
		{
		  printf("%s: ERROR: FPGA Firmware (0x%02x) not supported by this driver.\n",
			 __FUNCTION__,rdata & F1_VERSION_FIRMWARE_MASK);
		  printf("\tUpdate to 0x%02x to use this driver.\n",F1_SUPPORTED_FIRMWARE(rev));
		  errFlag = 1;
		  continue;
		}
	    }

	  if(rev == 2)
	    f1Nchips[boardID] = 8;
	  else
	    f1Nchips[boardID] = 6;

	}
      f1ID[nf1tdc] = boardID;
      if(boardID >= maxSlot) maxSlot = boardID;
      if(boardID <= minSlot) minSlot = boardID;

      f1p[boardID] = (struct f1tdc_struct *)(laddr_inc);

#ifdef VXWORKS
      printf("Initialized TDC %2d  Slot #%2d at address 0x%08x \n",
	     ii,f1ID[nf1tdc],(UINT32) f1p[(f1ID[nf1tdc])]);
#else
      printf("Initialized TDC %2d  Slot #%2d at VME (Local) address 0x%x (0x%x) \n",
	     ii,f1ID[nf1tdc],(UINT32) f1p[(f1ID[nf1tdc])] - f1tdcA24Offset, 
	     (UINT32) f1p[(f1ID[nf1tdc])]);
#endif
      nf1tdc++;
    }

  if((!noBoardInit) && (nf1tdc))
    {
      /* Parse the rest of the iFlag arguments for Trigger, SyncReset, and Clock Sources */
      if((iFlag & F1_CLKSRC_MASK) == F1_CLKSRC_VXS)
	{
	  printf("%s: Enabling f1TDC for VXS Clock",__FUNCTION__);
	  clkSrc = 0;  
	  f1ClockSource=1;

	  if(rev==2)
	    config=1;
	  else
	    config=3;

	  switch (iFlag&(F1_TRIGSRC_MASK|F1_SRSRC_MASK))
	    { /* P0/VXS Clock Source */
	    case 0: case 1:
	      printf(" and Software Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 2:
	      printf(" and Front Panel Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_FP;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 3:
	      printf(" and Front Panel Triggers (FP Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_FP;
	      srSrc = F1_SYNC_RESET_SRC_FP;
	      break;
	    case 4: case 6:
	      printf(" and VXS Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_P0;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 5: case 7:
	      printf(" and VXS Triggers (VXS Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_P0;
	      srSrc = F1_SYNC_RESET_SRC_P0;
	      break;
	    }
	}
      else if ((iFlag & F1_CLKSRC_MASK) == F1_CLKSRC_FP)
	{ /* Front Panel Clock Source */
	  printf("%s: Enabling f1TDC for Front Panel Clock",__FUNCTION__);
	  clkSrc = F1_REFCLK_SRC_INTERNALFP | F1_REFCLK_SRC_FP;
	  f1ClockSource=1;

	  if(rev==2)
	    config=1;
	  else
	    config=3;

	  switch (iFlag&(F1_TRIGSRC_MASK|F1_SRSRC_MASK))
	    {
	    case 0: case 1:
	      printf(" and Software Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 2: case 4: case 6:
	      printf(" and Front Panel Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_FP;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 3: case 5: case 7:
	      printf(" and Front Panel Triggers (FP Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_FP;
	      srSrc = F1_SYNC_RESET_SRC_FP;
	      break;
	    }
	}
      else
	{ /* Use internal clock */
	  printf("%s: Enabling f1TDC for Internal Clock",__FUNCTION__);
	  clkSrc = F1_REFCLK_SRC_INTERNALFP | F1_REFCLK_INTERNAL_ENABLE;
	  f1ClockSource=0;

	  if(rev==2)
	    config=0;
	  else
	    config=2;

	  switch (iFlag&(F1_TRIGSRC_MASK|F1_SRSRC_MASK))
	    {
	    case 0: case 1:
	      printf(" and Software Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 2:
	      printf(" and Front Panel Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_FP;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 3:
	      printf(" and Front Panel Triggers (FP Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_FP;
	      srSrc = F1_SYNC_RESET_SRC_FP;
	      break;
	    case 4: case 6:
	      printf(" and VXS Triggers (Software Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_P0;
	      srSrc = F1_SYNC_RESET_SRC_SOFT | F1_ENABLE_SOFT_CONTROL;
	      break;
	    case 5: case 7:
	      printf(" and VXS Triggers (VXS Sync Reset)\n");
	      trigSrc = F1_TRIGGER_SRC_P0;
	      srSrc = F1_SYNC_RESET_SRC_P0;
	      break;
	    }
	}


      /* Reset all TDCs */
      for(ii=0;ii<nf1tdc;ii++) 
	{
	  vmeWrite32(&(f1p[f1ID[ii]]->csr),F1_CSR_HARD_RESET);
	}
      taskDelay(60);

      /* Initialize Interrupt variables */
      f1tdcIntID = -1;
      f1tdcIntRunning = FALSE;
      f1tdcIntLevel = F1_VME_INT_LEVEL;
      f1tdcIntVec = F1_VME_INT_VEC;
      f1tdcIntRoutine = NULL;
      f1tdcIntArg = 0;

      /* Calculate the A32 Offset for use in Block Transfers */
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x09,(char *)f1tdcA32Base,(char **)&laddr);
#else
      res = vmeBusToLocalAdrs(0x09,(char *)f1tdcA32Base,(char **)&laddr);
#endif
      if (res != 0) 
	{
#ifdef VXWORKS
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",f1tdcA32Base);
#else
	  printf("f1Init: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n",f1tdcA32Base);
#endif
	  return(ERROR);
	} 
      else 
	{
	  f1tdcA32Offset = laddr - f1tdcA32Base;
	}

      for(ii=0;ii<nf1tdc;ii++) 
	{
	  /* Program an A32 access address for this TDC's FIFO */
	  a32addr = f1tdcA32Base + ii*F1_MAX_A32_MEM;
#ifdef VXWORKS
	  res = sysBusToLocalAdrs(0x09,(char *)a32addr,(char **)&laddr);
#else
	  res = vmeBusToLocalAdrs(0x09,(char *)a32addr,(char **)&laddr);
#endif
	  if (res != 0) 
	    {
#ifdef VXWORKS
	      printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",a32addr);
#else
	      printf("f1Init: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n",a32addr);
#endif
	      return(ERROR);
	    }

	  f1pd[f1ID[ii]] = (unsigned int *)(laddr);  /* Set a pointer to the FIFO */
	  vmeWrite32(&(f1p[f1ID[ii]]->adr32),(a32addr>>16) + 1);  /* Write the register and enable */

	}
      
      /* default Block Level to 1 */
      f1GSetBlockLevel(1);

      /* Use the user configuration, if it has been loaded. */
      if(f1ConfigData[4][0] != 0)
	config=4;
      
      /* Write the configuration to the f1tdc chips */
      if(nf1tdc)
	{
	  printf("%s: Using configuration %d.\n",
		 __FUNCTION__,config);
	  f1GConfigWrite((int *) &f1ConfigData[config], F1_ALL_CHIPS);
	}
      
      for(ii=0; ii<nf1tdc; ii++)
	{
	  /* Enable control signals (clock, syncreset, trigger) */
	  vmeWrite32(&f1p[f1ID[ii]]->ctrl,
		     (vmeRead32(&f1p[f1ID[ii]]->ctrl) & ~F1_CTRL_SIGNALS_MASK) |
		     clkSrc | srSrc | trigSrc);
	}
      
      // FIXME: Carry over code from v1... Wait for the Chips on each module to lock to SD Clock.
      for(ii=0;ii<nf1tdc;ii++) 
	{
	  int iwait=0;
	  while(iwait<100000) {
	    if(f1CheckLock(f1ID[ii]) == 0) break;
	    iwait++;
	  }
	  if(iwait==100000)
	    printf("%s: ERROR: Slot %d not locked\n",__FUNCTION__,f1ID[ii]);
	}

      f1GClearStatus(F1_ALL_CHIPS);

      /* If there are more than 1 TDC in the crate then setup the Muliblock Address
	 window. This must be the same on each board in the crate */
      if(nf1tdc > 1) 
	{
	  a32addr = f1tdcA32Base + (nf1tdc+1)*F1_MAX_A32_MEM; /* set MB base above individual board base */
#ifdef VXWORKS
	  res = sysBusToLocalAdrs(0x09,(char *)a32addr,(char **)&laddr);
#else
	  res = vmeBusToLocalAdrs(0x09,(char *)a32addr,(char **)&laddr);
#endif
	  if (res != 0) 
	    {
#ifdef VXWORKS
	      printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",a32addr);
#else
	      printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",a32addr);
#endif
	      return(ERROR);
	    }

	  f1pmb = (unsigned int *)(laddr);  /* Set a pointer to the FIFO */
	  for (ii=0;ii<nf1tdc;ii++) 
	    {
	      /* Write the register and enable */
	      vmeWrite32(&(f1p[f1ID[ii]]->adr_mb),
			 (a32addr+F1_MAX_A32MB_SIZE) + (a32addr>>16) + 1);
	    }
    
	  /* Set First Board and Last Board */
	  f1MaxSlot = maxSlot;
	  f1MinSlot = minSlot;
	  vmeWrite32(&(f1p[minSlot]->ctrl),
		     vmeRead32(&(f1p[minSlot]->ctrl)) | F1_FIRST_BOARD);
	  vmeWrite32(&(f1p[maxSlot]->ctrl),
		     vmeRead32(&(f1p[maxSlot]->ctrl)) | F1_LAST_BOARD);

	}
    } /* !noBoardInit */

  if(errFlag > 0) 
    {
      printf("f1Init: Unable to initialize all (%d) TDC Modules\n",ntdc);
      if(nf1tdc > 0)
	printf("f1Init: %d TDC(s) successfully initialized\n",nf1tdc );
      return(ERROR);
    } 
  else 
    {
      if(nf1tdc > 0)
	printf("f1Init: %d TDC(s) successfully initialized\n",nf1tdc );
      return(OK);
    }


}

int
f1CheckAddresses(int id)
{
  struct f1tdc_struct test;
  unsigned int offset=0, expected=0, base=0;
  
  base = (unsigned int) &test.version;

  printf("%s:\n\t ---------- Checking f1TDC address space ---------- \n",__FUNCTION__);

  offset = ((unsigned int) &test.ctrl2) - base;
  expected = 0x40;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "ctrl2",
	   expected,offset);

  offset = ((unsigned int) &test.status[1]) - base;
  expected = 0x80;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "status1",
	   expected,offset);

  offset = ((unsigned int) &test.test_config) - base;
  expected = 0xC0;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "test_config",
	   expected,offset);

  offset = ((unsigned int) &test.state) - base;
  expected = 0x100;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "state",
	   expected,offset);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Convert an index into a slot number, where the index is
 *          the element of an array of F1TDCs in the order in which they were
 *          initialized.
 *
 * @param i 
 *  - Initialization number
 * @return Slot number if Successfull, otherwise ERROR.
 *
 */
int
f1Slot(unsigned int i)
{
  if(i>=nf1tdc)
    {
      printf("%s: ERROR: Index (%d) >= f1TDCs initialized (%d).\n",
	     __FUNCTION__,i,nf1tdc);
      return ERROR;
    }

  return f1ID[i];
}

/**
 *  @ingroup Config
 *  @brief Write the specified configuration to provided chips of the module in slot id
 *      indicated by the chipmask 
 *
 *  @param id 
 *   - Slot Number
 *  @param config_data
 *   - Array of configuration data to write to f1TDC chips.  Each element
 *     refers to a specific f1TDC chip register.
 *  @param chipmask
 *   - Mask of which chips to write to.
 *
 * @return OK if Successfull, otherwise ERROR.
 */
int
f1ConfigWrite(int id, int *config_data, int chipMask)
{
  int ichip,jj, reg, ctrl=0;
  int order[16] = { 15,10,  0,1,6,  8,9,  11,12,13,14,  7,  2,3,4,5  };  
  unsigned int data, rval;
  unsigned int allchips_mask=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return(ERROR);
    }

  if(chipMask == 0) 
    { /* Assume all chips programmed the same */
      chipMask = F1_ALL_CHIPS;
    }

  /* Disable FIFOs and Edges */
  f1DisableData(id);

  F1LOCK;
  /* Save the current clock state */
  ctrl = vmeRead32(&f1p[id]->ctrl);
  
  /* Disable the clock if it is enabled */
  vmeWrite32(&f1p[id]->ctrl, (ctrl & ~F1_REFCLK_SRC_MASK) | F1_REFCLK_SRC_INTERNALFP);

  taskDelay(20);

  allchips_mask = ((1<<(f1Nchips[id]))-1);

  if((allchips_mask & chipMask) == allchips_mask)
    { /* Write the same configuration to all chips */
/*       printf("%s: Write same to all chips\n",__FUNCTION__); */
      for(jj=0; jj<16; jj++) /* write cfg data */
	{
	  reg = order[jj]; /* program registers in the correct order */

	  if((reg>=2) && (reg<=5)) /* Disable edges until "enable" */
	    data = config_data[reg] & F1_OFFSET_MASK;
	  else
	    data = config_data[reg] & 0xFFFF;
	      
	  f1WriteAllChips(id, reg, data);
	  taskDelay(1);

	  // FIXME: Put this back in, when f1 stores the config in memory
/* 	  rval = f1ReadChip(id, ichip, reg); */
/* 	  if((rval & 0xFFFF) != data) */
/* 	    { */
/* 	      printf("%s(%d): ERROR writing to chip %d reg %2d..\n", */
/* 		     __FUNCTION__,id, ichip, reg); */
/* 	      printf("\t Write (0x%04x) != Readback (0x%04x)\n", */
/* 		     data, rval%0xFFFF); */
/* 	    } */
	}
    }
  else
    {
/*       printf("%s: Just write to SOME chips\n",__FUNCTION__); */
      /* Loop over each chip, neglect the ones that are not turned on in chipMask */
      for(ichip=0; ichip<f1Nchips[id]; ichip++)
	{
	  if((chipMask)&(1<<ichip)) 
	    {
	      for(jj=0; jj<16; jj++) /* write cfg data */
		{
		  reg = order[jj]; /* program registers in the correct order */
		  
		  if((reg>=2) && (reg<=5)) /* Disable edges until "enable" */
		    data = config_data[reg] & F1_OFFSET_MASK;
		  else
		    data = config_data[reg] & 0xFFFF;
		  
		  f1WriteChip(id, ichip, reg, data);
		  taskDelay(1);
		  
		  rval = f1ReadChip(id, ichip, reg);
		  if((rval & 0xFFFF) != data)
		    {
		      printf("%s(%d): ERROR writing to chip %d reg %2d..\n",
			     __FUNCTION__,id, ichip, reg);
		      printf("\t Write (0x%04x) != Readback (0x%04x)\n",
			     data, rval%0xFFFF);
		    }
		}
	    }
	}
    }


  /* Return the original clock setting */
  vmeWrite32(&f1p[id]->ctrl, ctrl);

  taskDelay(20);

  for(ichip=0;ichip<f1Nchips[id];ichip++)
    {
      if(chipMask&(1<<ichip))
	{
	  vmeWrite16(&f1p[id]->stat[ichip], F1_CHIP_CLEAR_STATUS);
	}
    }
  F1UNLOCK;

  return(OK);

}

/**
 *  @ingroup Config
 *  @brief Write the specified configuration to provided chips of all initialized f1TDCs
 *      indicated by the chipmask 
 *
 *  @param config_data
 *   - Array of configuration data to write to f1TDC chips.  Each element
 *     refers to a specific f1TDC chip register.
 *  @param chipmask
 *   - Mask of which chips to write to.
 *
 * @return OK if Successfull, otherwise ERROR.
 */
int
f1GConfigWrite(int *config_data, int chipMask)
{
  int if1=0, id=0;
  int ichip,jj, reg, ctrl[F1_MAX_BOARDS+1];
  int order[16] = { 15,10,  0,1,6,  8,9,  11,12,13,14,  7,  2,3,4,5  };  
  unsigned int data, rval;
  unsigned int allchips_mask=0;

  if(chipMask == 0) 
    { /* Assume all chips programmed the same */
      chipMask = F1_ALL_CHIPS;
    }

  /* Disable FIFOs and Edges */
  for(if1=0; if1<nf1tdc; if1++)
    f1DisableData(f1ID[if1]);

  F1LOCK;
  
  /* Disable the clock if it is enabled */
  for(if1=0; if1<nf1tdc; if1++)
    {
      /* Save the current clock state */
      ctrl[if1] = vmeRead32(&f1p[f1ID[if1]]->ctrl);

      vmeWrite32(&f1p[f1ID[if1]]->ctrl,
		 (ctrl[if1] & ~F1_REFCLK_SRC_MASK) | F1_REFCLK_SRC_INTERNALFP);
    }
  taskDelay(30);

  for(if1=0; if1<nf1tdc; if1++)
    {
      id=f1Slot(if1);
      allchips_mask = ((1<<(f1Nchips[id]))-1);

      if((allchips_mask & chipMask) == allchips_mask)
	{ /* Write the same configuration to all chips */
	  /*       printf("%s: Write same to all chips\n",__FUNCTION__); */
	  for(jj=0; jj<16; jj++) /* write cfg data */
	    {
	      reg = order[jj]; /* program registers in the correct order */

	      if((reg>=2) && (reg<=5)) /* Disable edges until "enable" */
		data = config_data[reg] & F1_OFFSET_MASK;
	      else
		data = config_data[reg] & 0xFFFF;
	      
	      f1WriteAllChips(id, reg, data);
	      taskDelay(1);

	      // FIXME: Put this back in, when f1 stores the config in memory
	      /* 	  rval = f1ReadChip(id, ichip, reg); */
	      /* 	  if((rval & 0xFFFF) != data) */
	      /* 	    { */
	      /* 	      printf("%s(%d): ERROR writing to chip %d reg %2d..\n", */
	      /* 		     __FUNCTION__,id, ichip, reg); */
	      /* 	      printf("\t Write (0x%04x) != Readback (0x%04x)\n", */
	      /* 		     data, rval%0xFFFF); */
	      /* 	    } */
	    }
	}
      else
	{
	  /*       printf("%s: Just write to SOME chips\n",__FUNCTION__); */
	  /* Loop over each chip, neglect the ones that are not turned on in chipMask */
	  for(ichip=0; ichip<f1Nchips[id]; ichip++)
	    {
	      if((chipMask)&(1<<ichip)) 
		{
		  for(jj=0; jj<16; jj++) /* write cfg data */
		    {
		      reg = order[jj]; /* program registers in the correct order */
		  
		      if((reg>=2) && (reg<=5)) /* Disable edges until "enable" */
			data = config_data[reg] & F1_OFFSET_MASK;
		      else
			data = config_data[reg] & 0xFFFF;
		  
		      f1WriteChip(id, ichip, reg, data);
		      taskDelay(2);
		  
		      rval = f1ReadChip(id, ichip, reg);
		      if((rval & 0xFFFF) != data)
			{
			  printf("%s(%d): ERROR writing to chip %d reg %2d..\n",
				 __FUNCTION__,id, ichip, reg);
			  printf("\t Write (0x%04x) != Readback (0x%04x)\n",
				 data, rval%0xFFFF);
			}
		    }
		}
	    }
	}


      /* Return the original clock setting */
      vmeWrite32(&f1p[id]->ctrl, ctrl[if1]);
    }

  taskDelay(40);

  for(if1=0; if1<nf1tdc; if1++)
    {
      id=f1Slot(if1);
      for(ichip=0;ichip<f1Nchips[id];ichip++)
	{
	  if(chipMask&(1<<ichip))
	    {
	      vmeWrite16(&f1p[id]->stat[ichip], F1_CHIP_CLEAR_STATUS);
	    }
	}
    }
  F1UNLOCK;

  return(OK);

}


/**
 *  @ingroup Config
 *  @brief Set which preset/user configuration to use for specified slot id
 *         for indicated chips in chipmask
 *
 *  @param id 
 *   - Slot Number
 *  @param iflag
 *   -  Set from four default configuations
 * <pre>
 *    0 V2: Hi Rez      - Internal Clock (32 MHz)
 *    1 V2: Hi Rez      - External Clock (31.25 MHz)
 *    2 V3: Normal Rez  - Internal Clock (32 MHz)
 *    3 V3: Normal Rez  - External Clock (31.25 MHz)
 *    4 User specified (from file)
 * </pre>
 *  @param chipmask
 *   - Mask of which chips to write to.
 *
 * @return OK if Successfull, otherwise ERROR.
 */
int
f1SetConfig(int id, int iflag, int chipMask)
{

  int rev;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return(ERROR);
    }
  
  if((iflag<0)||(iflag>4)) 
    {
      printf("f1SetConfig: ERROR: Invalid config number. Choose from 0-4 where\n");
      printf("             0 -> V2: Hi Resolution (32 chan) - Internal Clock (32 MHz) \n");
      printf("             1 -> V2: Hi Resolution (32 chan) - External Clock (31.25 MHz) \n");
      printf("             2 -> V3: Normal Resolution (48 chan) - Internal Clock (32 MHz)\n");
      printf("             3 -> V3: Normal Resolution (48 chan) - External Clock (31.25 MHz)\n");
      printf("             4 -> User specified from a file (call f1ConfigReadFile() first)\n");
      return (ERROR);
    }

  rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  if(rev==2)
    {
      if((iflag == 2) || (iflag == 3))
	{
	  printf("%s: ERROR: Invalid config number (%d) for this module revision (%d).\n",
		 __FUNCTION__,iflag,rev);
	  return ERROR;
	}
    }

  if(rev==3)
    {
      if((iflag == 0) || (iflag == 1))
	{
	  printf("%s: ERROR: Invalid config number (%d) for this module revision (%d).\n",
		 __FUNCTION__,iflag,rev);
	  return ERROR;
	}
    }

  if(iflag==4) 
    { /* check if there is valid config data there */
      if (f1ConfigData[4][0] == 0) 
	{
	  printf("%s: ERROR: Configuration not loaded from file.\n",
		 __FUNCTION__);
	  return(ERROR);
	}
    }

  f1ConfigWrite(id,(int *)&f1ConfigData[iflag], chipMask);

  return(OK);

}

/**
 *  @ingroup Status
 *  @brief Read the f1TDC Chip Registers into user specified config_data array
 *
 *  @param id 
 *   - Slot Number
 *  @param config_data
 *   - local memory address to place register values
 *  @param chipID
 *   - Which f1TDC Chip to read from
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
f1ConfigRead(int id, unsigned int *config_data, int chipID)
{
  int jj;//, reg;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return(ERROR);
    }
  
  if((chipID<0)||(chipID>=f1Nchips[id])) 
    {
      printf("f1ConfigRead: ERROR : Invalid Chip ID %d (range 0-%d)\n",chipID,
	     f1Nchips[id]-1);
      return(ERROR);
    }

  F1LOCK;
  for(jj=0; jj<16; jj++)
    { /* READ cfg data */
      config_data[jj] = f1ReadChip(id, chipID, jj);
    }
  F1UNLOCK;

  return(OK);

}

/**
 *  @ingroup Config
 *  @brief Read in user defined (4) f1TDC chip registers from specified file
 *
 *  @param filename
 *   - Name of file that contains f1TDC chip registers
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
f1ConfigReadFile(char *filename)
{
  FILE *fd_1;
  unsigned int value, config[16];
  int ii, jj;

  if(filename == NULL) 
    {
      printf("f1ConfigReadFile: ERROR: No Config file specified.\n");
      return(ERROR);
    }

  fd_1 = fopen(filename,"r");
  if(fd_1 > 0) 
    {
      printf("Reading Data from file: %s\n",filename);
      jj = 4; /* location for file data */
      for(ii = 0; ii < 16; ii++) 
	{
	  fscanf(fd_1,"%x",&value);
	  f1ConfigData[jj][ii] = 0xFFFF & value;
	  config[ii] = f1ConfigData[jj][ii]; 
	  printf("ALL Chips: Reg %2d  =  %04x\n",ii,config[ii]);
	}        

      fclose(fd_1);
    
      return(OK);
    }
  else
    {
      printf("f1ConfigReadFile: ERROR opening file %s\n",filename);
      return(ERROR);
    }
}

static int
f1ChipsHaveSameConfig(int id)
{
  int ichip, ireg;
  unsigned short chipReg0;

  F1LOCK;
  for(ireg = 0; ireg < 16; ireg++) 
    {
      chipReg0 = f1ReadChip(id, 0, ireg);
      for(ichip = 1; ichip < f1Nchips[id]; ichip++) 
	{
	  if(chipReg0 != f1ReadChip(id, ichip, ireg))
	    {
	      F1UNLOCK;
	      return 0;
	    }
	}
    }
  F1UNLOCK;
      
  return 1;
}

typedef struct chipinfo_struct
{
  int chip;
  float clk_period;
  int rez;
  float bin_size;
  float full_range;
  float window;
  float latency;
  unsigned int rollover_count;
} chipInfo;

static void
f1ConfigDecode(int id, chipInfo *ci, int pflag)
{
  int ireg, ii;
  unsigned short chipReg[16];
  float factor, tframe;
  int sync;
  unsigned int refcnt, exponent, refclkdiv, hsdiv, trigwin, triglat;

  F1LOCK;
  for(ireg = 0; ireg < 16; ireg++) 
    {
      chipReg[ireg] = f1ReadChip(id, ci->chip, ireg);
    }
  F1UNLOCK;


  if(pflag)
    {
      printf("\n ---------------- Chip %d ----------------",ci->chip);
      
      for(ireg = 0; ireg < 16; ireg++) 
	{
	  if((ireg%8)==0) printf("\n");
	  printf("0x%04x  ",chipReg[ireg]);
	}
      printf("\n");
    }
      
  if( chipReg[1] & 0x8000 )
    {
      if(pflag)
	printf("High Resolution mode\n");
      factor = 0.5;
      ci->rez = 1;
    }
  else
    {
      if(pflag)
	printf("Normal Resolution mode\n");
      factor = 1.;
      ci->rez = 0;
    }   

  if( chipReg[15] & 0x4 )
    {
      sync=1;
      if(pflag)
	printf("Synchronous mode\n");
    } 
  else 
    {
      sync=0;
      if(pflag)
	printf("Non-synchronous mode (Start required)\n");
    }
      
  refcnt = ( chipReg[7] >> 6 ) & 0x1FF;
  tframe = (float)(ci->clk_period * (refcnt +2 ));
  if(pflag)
    printf("refcnt = %d   tframe (ns) = %.1f\n",refcnt,tframe);
      
  exponent =  ( chipReg[10] >> 8 ) & 0x7;
  refclkdiv = 1;
  for(ii = 0; ii < exponent; ii++)
    refclkdiv = 2 * refclkdiv;
  hsdiv = chipReg[10] & 0xFF;
  ci->bin_size = factor * (ci->clk_period/152.) * ( (float)refclkdiv )/( (float)hsdiv );
  ci->full_range = 65536 * ci->bin_size;
  if(pflag)
    printf("refclkdiv = %d   hsdiv = %d   bin_size (ns) = %.4f   full_range (ns) = %.1f\n",
	   refclkdiv,hsdiv,ci->bin_size,ci->full_range);
      
  trigwin = chipReg[8]&0xffff;
  triglat = chipReg[9]&0xffff;
  ci->window = ((float)trigwin) * ci->bin_size/factor;	
  ci->latency = ((float)triglat) * ci->bin_size/factor;	
  if(pflag)
    printf("trigwin = %d   triglat = %d   window (ns) = %.1f   latency (ns) = %.1f\n",
	   trigwin,triglat,ci->window,ci->latency);

  if(sync) 
    {
      ci->rollover_count = (unsigned int) (tframe/ci->bin_size);
      if(pflag)
	printf("Rollover count = %d\n",ci->rollover_count);
    } 
  else 
    {
      ci->rollover_count = 65536;
      if(pflag)
	printf("Rollover count = N/A (Full Range - 65536)\n");

    }
  
}

/**
 *  @ingroup Status
 *  @brief Print to standard out the configuration of the f1TDC chips
 *         specified by the chipmask and module in slot id
 *
 *  @param id 
 *   - Slot Number
 *  @param chipMask
 *   - Mask of chips in module to show
 *
 */
void 
f1ConfigShow(int id, int chipMask)
{
  int ii_chip;
  chipInfo ci;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("f1ConfigShow: ERROR : TDC in slot %d is not initialized \n",id);
      return;
    }

  if(chipMask == 0) /* Print out config info for all chips */
    chipMask = F1_ALL_CHIPS;

  if(f1ClockSource==0) /* Internal */
    ci.clk_period = 31.25;
  else  /* External */
    ci.clk_period = 32.;

  for(ii_chip = 0; ii_chip < f1Nchips[id]; ii_chip++) 
    {
      if((chipMask)&(1<<ii_chip)) 
	{
	  ci.chip = ii_chip;
	  f1ConfigDecode(id, &ci, 1);
	}
    }	

}

/**
 *  @ingroup Status
 *  @brief Fills 'rval' with a character array containing the fa250 serial number.
 *  @param id 
 *    - Slot number
 *  @param rval Where to return Serial number string
 *  @return length of character array 'rval' if successful, otherwise ERROR
*/
int
f1GetSerialNumber(int id, char **rval)
{
  char read_char;
  char serial_read[8];
  int data_word,ii,jj;
  int serial_length = 7, ret_len=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return ERROR;
    }


  // read serial number back from eeprom character by character
  //	printf("\n read serial number back from eeprom character by character\n");
  for(ii=0; ii<serial_length; ii++)						
    {
      vmeWrite32(&f1p[id]->serial_eprom, 0x3000 | ii);	
      taskDelay(1);
      data_word = vmeRead32(&f1p[id]->serial_eprom);
      //		printf("data_word = %X  data_byte = %X\n", data_word, (data_word >>24));
      read_char = (char)(data_word >> 24);
      //		printf("eprom character = %c\n", read_char);
      serial_read[ii] = read_char;			// build the serial number
      jj = ii;
    }
  serial_read[jj+1] = '\0';				// terminate string
  printf("\n--- Module serial number = %s ---\n\n", serial_read);

  if(rval!=NULL)
    strcpy((char *)rval, serial_read);

  ret_len = (int)strlen(serial_read);

  return ret_len;
}

/**
 *  @ingroup Status
 *  @brief Get the firmware version of the FPGA
 *
 *  @param id 
 *    - Slot number
 *  @param  pflag Print to standard out flag
 *     - 0: Print nothing to stdout
 *     - !0: Print firmware versions to stdout
 *
 *   RETURNS: FPGA Version if successful
 *            or -1 if error
 */
int
f1GetFirmwareVersion(int id, int pflag)
{
  int rval=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return ERROR;
    }

  F1LOCK;
  rval = vmeRead32(&f1p[id]->version) & F1_VERSION_FIRMWARE_MASK;
  F1UNLOCK;

  if(pflag)
    {
      printf("%s:  Board Firmware Rev = 0x%04x\n",
	     __FUNCTION__,
	     rval);
    }

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Print Status of f1TDC to standard out
 *  @param id 
 *   - Slot Number
 *  @param sflag Status flag
 *   - !1: Show control status of the module
 *   -  1: Show minimal error status of the f1TDC chips
 *   -  2: Show f1TDC chip configuration
 *
 */
void
f1Status(int id, int sflag)
{
  int jj;
  unsigned int a32Base, ambMin, ambMax;
  unsigned int version;
  unsigned int csr, ctrl, ctrl2, count, blevel, intr, addr32, addrMB;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int cmask=0;
  unsigned int bcount=0, ramWords=0;
  unsigned int trig1Cnt=0, trig2Cnt=0, srCnt=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("f1Status: ERROR : TDC in slot %d is not initialized \n",id);
      return;
    }

  F1LOCK;
  version = vmeRead32(&f1p[id]->version)&0xFFFF;
  csr    = vmeRead32(&f1p[id]->csr);
  ctrl   = vmeRead32(&f1p[id]->ctrl);
  ctrl2  = vmeRead32(&f1p[id]->ctrl2);
  count  = vmeRead32(&f1p[id]->ev_count)&F1_EVENT_COUNT_MASK;
  blevel  = vmeRead32(&f1p[id]->blocklevel)&F1_BLOCKLEVEL_MASK;
  intr   = vmeRead32(&f1p[id]->intr);
  addr32 = vmeRead32(&f1p[id]->adr32);
  a32Base = (addr32&F1_A32_ADDR_MASK)<<16;
  addrMB = vmeRead32(&f1p[id]->adr_mb);
  ambMin =  (addrMB&F1_AMB_MIN_MASK)<<16;
  ambMax =  (addrMB&F1_AMB_MAX_MASK);
  bcount = vmeRead32(&f1p[id]->block_count);
  ramWords = vmeRead32(&f1p[id]->block_word_count_fifo);
  trig1Cnt = vmeRead32(&f1p[id]->scaler1);
  trig2Cnt = vmeRead32(&f1p[id]->trig2_scaler);
  srCnt = vmeRead32(&f1p[id]->sync_scaler);
  F1UNLOCK;

#ifdef VXWORKS
  printf("\nSTATUS for TDC in slot %d at base address 0x%x \n",id,(UINT32) f1p[id]);
#else
  printf("\nSTATUS for TDC in slot %d at VME (Local) base address 0x%x (0x%x) \n",
	 id,(UINT32) f1p[id] - f1tdcA24Offset, (UINT32) f1p[id]);
#endif
  printf("---------------------------------------------------------------------- \n");
  printf(" Module Version = 0x%02x : Firmware Version = 0x%02x\n",
	 (version & F1_VERSION_BOARDREV_MASK)>>8,
	 version & F1_VERSION_FIRMWARE_MASK);

  if(sflag != 1) 
    {
      if(addrMB&F1_AMB_ENABLE) 
	{
	  printf(" Alternate VME Addressing: Multiblock Enabled\n");
	  if(addr32&F1_A32_ENABLE)
	    printf("   A32 Enabled at VME (Local) base 0x%08x (0x%08x)\n",a32Base,(UINT32) f1pd[id]);
	  else
	    printf("   A32 Disabled\n");
	  
	  printf("   Muliblock VME Address Range 0x%08x - 0x%08x\n",ambMin,ambMax);
	}
      else
	{
	  printf(" Alternate VME Addressing: Multiblock Disabled\n");
	  if(addr32&F1_A32_ENABLE)
	    printf("   A32 Enabled at VME (Local) base 0x%08x (0x%08x)\n",a32Base,(UINT32) f1pd[id]);
	  else
	    printf("   A32 Disabled\n");
	}
    
      if(ctrl&F1_INT_ENABLE) 
	{
	  printf("\n  Interrupts ENABLED: ");
/* 	  if(ctrl&F1_EVENT_LEVEL_INT) printf("EventLevel(%d)",level); */
/* 	  if(ctrl&F1_ERROR_INT) printf("Errors "); */
/* 	  printf("\n"); */
	  printf("  VME INT Vector = 0x%x  Level = %d\n",(intr&F1_INT_VEC_MASK),((intr&F1_INT_LEVEL_MASK)>>8));
	}

      printf("\n Signal Sources: \n");

      printf("   Ref Clock : ");
      if(ctrl&F1_REFCLK_SRC_INTERNALFP)
	{
	  if(ctrl&F1_REFCLK_SRC_FP)
	    printf("Front Panel\n");
	  else
	    {
	      printf("Internal\n");
	    }
	}
      else
	printf("VXS\n");

      printf("   Trig Src  : ");
      if((ctrl&F1_TRIGGER_SRC_MASK)==F1_TRIGGER_SRC_FP)
	printf("Front Panel\n");
      else if((ctrl&F1_TRIGGER_SRC_MASK)==F1_TRIGGER_SRC_P0)
	printf("VXS\n");
      else if((ctrl&F1_TRIGGER_SRC_MASK)==F1_TRIGGER_SRC_SOFT)
	printf("Software\n");
      else
	printf("DISABLED\n");

      printf("   Sync Reset: ");
      if((ctrl&F1_SYNC_RESET_SRC_MASK)==F1_SYNC_RESET_SRC_FP)
	printf("Front Panel\n");
      else if((ctrl&F1_SYNC_RESET_SRC_MASK)==F1_SYNC_RESET_SRC_P0)
	printf("VXS\n");
      else if((ctrl&F1_SYNC_RESET_SRC_MASK)==F1_SYNC_RESET_SRC_SOFT)
	printf("Software\n");
      else
	printf("DISABLED\n");

      printf("   Start     : ");
      if((ctrl&F1_START_SRC_MASK)==F1_START_SRC_FP)
	printf("Front Panel\n");
      else if((ctrl&F1_START_SRC_MASK)==F1_START_SRC_P0)
	printf("VXS\n");
      else if((ctrl&F1_START_SRC_MASK)==F1_START_SRC_SOFT)
	printf("Software\n");
      else
	printf("DISABLED\n");

      printf("\n Configuration: \n");
      if(ctrl&F1_REFCLK_INTERNAL_ENABLE)
	printf("   Internal Clock ON\n");
      else
	printf("   Internal Clock OFF\n");
	      

      if(ctrl&F1_ENABLE_BERR)
	printf("   Bus Error ENABLED\n");
      else
	printf("   Bus Error DISABLED\n");

      printf("   MultiBlock transfer ");
      if(ctrl&F1_ENABLE_MULTIBLOCK) 
	{
	  int tP0, tP2;
	  tP0 = ctrl&F1_MB_TOKEN_P0;
	  tP2 = ctrl&F1_MB_TOKEN_P2;

	  printf("ENABLED ");
	  if(ctrl&F1_FIRST_BOARD)
	    printf("(First Board ");
	  else if(ctrl&F1_LAST_BOARD)
	    printf("(Last Board ");
	  else
	    printf("( ");

	  if(tP0 || tP2)
	    {
	      printf("token via ");
	      if(tP0)
		printf("VXS");
	      if(tP2)
		printf("P2");
	    }
	  printf(")\n");

	} 
      else 
	{
	  printf("DISABLED\n");
	}
    
      printf("\n");
      if(csr&F1_CSR_ERROR_MASK)
	{
	  printf("  CSR        Register = 0x%08x - Error Condition\n",csr);
	}
      else
	{
	  printf("  CSR        Register = 0x%08x \n",csr);
	}
      printf("  Control 1  Register = 0x%08x \n",ctrl);
      if(ctrl2&F1_GO_DATA)
	{
	  printf("  Control 2  Register = 0x%08x - Enabled for triggers\n",ctrl2);
	}
      else
	{
	  printf("  Control 2  Register = 0x%08x - Disabled\n",ctrl2);
	}

      printf("  Trigger      Scaler = %d\n",trig1Cnt);
      printf("  Trigger 2    Scaler = %d\n",trig2Cnt);
      printf("  SyncReset    Scaler = %d\n",srCnt);


      printf("\n");
      if(csr&F1_CSR_BLOCK_READY)
	{
	  printf("  Blocks in FIFO           = %d  (Block level = %d) - Block Available\n",bcount,blevel);
	  printf("  RAM Level (Bytes)        = %d \n",(ramWords*8));
	}
      else if((csr&F1_CSR_MODULE_EMPTY)==F1_CSR_MODULE_EMPTY)
	printf("  Events in FIFO           = %d  (Block level = %d)\n",count,blevel);
      else
	{
	  printf("  Events in FIFO           = %d  (Block level = %d) - Data Available\n",count,blevel);
	  printf("  RAM Level (Bytes)        = %d \n",(ramWords*8));
	}
    
      /* Print out Chip Status */
      f1ChipStatus(id, 0);
    } 
  else 
    {
      /* Print minimal Error status of Chips on the TDC */
      F1LOCK;
      for(jj=0;jj<f1Nchips[id];jj++) 
	{
	  chipstat[jj]  = vmeRead16(&f1p[id]->stat[jj]);
	  if(((chipstat[jj]&F1_CHIP_RES_LOCKED)==0)||(chipstat[jj]&F1_CHIP_ERROR_COND))
	    cmask |= (1<<jj);
	}
      F1UNLOCK;
      if(cmask)
	f1ChipStatus(id,1);
      else
	printf(" Chip Status: ALL Chips - OK \n");
    }

  if(sflag == 2) 
    { /* Print out Chip configuration */
      f1ConfigShow(id,F1_ALL_CHIPS);
    }

  printf("---------------------------------------------------------------------- \n");
  
}

/**
 *  @ingroup Status
 *  @brief Print Summary of Status of all initialized f1TDCs to standard out
 *  @param sflag Status flag
 *   - Not used
 *
 */
void 
f1GStatus(int sFlag)
{
  int itdc, id, ichip;
  struct f1tdc_struct st[20];
  chipInfo ci[20][8];
  int chipCommonConfig[20];
  unsigned int a24addr[20];
  int nchips, ibit = 0;
  unsigned int errmask;
  
  F1LOCK;
  for (itdc=0;itdc<nf1tdc;itdc++) 
    {
      id = f1Slot(itdc);
      a24addr[id]    = (unsigned int)f1p[id] - f1tdcA24Offset;
      st[id].version = vmeRead32(&f1p[id]->version);
      st[id].adr32   = vmeRead32(&f1p[id]->adr32);
      st[id].adr_mb  = vmeRead32(&f1p[id]->adr_mb);

      st[id].ctrl    = vmeRead32(&f1p[id]->ctrl);
      st[id].ctrl2   = vmeRead32(&f1p[id]->ctrl2);

      st[id].csr     = vmeRead32(&f1p[id]->csr);

      st[id].scaler1      = vmeRead32(&f1p[id]->scaler1);
      st[id].trig2_scaler = vmeRead32(&f1p[id]->trig2_scaler);
      st[id].sync_scaler  = vmeRead32(&f1p[id]->sync_scaler);

      st[id].block_count = vmeRead32(&f1p[id]->block_count);
      st[id].blocklevel  = vmeRead32(&f1p[id]->blocklevel);

    }
  F1UNLOCK;

  for (itdc=0;itdc<nf1tdc;itdc++) 
    {
      id = f1Slot(itdc);
      if(f1ChipsHaveSameConfig(id))
	{
	  chipCommonConfig[id]=1;
	  if(f1ClockSource==0) /* Internal */
	    ci[id][0].clk_period = 31.25;
	  else  /* External */
	    ci[id][0].clk_period = 32.;
	  ci[id][0].chip = 0;
	  printf(" before %2d: %lf\n",id,ci[id][0].bin_size);
	  f1ConfigDecode(id,&ci[id][0],0);
	  printf("  after %2d: %lf\n",id,ci[id][0].bin_size);
	}
      else
	{
	  chipCommonConfig[id]=0;
	  for(ichip=0; ichip<f1Nchips[id]; ichip++)
	    {
	      if(f1ClockSource==0) /* Internal */
		ci[id][0].clk_period = 31.25;
	      else  /* External */
		ci[id][0].clk_period = 32.;
	      ci[id][ichip].chip = ichip;
	      f1ConfigDecode(id,&ci[id][ichip],0);
	    }
	}
    }


  printf("\n");
  
  printf("                       f1TDC Module Configuration Summary\n\n");
  printf("     Firmware  ..................Addresses.................\n");
  printf("Slot   Rev        A24         A32      A32 Multiblock Range\n");
  printf("--------------------------------------------------------------------------------\n");

  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);
      printf(" %2d   ",id);

      printf("0x%02x     ",st[id].version&F1_VERSION_FIRMWARE_MASK);

      printf("0x%06x   ",
	     a24addr[id]);

      if(st[id].adr32 &F1_A32_ENABLE)
	{
	  printf("0x%08x  ",
		 (st[id].adr32&F1_A32_ADDR_MASK)<<16);
	}
      else
	{
	  printf("  Disabled  ");
	}

      if(st[id].adr_mb & F1_AMB_ENABLE) 
	{
	  printf("0x%08x-0x%08x",
		 (st[id].adr_mb&F1_AMB_MIN_MASK)<<16,
		 (st[id].adr_mb&F1_AMB_MAX_MASK));
	}
      else
	{
	  printf("Disabled");
	}

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");


  printf("\n");
  printf("      Block  .Signal Sources..                        ....Channel....\n");
  printf("Slot  Level  Clk   Trig   Sync     MBlk  Token  BERR  Enabled Mask\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);
      printf(" %2d   ",id);

      printf("%3d    ",st[id].blocklevel & F1_BLOCKLEVEL_MASK);

      printf("%s ", 
	     (st[id].ctrl & F1_REFCLK_SRC_MASK)==F1_REFCLK_INTERNAL ? " INT " :
	     (st[id].ctrl & F1_REFCLK_SRC_MASK)==F1_REFCLK_VXS ? " VXS " :
	     (st[id].ctrl & F1_REFCLK_SRC_MASK)==F1_REFCLK_FP ? "  FP " :
	     " ??? ");

      printf("%s   ",
	     (st[id].ctrl & F1_TRIGGER_SRC_MASK)==F1_TRIGGER_SRC_SOFT ? "SOFT " :
	     (st[id].ctrl & F1_TRIGGER_SRC_MASK)==F1_TRIGGER_SRC_P0 ? " VXS " :
	     (st[id].ctrl & F1_TRIGGER_SRC_MASK)==F1_TRIGGER_SRC_FP ? " FP  " :
	     " ??? ");

      printf("%s    ",
	     (st[id].ctrl & F1_SYNC_RESET_SRC_MASK)==F1_SYNC_RESET_SRC_SOFT ? "SOFT " :
	     (st[id].ctrl & F1_SYNC_RESET_SRC_MASK)==F1_SYNC_RESET_SRC_FP ? " FP " :
	     (st[id].ctrl & F1_SYNC_RESET_SRC_MASK)==F1_SYNC_RESET_SRC_P0 ? "VXS " :
	     " ??? ");

      printf("%s   ",
	     (st[id].ctrl & F1_ENABLE_MULTIBLOCK) ? "YES":" NO");

      printf("%s",
	     st[id].ctrl & (F1_MB_TOKEN_P0)?"VXS":
	     st[id].ctrl & (F1_MB_TOKEN_P2)?" P2":
	     " NO");
      printf("%s  ",
	     st[id].ctrl & (F1_FIRST_BOARD) ? "-F":
	     st[id].ctrl & (F1_LAST_BOARD) ? "-L":
	     "  ");

      printf("%s   ",
	     st[id].ctrl & F1_ENABLE_BERR ? "YES" : " NO");

      if(((f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8)==3)
	printf("0x%04X %08X",(unsigned short)(~(f1ChannelDisable[id]>>32)&0xFFFF),
	       (unsigned int)(~f1ChannelDisable[id] & 0xFFFFFFFF));
      else
	printf("0x%08X",
	       (unsigned int)(~f1ChannelDisable[id] & 0xFFFFFFFF));

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        f1TDC Chip Configuration\n\n");
  printf("                                       Bin      Full     Rollover\n");
  printf("Slot  Chip    Rez     PL       PTW     Size     Range    Count\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);
      printf(" %2d    ",id);

      nchips=f1Nchips[id];
      if(chipCommonConfig[id]==1)
	nchips=1;

      for(ichip=0; ichip<nchips; ichip++)
	{
	  if(nchips==1)
	    printf("ALL    ");
	  else
	    {
	      if(ichip!=0)
		printf("       ");

	      printf("%d      ",ci[id][ichip].chip);
	    }

	  printf("%s   ",(ci[id][ichip].rez==1)? "High":"Norm");

	  printf("%5.1f   ",ci[id][ichip].window);

	  printf("%5.1f   ",ci[id][ichip].latency);

	  printf("%2.4f   ",ci[id][ichip].bin_size);

	  printf("%5.1f   ",ci[id][ichip].full_range);

	  printf("%d",ci[id][ichip].rollover_count);

	  printf("\n");
	}
	
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        f1TDC Signal Scalers\n\n");
  printf("Slot       Trig1       Trig2   SyncReset\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);
      printf(" %2d   ",id);

      printf("%10d  ", st[id].scaler1);

      printf("%10d  ", st[id].trig2_scaler);

      printf("%10d  ", st[id].sync_scaler);

      printf("\n");
    }

  printf("--------------------------------------------------------------------------------\n");
  printf("\n");
  printf("                          f1TDC Data Status\n\n");
  printf("      Trigger   Block                   ...TDC Chip Error Status..\n");
  printf("Slot  Source    Ready  Blocks In Fifo   Config   Other (Chips with errors)\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);
      printf(" %2d  ",id);

      printf("%s    ",
	     st[id].ctrl2 & F1_GO_DATA ? " Enabled" : "Disabled");

      printf("%s       ",
	     st[id].csr & F1_CSR_BLOCK_READY ? "YES" : " NO");

      printf("%10d   ",
	     st[id].block_count&F1_BLOCK_COUNT_MASK);

      printf("%s    ",
	     st[id].csr & F1_CSR_CONFIG_ERROR ? "ERROR" : "  OK " );

      errmask=F1_CSR_ERROR_MASK;
      if(((f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8)==2)
	{
	  errmask = F1_CSR_V3_ERROR_MASK;
	}
      printf("%s ",
	     st[id].csr & errmask ? "ERROR" : "  OK " );
      if(st[id].csr & errmask)
	{
	  ibit=0;
	  printf("(");
	  for(ibit=0; ibit<f1Nchips[id]; ibit++)
	    {
	      if(((st[id].csr & errmask)>>8) & (1<<ibit))
		printf("%d",ibit);
	      else
		printf(" ");
	    }
	  printf(")");
	}

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("\n");
  
}

/**
 *  @ingroup Status
 *  @brief Print Status of f1TDC chips to standard out
 *  @param id 
 *   - Slot Number
 *  @param pflag Print to standard out flag
 *   -  0: Show minimal information
 *   - !0: Show description of error latch, if present
 *
 */
void
f1ChipStatus(int id, int pflag)
{

  int ii, reg, lock, latch, stat;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return;
    }

  F1LOCK;
  for(ii=0;ii<f1Nchips[id];ii++) 
    {
      chipstat[ii]  = vmeRead16(&f1p[id]->stat[ii]);
    }
  F1UNLOCK;

  printf("\n CHIP Status: (slot %d)\n",id);
  for(ii=0; ii<f1Nchips[id]; ii++) 
    {
      reg = chipstat[ii]&0xffff;
      lock = reg&F1_CHIP_RES_LOCKED;
      stat = reg&F1_CHIP_STAT_MASK;
      latch = (reg&F1_CHIP_LATCH_STAT_MASK)>>8;
      if(!(reg&F1_CHIP_INITIALIZED)) 
	{
	  printf("   CHIP %d  Reg = 0x%04x -  NOT Initialized  \n",ii,reg);
	}
      else
	{
	  if(lock==0) 
	    {
	      printf("   CHIP %d  Reg = 0x%04x ** Resolution NOT Locked **\n",ii,reg);
	    }
	  else if(latch) 
	    {
	      printf("   CHIP %d  Reg = 0x%04x ** Check Latched Status **\n",ii,reg);
	      if(pflag) 
		{
		  if(lock==1)
		    printf("        Resolution locked\n");
		  if(latch&F1_CHIP_RES_LOCKED)
		    printf("        Loss of Resolution Lock occurred\n");
		  if(latch&F1_CHIP_HITFIFO_OVERFLOW)
		    printf("        Hit FIFO Overflow occurred\n");
		  if(latch&F1_CHIP_TRIGFIFO_OVERFLOW)
		    printf("        Trigger FIFO Overflow occured\n");
		  if(latch&F1_CHIP_OUTFIFO_OVERFLOW)
		    printf("        Output FIFO Overflow occured\n");
		  if(latch&F1_CHIP_EXTFIFO_FULL)
		    printf("        External FIFO Full occured\n");
		}
	    }
	  else
	    {
	      printf("   CHIP %d  Reg = 0x%04x - OK\n",ii,reg);
	      if(pflag) 
		{
		  if(stat&F1_CHIP_HITFIFO_OVERFLOW)
		    printf("        Hit FIFO has Overflowed\n");
		  if(stat&F1_CHIP_TRIGFIFO_OVERFLOW)
		    printf("        Trigger has FIFO Overflowed\n");
		  if(stat&F1_CHIP_OUTFIFO_OVERFLOW)
		    printf("        Output has FIFO Overflowed\n");
		  if(stat&F1_CHIP_EXTFIFO_FULL)
		    printf("        External FIFO is Full\n");
		  if(stat&F1_CHIP_EXTFIFO_ALMOST_FULL)
		    printf("        External FIFO Almost Full (Busy Asserted)\n");
		  if((stat&F1_CHIP_EXTFIFO_EMPTY) == 0)
		    printf("        External FIFO NOT Empty\n");
		}
	    }	
	}

    }      
}

/**
 *  @ingroup Readout
 *  @brief General Data readout routine
 *
 *  @param  id     Slot number of module to read
 *  @param  data   local memory address to place data
 *  @param  nwrds  Max number of words to transfer
 *  @param  rflag  Readout Flag
 * <pre>
 *              0 - programmed I/O from the specified board
 *              1 - DMA transfer using Universe/Tempe DMA Engine 
 *                    (DMA VME transfer Mode must be setup prior)
 *              2 - Multiblock DMA transfer (Multiblock must be enabled
 *                     and daisychain in place or SD being used)
 * </pre>
 *  @return Number of words inserted into data if successful.  Otherwise ERROR.
 */
int
f1ReadBlock(int id, volatile UINT32 *data, int nwrds, int rflag)
{
  int ii;
  int stat, retVal, xferCount;
  int dCnt, berr=0;
  int dummy=0;
  volatile unsigned int *laddr;
  unsigned int bhead, val;
  unsigned int vmeAdr, csr=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1ReadBlock: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  if(data==NULL) 
    {
      logMsg("f1ReadBlock: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  f1BlockError=F1_BLOCKERROR_NO_ERROR;
  if(nwrds <= 0) nwrds=F1_MAX_TDC_CHANNELS*F1_MAX_HITS_PER_CHANNEL;

  F1LOCK;
  if(rflag >= 1) { /* Block Transfers */
    
    /*Assume that the DMA programming is already setup. */
    /* Don't Bother checking if there is valid data - that should be done prior
       to calling the read routine */

    /* Check for 8 byte boundary for address - insert dummy word (Slot 30 TDC DATA 0xffff)*/
    if((unsigned long) (data)&0x7) 
      {
#ifdef VXWORKS
	*data = F1_DUMMY_DATA;
#else
	*data = LSWAP(F1_DUMMY_DATA);
#endif
	dummy = 1;
	laddr = (data + 1);
      } 
    else 
      {
	dummy = 0;
	laddr = data;
      }

    if(rflag == 2) 
      { /* Multiblock Mode */
	if((vmeRead32(&f1p[id]->ctrl)&F1_FIRST_BOARD)==0) 
	  {
	    logMsg("f1ReadBlock: ERROR: TDC in slot %d is not First Board\n",id,0,0,0,0,0);
	    F1UNLOCK;
	    return(ERROR);
	  }
	vmeAdr = (unsigned int)(f1pmb) - f1tdcA32Offset;
      }
    else
      {
	vmeAdr = (unsigned int)(f1pd[id]) - f1tdcA32Offset;
      }

#ifdef VXWORKS
    retVal = sysVmeDmaSend((UINT32)laddr, vmeAdr, (nwrds<<2), 0);
#else
    retVal = vmeDmaSend((UINT32)laddr, vmeAdr, (nwrds<<2));
#endif

    if(retVal != 0) 
      {
	logMsg("f1ReadBlock: ERROR in DMA transfer Initialization 0x%x\n",retVal,0,0,0,0,0);
	F1UNLOCK;
	return(retVal);
      }
    /* Wait until Done or Error */
#ifdef VXWORKS
    retVal = sysVmeDmaDone(10000,1);
#else
    retVal = vmeDmaDone();
#endif

    if(retVal > 0) 
      {
	/* Check to see that Bus error was generated by TDC */
	if(rflag == 2) 
	  {
	    csr = vmeRead32(&f1p[f1MaxSlot]->csr);  /* from Last TDC */
	  }
	else
	  {
	    csr = vmeRead32(&f1p[id]->csr);  /* from TDC id */
	  }

	stat = (csr)&F1_CSR_BERR_STATUS;

	if((retVal > 0) && (stat)) 
	  {
#ifdef VXWORKS
	    xferCount = (nwrds - (retVal>>2) + dummy);  /* Number of Longwords transfered */
#else
	    xferCount = (retVal>>2) + dummy;  /* Number of Longwords transfered */
#endif
	    F1UNLOCK;
	    return(xferCount); /* Return number of data words transfered */
	  }
	else
	  {
#ifdef VXWORKS
	    xferCount = (nwrds - (retVal>>2) + dummy);  /* Number of Longwords transfered */
#else 
	    xferCount = (retVal>>2) + dummy;  /* Number of Longwords transfered */
#endif
	    logMsg("f1ReadBlock: DMA transfer terminated by unknown BUS Error (csr=0x%x nwrds=%d)\n",
		   csr, xferCount, 0, 0, 0, 0);
	    f1BlockError=F1_BLOCKERROR_UNKNOWN_BUS_ERROR;
	    F1UNLOCK;
	    if(rflag == 2) 
	      f1GetTokenStatus(1);

	    return(xferCount);
	  }
      } 
    else if (retVal == 0)
      { /* Block Error finished without Bus Error */
#ifdef VXWORKS
	logMsg("f1ReadBlock: WARN: DMA transfer terminated by word count 0x%x\n",nwrds,0,0,0,0,0);
	f1BlockError=F1_BLOCKERROR_TERM_ON_WORDCOUNT;
#else
	logMsg("f1ReadBlock: WARN: DMA transfer returned zero word count 0x%x\n",nwrds,0,0,0,0,0);
	f1BlockError=F1_BLOCKERROR_ZERO_WORD_COUNT;
#endif
	F1UNLOCK;
	if(rflag == 2) 
	  f1GetTokenStatus(1);

	return(nwrds);
      } 
    else 
      {  /* Error in DMA */
#ifdef VXWORKS
	logMsg("f1ReadBlock: ERROR: sysVmeDmaDone returned an Error\n",0,0,0,0,0,0);
#else
	logMsg("f1ReadBlock: ERROR: vmeDmaDone returned an Error\n",0,0,0,0,0,0);
#endif
	f1BlockError=F1_BLOCKERROR_DMADONE_ERROR;
	F1UNLOCK;
	if(rflag == 2) 
	  f1GetTokenStatus(1);

	return(retVal);
      }

  } 
  else 
    {  /*Programmed IO */

      /* Check if Bus Errors are enabled. If so then disable for Prog I/O reading */
      berr = vmeRead32(&f1p[id]->ctrl)&F1_ENABLE_BERR;
      if(berr)
	vmeWrite32(&f1p[id]->ctrl, vmeRead32(&f1p[id]->ctrl) & ~F1_ENABLE_BERR);

      dCnt = 0;
      /* Read Block Header - should be first word */
      bhead = (unsigned int) *f1pd[id];
#ifndef VXWORKS
      bhead = LSWAP(bhead);
#endif

      if((bhead&F1_DATA_TYPE_DEFINE)&&((bhead&F1_DATA_TYPE_MASK) == F1_DATA_BLOCK_HEADER)) 
	{
#ifdef VXWORKS
	  data[dCnt] = bhead;
#else
	  data[dCnt] = LSWAP(bhead); /* Swap back to little-endian */
#endif
	  dCnt++;
	}
      else
	{
	  /* We got bad data - Check if there is any data at all */
	  if( (vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK) == 0) 
	    {
	      logMsg("f1ReadBlock: FIFO Empty (0x%08x)\n",bhead,0,0,0,0,0);
	      F1UNLOCK;
	      return(0);
	    } 
	  else 
	    {
	      logMsg("f1ReadBlock: ERROR: Invalid Header Word 0x%08x\n",bhead,0,0,0,0,0);
	      F1UNLOCK;
	      return(ERROR);
	    }
	}

      ii=0;
      while(ii<nwrds) 
	{
	  val = (unsigned int) *f1pd[id];
	  data[ii+1] = val;
#ifndef VXWORKS
	  val = LSWAP(val);
#endif
	  if( (val&F1_DATA_TYPE_DEFINE) 
	      && ((val&F1_DATA_TYPE_MASK) == F1_DATA_BLOCK_TRAILER) )
	    break;
	  ii++;
	}
      ii++;
      dCnt += ii;

      if(berr)
	{
	  vmeWrite32(&f1p[id]->ctrl, vmeRead32(&f1p[id]->ctrl) | F1_ENABLE_BERR);
	}

      F1UNLOCK;
      return(dCnt);
    }

  F1UNLOCK;
  return(OK);
}

/**
 *  @ingroup Status
 *  @brief Return the type of error that occurred while attempting a
 *    block read from f1ReadBlock.
 *  @param pflag
 *     - >0: Print error message to standard out
 *  @sa faReadBlock
 *  @return OK if successful, otherwise ERROR.
 */
int
f1GetBlockError(int pflag)
{
  int rval=0;
  const char *block_error_names[F1_BLOCKERROR_NTYPES] =
    {
      "NO ERROR",
      "DMA Terminated on Word Count",
      "Unknown Bus Error",
      "Zero Word Count",
      "DmaDone Error"
    };

  rval = f1BlockError;
  if(pflag)
    {
      if(rval!=F1_BLOCKERROR_NO_ERROR)
	{
	  logMsg("f1GetBlockError: Block Transfer Error: %s\n",
		 block_error_names[rval],2,3,4,5,6);
	}
    }

  return rval;
}

/**
 *  @ingroup Readout
 *  @brief Readout and print event to standard out.
 *
 *  @param  id     Slot number of module to read
 *  @param  rflag  Not used
 *  @return Number of words read if successful.  Otherwise ERROR.
 */
int
f1PrintEvent(int id, int rflag)
{
  int ii, evnum, trigtime, MAXWORDS=64*8;
  int dCnt, berr=0;
  unsigned int head, tail=0, val, chip;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("f1PrintEvent: ERROR : TDC in slot %d is not initialized \n",id);
      return(ERROR);
    }

  /* Check if data available */
  F1LOCK;
  if((vmeRead32(&f1p[id]->ev_count)&F1_EVENT_COUNT_MASK)==0) 
    {
      printf("f1PrintEvent: ERROR: FIFO Empty\n");
      F1UNLOCK;
      return(0);
    }
  F1UNLOCK;

  /* Check if Bus Errors are enabled. If so then disable for reading */
  F1LOCK;
  berr = vmeRead32(&f1p[id]->ctrl)&F1_ENABLE_BERR;
  if(berr)
    vmeWrite32(&f1p[id]->ctrl,vmeRead32(&f1p[id]->ctrl) & ~F1_ENABLE_BERR);
  F1UNLOCK;
  
  dCnt = 0;
  /* Read Header - */
  F1LOCK;
  head = (unsigned int) *f1pd[id];
  F1UNLOCK;
#ifndef VXWORKS
  head = LSWAP(head);
#endif

  /* check if valid data */
  if((head&F1_DATA_SLOT_MASK) == F1_DATA_INVALID) 
    {
      printf("f1PrintEvent: ERROR: Invalid Data from FIFO 0x%08x\n",head);
      return(ERROR);
    }

  if((head&F1_HT_DATA_MASK) != F1_HEAD_DATA) 
    {
      printf("f1PrintEvent: ERROR: Invalid Header Word 0x%08x\n",head);
      return(ERROR);
    }
  else
    {
      printf("TDC DATA for Module in Slot %d\n",id);
      evnum = (head&F1_HT_EVENT_MASK)>>16;
      trigtime = (head&F1_HT_TRIG_MASK)>>7;
      chip = (head&F1_HT_CHIP_MASK)>>3;
      dCnt++;
      printf("  Header  = 0x%08x(chip %d)   Event = %d   Trigger Time = %d ",head,chip,evnum,trigtime); 
    }

  ii=0;
  while(ii<MAXWORDS) 
    {
      if((ii%5) == 0) printf("\n    ");
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if((val&F1_HT_DATA_MASK) == F1_TAIL_DATA) 
	{
	  tail = val;
	  ii++;
	  printf("\n");
	  break;
	}
      else if ((val&F1_HT_DATA_MASK) == F1_HEAD_DATA) 
	{
	  chip = (val&F1_HT_CHIP_MASK)>>3;
	  printf("  0x%08x(H%d T%d)",val,chip,(val&F1_HT_TRIG_MASK)>>7);
	  ii++;
	}
      else
	{
	  printf("  0x%08x    ",val);
	  ii++;
	}
    }
  dCnt += ii;

#ifdef VERSION1_KILL
  /* Check if this is an end of Block and there is a filler word. If 
     so then it should be read out and discarded */
  F1LOCK;
  status = vmeRead32(&f1p[id]->csr);
  F1UNLOCK;

  if((status&F1_CSR_NEXT_BUF_NO)&&(status&F1_CSR_FILLER_FLAG)) 
    {
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if(val&F1_DATA_SLOT_MASK)
	printf("f1PrintData: ERROR: Invalid filler word 0x%08x\n",val);

      printf("  Trailer = 0x%08x   Word Count = %d  Filler = 0x%08x\n",tail,dCnt,val);
    }
  else
#endif // VERSION1
    {
      printf("  Trailer = 0x%08x   Word Count = %d\n",tail,dCnt);
    }

  if(berr)
    {
      F1LOCK;
      vmeWrite32(&f1p[id]->ctrl,vmeRead32(&f1p[id]->ctrl) | F1_ENABLE_BERR);
      F1UNLOCK;
    }

  return(dCnt);
  
}

/**
 *  @ingroup Readout
 *  @brief Routine to flush a partial event from the FIFO. Read until a valid trailer is found 
 *
 *  @param id 
 *   - Slot Number
 *
 *  @return OK if successful, otherwise ERROR.
*/
int
f1FlushEvent(int id)
{
  /*   int ii, evnum, trigtime, MAXWORDS=64*8; */
  int ii, MAXWORDS=64*8;
  int berr=0;
  unsigned int tail, val;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1FlushEvent: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  /* Check if data available - If not then just issue a Clear */
  F1LOCK;
  if((vmeRead32(&f1p[id]->ev_count)&F1_EVENT_COUNT_MASK)==0) 
    {
      F1UNLOCK;
      f1Clear(id);
      return(0);
    }
  F1UNLOCK;

  /* Check if Bus Errors are enabled. If so then disable for reading */
  F1LOCK;
  berr = vmeRead32(&f1p[id]->ctrl)&F1_ENABLE_BERR;
  if(berr)
    vmeWrite32(&f1p[id]->ctrl, vmeRead32(&f1p[id]->ctrl) & ~F1_ENABLE_BERR);
  F1UNLOCK;


  ii=0;
  while(ii<MAXWORDS) 
    {
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if( ((val&F1_HT_DATA_MASK) == F1_TAIL_DATA) && ((val&F1_DATA_SLOT_MASK)!=F1_DATA_INVALID)) 
	{
	  tail = val;
	  ii++;
	  break;
	}
      else
	{
	  ii++;
	}
    }

#ifdef VERSION1_KILL
  /* Check if this is an end of Block and there is a filler word. If 
     so then it should be read out and discarded */
  F1LOCK;
  status = vmeRead32(&f1p[id]->csr);
  F1UNLOCK;
  if((status&F1_CSR_NEXT_BUF_NO)&&(status&F1_CSR_FILLER_FLAG)) 
    {
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      ii++;
      if(val&F1_DATA_SLOT_MASK)
	logMsg("f1FlushEvent: ERROR: Invalid filler word 0x%08x\n",val,0,0,0,0,0);

    }
#endif // VERSION1

  if(berr)
    {
      F1LOCK;
      vmeWrite32(&f1p[id]->ctrl, vmeRead32(&f1p[id]->ctrl) | F1_ENABLE_BERR);
      F1UNLOCK;
    }  
  return(ii);
  
}

/**
 *  @ingroup Readout
 *  @brief Readout and print event from all initialized f1TDCs to standard out
 *
 *  @param  rflag  Not used
 *  @return Number of words read if successful.  Otherwise ERROR.
 */
int
f1GPrintEvent(int rflag)
{
  int ii, id, count, total=0;
  int mask=0, scan=0;

  /*check for data in all TDCs*/
  for(ii=0;ii<nf1tdc;ii++) 
    {
      mask |= (1<<(f1ID[ii]));
    }
  scan = f1DataScan(0);

  if((scan !=0)&&(scan == mask)) 
    {
      for(ii=0;ii<nf1tdc;ii++) 
	{
	  id = f1ID[ii];
	  count = f1PrintEvent(id,rflag);
	  total += count;
	  printf("\n");
	}

      printf("f1GPrintEvent: TOTALS:  TDCs = %d  Word Count = %d\n",nf1tdc,total);
      return(total);

    } 
  else 
    {
      printf("f1GPrintEvent: ERROR: Not all modules have data  scan = 0x%x mask = 0x%x\n",scan,mask);
      return(0);
    }

}

/**
 * @ingroup Config
 * @brief Perform a soft reset on the f1TDC module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1Clear(int id)
{
  f1SoftReset(id);
}

/**
 * @ingroup Config
 * @brief Perform a soft reset on all initialized f1TDC modules
 *
 */
void
f1GClear()
{
  f1GSoftReset();
}

/**
 * @ingroup Config
 * @brief Perform a soft reset on the f1TDC module
 *
 *  @param id 
 *   - Slot Number
 */
void
f1SoftReset(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Reset: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->csr, F1_CSR_SOFT_RESET);
  taskDelay(30);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Perform a soft reset on all initialized f1TDC modules
 *
 */
void
f1GSoftReset()
{
  int if1;
  
  F1LOCK;
  for(if1 = 0; if1 < nf1tdc; if1++) 
    {
      vmeWrite32(&f1p[f1Slot(if1)]->csr, F1_CSR_SOFT_RESET);
    }
  taskDelay(30);
  F1UNLOCK;

}


/**
 * @ingroup Config
 * @brief Clear the latched error status of specified f1TDC chips in the chipMask
 *
 * @param id 
 *   - Slot Number
 * @param chipMask
 *  - Mask for f1TDC chips to clear latched error status
 *
 */
void
f1ClearStatus(int id, unsigned int chipMask)
{
  
  int ii;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1ClearStatus: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  /* Default clear all chips latched status bits */
  if(chipMask<=0) chipMask=F1_ALL_CHIPS;
  
  F1LOCK;
  for(ii=0;ii<f1Nchips[id];ii++)
    {
      if(chipMask&(1<<ii))
	{
	  vmeWrite16(&f1p[id]->stat[ii], F1_CHIP_CLEAR_STATUS);
	}
    }
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Clear the latched error status of specified f1TDC chips in the chipMask in
 *        all initialized f1TDCs
 *
 * @param chipMask
 *  - Mask for f1TDC chips to clear latched error status
 *
 */
void
f1GClearStatus(unsigned int chipMask)
{
  int ii, id;

  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1ClearStatus(id,chipMask);
    }
}

/**
 * @ingroup Status
 * @brief Return the Error status for all the f1TDC chips on the module 
 *
 * @param id 
 *   - Slot Number
 * @param sflag Status flag
 *   -  0: Just read the status and return error info
 *   - !0: Do full latch and persistance check of all chips
 *
 * @return Mask of chips that report a latched error status
 */
unsigned int
f1ErrorStatus(int id, int sflag)
{
  int jj;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int err=0;
  unsigned int cmask=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1ErrorStatus: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(0);
    }

  if(sflag==1) 
    {    /* Do full latch and persistance check of all chips */
      F1LOCK;
      for(jj=0;jj<f1Nchips[id];jj++) 
	{
	  chipstat[jj]  = vmeRead16(&f1p[id]->stat[jj]);
	  if(((chipstat[jj]&F1_CHIP_RES_LOCKED)==0)||(chipstat[jj]&F1_CHIP_ERROR_COND))
	    cmask |= (1<<jj);
	}
      F1UNLOCK;
      return(cmask);

    } 
  else 
    {  /* Just Read CSR to get error info */
      F1LOCK;
      err = vmeRead32(&f1p[id]->csr); 
      F1UNLOCK;

      if(err&F1_CSR_ERROR_MASK)                   /* an Error condition exists */
	return((err&F1_CSR_ERROR_MASK)>>8);
      else
	return(0);    
    }

}

/**
 * @ingroup Status
 * @brief Return the Error status for all the f1TDC chips on all initialized modules
 *
 * @param sflag Status flag
 *   -  0: Just read the status and return error info
 *   - !0: Do full latch and persistance check of all chips
 *
 * @return Slot Mask of f1TDCs that report a latched error status
 */
unsigned int
f1GErrorStatus(int sflag)
{
  int ii, jj, id, count;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int emask=0;

  if(sflag==1) 
    { /* Do full latch and persistance check of all chips */
      F1LOCK;
      for(ii=0;ii<nf1tdc;ii++) 
	{
	  id = f1ID[ii];
	  for(jj=0;jj<f1Nchips[id];jj++) 
	    {
	      chipstat[jj]  = vmeRead16(&f1p[id]->stat[jj]);
	      if(((chipstat[jj]&F1_CHIP_RES_LOCKED)==0)||(chipstat[jj]&F1_CHIP_ERROR_COND))
		emask |= (1<<id);
	    }
	}
      F1UNLOCK;

      return(emask);

    }
  else
    {  /* Do quick check of CSR registers */
      F1LOCK;
      for(ii=0;ii<nf1tdc;ii++) 
	{
	  id = f1ID[ii];
	  count = (vmeRead32(&f1p[id]->csr)&F1_CSR_ERROR_MASK);
	  if(count)
	    emask |= (1<<id);
	}
      F1UNLOCK;

      return(emask);
    }

}

/**
 * @ingroup Status
 * @brief Get Resolution lock status for all chips on the board
 *
 *  @param id 
 *   - Slot Number
 *
 * @return  0 if all chips are locked, else a bitmask of unlocked chips, otherwise ERROR
 */
int
f1CheckLock(int id)
{
  int jj;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int cmask=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1CheckLock: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

#ifdef VXWORKS
  /* Get Chip status Register data in longword chunks - faster than in 16bit mode*/
  bcopyLongs((char *)&f1p[id]->stat[0],(char *)&chipstat[0],4);
#else
  F1LOCK;
  for(jj=0;jj<8;jj++) 
    {
      chipstat[jj] = vmeRead16(&f1p[id]->stat[jj]);
    }
  F1UNLOCK;
#endif

  for(jj=0;jj<f1Nchips[id];jj++) 
    {
      if((chipstat[jj]&F1_CHIP_RES_LOCKED)==0)
	cmask |= (1<<jj);
    }

  return(cmask);
}

/**
 * @ingroup Status
 * @brief Get Resolution lock status for all chips on all initialized f1TDCs
 *
 * @param pflag Print Flag
 *   - !0: Show which slot contains unlocked chips to standard out
 * @return  0 if all chips are locked, else a slot mask of modules with unlocked chips, otherwise ERROR
 */
int
f1GCheckLock(int pflag)
{
  int ii, id, mmask = 0;
  unsigned int emask=0,stat;

  if(nf1tdc<=0) 
    {
      logMsg("f1GCheckLock: ERROR: No TDCs Initialized \n",0,0,0,0,0,0);
      return(ERROR);
    }

  /* Do quick check of CSR registers */
  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      stat = (vmeRead32(&f1p[id]->csr)&F1_CSR_ERROR_MASK);
      /* If there is an error check if it is Resolution NOT locked */
      if(stat) 
	{
	  emask = f1CheckLock(id);
	  if(emask) 
	    {
	      mmask |= (1<<id);
	      if(pflag) printf("TDC Slot %d  Unlocked chip mask 0x%02x\n",id,emask);
	    }
	}
    }
  F1UNLOCK;

  return(mmask);
}

/**
 * @ingroup Config
 * @brief Perform a hard reset of the module
 *
 *      Hard reset of module.  All f1TDC chip registers are re-written.
 *      A32 and A32 Multiblock settings are not restored.
 *
 *  @param id 
 *   - Slot Number
 *  @param iFlag
 *   - Not used
 */
void
f1Reset(int id, int iFlag)
{
  unsigned int a32addr, addrMB;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Reset: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  if((iFlag<0)||(iFlag>3)) iFlag=2;  /* Default to Normal Syncronous */

  F1LOCK;
  a32addr = vmeRead32(&f1p[id]->adr32);
  addrMB  = vmeRead32(&f1p[id]->adr_mb);

  vmeWrite32(&f1p[id]->csr, F1_CSR_HARD_RESET);
  taskDelay(30);
  F1UNLOCK;

  f1ConfigWrite(id, (int *) &f1ConfigData[iFlag], F1_ALL_CHIPS);
    
#ifdef SKIPTHIS
  F1LOCK;
  vmeWrite32(&f1p[id]->adr32, a32addr);
  vmeWrite32(&f1p[id]->adr_mb, addrMB);
  F1UNLOCK;

  // FIXME: Change this, if using SD
  f1EnableClk(id,0);
#endif

  f1ClearStatus(id,F1_ALL_CHIPS);

}




/**
 * @ingroup Config
 * @brief Perform a software Sync Reset on the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1SyncReset(int id)
{
  unsigned int ctrl=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SyncReset: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  F1LOCK;
  ctrl = vmeRead32(&f1p[id]->ctrl);
  if(ctrl & F1_ENABLE_SOFT_CONTROL)
    {
      if((ctrl&F1_SYNC_RESET_SRC_MASK) == F1_SYNC_RESET_SRC_SOFT)
	vmeWrite32(&f1p[id]->csr, F1_CSR_SYNC_RESET);
      else
	logMsg("f1SyncReset: ERROR: Software Sync Reset not enabled",0,0,0,0,0,0);
    }
  else
    {
	logMsg("f1SyncReset: ERROR: Software Control Signals not enabled",0,0,0,0,0,0);
    }
  F1UNLOCK;
}

/**
 * @ingroup Config
 * @brief Perform a software Sync Reset for all initialized modules
 *
 */
void
f1GSyncReset()
{
  unsigned int ctrl=0;
  int ii, id;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      ctrl = vmeRead32(&f1p[id]->ctrl);
      if(ctrl & F1_ENABLE_SOFT_CONTROL)
	{
	  if((ctrl&F1_SYNC_RESET_SRC_MASK) == F1_SYNC_RESET_SRC_SOFT)
	    vmeWrite32(&f1p[id]->csr, F1_CSR_SYNC_RESET);
	  else
	    logMsg("f1GSyncReset: ERROR: Software Sync Reset not enabled for TDC in slot %d",id,0,0,0,0,0);
	}
      else
	{
	  logMsg("f1GSyncReset: ERROR: Software Control Signals not enabled for TDC in slot %d",
		 id,0,0,0,0,0);
	}
    }
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Issue a software trigger to the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1Trig(int id)
{
  unsigned int ctrl=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Trig: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  F1LOCK;
  ctrl = vmeRead32(&f1p[id]->ctrl);
  if(ctrl & F1_ENABLE_SOFT_CONTROL)
    {
      if((ctrl&F1_TRIGGER_SRC_MASK) == F1_TRIGGER_SRC_SOFT)
	vmeWrite32(&f1p[id]->csr, F1_CSR_TRIGGER);
      else
	logMsg("f1Trig: ERROR: Software Triggers not enabled",0,0,0,0,0,0);
    }
  else
    {
      logMsg("f1Trig: ERROR: Software Control Signals not enabled",0,0,0,0,0,0);
    }
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Issue a software trigger to all initialized modules
 *
 *
 */
void
f1GTrig()
{
  unsigned int ctrl=0;
  int ii, id;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      ctrl = vmeRead32(&f1p[id]->ctrl);
      if(ctrl & F1_ENABLE_SOFT_CONTROL)
	{
	  if((ctrl&F1_TRIGGER_SRC_MASK) == F1_TRIGGER_SRC_SOFT)
	    vmeWrite32(&f1p[id]->csr, F1_CSR_TRIGGER);
	  else
	    logMsg("f1GTrig: ERROR: Software Triggers not enabled for TDC in slot %d",id,0,0,0,0,0);
	}
      else
	{
	  logMsg("f1GTrig: ERROR: Software Control Signals not enabled for TDC in slot %d",
		 id,0,0,0,0,0);
	}
    }
  F1UNLOCK;

}

/**
 * @ingroup Deprec
 * @brief Issue a software Start signal to the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1Start(int id)
{
  unsigned int ctrl=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Start: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  F1LOCK;
  ctrl = vmeRead32(&f1p[id]->ctrl);
  if(ctrl & F1_ENABLE_SOFT_CONTROL)
    {
      if((ctrl&F1_START_SRC_MASK) == F1_START_SRC_SOFT)
	vmeWrite32(&f1p[id]->csr, F1_CSR_START);
      else
	logMsg("f1Start: ERROR: Software Start not enabled",0,0,0,0,0,0);
    }
  else
    {
      logMsg("f1Start: ERROR: Software Control Signals not enabled",0,0,0,0,0,0);
    }
  F1UNLOCK;

}

/**
 * @ingroup Deprec
 * @brief Issue a software Start signal to all initialized modules
 *
 *
 */
void
f1GStart()
{
  unsigned int ctrl=0;
  int ii, id;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      ctrl = vmeRead32(&f1p[id]->ctrl);
      if(ctrl & F1_ENABLE_SOFT_CONTROL)
	{
	  if((ctrl&F1_START_SRC_MASK) == F1_START_SRC_SOFT)
	    vmeWrite32(&f1p[id]->csr, F1_CSR_START);
	  else
	    logMsg("f1GStart: ERROR: Software Start not enabled for TDC in slot %d",id,0,0,0,0,0);
	}
      else
	{
	  logMsg("f1GStart: ERROR: Software Control Signals not enabled for TDC in slot %d",
		 id,0,0,0,0,0);
	}
    }
  F1UNLOCK;

}



/**
 * @ingroup Readout
 * @brief Determine if an event is ready for readout on the module
 *
 *  @param id 
 *   - Slot Number
 *  @return Number of events ready for readout, otherwise ERROR
 */
/* Return Event count for TDC in slot id */
int
f1Dready(int id)
{
  int rval;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Dready: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  F1LOCK;
  rval = vmeRead32(&f1p[id]->ev_count)&F1_EVENT_COUNT_MASK;
  F1UNLOCK;

  return(rval);
}

/**
 * @ingroup Readout
 * @brief Return block available for readout status of the module
 *
 *  @param id 
 *   - Slot Number
 * @return 1 if block available for readout, 0 if not, otherwise ERROR
 */
/* Return True if there is an event Block ready for readout */
int
f1Bready(int id)
{
  int stat;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Bready: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  F1LOCK;
  stat = (vmeRead32(&f1p[id]->csr)&F1_CSR_BLOCK_READY)>>4;
  F1UNLOCK;

  return(stat);
}

/**
 * @ingroup Readout
 * @brief Return the mask of all initialized modules with blocks available for readout
 *
 * @return mask of all initialized modules with blocks available for readout
 */
unsigned int
f1GBready()
{
  int ii, id, stat;
  unsigned int dmask=0;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      stat = vmeRead32(&f1p[id]->csr)&F1_CSR_BLOCK_READY;
      if(stat)
	dmask |= (1<<id);
    }
  F1UNLOCK;
  
  return(dmask);
}


/**
 * @ingroup Readout
 * @brief Return the mask of all initialized modules with events available for readout
 * @param pflag Print Flag
 *   - >0: Print event count of each module to standard out.
 * @return mask of all initialized modules with events available for readout
 */
/* Return Slot mask for modules with data avaialable */
int
f1DataScan(int pflag)
{
  int ii, count, id, dmask=0;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      count = vmeRead32(&f1p[id]->ev_count)&F1_EVENT_COUNT_MASK;
      if(count)
	dmask |= (1<<id);

      if(pflag) logMsg(" F1TDC %2d Slot %2d  Count=%d\n",ii,id,count,0,0,0);
    }
  F1UNLOCK;
  
  return(dmask);
}

/**
 * @ingroup Status
 * @brief Return the mask of all initialized modules
 *
 * @return mask of all initialized modules
 */
/* return Scan mask for all Initialized TDCs */
unsigned int
f1ScanMask()
{
  int ii, id, dmask=0;

  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      dmask |= (1<<id);
    }
  
  return(dmask);
}


/**
 * @ingroup Status
 * @brief Return the mask of f1TDCs chips on the module that are set in high resolution mode
 *
 *  @param id 
 *   - Slot Number
 *  @return mask of f1TDCs chips on the module that are set in high resolution mode, otherwise ERROR
 */
int
f1GetRez(int id)
{
  int ii, rez = 0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1GetRez: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  F1LOCK;
  for(ii=0;ii<f1Nchips[id];ii++) 
    {
      if(f1ReadChip(id, ii, 1)&F1_HIREZ_MODE)
	rez |= (1<<ii);
    }
  F1UNLOCK;
  
  return(rez);

}

/**
 * @ingroup Config
 * @brief Set the window parameters for specified f1TDC chips in chipMask on the module
 *
 *  @param id 
 *   - Slot Number
 *  @param window
 *   - Window size
 *  @param latency
 *   - Window latency
 *  @param chipMask
 *   - Mask of chips to write window settings
 *
 *  @return OK if successful, otherwise ERROR
 */
int
f1SetWindow(int id, int window, int latency, int chipMask)
{
  int ichip, jj, enMask;
  int exponent, refclkdiv, hsdiv;
  int tframe, winMax, latMax;
  unsigned int rval, config[16];
  float clk_period;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SetWindow: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  if(chipMask<=0) chipMask = F1_ALL_CHIPS;
  enMask = f1Enabled(id);

  if(f1ClockSource==0) /* Internal */
    clk_period = 31.25;
  else  /* External */
    clk_period = 32.;
  
  for(ichip=0;ichip<f1Nchips[id];ichip++) 
    {
      if(chipMask&(1<<ichip)) 
	{
	  f1ConfigRead(id,config,ichip);

	  /* Check if window and latency are OK */
	  tframe = clk_period*(((config[7]>>6)&0x1ff) + 2);
	  winMax = (tframe*(0.4));
	  latMax = (tframe*(0.9));

	  if((window>winMax)||(window<=0)) 
	    {
	      logMsg("f1SetWindow: Trig Window for chip %d Out of range. Set to %d ns\n",ichip,winMax,0,0,0,0);
	      window = winMax;
	    }
	  if(latency<window) 
	    {
	      logMsg("f1SetWindow: Trig Latency for chip %d is too small. Set to %d ns\n",ichip,window,0,0,0,0);
	      latency = window;
	    } 
	  else if(latency>latMax) 
	    {
	      logMsg("f1SetWindow: Trig Latency for chip %d Out of range. Set to %d ns\n",ichip,latMax,0,0,0,0);
	      latency = latMax;
	    }
	
	  exponent =  ((config[10])>>8)&0x7;
	  refclkdiv = 1;
	  for(jj = 0; jj<exponent;jj++)
	    refclkdiv = 2 * refclkdiv;
	  hsdiv = (config[10])&0xFF;

	  config[8] = (int) ((float)(152*hsdiv*window)/(float)(clk_period*refclkdiv));
	  config[9] = (int) ((float)(152*hsdiv*latency)/(float)(clk_period*refclkdiv));

	  /* Rewrite Window and Latency registers */
	  F1LOCK;
	  for(jj=8;jj<=9;jj++) 
	    {
	      f1WriteChip(id, ichip, jj, config[jj]);
	      taskDelay(1);
	      rval = f1ReadChip(id, ichip, jj);
	      if(rval != config[jj]) 
		{
		  logMsg("f1SetWindow: Error writing Config (%x != %x) slot=%d\n",rval,config[jj],id,0,0,0);
		}
	    }
	  F1UNLOCK;

	  /*f1ConfigWrite(id,config,(1<<ichip)); */
	} 
    }

  f1ClearStatus(id, F1_ALL_CHIPS);  /* Clear any latched status bits */

  if(enMask)
    f1EnableData(id,enMask);     /* renable any chips that were enabled */

  return(OK);

}

/**
 * @ingroup Config
 * @brief Set the window parameters for specified f1TDC chips in chipMask for all initialized modules
 *
 *  @param window
 *   - Window size
 *  @param latency
 *   - Window latency
 *  @param chipMask
 *   - Mask of chips to write window settings
 *
 *  @return OK if successful, otherwise ERROR
 */
void
f1GSetWindow(int window, int latency, int chipMask)
{
  int id, ii;

  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1SetWindow(id,window,latency,chipMask);
    }

}

/**
 * @ingroup Status
 * @brief Return the value of the CSR register of the module
 *
 *  @param id 
 *   - Slot Number
 *
 * @return Value stored on the CSR register, otherwise ERROR
 */
unsigned int
f1ReadCSR(int id)
{
  unsigned int rval;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1ReadCSR: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(0xffffffff);
    }
  
  F1LOCK;
  rval = vmeRead32(&f1p[id]->csr);
  F1UNLOCK;

  return(rval);
}

/**
 * @ingroup Config
 * @brief Write provided value to the CTRL register of the module
 *
 *  @param id 
 *   - Slot Number
 *  @param val 
 *   - Value to write to CTRL register
 *
 * @return Read value at CTRL register after the write, otherwise ERROR
 */
int
f1WriteControl(int id, unsigned int val)
{
  int rval;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1WriteControl: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl, val);
  rval = vmeRead32(&f1p[id]->ctrl)&F1_CONTROL_MASK;
  F1UNLOCK;

  return(rval);
}

/**
 * @ingroup Config
 * @brief Write provided value to the CTRL register to all initialized modules
 *
 *  @param id 
 *   - Slot Number
 *  @param val 
 *   - Value to write to CTRL register
 */
void
f1GWriteControl(unsigned int val)
{
  int id, ii;
  
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1WriteControl(id, val);
    }
}

/**
 * @ingroup Config
 * @brief Enable f1TDC FPGA data fifo on the module
 *
 *  @param id 
 *   - Slot Number
 * @return OK if successful, otherwise ERROR
 */
int
f1Enable(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Enable: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl2, F1_GO_DATA);
  F1UNLOCK;
  
  return OK;
}

/**
 * @ingroup Config
 * @brief Enable f1TDC FPGA data fifo on all initialized modules
 *
 * @return OK
 */
int
f1GEnable()
{
  int id, ii;
  
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1Enable(id);
    }

  return OK;
}

/**
 * @ingroup Config
 * @brief Disable f1TDC FPGA data fifo on the module
 *
 *  @param id 
 *   - Slot Number
 * @return OK if successful, otherwise ERROR
 */
int
f1Disable(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Disable: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl2, 0);
  F1UNLOCK;
  
  return OK;
}

/**
 * @ingroup Config
 * @brief Disable f1TDC FPGA data fifo on all initialized modules
 *
 * @return OK
 */
int
f1GDisable()
{
  int id, ii;
  
  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1Disable(id);
    }

  return OK;
}


/**
 * @ingroup Status
 * @brief Return enabled/disabled status of FPGA data fifo
 *
 *  @param id 
 *   - Slot Number
 * @return 1 if enabled, 0 if disabled, otherwise ERROR
 */
int
f1Enabled(int id)
{
  int res=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1Enabled: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  F1LOCK;
  res = (vmeRead32(&f1p[id]->ctrl)&0xff0000)>>16;
  F1UNLOCK;
  
  return(res);  /* Return the output FIFO enable mask */
}


/**
 * @ingroup Config
 * @brief Enable data on f1TDC chips specified in chipMask for the module
 *
 *  @param id 
 *   - Slot Number
 * @param chipMask
 *   - Mask of chips to enable
 * @return OK if successful, otherwise ERROR
 */
int
f1EnableData(int id, int chipMask)
{
  int ichip, jj, mask=0;
  unsigned int reg, rval;
  unsigned int allchips_mask=0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EnableData: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  if(chipMask<=0) chipMask = F1_ALL_CHIPS; /* Enable all Chips */

  allchips_mask = ((1<<(f1Nchips[id]))-1);

  /* Mask only the chips on the module */
  chipMask = chipMask & allchips_mask;

  /* Enable FIFOs for each chip */
  mask = (chipMask&0xff)<<16;
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl, vmeRead32(&f1p[id]->ctrl) | mask);

  /* Enable Edges for each chip */
  if((allchips_mask & chipMask) == allchips_mask)
    { /* Write the same configuration to all chips */
      ichip=0;
      for(jj=2; jj<6; jj++) /* write cfg data */
	{
	  reg = f1ReadChip(id, ichip, jj);
	  reg = reg | F1_ENABLE_EDGES;
	  f1WriteAllChips(id, jj, reg);
	  taskDelay(1);

	  // FIXME: Put this back in, when f1 stores the config in memory
	  /* 	  rval = f1ReadChip(id, ichip, reg); */
	  /* 	  if((rval & 0xFFFF) != data) */
	  /* 	    { */
	  /* 	      printf("%s(%d): ERROR writing to chip %d reg %2d..\n", */
	  /* 		     __FUNCTION__,id, ichip, reg); */
	  /* 	      printf("\t Write (0x%04x) != Readback (0x%04x)\n", */
	  /* 		     data, rval%0xFFFF); */
	  /* 	    } */
	}
    }
  else
    {
      for(ichip=0;ichip<f1Nchips[id];ichip++) 
	{
	  if((chipMask)&(1<<ichip)) 
	    {
	      for(jj=2;jj<6;jj++) 
		{
		  reg = f1ReadChip(id, ichip, jj);
		  reg = reg | F1_ENABLE_EDGES;
		  f1WriteChip(id, ichip, jj, reg);
		  taskDelay(1);
		  /* read it back and check */
		  rval = f1ReadChip(id, ichip, jj);
		  if(rval != reg) 
		    {
		      logMsg("f1EnableData: Error writing Config (%x != %x) \n",rval,reg,0,0,0,0);
		    }
		}
	    }
	}
    }
  F1UNLOCK;
  
  return(OK);
}

/**
 * @ingroup Config
 * @brief Enable data on f1TDC chips specified in chipMask for all initialized modules
 *
 *  @param id 
 *   - Slot Number
 * @param chipMask
 *   - Mask of chips to enable
 * @return OK if successful, otherwise ERROR
 */
void
f1GEnableData(int chipMask)
{
  int ii, id;

  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1EnableData(id,chipMask);
    }
  
}

/**
 * @ingroup Config
 * @brief Disable data on all f1TDC chips for the module
 *
 *  @param id 
 *   - Slot Number
 *
 * @return OK if successful, otherwise ERROR
 */
int
f1DisableData(int id)
{
  int ichip, jj, mask=0;
  unsigned int reg, rval;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1DisableData: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  /* Disable FIFOs for All chips */
  mask = (0xff)<<16;
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl, vmeRead32(&f1p[id]->ctrl) & ~mask);

  /* Disable Edges for All chips */
  for(ichip=0;ichip<f1Nchips[id];ichip++) 
    {
      for(jj=2;jj<6;jj++) 
	{
	  reg = f1ReadChip(id, ichip, jj);
	  reg = reg & ~(F1_ENABLE_EDGES);
	  f1WriteChip(id, ichip, jj, reg);
	  taskDelay(1);
	  /* read it back and check */
	  rval = f1ReadChip(id, ichip, jj);
	  if(rval != reg) 
	    {
	      logMsg("f1DisableData: Error writing Config (%x != %x) \n",rval,reg,0,0,0,0);
	    }
	}
    }
  F1UNLOCK;
  
  return(OK);
}

typedef struct chipchannel_struct
{
  int input;
  int rev;
  int chip;
  int chan;
  int maxchan;
  int reg;
  int odd;
  int rez;
} chipchanInfo;

static void
f1Input2ChipChannel(chipchanInfo *ci)
{
  int input=0;
  if(ci->rev == 2)  /* V2 */
    {
      ci->rez = 1; /* high resolution */
      ci->maxchan = 31;
    }
  else  /* V3 */
    {
      ci->rez = 0; /* normal resolution */
      ci->maxchan = 47;
    }

  input = ci->input;

  if(ci->rez==1)
    input = input*2;
  else
    input = input;

  ci->chip = (int)(input/8);
  ci->chan = ~((input%8))  & 0x7;    /* In V2 and V3, Front Panel Channel number is 
				    inverted WRT the Chip Channel Number (e.g. 0->7,
				    1->6). This is fixed in firmware for the readout data, 
				    but must be fixed by the software driver for
				    writing to the f1chip. */
  ci->reg = (int)(((ci->chan)/2) + 2);  /* Reg  2 - 6 */
  ci->odd = ci->chan%2;                    /* Odd (1) or Even (0) Channel */
  
}

static int
f1SetChannel(int id, int input, int enable)
{
  chipchanInfo c;
  unsigned int rval, check_rval;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SetChannel: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  c.rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  c.input = input;

  f1Input2ChipChannel(&c);

  /* Determine Chip number and Channel for the given input */
  if((input<0)||(input>c.maxchan)) 
    {
      logMsg("f1SetChannel: ERROR: Invalid channel number (%d)\n",
	     input, 1, 2, 3, 4, 5);
      return(ERROR);
    } 

  /* Disable Edges for specified Channel */
  F1LOCK;
  rval = f1ReadChip(id, c.chip, c.reg);

  if(c.rez) 
    {
      if(enable>0)
	rval = rval | ~F1_DISABLE_EDGES;
      else
	rval = rval & F1_DISABLE_EDGES;
    } 
  else 
    {
      if(c.odd)
	{
	  if(enable>0)
	    rval = rval | ~F1_DISABLE_EDGES_ODD;
	  else
	    rval = rval & F1_DISABLE_EDGES_ODD;
	}
      else
	{
	  if(enable>0)
	    rval = rval | ~F1_DISABLE_EDGES_EVEN;
	  else
	    rval = rval & F1_DISABLE_EDGES_EVEN;
	}
    }
  
  f1WriteChip(id, c.chip, c.reg, rval);
  taskDelay(1);
  /* read it back and check */
  check_rval = f1ReadChip(id, c.chip, c.reg);
  F1UNLOCK;
  
  if(rval != check_rval) 
    {
      logMsg("f1SetChannel: Error writing Config (%x != %x) \n",rval,check_rval,0,0,0,0);
    }
  
  if(enable>0)
    f1ChannelDisable[id] &= ~(1<<c.input); // FIXME: Put in a maxchan mask here
  else
    f1ChannelDisable[id] |= (1<<c.input);
  
  return(OK);
  
}

/**
 * @ingroup Config
 * @brief Disable an individual channel input
 *
 *  @param id 
 *   - Slot Number
 * @param input
 *   - Input channel to disable
 * @return OK if successful, otherwise ERROR
 */
int
f1DisableChannel(int id, int input)
{
  return f1SetChannel(id,input,0);
}

/**
 * @ingroup Config
 * @brief Enable an individual channel input
 *
 *  @param id 
 *   - Slot Number
 * @param input
 *   - Input channel to enable
 * @return OK if successful, otherwise ERROR
 */
/* Enable Individual Inputs between 0 and 31 for V2 OR 0 and 47 for V3*/
int
f1EnableChannel(int id, int input)
{
  return f1SetChannel(id,input,1);
}

static void
f1SetChannelMask(int id, unsigned long long int mask, int enable)
{
  int ichip, ireg, ichan;
  chipchanInfo c;
  unsigned short chipRegs[8][16]; /* Temporary storage of registers */

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1DisableChannelMask: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  F1LOCK;
  c.rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;

  /* Grab the initial registers from each chip */
  for(ichip=0; ichip<f1Nchips[id]; ichip++)
    {
      for(ireg=2; ireg<=6; ireg++)
	{
	  chipRegs[ichip][ireg] = f1ReadChip(id,ichip,ireg);
	}
    }

  /* Modify the chip registers, as prescribed from the input mask */
  for(ichan=0; ichan<c.maxchan; ichan++)
    {
      /* Determine the chip number and register from the input channel */
      c.input = ichan;
      f1Input2ChipChannel(&c);

      if((mask & (1<<ichan)) == (1<<ichan))
	{
	  if(c.rez)
	    {
	      if(enable>0)
		chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] | ~F1_DISABLE_EDGES;
	      else
		chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] & F1_DISABLE_EDGES;
	    }
	  else
	    {
	      if(c.odd)
		{
		  if(enable>0)
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] | ~F1_DISABLE_EDGES_ODD;
		  else
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] & F1_DISABLE_EDGES_ODD;
		}
	      else
		{
		  if(enable>0)
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] | ~F1_DISABLE_EDGES_EVEN;
		  else
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] & F1_DISABLE_EDGES_EVEN;
		}
	    }
	}
      else
	{
	  if(c.rez)
	    {
	      if(enable>0)
		chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] & F1_DISABLE_EDGES;
	      else
		chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] | ~F1_DISABLE_EDGES;
	    }
	  else
	    {
	      if(c.odd)
		{
		  if(enable>0)
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] & F1_DISABLE_EDGES_ODD;
		  else
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] | ~F1_DISABLE_EDGES_ODD;
		}
	      else
		{
		  if(enable>0)
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] & F1_DISABLE_EDGES_EVEN;
		  else
		    chipRegs[c.chip][c.reg] = chipRegs[c.chip][c.reg] | ~F1_DISABLE_EDGES_EVEN;
		}
	    }
	}
    }

  /* Write the new registers for each chip */
  for(ichip=0; ichip<f1Nchips[id]; ichip++)
    {
      for(ireg=2; ireg<=6; ireg++)
	{
	  f1WriteChip(id,ichip,ireg,chipRegs[ichip][ireg]);
	}
    }

  if(enable>0)
    f1ChannelDisable[id] = ~mask; // FIXME: Add appropriate maxchan mask
  else
    f1ChannelDisable[id] = mask;

  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Disable inputs indicated in channel mask for the module
 *
 *  @param id 
 *   - Slot Number
 * @param mask
 *   - Mask of input channels to DISABLE
 *
 */
void
f1DisableChannelMask(int id, unsigned long long int mask)
{
  f1SetChannelMask(id,mask,0);
}

/**
 * @ingroup Config
 * @brief Enable inputs indicated in channel mask for the module
 *
 *  @param id 
 *   - Slot Number
 * @param mask
 *   - Mask of input channels to Enable
 *
 */
void
f1EnableChannelMask(int id, unsigned long long int mask)
{
  f1SetChannelMask(id,mask,1);
}

/*
  F1 Chip Write and Read routines.  
  These routines are static because they do not use mutex locks/unlocks.
  Be sure to implement those mutex's when calling these routines
*/

/* A temporary place to store chip registers, until the firmware allows for the
   module to store them */
static unsigned short f1ChipRegs[F1_MAX_BOARDS+1][8][16];

/* Write a 2 byte word to a single register on a f1tdc chip */
static int
f1WriteChip(int id, int chip, int reg, unsigned short data)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return ERROR;
    }
  
  if((chip<0) || (chip>=f1Nchips[id]))
    {
      printf("%s: ERROR: Invalid chip (%d)\n",
	     __FUNCTION__,chip);
      return ERROR;
    }
  
  if((reg<0) || (reg>=16))
    {
      printf("%s: ERROR: Invalid register (%d)\n",
	     __FUNCTION__,reg);
      return ERROR;
    }

  vmeWrite32(&f1p[id]->config, (chip<<21) | (reg<<16) | data); 
  f1ChipRegs[id][chip][reg] = data;
  
  return OK;
}

/* Write a 2 byte word to a single register on all f1tdc chips in the module */
static int
f1WriteAllChips(int id, int reg, unsigned short data)
{
  int ichip=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return ERROR;
    }
  
  if((reg<0) || (reg>=16))
    {
      printf("%s: ERROR: Invalid register (%d)\n",
	     __FUNCTION__,reg);
      return ERROR;
    }

  vmeWrite32(&f1p[id]->config, F1_CONFIG_COMMON | (reg<<16) | data); 
  for(ichip=0; ichip<8; ichip++)
    f1ChipRegs[id][ichip][reg] = data;

  return OK;
}

/* Read a 2 byte word from a single register on an f1tdc chip */
static unsigned short
f1ReadChip(int id, int chip, int reg)
{
  unsigned short rval;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",
	     __FUNCTION__,id);
      return ERROR;
    }

  // FIXME: for now... register values stored in library.
  // Future firmware well have these available for readout on the module
  rval = f1ChipRegs[id][chip][reg];

  return rval;
}

/**
 * @ingroup Config
 * @brief Enable the specified clock source on the module
 *
 *  @param id 
 *   - Slot Number
 *  @param cflag Clock Source Flag
 *   - 0: P0/VXS
 *   - 1: Internal
 *   - 2: Front Panel
 */
void
f1EnableClk(int id, int cflag)
{
  unsigned int clkbits=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EnableClk: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }

  if((cflag<0) || (cflag>2))
    {
      printf("%s: ERROR: Invalid clock source (%d)\n",__FUNCTION__,cflag);
      return;
    }
  
  F1LOCK;
  switch(cflag)
    {
    case 1:
      clkbits = F1_REFCLK_SRC_INTERNALFP | F1_REFCLK_INTERNAL_ENABLE;
      break;

    case 2:
      clkbits = F1_REFCLK_SRC_INTERNALFP | F1_REFCLK_SRC_FP;
      break;
      
    case 0:
    default:
      clkbits = 0;
    }

    vmeWrite32(&f1p[id]->ctrl, 
	       (vmeRead32(&f1p[id]->ctrl) & ~F1_REFCLK_SRC_MASK) |
	       clkbits);

  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Disable the current clock source on the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1DisableClk(int id)
{

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1DisableClk: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl, 
	     (vmeRead32(&f1p[id]->ctrl) & ~F1_REFCLK_SRC_MASK) | 
	     F1_REFCLK_SRC_INTERNALFP);
  F1UNLOCK;
  
}

/**
 * @ingroup Config
 * @brief Enable lead and trailing edges for the f1TDC chips indicated by the chipMask for the module
 *
 *  @param id 
 *   - Slot Number
 *  @param chipMask
 *   - Mask of chips to enable Lead and Trailing edges
 *
 * @return Mask of initialized slots with leading and trailing edges enabled.
 */
unsigned int
f1EnableLetra(int id, int chipMask)
{
  /*   int ii; */

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EnableLetra: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(0);
    }

  if(chipMask<=0) chipMask = F1_ALL_CHIPS; /* Enable all Chips */

  f1LetraMode |= (1<<id);

  return(f1LetraMode);
}

/**
 * @ingroup Config
 * @brief Disable lead and trailing edges for the f1TDC chips indicated by the chipMask for the module
 *  @param chipMask
 *   - Mask of chips to disable Lead and Trailing edges
 *
 *  @param id 
 *   - Slot Number
 *
 * @return Mask of initialized slots with leading and trailing edges enabled.
 */
unsigned int
f1DisableLetra(int id, int chipMask)
{
  /*   int ii; */

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1DisableLetra: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(0);
    }
  
  if(chipMask<=0) chipMask = F1_ALL_CHIPS; /* Disable all Chips */

  f1LetraMode &= ~(1<<id);

  return(f1LetraMode);
}


/**
 * @ingroup Config
 * @brief Enable software triggers on the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1EnableSoftTrig(int id)
{

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EnableSoftTrig: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl,
	     vmeRead32(&f1p[id]->ctrl) | F1_ENABLE_SOFT_CONTROL);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Enable software triggers for all initialized modules
 *
 *
 */
void
f1GEnableSoftTrig()
{
  int ii;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
	       vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_ENABLE_SOFT_CONTROL);
  F1UNLOCK;

}


/**
 * @ingroup Config
 * @brief Disable software triggers on the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1DisableSoftTrig(int id)
{

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1DisableSoftTrig: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl,
	     vmeRead32(&f1p[id]->ctrl) & ~F1_ENABLE_SOFT_CONTROL);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Enable bus error block termination on the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1EnableBusError(int id)
{

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EnableBusError: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl,
	     vmeRead32(&f1p[id]->ctrl) | F1_ENABLE_BERR);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Enable bus error block termination for all initialized modules
 *
 *
 */
void
f1GEnableBusError()
{
  int ii;

  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
	       vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_ENABLE_BERR);
  F1UNLOCK;

}


/**
 * @ingroup Config
 * @brief Disable bus error block termination on the module
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1DisableBusError(int id)
{

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1DisableBusError: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl,
	     vmeRead32(&f1p[id]->ctrl) & ~F1_ENABLE_BERR);
  F1UNLOCK;

}


/**
 * @ingroup Config
 * @brief Set the block level (number of events per block) on the module
 *
 *  @param id 
 *   - Slot Number
 * @param level
 *   - Number of events per block
 *
 * @return OK if successful, otherwise ERROR
 */
int
f1SetBlockLevel(int id, int level)
{
  int rval;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SetBlockLevel: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }
  
  if(level<=0) level = 1;
  F1LOCK;
  vmeWrite32(&f1p[id]->blocklevel, level);
  rval = vmeRead32(&f1p[id]->blocklevel)&F1_BLOCKLEVEL_MASK;
  F1UNLOCK;

  return(rval);

}

/**
 * @ingroup Config
 * @brief Set the clock source for the selected TDC.
 *
 *  @param id 
 *   - Slot Number
 * @param source  Clock Source
 *   - 0: Internal
 *   - 1: FP
 *   - 2: VXS
 *
 * @return OK if successful, otherwise ERROR
 */
int
f1SetClkSource(int id, int source)
{
  int clkSrc = 0;

  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL))
    {
      logMsg("f1SetClkSource: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return(ERROR);
    }

  switch(source)
    {
    case 0: /* Internal */
      clkSrc = F1_REFCLK_SRC_INTERNALFP | F1_REFCLK_INTERNAL_ENABLE;
      break;

    case 1: /* Front Panel */
      clkSrc = F1_REFCLK_SRC_INTERNALFP | F1_REFCLK_SRC_FP;
      break;

    case 2: /* VXS */
      clkSrc = 0;
      break;

    default:
      printf("%s: Invalid Clock source (%d)\n",
	     __FUNCTION__, source);
      return ERROR;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->ctrl,
	     (vmeRead32(&f1p[id]->ctrl) & ~F1_REFCLK_SRC_MASK)
	     | clkSrc);
  F1UNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the block level (number of events per block) on all initialized modules
 *
 *  @param id 
 *   - Slot Number
 * @param level
 *   - Number of events per block
 */
void
f1GSetBlockLevel(int level)
{
  int ii;

  if(level<=0) level = 1;
  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->blocklevel), level);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Enable multiblock readout for all initialized modules
 *
 * @param tflag Token Flag
 *  -  0: Token passed through P2
 *  - >0: Token passed through P0/VXS
 */
void
f1EnableMultiBlock(int tflag)
{
  int ii, id;
  unsigned int mode=0;

  if((nf1tdc <= 1) || (f1p[f1ID[0]] == NULL)) 
    {
      logMsg("f1EnableMultiBlock: ERROR : Cannot Enable MultiBlock mode \n",0,0,0,0,0,0);
      return;
    }

  
  for(ii=0;ii<nf1tdc;ii++) 
    {
      /* if token > 0 then send via P0 else via P2 */
      if(tflag)
	mode = F1_MB_TOKEN_P0 | F1_ENABLE_MULTIBLOCK; 
      else
	mode = F1_MB_TOKEN_P2 | F1_ENABLE_MULTIBLOCK; 

      id = f1ID[ii];

      f1DisableBusError(id);
      if(id == f1MinSlot) 
	{
	  mode |= F1_FIRST_BOARD;
	}

      if(id == f1MaxSlot) 
	{
	  mode |= F1_LAST_BOARD;
	  f1EnableBusError(id);
	}

      F1LOCK;
      vmeWrite32(&f1p[id]->ctrl,
		 (vmeRead32(&f1p[id]->ctrl) & ~F1_MB_CONFIG_MASK) | mode);
      F1UNLOCK;
    }

}

/**
 * @ingroup Config
 * @brief Disable multiblock readout for all initialized modules
 */
void
f1DisableMultiBlock()
{
  int ii;

  if((nf1tdc <= 1) || (f1p[f1ID[0]] == NULL)) 
    {
      logMsg("f1DisableMultiBlock: ERROR : Cannot Disable MultiBlock Mode\n",0,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  for(ii=0;ii<nf1tdc;ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
	       vmeRead32(&(f1p[f1ID[ii]]->ctrl)) & ~F1_ENABLE_MULTIBLOCK);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Reset the token for the module
 *
 *   This routine only has an effect for the module marked as "FIRST" in the
 *   multiblock chain.
 *
 *  @param id 
 *   - Slot Number
 *
 */
void
f1ResetToken(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1ResetToken: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->csr, F1_CSR_TOKEN_RETURN);
  F1UNLOCK;

}

/**
 * @ingroup Config
 * @brief Force f1TDC Chip Headers into the data stream for the module 
 *
 * @param id
 *   - Slot Number
 * @param enable
 *   - 0: Disable forcing of chip headers
 *   - >0: Enable forcing of chip headers
 * 
 * @return OK if successful, otherwise ERROR
 */
int
f1SetForceChipHeaders(int id, int enable)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,id);
      return ERROR;
    }
  
  F1LOCK;
  if(enable)
    {
      vmeWrite32(&f1p[id]->ctrl2, 
		 vmeRead32(&f1p[id]->ctrl2) | F1_FORCE_CHIP_HEADERS);
    }
  else
    {
      vmeWrite32(&f1p[id]->ctrl2, 
		 vmeRead32(&f1p[id]->ctrl2) & ~F1_FORCE_CHIP_HEADERS);
    }
  F1UNLOCK;
  
  return OK;
}

/**
 * @ingroup Config
 * @brief Force f1TDC Chip Headers into the data stream for all initialized modules
 *
 * @param enable
 *   - 0: Disable forcing of chip headers
 *   - >0: Enable forcing of chip headers
 */
void
f1GSetForceChipHeaders(int enable)
{
  int id, ii;

  for(ii=0;ii<nf1tdc;ii++) 
    {
      id = f1ID[ii];
      f1SetForceChipHeaders(id, enable);
    }

}

/**
 * @ingroup PulserConfig
 * @brief Reset (initialize) pulser
 *
 *  @param id 
 *   - Slot Number
 *
 */
int
f1ResetPulser(int id)
{
 int rev=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1ResetPulser: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return ERROR;
    }

  rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  if(rev != 3)
    {
      logMsg("f1ResetPulser: ERROR: f1TDC Board Revision (%d) does not have pulser support",
	     rev,2,3,4,5,6);
      return ERROR;
    }
 
  F1LOCK;
  vmeWrite32(&f1p[id]->pulser_dac, F1_PULSER_DAC_RESET);
  taskDelay(1);
  vmeWrite32(&f1p[id]->pulser_dac, F1_PULSER_DAC_INT_REF);
  taskDelay(1);
  F1UNLOCK;

  return OK;
}

/**
 * @ingroup PulserConfig
 * @brief Set the delay between the output pulse and f1TDC trigger
 *
 *  @param id 
 *   - Slot Number
 *
 */
int
f1SetPulserTriggerDelay(int id, int delay)
{
  int rev=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SetPulserTriggerDelay: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return ERROR;
    }

  rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  if(rev != 3)
    {
      logMsg("f1SetPulserTriggerDelay: ERROR: f1TDC Board Revision (%d) does not have pulser support",
	     rev,2,3,4,5,6);
      return ERROR;
    }
  
  if(delay>F1_PULSER_DELAY_MASK)
    {
      logMsg("f1SetPulserTriggerDelay: ERROR: delay (%d) out of range.  Must be <= %d",
	     delay,F1_PULSER_DELAY_MASK,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->pulser_delay, delay);
  F1UNLOCK;

  return OK;
}


/**
 * @ingroup PulserConfig
 * @brief  Set the DAC level for the outgoing pulse
 *
 *  @param id 
 *   - Slot Number
 * @param output
 *   - 1: channels  0-23
 *   - 2: channels 24-37
 *   - 3: all channels
 *
 * @param dac
 *    - 400 should be sufficient for the Hall D descriminator cards)
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
f1SetPulserDAC(int id, int output, int dac)
{
  int rev=0;
  unsigned int selection=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SetPulserDAC: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return ERROR;
    }

  rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  if(rev != 3)
    {
      logMsg("f1SetPulserDAC: ERROR: f1TDC Board Revision (%d) does not have pulser support",
	     rev,2,3,4,5,6);
      return ERROR;
    }
  
  if(dac>(F1_PULSER_DAC_MASK>>4))
    {
      logMsg("f1SetPulserDAC: ERROR: dac (%d) out of range.  Must be <= %d",
	     dac,F1_PULSER_DAC_MASK,3,4,5,6);
      return ERROR;
    }

  switch(output)
    {
    case 1: /* DAC A: lower cable - channels 0-23 */
      selection = F1_PULSER_DAC_A_VALUE;
      break;

    case 2: /* DAC B: upper cable - channels 24-47 */
      selection = F1_PULSER_DAC_B_VALUE;
      break;
     
    case 3: /* DAC A and B: both cables */
      selection = F1_PULSER_DAC_BOTH_VALUE;
      break;

    default:
      logMsg("f1SetPulserDAC: ERROR: Invalid DAC output selection (%d)",
	     output,2,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->pulser_dac, (dac<<4) | selection);
  F1UNLOCK;

  return OK;
}

/**
 * @ingroup PulserConfig
 * @brief Trigger the pulser
 *
 *  @param id 
 *   - Slot Number
 * @param output
 *   - 1: Pulse out only
 *   - 2: f1TDC Trigger only
 *   - 3: Both pulse and trigger
 *  @return OK if successful, otherwise ERROR.
 */
int
f1SoftPulser(int id, int output)
{
  int rev=0;
  unsigned int selection=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1SoftPulser: ERROR : TDC in slot %d is not initialized \n",id,0,0,0,0,0);
      return ERROR;
    }

  rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  if(rev != 3)
    {
      logMsg("f1SoftPulser: ERROR: f1TDC Board Revision (%d) does not have pulser support",
	     rev,2,3,4,5,6);
      return ERROR;
    }

  switch(output)
    {
    case 0: /* Just the pulse out */
      selection = F1_PULSER_PULSE_OUT;
      break;

    case 1: /* Just the trigger out */
      selection = F1_PULSER_TRIGGER_OUT;
      break;

    case 2: /* Pulse and trigger out */
      selection = F1_PULSER_PULSE_OUT | F1_PULSER_TRIGGER_OUT;
      break;

    default:
      logMsg("f1SoftPulser: ERROR: Invalid output option (%d)",
	     output,2,3,4,5,6);
      return ERROR;
    }


  F1LOCK;
  vmeWrite32(&f1p[id]->pulser_control, selection);
  F1UNLOCK;

  return OK;
}

/**
 * @ingroup Status
 *  @brief Return the base address of the A32 for specified module
 *  @param id 
 *   - Slot Number
 *  @return A32 address base, if successful. Otherwise ERROR.
 */

unsigned int
f1GetA32(int id)
{
  unsigned int rval = 0;
  if(f1pd[id])
    {
      rval = (unsigned int)f1pd[id] - f1tdcA32Offset;
    }
  else
    {
      logMsg("f1GetA32(%d): A32 pointer not initialized\n",
	     id, 2, 3, 4, 5, 6);
      rval = ERROR;
    }

  return rval;
}

/**
 * @ingroup Status
 *  @brief Return the base address of the A32 Multiblock
 *  @return A32 multiblock address base, if successful. Otherwise ERROR.
 */

unsigned int
f1GetA32M()
{
  unsigned int rval = 0;
  if(f1pmb)
    {
      rval = (unsigned int)f1pmb - f1tdcA32Offset;
    }
  else
    {
      logMsg("f1GetA32M: A32M pointer not initialized\n",
	     1, 2, 3, 4, 5, 6);
      rval = ERROR;
    }

  return rval;
}

/**
 * @ingroup Status
 *  @brief Return slot mask of modules with token
 *  @param pflag Option to print status to standard out.
 *  @return Mask of slots with the token, if successful. Otherwise ERROR.
 */

unsigned int
f1GetTokenStatus(int pflag)
{
  unsigned int stat = 0, tokenmask = 0;
  int if1 = 0;

  if(pflag)
    logMsg("f1GetTokenStatus: Token in slot(s) ",1,2,3,4,5,6);

  F1LOCK;
  for(if1 = 0; if1 < nf1tdc; if1++) 
    {
      stat = vmeRead32(&f1p[f1ID[if1]]->csr);
      if(stat & F1_CSR_TOKEN_STATUS) 
	{
	  tokenmask |= (1<<f1ID[if1]);

	  if(pflag)
	    logMsg("%2d ", f1ID[if1], 2, 3, 4, 5, 6);
	}
    }
  F1UNLOCK;

  if(pflag)
    logMsg("\n", 1, 2, 3, 4, 5, 6);
  
  return tokenmask;
}

/**
 * @ingroup DebugSW
 *  @brief Enable/Disable System test mode
 *  @param id 
 *   - Slot Number
 *  @param mode 
 *    -  0: Disable Test Mode
 *    - >0: Enable Test Mode
 */
void
f1TestSetSystemTestMode(int id, int mode)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  F1LOCK;

  if(mode>=1)
    {
      vmeWrite32(&(f1p[id]->ctrl2),
		 vmeRead32(&f1p[id]->ctrl2) | F1_SINGLE_BOARD_TEST_MODE);
    }
  else
    {
      vmeWrite32(&(f1p[id]->ctrl2),
		 vmeRead32(&f1p[id]->ctrl2) & ~F1_SINGLE_BOARD_TEST_MODE);
    }


  printf(" ctrl2 = 0x%08x\n",vmeRead32(&f1p[id]->ctrl2));
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Set the level of Trig Out to the SD
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @param mode 
 *    -  0: Not asserted
 *    - >0: Asserted
 */
void
f1TestSetTrigOut(int id, int mode)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  if(mode>=1) 
    reg=F1_TEST_TRIG_OUT;
  else 
    reg=0;
    
  F1LOCK;
  vmeWrite32(&(f1p[id]->test_config),reg);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Set the level of Busy Out to the SD
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @param mode 
 *    -  0: Not asserted
 *    - >0: Asserted
 *
 */
void
f1TestSetBusyOut(int id, int mode)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  if(mode>=1) 
    reg=F1_TEST_BUSY_OUT;
  else 
    reg=0;
    
  F1LOCK;
  vmeWrite32(&(f1p[id]->test_config),reg);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Set the level of the SD Link
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @param mode 
 *    -  0: Not asserted
 *    - >0: Asserted
 */
void
f1TestSetSdLink(int id, int mode)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  if(mode>=1) 
    reg=F1_TEST_SDLINK_OUT;
  else 
    reg=0;
    
  F1LOCK;
  vmeWrite32(&(f1p[id]->test_config),reg);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Set the level of Token Out to the SD
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @param mode 
 *    -  0: Not asserted
 *    - >0: Asserted
 *
 */
void
f1TestSetTokenOut(int id, int mode)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  if(mode>=1) 
    reg=F1_TEST_TOKEN_OUT;
  else 
    reg=0;
    
  F1LOCK;
  vmeWrite32(&(f1p[id]->test_config),reg);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Get the level of the StatBitB to the SD
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return 1 if asserted, 0 if not, otherwise ERROR.
 *
 */
int
f1TestGetStatBitB(int id)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = (vmeRead32(&f1p[id]->test_config) & F1_TEST_STATBITB_IN)>>8;
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Get the level of the Token In from the SD
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return 1 if asserted, 0 if not, otherwise ERROR.
 */
int
f1TestGetTokenIn(int id)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = (vmeRead32(&f1p[id]->test_config) & F1_TEST_TOKEN_IN)>>9;
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Return the status of the 250Mhz Clock Counter
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return 1 if counting, 0 if not counting, otherwise ERROR.
 */
int
f1TestGetClockCounterStatus(int id)
{
  int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = (vmeRead32(&f1p[id]->test_config) & F1_TEST_CLOCK_COUNTER_STATUS)>>15;
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Return the value of the 250Mhz Clock scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return 250Mhz Clock scaler counter if successful, otherwise ERROR.
 */
unsigned int
f1TestGetClockCounter(int id)
{
  unsigned int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = vmeRead32(&f1p[id]->clock_scaler);
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Return the value of the SyncReset scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return SyncReset scaler counter if successful, otherwise ERROR.
 */
unsigned int
f1TestGetSyncCounter(int id)
{
  unsigned int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = vmeRead32(&f1p[id]->p0_sync_scaler);
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Return the value of the trig1 scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return trig1 scaler counter if successful, otherwise ERROR.
 */
unsigned int
f1TestGetTrig1Counter(int id)
{
  unsigned int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = vmeRead32(&f1p[id]->p0_trig1_scaler);
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Return the value of the trig2 scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 *  @return trig2 scaler counter if successful, otherwise ERROR.
 */
unsigned int
f1TestGetTrig2Counter(int id)
{
  unsigned int reg=0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return ERROR;
    }

  F1LOCK;
  reg = vmeRead32(&f1p[id]->p0_trig2_scaler);
  F1UNLOCK;

  return reg;

}

/**
 * @ingroup DebugSW
 *  @brief Reset the counter of the 250MHz Clock scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 */
void
f1TestResetClockCounter(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->clock_scaler,F1_CLOCK_SCALER_RESET);
  vmeWrite32(&f1p[id]->clock_scaler,F1_CLOCK_SCALER_START);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Reset the counter of the SyncReset scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 */
void
f1TestResetSyncCounter(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->p0_sync_scaler,F1_SYNC_SCALER_RESET);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Reset the counter of the trig1 scaler
 *
 *   Available only in System Test Mode   
 *
 *  @sa f1TestSetSystemTestMode
 *  @param id 
 *   - Slot Number
 */
void
f1TestResetTrig1Counter(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->p0_trig1_scaler,F1_TRIG1_SCALER_RESET);
  F1UNLOCK;

}

/**
 * @ingroup DebugSW
 *  @brief Reset the counter of the trig2 scaler
 *
 *   Available only in System Test Mode   
 *
 *  @param id 
 *   - Slot Number
 */
void
f1TestResetTrig2Counter(int id)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      printf("%s: ERROR : TDC in slot %d is not initialized \n",__FUNCTION__,
	     id);
      return;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->p0_trig2_scaler,F1_TRIG2_SCALER_RESET);
  F1UNLOCK;

}


/* Test routines */
/* Max words = 64 channels * 8 hits/chan + 8 headers + 1 trailer */
#define TEST_MAX_WORDS   (64*8 + 9)

int f1TestEventCount, f1TestClearCount, f1TestErrorCount;
unsigned int f1TestData[TEST_MAX_WORDS];

void
f1TestRead()
{
  int ii, slot, retval, scanval;
  int maxwords=TEST_MAX_WORDS;
  int mask = 0;
  unsigned int errmask;
  unsigned int *data;

  maxwords = TEST_MAX_WORDS*nf1tdc;
  data = (unsigned int *) malloc((maxwords<<2));
  bzero((char *)data,(maxwords<<2));
  mask = f1ScanMask();
  if(mask <=0) 
    {
      printf("No TDCs available for Readout\n");
      return;
    }
  else
    {
      printf("F1 TDC Mask = 0x%x\n",mask);
      printf("F1 DATA Buffer (%d bytes)  at addr = 0x%x\n",(maxwords<<2), (int) data);
    }

  f1TestEventCount = 0;
  f1TestClearCount = 0;
  f1TestErrorCount = 0;

  f1GEnableSoftTrig();
    
  while(1) 
    {

      f1GTrig();
  
      scanval = f1DataScan(0);
    
      if(scanval == mask) 
	{
	  f1TestEventCount++;
	  retval = f1ReadBlock(f1MinSlot,data,maxwords,2);
	  if(retval<=0) 
	    {
	      printf("Error in reading data retval=%d\n address = 0x%x",retval,(int) data);
	      return;
	    }
	  /* check Error Status */
	  errmask = f1GErrorStatus(1);
	  if(errmask) 
	    {
	      f1TestErrorCount++;
	      for(ii=0;ii<nf1tdc;ii++) 
		{
		  slot = f1ID[ii];
		  if((1<<slot)&errmask) 
		    {
		      f1ChipStatus(slot,1);
		      f1ClearStatus(slot,0);
		    }
		}
	    }

	  if(retval==maxwords) 
	    {
	      f1TestClearCount++;
	      printf("f1TestRead: ERROR: Too much data: Clearing TDCs\n");
	      f1GClear();
	    }
	} 
      else 
	{
	  printf("Bad Scan mask 0x%x (should be 0x%x)\n",scanval,mask);
	  taskDelay(1);
	}

      taskDelay(1);
    }

}

void
f1ISR(int arg)
{
  int ii, slot, retval, scanval;
  int maxwords=TEST_MAX_WORDS;
  unsigned int mask,errmask,stat;


  f1TestEventCount++;
  mask = f1ScanMask();
  scanval = f1DataScan(0);
    
  if(scanval == mask) 
    {
      retval = f1ReadBlock(f1MinSlot,&f1TestData[0],maxwords,2);
      if(retval<=0) 
	{
	  logMsg("Error in reading data retval=%d\n address = 0x%x",retval,(int) f1TestData,0,0,0,0);
	  return;
	}

      if(retval >= maxwords) 
	{
	  /* Check who has the Token */
	  for(ii=0;ii<nf1tdc;ii++) 
	    {
	      stat = f1ReadCSR(f1ID[ii]);
	      if(stat&F1_CSR_TOKEN_STATUS) 
		{
		  logMsg("Token stuck in TDC slot %d on event %d\n",f1ID[ii],f1TestEventCount,0,0,0,0);
		  break;
		}
	    }
	  f1GClear();
	}

      /* check Error Status */
      errmask = f1GErrorStatus(1);
      if(errmask) 
	{
	  f1TestErrorCount++;
	  for(ii=0;ii<nf1tdc;ii++) 
	    {
	      slot = f1ID[ii];
	      if((1<<slot)&errmask) 
		{
		  f1ClearStatus(slot,0);
		}
	    }
	  /*check again */
	  errmask = f1GErrorStatus(1);
	  if(errmask)
	    logMsg("Persistant Error present 0x%x\n",errmask,0,0,0,0,0);
	}

    } 
  else 
    {
      logMsg("f1ISR: WARN: scanmask not correct 0x%x should be 0x%x\n",scanval,mask,0,0,0,0);
      f1GClear();
    }

}

/* State Machine debugging routines */
int
f1StateArmBuffer(int id, int enable)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1StateArmBuffer: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  if(enable)
    vmeWrite32(&f1p[id]->state.csr, F1_STATE_CSR_ARM_BUFFER);
  else
    vmeWrite32(&f1p[id]->state.csr, 0);
  F1UNLOCK;
  
  return OK;
}

int
f1StateReadBuffer(int id, volatile unsigned int *data, int nwords)
{
  int rval=0, idata=0, ndata=0;
  
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1StateReadBuffer: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  /* Read in how many words are available */
  ndata = vmeRead32(&f1p[id]->state.csr) & F1_STATE_CSR_BUFFER_WORDS_MASK;

  if(ndata == 0)
    {
      logMsg("f1StateReadBuffer(%d): WARN: No words in State Machine buffer\n",
	     id, 2, 3, 4, 5, 6);
      rval = 0;
    }
  else
    {
      if(ndata > nwords)
	{
	  logMsg("f1StateReadBuffer(%d): WARN: %d words remain in State Machine buffer\n",
		 id, ndata, 3, 4, 5, 6);
	  
	}
      for(idata = 0; idata < ndata; idata++)
	{
	  data[idata] = vmeRead32(&f1p[id]->state.value) & F1_STATE_VALUE_MASK;
	}
    }
  
  F1UNLOCK;
  
  return ndata;
}

int
f1StatePrintBuffer(int id)
{
  unsigned int data[0xff];
  int nwords = 0, idata = 0;
  
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1StatePrintBuffer: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  nwords = f1StateReadBuffer(id, (volatile unsigned int *)&data, 0xff);
  if(nwords < 0)
    {
      logMsg("f1StatePrintBuffer(%d): ERROR: Unable to retreive state machine data\n", id, 2, 3, 4, 5, 6);
      return ERROR;
    }
  
  printf("\n--- number of state values saved = %d\n\n", nwords);
  for(idata = 0; idata < nwords; idata++)
    {
      printf("state %4d   value = %5X\n",
	     (idata + 1),
	     data[idata]);
    }
  
  return OK;
}

int
f1EBControl(int id, int fake_on_timeout, int fake_on_skipped, int delete_on_timeout,
	    int timeout_val)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EBControl: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  if(fake_on_timeout)
    fake_on_timeout = 1;
  else
    fake_on_timeout = 0;
  
  if(fake_on_skipped)
    fake_on_skipped = 1;
  else
    fake_on_skipped = 0;
  
  if(delete_on_timeout)
    delete_on_timeout = 1;
  else
    delete_on_timeout = 0;

  if((timeout_val < 0) || (timeout_val > 0xFF))
    {
      logMsg("f1EBControl: ERROR: Invalid timeout_val (%d)\n",
	     timeout_val, 2, 3, 4, 5, 6);
      return ERROR;
    }

  F1LOCK;
  vmeWrite32(&f1p[id]->eb.build_timer,
	     (fake_on_timeout << 15) |
	     (fake_on_skipped << 14) |
	     (delete_on_timeout << 13) |
	     timeout_val);
  F1UNLOCK;
  
  return OK;
}

unsigned int
f1EBGetInsertedDataCount(int id)
{
  unsigned int rval = 0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EBGetInsertedDataCount: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }


  F1LOCK;
  rval = vmeRead32(&f1p[id]->eb.insert_count);
  F1UNLOCK;
  
  return rval;
}

int
f1EBGetDeletedDataCount(int id)
{
  int rval = 0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EBGetDeletededDataCount: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  rval = vmeRead32(&f1p[id]->eb.delete_count) & 0xFFFF;
  F1UNLOCK;
  
  return rval;
}

int
f1EBGetMonitor(int id)
{
  int rval = 0;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EBGetMonitor: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  rval = vmeRead32(&f1p[id]->eb.monitor) & 0xFFFFFF;
  F1UNLOCK;
  
  return rval;
}

int
f1EBResetCounters(int id, int flag)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EBResetCounters: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  if(flag <= 0)
    flag = 0xF;
  
  F1LOCK;
  if(flag & 0x1)
    vmeWrite32(&f1p[id]->eb.insert_count, 1<<31);

  if(flag & 0x2)
    vmeWrite32(&f1p[id]->eb.delete_count, 1<<31);

  if(flag & 0x4)
    vmeWrite32(&f1p[id]->eb.progress, 1<<31);

  F1UNLOCK;
  
  return OK;
}

int
f1EBStatus(int id)
{
  struct F1TDC_EB_REGS reb;
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1EBStatus: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  F1LOCK;
  reb.build_timer = vmeRead32(&f1p[id]->eb.build_timer);
  reb.insert_count = vmeRead32(&f1p[id]->eb.insert_count);
  reb.delete_count = vmeRead32(&f1p[id]->eb.delete_count);
  reb.monitor = vmeRead32(&f1p[id]->eb.monitor);
  reb.progress = vmeRead32(&f1p[id]->eb.progress);
  F1UNLOCK;

#ifdef VXWORKS
  printf("\n EB STATUS for TDC in slot %d at base address 0x%x \n",
	 id, (UINT32) f1p[id]);
#else
  printf("\n EB STATUS for TDC in slot %d at VME (Local) base address 0x%x (0x%x) \n",
	 id,(UINT32) f1p[id] - f1tdcA24Offset, (UINT32) f1p[id]);
#endif
  printf("--------------------------------------------------------------------------------\n");
  printf("                             Timer and Control\n");
  printf("\n");
  printf(" Insert fake data on timeout                = %s\n",
	 (reb.build_timer & (1<<15))? "ENABLED" : "DISABLED");
  printf(" Insert fake data on skipped trigger number = %s\n",
	 (reb.build_timer & (1<<14))? "ENABLED" : "DISABLED");
  printf(" Delete data arriving after timeout         = %s\n",
	 (reb.build_timer & (1<<14))? "ENABLED" : "DISABLED");
  printf("\n");
  printf(" Timeout value = 0x%x (%.2f us)\n",
	 reb.build_timer & 0xFF, ((float)(reb.build_timer & 0xFF))*0.02 );
  printf("\n");
  
  printf("                                Counters\n");
  printf("Inserted data due to timeouts                 = %d\n",
	 (reb.insert_count & 0xFFFF0000) >> 16);
  printf("Inserted data due to skipped trigger numbers  = %d\n",
	 reb.insert_count & 0xFFFF);
  printf("Deleted data due data arriving after timeouts = %d\n",
	 reb.delete_count);
  printf("\n");

  printf("                          Event Block Monitor\n");
  printf("  Block of events accepted                            : %s\n",
	 (reb.monitor & (1 << 31)) ? "YES" : "NO");
  printf("  Block of events ready for readout                   : %s\n",
	 (reb.monitor & (1 << 30)) ? "YES" : "NO");
  printf("  Number of events on board waiting to be built       : %d\n",
	 (reb.monitor & 0x3FFC0000) >> 18);
  printf("  Number of completely built blocks of events on board: %d\n",
	 (reb.monitor & 0x0003FE00) >> 9);
  printf("  Number of events built in current block build       : %d\n",
	 (reb.monitor & 0x000001FF));
  printf("\n");

  printf("                          Event Build Progress\n");
  printf("  Maximum number of events on board waiting to be build\n");
  printf("    (since last reset): %d\n",
	 reb.progress & 0x00FFFFFF);
  
  printf("---------------------------------------------------------------------- \n");
  
  printf("\n");
  printf("\n");

  return OK;
}

int
f1BusyControl(int id, int en_fifo_word_count, int en_event_not_built_count,
	      int min_not_built_count)
{
  if(id==0) id=f1ID[0];

  if((id<=0) || (id>21) || (f1p[id] == NULL)) 
    {
      logMsg("f1BusyControl: ERROR : TDC in slot %d is not initialized \n",
	     id,2,3,4,5,6);
      return ERROR;
    }

  if(en_fifo_word_count >= 0)
    en_fifo_word_count = 1;
  else
    en_fifo_word_count = 0;
  
  if(en_event_not_built_count >= 0)
    en_event_not_built_count = 1;
  else
    en_event_not_built_count = 0;

  if((min_not_built_count < 0) || (min_not_built_count > 0xFFFF))
    {
      logMsg("f1BusyControl: ERROR: Invalid min_not_built_count (%d)\n",
	     min_not_built_count, 2, 3, 4, 5, 6);
      return ERROR;
    }
  
  F1LOCK;
  vmeWrite32(&f1p[id]->busy_control,
	     (en_fifo_word_count << 31) |
	     (en_event_not_built_count << 30) |
	     min_not_built_count);
  F1UNLOCK;

  return OK;
}

void
f1GBusyControl(int en_fifo_word_count, int en_event_not_built_count,
	      int min_not_built_count)
{
  int if1;

  for(if1 = 0; if1 < nf1tdc; if1++)
    f1BusyControl(f1Slot(if1),
		  en_fifo_word_count, en_event_not_built_count,
		  min_not_built_count);
  
}

void
f1GBusyStatus()
{
  unsigned int busy_control[F1_MAX_BOARDS+1];
  int if1, islot;
  
  F1LOCK;
  for(if1 = 0; if1 < nf1tdc; if1++)
    busy_control[if1] = vmeRead32(&f1p[f1Slot(if1)]->busy_control);
  F1UNLOCK;

  printf("\n");
  printf("\n");
  printf("                                  f1TDC Busy Status\n");
  printf("        ...Busy Assertion from...\n");
  printf("      Output fifo  Events not   Min Events ...Current Busy...  ...Latched...\n");
  printf("Slot   word count  built count  for busy   Status  Word Event  Word    Event\n");
  printf("--------------------------------------------------------------------------------\n");

  for(if1 = 0; if1 < nf1tdc; if1++)
    {
      islot = f1Slot(if1);
      printf(" %2d     ", islot);

      printf("%s    ",
	     (busy_control[if1] & F1_BUSYCONTROL_FIFO_WORD_CNT_EN) ?
	     " ENABLED" : "DISABLED");

      printf("%s      ",
	     (busy_control[if1] & F1_BUSYCONTROL_EVENTS_NOT_BUILT_CNT_EN) ?
	     " ENABLED" : "DISABLED");

      printf("%5d    ",
	     (busy_control[if1] & F1_BUSYCONTROL_MIN_EVENT_BUILT_BUSY_MASK));

      printf("%s    ",
	     (busy_control[if1] & F1_BUSYCONTROL_BUSY_STATUS) ?
	     "BUSY" : "----");

      printf("%s ",
	     (busy_control[if1] & F1_BUSYCONTROL_WORD_COUNT_BUSY_STATUS) ?
	     "BUSY" : "----");

      printf("%s   ",
	     (busy_control[if1] & F1_BUSYCONTROL_EVENT_COUNT_BUSY_STATUS) ?
	     "BUSY" : "----");

      printf("%s ",
	     (busy_control[if1] & F1_BUSYCONTROL_LATCHED_WORD_COUNT_BUSY) ?
	     "LATCHED" : "-------");

      printf("%s",
	     (busy_control[if1] & F1_BUSYCONTROL_LATCHED_EVENT_COUNT_BUSY) ?
	     "LATCHED" : "-------");

      printf("\n");
    }

  printf("--------------------------------------------------------------------------------\n");
  printf("\n");
  printf("\n");
  
}



/**
 * @ingroup Status
 * @brief Decode a data word from an f1TDC and print to standard out.
 *  @param id 
 *   - Slot Number
 *  @param data 32bit f1TDC data word
 *
 *
 */
void 
f1DataDecode(int id, unsigned int data)
{
  static unsigned int type_last = 15;	/* initialize to type FILLER WORD */
	
  unsigned int data_type, slot_id_hd, slot_id_tr, blk_evts, blk_num, blk_words;
  unsigned int new_type, evt_num, time_1, time_2, mod_id;
  int rev=0;
  int mode=0, factor=0;

  rev = (f1Rev[id] & F1_VERSION_BOARDREV_MASK)>>8;
  if(rev==2)
    mode = 1;

  factor = 2 - mode;
	
  if( data & 0x80000000 )		/* data type defining word */
    {
      new_type = 1;
      data_type = (data & 0x78000000) >> 27;
    }
  else
    {
      new_type = 0;
      data_type = type_last;
    }
        
  switch( data_type )
    {
    case 0:		/* BLOCK HEADER */
      slot_id_hd = (data & 0x7C00000) >> 22;
      mod_id = (data & 0x003C0000) >> 18;
      blk_num = (data & 0x3FF00) >> 8;
      blk_evts = (data & 0xFF);
      printf("      %08X - BLOCK HEADER  - slot = %u  id = %u  blk_evts = %u   n_blk = %u\n",
	     data, slot_id_hd, mod_id, blk_evts, blk_num);
      break;

    case 1:		/* BLOCK TRAILER */
      slot_id_tr = (data & 0x7C00000) >> 22;
      blk_words = (data & 0x3FFFFF);
      printf("      %08X - BLOCK TRAILER - slot = %u   n_words = %u\n",
	     data, slot_id_tr, blk_words);
      break;

    case 2:		/* EVENT HEADER */
      evt_num = (data & 0x3FFFFF);
      printf("      %08X - EVENT HEADER - evt_num = %u\n", data, evt_num);
      break;

    case 3:		/* TRIGGER TIME */
      if( new_type )
	{
	  time_1 = (data & 0xFFFFFF);
	  printf("      %08X - TRIGGER TIME 1 - time = %u\n", data, time_1);
	  type_last = 3;
	}    
      else
	{
	  if( type_last == 3 )
	    {
	      time_2 = (data & 0xFFFF);
	      printf("      %08X - TRIGGER TIME 2 - time = %u\n", data, time_2);
	    }    
	  else
	    printf("      %08X - TRIGGER TIME - (ERROR)\n", data);	                
	}    
      break;

    case 7:		/* EVENT DATA */
      printf("TDC = %08X   ED   ERR=%X  fake = %u  chip=%u  chan=%u  t = %u ", 
	     data, 
	     ((data >> 24) & 0x7), // ERR
	     ((data >> 22) & 1),   // Fake data flag
	     ((data >> 19) & 0x7), // chip
	     ((data >> 16) & 0x7), // chan
	     (data & 0xFFFF)); // t
      printf("(%u ps)\n", 
	     ( (data & 0xFFFF) * 56 * factor ));
      break;

    case 8:		/* CHIP HEADER */
      /* need 2 prints - maximum # of variables is 7 in VxWorks printf (?) */
      printf("TDC = %08X --CH-- (%X,%u)  chip=%u  chan=%u  trig = %u  t = %3u ", 
	     data, 
	     ((data >> 24) & 0x7), 
	     ((data >> 6) & 0x1), 
	     ((data >> 3) & 0x7), // chip
	     (data & 0x7),  //chan
	     ((data >> 16) & 0x3F),  // trig
	     ((data >> 7) & 0x1FF)); // t
      printf("(%u ps) (P=%u)\n", 
	     ( ( (data >> 7) & 0x1FF) * 56 * factor * 128 ),
	     ((data >> 6) & 0x1));
      break;

    case 14:		/* DATA NOT VALID (no data available) */
      printf("      %08X - DATA NOT VALID = %u\n", data, data_type);
      break;
    case 15:		/* FILLER WORD */
      printf("      %08X - FILLER WORD = %u\n", data, data_type);
      break;
       	        
    case 4:		/* UNDEFINED TYPE */
    case 5:		/* UNDEFINED TYPE */
    case 6:		/* UNDEFINED TYPE */
    case 9:		/* UNDEFINED TYPE */
    case 10:		/* UNDEFINED TYPE */
    case 11:		/* UNDEFINED TYPE */
    case 12:		/* UNDEFINED TYPE */
    case 13:		/* UNDEFINED TYPE */
    default:
      printf("      %08X - UNDEFINED TYPE = %u\n", data, data_type);
      break;
    }
	        
}        
