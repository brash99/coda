/*----------------------------------------------------------------------------*/
/**
 * @mainpage
 * <pre>
 *  Copyright (c) 2012        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Primitive trigger control for VME CPUs using the TJNAF Trigger
 *     Supervisor (TS) card
 *
 * </pre>
 *----------------------------------------------------------------------------*/

#ifdef VXWORKS
#include <vxWorks.h>
#include <sysLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <iv.h>
#include <semLib.h>
#include <vxLib.h>
#else
#include <sys/prctl.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "jvme.h"
#include "tsLib.h"

/** Mutex to guard TS read/writes */
pthread_mutex_t   tsMutex = PTHREAD_MUTEX_INITIALIZER;
 /** Mutex Lock */
#define TSLOCK     if(pthread_mutex_lock(&tsMutex)<0) perror("pthread_mutex_lock");
  /** Mutex Unlock */
#define TSUNLOCK   if(pthread_mutex_unlock(&tsMutex)<0) perror("pthread_mutex_unlock");

#define TILOCK TSLOCK
#define TIUNLOCK TSUNLOCK

#define tiVMESlot2PayloadPort tsVMESlot2PayloadPort


/* Global Variables */
volatile struct TS_A24RegStruct  *TSp=NULL;   /**< pointer to TS memory map */
volatile        unsigned int     *TSpd=NULL;  /**< pointer to TS data FIFO */
unsigned long tsA24Offset=0;                  /**< Difference in CPU A24 Base and VME A24 Base */
unsigned int tsA32Base  =0x10000000;                   /**< Minimum VME A32 Address for use by TS */
unsigned long tsA32Offset=0;                            /**< Difference in CPU A32 Base and VME A32 Base */
int tsCrateID=0x59;                           /**< Crate ID */
int tsBlockLevel=0;                           /**< Current Block level for TS */
int tsNextBlockLevel=0;                       /**< Next Block level for TS */
int tsBlockBufferLevel=0;                     /**< Current Block Buffer level for TS */
unsigned int        tsIntCount    = 0;
unsigned int        tsAckCount    = 0;
unsigned int        tsDaqCount    = 0;       /**< Block count from previous update (in daqStatus) */
unsigned int        tsReadoutMode = 0;
unsigned int        tsTriggerSource = 0;     /**< Set with tsSetTriggerSource(...) */
unsigned int        tsSlaveMask   = 0;       /**< TI Slaves (mask) to be used with TI Master */
int                 tsDoAck       = 0;       /**< Instruction to perform a Readout Acknowledge */
int                 tsNeedAck     = 0;       /**< Requirement to perform a Readout Acknowledge */
static BOOL         tsIntRunning  = FALSE;   /**< running flag */
static VOIDFUNCPTR  tsIntRoutine  = NULL;    /**< user intererrupt service routine */
static int          tsIntArg      = 0;       /**< arg to user routine */
static unsigned int tsIntLevel    = TS_INT_LEVEL;       /**< VME Interrupt level */
static unsigned int tsIntVec      = TS_INT_VEC;  /**< default interrupt vector */
static VOIDFUNCPTR  tsAckRoutine  = NULL;    /**< user trigger acknowledge routine */
static int          tsAckArg      = 0;       /**< arg to user trigger ack routine */
static int          tsVersion     = 0x0;     /**< Firmware version */
static int          tsSyncEventFlag = 0;     /**< Sync Event/Block Flag */
static int          tsSyncEventReceived = 0; /**< Indicates reception of sync event */
static int          tsPartitionID = 0;       /**< Partition ID (1-4) */
volatile struct PartitionStruct *TSpart=NULL; /**< pointer to partition registers */
static int          tsDoSyncResetRequest =0; /**< Option to request a sync reset during readout ack */
static int          tsSlotNumber=0;          /**< Slot number in which the TI resides */
static int          tsSwapTriggerBlock=0;    /**< Decision on whether or not to swap the trigger block endianness */
static int          tsBusError=0;            /**< Bus Error block termination */
static int          tsNoVXS=0;               /**< 1 if not in VXS Crate */
static int          tsSyncResetType=TS_SYNCCOMMAND_SYNCRESET_4US;  /* Set default SyncReset Type to Fixed 4 us */

static unsigned int tsTrigPatternData[8][256];  /**< Trigger Table to be loaded */
static int          tsDuplicationMode = 0;   /**< 0: Normal TS Mode, 1: Duplication TS Mode */

/* Interrupt/Polling routine prototypes (static) */
static void tsInt(void);
#ifndef VXWORKS
static void tsPoll(void);
static void tsStartPollingThread(void);

static void tsPartPoll(void);
static void tsPartStartPollingThread(void);
/* polling thread pthread and pthread_attr */
pthread_attr_t tspollthread_attr;
pthread_t      tspollthread;
#endif

#ifdef VXWORKS
extern  int sysBusToLocalAdrs(int, char *, char **);
extern  int intDisconnect(int);
extern  int sysIntEnable(int);
IMPORT  STATUS sysIntDisable(int);
IMPORT  STATUS sysVmeDmaDone(int, int);
IMPORT  STATUS sysVmeDmaSend(UINT32, UINT32, int, BOOL);
#endif

/** This is either 20 or 21 */
#define MAX_VME_SLOTS 21
/** VXS Payload Port to VME Slot map */
unsigned short PayloadPort[MAX_VME_SLOTS+1] =
  {
    0,     /**< Filler for mythical VME slot 0 */
#if MAX_VME_SLOTS == 21
    0,     /**< VME Controller */
#endif
    17, 15, 13, 11, 9, 7, 5, 3, 1,
    0,     /**< Switch Slot A - SD */
    0,     /**< Switch Slot B - CTP/GTP */
    2, 4, 6, 8, 10, 12, 14, 16,
    18     /**< VME Slot Furthest to the Right - TS */
  };

/**
 * @defgroup PreInit Pre-Initialization
 * @defgroup Config Initialization/Configuration
 * @defgroup Status Status
 * @defgroup Readout Data Readout
 * @defgroup IntPoll Interrupt/Polling
 * @defgroup Part TS Partitioning
 * @defgroup Dupl TS Duplication
 * @defgroup Deprec Deprecated - To be removed
 */

/**
 * @ingroup PreInit
 * @brief Set the CrateID to be used during initialization
 *
 * @param cid Crate ID
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetCrateID_preInit(int cid)
{
  if((cid<0) || (cid>0xff))
    {
      printf("%s: ERROR: Invalid Crate ID (%d)\n",
	     __FUNCTION__,cid);
      return ERROR;
    }

  tsCrateID = cid;

  return OK;
}


/**
 *  @ingroup Config
 *  @brief Initialize the TSp register space into local memory,
 *  and setup registers given user input
 *
 * @param tAddr
 *  - A24 VME Address of the TS
 *  - Slot number of TS (1 - 21)
 * @param   mode   - Readout/Triggering Mode
 *  - 0: External Trigger - Interrupt Mode
 *  - 2: External Trigger - Polling Mode
 *
 * @param iFlag  - Initialization mask
 *  - 0: Do not initialize the board, just setup the pointers to the registers
 *  - 2: Ignore firmware check
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
tsInit(unsigned int tAddr, unsigned int mode, int iFlag)
{
  unsigned long laddr;
  unsigned int rval, boardID, i2cread=0;
  unsigned int firmwareInfo;
  int stat;
  int noBoardInit=0, noFirmwareCheck=0;
  int tsType=0;

  /* Check VME address */
  if(tAddr<0 || tAddr>0xffffff)
    {
      printf("%s: ERROR: Invalid VME Address (%d)\n",__FUNCTION__,
	     tAddr);
    }
  if(tAddr==0)
    {
      printf("%s: Scanning for TS...\n",__FUNCTION__);
      tAddr=tsFind();

      if(tAddr==0)
	{
	  printf("%s: ERROR: Unable to find TS\n",__FUNCTION__);
	  return ERROR;
	}
    }
  if(tAddr<22)
    {
      /* User enter slot number, shift it to VME A24 address */
      printf("%s: Initializing using slot number %d (VME address 0x%x)\n",
	     __FUNCTION__,
	     tAddr, tAddr<<19);
      tAddr = tAddr<<19;
    }

  if(iFlag&TS_INIT_NO_INIT)
    {
      noBoardInit = 1;
    }
  if(iFlag&TS_INIT_SKIP_FIRMWARE_CHECK)
    {
      noFirmwareCheck=1;
    }
  if(iFlag&TS_INIT_DUPLICATION_MODE)
    {
      tsDuplicationMode=1;
    }
  else
    {
      tsDuplicationMode=0;
    }

  /* Form VME base address from slot number */
#ifdef VXWORKS
  stat = sysBusToLocalAdrs(0x39,(char *)(unsigned long)tAddr,(char **)&laddr);
  if (stat != 0)
    {
      printf("%s: ERROR: Error in sysBusToLocalAdrs res=%d \n",__FUNCTION__,stat);
      return ERROR;
    }
  else
    {
      printf("TS address = 0x%lx\n",laddr);
    }
#else
  stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)tAddr,(char **)&laddr);
  if (stat != 0)
    {
      printf("%s: ERROR: Error in vmeBusToLocalAdrs res=%d \n",__FUNCTION__,stat);
      return ERROR;
    }
  else
    {
      if(!noBoardInit)
	printf("TS VME (Local) address = 0x%.8x (0x%.8lx)\n",tAddr,laddr);
    }
#endif
  tsA24Offset = laddr-tAddr;

  /* Set Up pointer */
  TSp = (struct TS_A24RegStruct *)laddr;

  /* Check if TS board is readable */
#ifdef VXWORKS
  stat = vxMemProbe((char *)(&TSp->boardID),0,4,(char *)&rval);
#else
  stat = vmeMemProbe((char *)(&TSp->boardID),4,(char *)&rval);
#endif

  if (stat != 0)
    {
      printf("%s: ERROR: TS card not addressable\n",__FUNCTION__);
      TSp = NULL;
      return(-1);
    }
  else
    {
      /* Check that it is a TS */
      if(((rval&TS_BOARDID_TYPE_MASK)>>16) != TS_BOARDID_TYPE_TS)
	{
	  printf("%s: ERROR: Invalid Board ID: 0x%x (rval = 0x%08x)\n",
		 __FUNCTION__,
		 (rval&TS_BOARDID_TYPE_MASK)>>16,rval);
	  TSp=NULL;
	  return(ERROR);
	}
      /* Check if this is board has a valid slot number */
      boardID =  (rval&TS_BOARDID_GEOADR_MASK)>>8;
      if((boardID <= 0)||(boardID >21))
	{
	  printf("%s: ERROR: Board Slot ID is not in range: %d\n",
		 __FUNCTION__,boardID);
	  TSp=NULL;
	  return(ERROR);
	}
      tsSlotNumber = boardID;

      /* Determine whether or not we'll need to swap the trigger block endianess */
      if( ((TSp->boardID & TS_BOARDID_TYPE_MASK)>>16) != TS_BOARDID_TYPE_TS)
	tsSwapTriggerBlock=1;
      else
	tsSwapTriggerBlock=0;
    }

  /* Check to see if we're in a VXS Crate */
  if((boardID==20) || (boardID==21))
    { /* It's possible... now check for valid i2c to SWB (SD) */
      i2cread = vmeRead32(&TSp->SWB[(0x3C7C/4)]) & 0xFFFF; /* Device 1, Address 0x1F */
      if((i2cread!=0) && (i2cread!=0xffff))
	{ /* Valid response */
	  vmeSetMaximumVMESlots(boardID);
	  tsNoVXS=0;
	}
      else
	{
	  tsNoVXS=0;
	}
    }


  /* Check if we should exit here, or initialize some board defaults */
  if(noBoardInit)
    {
      return OK;
    }

  /* Perform soft reset */
  tsReset();

  /* Get the Firmware Information and print out some details */
  firmwareInfo = tsGetFirmwareVersion();
  if(firmwareInfo>0)
    {
      printf("  User ID: 0x%x \tFirmware (type - revision): %X - %x.%x\n",
	     (firmwareInfo&TS_FIRMWARE_ID_MASK)>>16,
	     (firmwareInfo&TS_FIRMWARE_TYPE_MASK)>>12,
	     (firmwareInfo&TS_FIRMWARE_MAJOR_VERSION_MASK)>>4,
	     firmwareInfo&TS_FIRWMARE_MINOR_VERSION_MASK);

      tsVersion = firmwareInfo&0xFFF;
      tsType    = (firmwareInfo&TS_FIRMWARE_TYPE_MASK)>>12;
      if((tsVersion < TS_SUPPORTED_FIRMWARE) || (tsType!=TS_SUPPORTED_TYPE))
	{
	  if(noFirmwareCheck)
	    {
	      printf("%s: WARN: Type %x Firmware version (0x%x) not supported by this driver.\n  Supported: Type %x version 0x%x (IGNORED)\n",
		     __FUNCTION__,
		     tsType,tsVersion,TS_SUPPORTED_TYPE,TS_SUPPORTED_FIRMWARE);
	    }
	  else
	    {
	      printf("%s: ERROR: Type %x Firmware version (0x%x) not supported by this driver.\n  Supported Type %x version 0x%x\n",
		     __FUNCTION__,
		     tsType,tsVersion,TS_SUPPORTED_TYPE,TS_SUPPORTED_FIRMWARE);
	      TSp=NULL;
	      return ERROR;
	    }
	}
    }
  else
    {
      printf("%s:  ERROR: Invalid firmware 0x%08x\n",
	     __FUNCTION__,firmwareInfo);
      return ERROR;
    }

  /*** SET DEFAULTS ***/
  /* Disable trigger sources */
  tsDisableTriggerSource(0);

  tsReadoutMode = mode;
  switch(mode)
    {
    case TS_READOUT_EXT_INT:
    case TS_READOUT_EXT_POLL:
      if(tsNoVXS==1)
	{
	  /* BUSY from Loopback */
	  tsSetBusySource(TS_BUSY_LOOPBACK,1);
	}
      else
	{
	  /* BUSY from Loopback and Switch Slot B */
	  tsSetBusySource(TS_BUSY_LOOPBACK | TS_BUSY_SWB,1);
	}
      /* Onboard Clock Source */
      tsSetClockSource(TS_CLOCK_INTERNAL);
      /* Loopback Sync Source */
      tsSetSyncSource(TS_SYNC_LOOPBACK);
      break;

    default:
      printf("%s: ERROR: Invalid TS Mode %d\n",
	     __FUNCTION__,mode);
      return ERROR;
    }
  tsReadoutMode = mode;

  /* Initialize trigger table with default patterns */
  tsTriggerTableDefault();

  /* Reset I2C engine */
  vmeWrite32(&TSp->reset,TS_RESET_I2C);

  /* Set Default Block Level to 1, and default crateID */
  tsSetBlockLevel(1);
  tsSetCrateID(tsCrateID);

  /* Set Event format for CODA 3.0 */
  tsSetEventFormat(3);

  /* Setup A32 data buffer with library default */
  tsSetAdr32(tsA32Base);

  /* Enable Bus Errors on Block Transfer Terminiation, by default */
  tsEnableBusError();

  /* Set prescale factor to 1, by default */
  tsSetPrescale(0);

  /* MGT reset */
  tsResetMGT();

  /* Set this to 1 (ROC Lock mode), by default. */
  tsSetBlockBufferLevel(1);

  /* Setup a default Sync Delay and Pulse width */
  tsSetSyncDelayWidth(0x54, 0x3f, 0);

  /* Set Trigger 1 pulse delay (0*4ns = 0ns) and width [(7+1)*4ns = 32ns] */
  tsSetTriggerPulse(1,0,7);

  /* Set Trigger 2 pulse delay (0*4ns = 0ns) and width [(7+1)*4ns = 32ns] */
  tsSetTriggerPulse(2,0,7);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Find the TS within the prescribed "GEO Slot to A24 VME Address"
 *           range from slot 2 to 21.
 *
 *  @return A24 VME address if found.  Otherwise, 0
 */
unsigned int
tsFind()
{
  int islot, stat, tsFound=0;
  unsigned int tAddr, rval;
  unsigned long laddr;

  for(islot = 0; islot<20; islot++)
    {
      /* Form VME base address from slot number
       Start from slot 21 and 20, then go from 2 to 19 */
      switch(islot)
	{
	case 0:
	  tAddr = (21<<19);
	  break;
	case 1:
	  tAddr = (20<<19);
	  break;
	default:
	  tAddr = (islot<<19);
	}

#ifdef VXWORKS
      stat = sysBusToLocalAdrs(0x39,(char *)tAddr,(char **)&laddr);
#else
      stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)tAddr,(char **)&laddr);
#endif
      if(stat != 0)
	continue;

      /* Check if this address is readable */
#ifdef VXWORKS
      stat = vxMemProbe((char *)(laddr),0,4,(char *)&rval);
#else
      stat = vmeMemProbe((char *)(laddr),4,(char *)&rval);
#endif

      if (stat != 0)
	{
	  continue;
	}
      else
	{
	  /* Check that it is a TI */
	  if(((rval&TS_BOARDID_TYPE_MASK)>>16) != TS_BOARDID_TYPE_TS)
	    {
	      continue;
	    }
	  else
	    {
	      printf("%s: Found TS at 0x%08x\n",__FUNCTION__,tAddr);
	      tsFound=1;
	      break;
	    }
	}
    }

  if(tsFound)
    return tAddr;
  else
    return 0;

}


int
tsCheckAddresses()
{
  unsigned int offset=0, expected=0, base=0;
  int rval=OK;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  printf("%s:\n\t ---------- Checking TS address space ---------- \n",__FUNCTION__);

  base = (unsigned long) &TSp->boardID;

  offset = ((unsigned long) &TSp->trigger) - base;
  expected = 0x20;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->triggerSource not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->GTPtrigger) - base;
  expected = 0x40;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->GTPtrigger not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->syncWidth) - base;
  expected = 0x80;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->syncWidth not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->adr24) - base;
  expected = 0xD0;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->adr24 not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->reset) - base;
  expected = 0x100;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->reset not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->part1) - base;
  expected = 0x134;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->part1 not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->Scalers1) - base;
  expected = 0x180;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->Scalers1 not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->part2) - base;
  expected = 0x334;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->part2 not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->Scalers2) - base;
  expected = 0x380;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->Scalers2 not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->GTPTriggerTable[0]) - base;
  expected = 0x1080;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->GTPTriggerTable not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->FPTriggerTable[0]) - base;
  expected = 0x1090;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->FPTriggerTable not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }


  offset = ((unsigned long) &TSp->JTAGPROMBase[0]) - base;
  expected = 0x10000;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->JTAGPROMBase[0] not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->JTAGFPGABase[0]) - base;
  expected = 0x20000;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->JTAGFPGABase[0] not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->SWA[0]) - base;
  expected = 0x30000;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->SWA[0] not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  offset = ((unsigned long) &TSp->SWB[0]) - base;
  expected = 0x40000;
  if(offset != expected)
    {
      printf("%s: ERROR TSp->SWB[0] not at offset = 0x%x (@ 0x%x)\n",
	     __FUNCTION__,expected,offset);
      rval = ERROR;
    }

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Print some status information of the TS to standard out
 *
 *  @param    pflag  if pflag>0, print out raw registers
 *
 */
void
tsStatus(int pflag)
{
  unsigned int boardID, fiber, intsetup;
  unsigned int adr32, blocklevel, dataFormat, vmeControl, trigger, sync;
  unsigned int busy, clock, prescale, blockBuffer;
  unsigned int GTPtrigger, fpInput;
  unsigned int output;
  unsigned int livetime, busytime;
  unsigned int inputCounter;
  unsigned long TSBase;
  unsigned int blockStatus[5], iblock, nblocksReady, nblocksNeedAck;
  unsigned int nblocks;
  unsigned int ifiber, fibermask;
  unsigned int part_blockBuffer=0, part_busyConfig=0, part_busytime=0;
  unsigned long long int l1a_count=0;
  unsigned int syncEventCtrl, blocklimit;
  unsigned int GTPtriggerBufferLength;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  l1a_count    = tsGetEventCounter();
  tsGetCurrentBlockLevel();
  TSLOCK;
  boardID      = vmeRead32(&TSp->boardID);
  fiber        = vmeRead32(&TSp->fiber);
  intsetup     = vmeRead32(&TSp->intsetup);

  adr32        = vmeRead32(&TSp->adr32);
  blocklevel   = vmeRead32(&TSp->blocklevel);
  dataFormat   = vmeRead32(&TSp->dataFormat);
  vmeControl   = vmeRead32(&TSp->vmeControl);
  trigger      = vmeRead32(&TSp->trigger);
  sync         = vmeRead32(&TSp->sync);
  busy         = vmeRead32(&TSp->busy);
  clock        = vmeRead32(&TSp->clock);
  prescale     = vmeRead32(&TSp->trig1Prescale);
  blockBuffer  = vmeRead32(&TSp->blockBuffer);

  GTPtrigger   = vmeRead32(&TSp->GTPtrigger);
  fpInput      = vmeRead32(&TSp->fpInput);

  output       = vmeRead32(&TSp->output);
  syncEventCtrl= vmeRead32(&TSp->syncEventCtrl);
  blocklimit   = vmeRead32(&TSp->blocklimit);

  /* Latch scalers before readout */
  vmeWrite32(&TSp->reset, TS_RESET_LATCH_TIMERS);
  livetime     = vmeRead32(&TSp->livetime);
  busytime     = vmeRead32(&TSp->busytime);

  GTPtriggerBufferLength = vmeRead32(&TSp->GTPtriggerBufferLength);

  inputCounter = vmeRead32(&TSp->inputCounter);

  for(iblock=0;iblock<4;iblock++)
    blockStatus[iblock] = vmeRead32(&TSp->blockStatus[iblock]);

  blockStatus[4] = vmeRead32(&TSp->adr24);
  nblocks      = vmeRead32(&TSp->nblocks);

  if((tsPartitionID!=0) && (TSpart!=NULL))
    {
      part_blockBuffer = vmeRead32(&TSpart->blockBuffer);
      part_busyConfig  = vmeRead32(&TSpart->busyConfig);
      part_busytime    = vmeRead32(&TSpart->busytime);
    }

  TSUNLOCK;

  TSBase = (unsigned long)TSp;

  printf("\n");
#ifdef VXWORKS
  printf("STATUS for TS at base address 0x%08x \n",
	 (unsigned int) TSp);
#else
  printf("STATUS for TS at VME (Local) base address 0x%08x (0x%08lx) \n",
	 (unsigned int)((unsigned long) TSp - tsA24Offset), (unsigned long) TSp);
#endif
  printf("--------------------------------------------------------------------------------\n");
  printf(" A32 Data buffer ");
  if((vmeControl&TS_VMECONTROL_A32) == TS_VMECONTROL_A32)
    {
      printf("ENABLED at ");
#ifdef VXWORKS
      printf("base address 0x%08x\n",
	     (unsigned int)TSpd);
#else
      printf("VME (Local) base address 0x%08x (0x%08lx)\n",
	     (unsigned int)((unsigned long)TSpd - tsA32Offset), (unsigned long)TSpd);
#endif
    }
  else
    printf("DISABLED\n");


  printf(" Readout Count: %d\n",tsIntCount);
  printf("     Ack Count: %d\n",tsAckCount);
  printf("     L1A Count: %llu\n",l1a_count);
  printf("   Block Count: %d\n",nblocks & TS_NBLOCKS_COUNT_MASK);
  printf("   Block Limit: %d   %s\n",blocklimit,
	 (blockBuffer & TS_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT)?"* Finished *":"");
  printf("trig1 prescale: %d\n", prescale);

  if(pflag>0)
    {
      printf("\n");
      printf(" Registers (offset):\n");
      printf("  boardID     (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->boardID) - TSBase, boardID);
      printf("  fiber       (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->fiber) - TSBase, fiber);
      printf("  intsetup    (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->intsetup) - TSBase, intsetup);
      printf("  adr32       (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->adr32) - TSBase, adr32);
      printf("  blocklevel  (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->blocklevel) - TSBase, blocklevel);
      printf("  dataFormat  (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->dataFormat) - TSBase, dataFormat);
      printf("  vmeControl  (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->vmeControl) - TSBase, vmeControl);
      printf("  trigger     (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->trigger) - TSBase, trigger);
      printf("  sync        (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->sync) - TSBase, sync);
      printf("  busy        (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->busy) - TSBase, busy);
      printf("  clock       (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->clock) - TSBase, clock);
      printf("  blockBuffer (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->blockBuffer) - TSBase, blockBuffer);

      printf("  output      (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->output) - TSBase, output);
      printf("  nblocks     (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->nblocks) - TSBase, nblocks);

      printf("  livetime    (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->livetime) - TSBase, livetime);
      printf("  busytime    (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->busytime) - TSBase, busytime);
      printf("  GTPtrgBufLen(0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->GTPtriggerBufferLength) - TSBase, GTPtriggerBufferLength);
    }
  printf("\n");

  printf(" Block Level        = %d ", tsBlockLevel);
  if(tsBlockLevel != tsNextBlockLevel)
    printf("(To be set = %d)\n", tsNextBlockLevel);
  else
    printf("\n");

  printf(" Block Buffer Level = %d\n",
	 blockBuffer & TS_BLOCKBUFFER_BUFFERLEVEL_MASK);

  if((syncEventCtrl & TS_SYNCEVENTCTRL_NBLOCKS_MASK) == 0)
    printf(" Sync Events DISABLED\n");
  else
    printf(" Sync Event period  = %d blocks\n",
	   syncEventCtrl & TS_SYNCEVENTCTRL_NBLOCKS_MASK);

  printf("\n");

  if(tsSlaveMask)
    {
      printf(" TI Slaves Configured on HFBR (0x%x) = ",tsSlaveMask);
      for(ifiber=0; ifiber<2; ifiber++)
	{
	  if( tsSlaveMask & (1<<ifiber))
	    printf(" %d",ifiber+1);
	}
      printf("\n");
    }
  else
    printf(" No TI Slaves Configured on HFBR\n");


  if(tsTriggerSource&TS_TRIGGER_SOURCEMASK)
    {
      if(trigger)
	printf(" Trigger input source (%s) =\n",
	       (blockBuffer & TS_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT)?"DISABLED on Block Limit":
	       "ENABLED");
      else
	printf(" Trigger input source (DISABLED) =\n");
      if(tsTriggerSource & TS_TRIGGER_P0)
	printf("   P0 Input\n");
      if(tsTriggerSource & TS_TRIGGER_HFBR1)
	printf("   HFBR #1 Input\n");
      if(tsTriggerSource & TS_TRIGGER_LOOPBACK)
	printf("   Loopback\n");
      if(tsTriggerSource & TS_TRIGGER_FPTRG)
	printf("   Front Panel TRG\n");
      if(tsTriggerSource & TS_TRIGGER_VME)
	printf("   VME Command\n");
      if(tsTriggerSource & TS_TRIGGER_TSINPUTS)
	printf("   Front Panel TS Inputs\n");
      if(tsTriggerSource & TS_TRIGGER_TSREV2)
	printf("   Trigger Supervisor (rev2)\n");
      if(tsTriggerSource & TS_TRIGGER_PULSER)
	printf("   Internal Pulser\n");
      if(tsTriggerSource & TS_TRIGGER_ENABLE)
	printf("   FP/Ext/GTP\n");
    }
  else
    {
      printf(" No Trigger input sources\n");
    }

  if(sync&TS_SYNC_SOURCEMASK)
    {
      printf(" Sync source = \n");
      if(sync & TS_SYNC_P0)
	printf("   P0 Input\n");
      if(sync & TS_SYNC_HFBR1)
	printf("   HFBR #1 Input\n");
      if(sync & TS_SYNC_HFBR5)
	printf("   HFBR #5 Input\n");
      if(sync & TS_SYNC_FP)
	printf("   Front Panel Input\n");
      if(sync & TS_SYNC_LOOPBACK)
	printf("   Loopback\n");
      if(sync & TS_SYNC_USER_SYNCRESET_ENABLED)
	printf("   User SYNCRESET Receieve Enabled\n");
    }
  else
    {
      printf(" No SYNC input source configured\n");
    }

  if(busy&TS_BUSY_SOURCEMASK)
    {
      printf(" BUSY input source = \n");
      if(busy & TS_BUSY_SWA)
	printf("   Switch Slot A    %s\n",(busy&TS_BUSY_MONITOR_SWA)?"** BUSY **":"");
      if(busy & TS_BUSY_SWB)
	printf("   Switch Slot B    %s\n",(busy&TS_BUSY_MONITOR_SWB)?"** BUSY **":"");
      if(busy & TS_BUSY_P2)
	printf("   P2 Input         %s\n",(busy&TS_BUSY_MONITOR_P2)?"** BUSY **":"");
      if(busy & TS_BUSY_FP_FTDC)
	printf("   Front Panel TDC  %s\n",(busy&TS_BUSY_MONITOR_FP_FTDC)?"** BUSY **":"");
      if(busy & TS_BUSY_FP_FADC)
	printf("   Front Panel ADC  %s\n",(busy&TS_BUSY_MONITOR_FP_FADC)?"** BUSY **":"");
      if(busy & TS_BUSY_FP)
	printf("   Front Panel      %s\n",(busy&TS_BUSY_MONITOR_FP)?"** BUSY **":"");
      if(busy & TS_BUSY_LOOPBACK)
	printf("   Loopback         %s\n",(busy&TS_BUSY_MONITOR_LOOPBACK)?"** BUSY **":"");
      if(busy & TS_BUSY_TI_A)
	printf("   Fiber 1          %s\n",(busy&TS_BUSY_MONITOR_TI_A)?"** BUSY **":"");
      if(busy & TS_BUSY_TI_B)
	printf("   Fiber 2          %s\n",(busy&TS_BUSY_MONITOR_TI_B)?"** BUSY **":"");
      if(busy & TS_BUSY_INT)
	printf("   Unread blocks\n");
      if(busy & TS_BUSY_ALL_BRANCHES)
	printf("   Dupl Branches\n  %s\n",(busy&TS_BUSY_MONITOR_DUPL)?"** BUSY **":"");

    }
  else
    {
      printf(" No BUSY input source configured\n");
    }

  printf(" External Trigger Inputs:\n");
  printf("         GTP Input MASK = 0x%08x\n",GTPtrigger);
  printf("          FP Input MASK = 0x%08x\n",fpInput);

  printf("\n");
  printf(" Trigger Rules:\n");
  tsPrintTriggerHoldoff(pflag);

  if(intsetup&TS_INTSETUP_ENABLE)
    printf(" Interrupts ENABLED\n");
  else
    printf(" Interrupts DISABLED\n");
  printf("   Level = %d   Vector = 0x%02x\n",
	 (intsetup&TS_INTSETUP_LEVEL_MASK)>>8, (intsetup&TS_INTSETUP_VECTOR_MASK));

  if(vmeControl&TS_VMECONTROL_BERR)
    printf(" Bus Errors Enabled\n");
  else
    printf(" Bus Errors Disabled\n");

  printf("\n");
  printf(" Blocks ready for readout: %d\n",(blockBuffer&TS_BLOCKBUFFER_BLOCKS_READY_MASK)>>24);

  /* Slave block status */
  fibermask = tsSlaveMask;
  for(ifiber=0; ifiber<8; ifiber++)
    {
      if( fibermask & (1<<ifiber) )
	{
	  if( (ifiber % 2) == 0)
	    {
	      nblocksReady   = blockStatus[ifiber/2] & TS_BLOCKSTATUS_NBLOCKS_READY0;
	      nblocksNeedAck = (blockStatus[ifiber/2] & TS_BLOCKSTATUS_NBLOCKS_NEEDACK0)>>8;
	    }
	  else
	    {
	      nblocksReady   = (blockStatus[(ifiber-1)/2] & TS_BLOCKSTATUS_NBLOCKS_READY1)>>16;
	      nblocksNeedAck = (blockStatus[(ifiber-1)/2] & TS_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
	    }
	  printf("  Fiber %d  :  Blocks ready / need acknowledge: %d / %d\n",
		 ifiber+1,nblocksReady, nblocksNeedAck);
	}
    }

  /* Loopback block status */
  nblocksReady   = (blockStatus[4] & TS_BLOCKSTATUS_NBLOCKS_READY1)>>16;
  nblocksNeedAck = (blockStatus[4] & TS_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
  printf("  Loopback :  Blocks ready / need acknowledge: %d / %d\n",
	 nblocksReady, nblocksNeedAck);
  printf("              Events in current block: %d\n",
	 (nblocks & TS_NBLOCKS_EVENTS_IN_BLOCK_MASK)>>24);

  if((tsPartitionID!=0) && (TSpart!=NULL))
    {
      printf("Partition ID%d\n",tsPartitionID);
      printf("  blockBuffer = 0x%08x\n",part_blockBuffer);
      printf("  busyConfig  = 0x%08x\n",part_busyConfig);
      printf("  busytime    = 0x%08x\n",part_busytime);
    }

  printf("\n");
  printf(" Input counter %d\n",inputCounter);

  printf("--------------------------------------------------------------------------------\n");
  printf("\n\n");

}

/**
 *  @ingroup Config
 *  @brief Print a summary of all fiber port connections to potential TI Slaves
 *
 *  @param pflag
 *   - 0 - Default output
 *   - 1 - Print Raw Registers
 *
 */
void
tsSlaveStatus(int pflag)
{
  int iport=0, ibs=0, ifiber=0;
  unsigned int TSBase;
  unsigned int hfbr_tiID[2];
  unsigned int master_tiID;
  unsigned int blockStatus[3];
  unsigned int fiber=0, busy=0, trigsrc=0;
  int nblocksReady=0, nblocksNeedAck=0, slaveCount=0;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  for(iport=0; iport<2; iport++)
    {
      hfbr_tiID[iport] = vmeRead32(&TSp->hfbr_tiID[iport]);
    }
  master_tiID = vmeRead32(&TSp->master_tiID);
  fiber       = vmeRead32(&TSp->fiber);
  busy        = vmeRead32(&TSp->busy);
  trigsrc     = vmeRead32(&TSp->trigger);
  for(ibs=0; ibs<2; ibs++)
    {
      blockStatus[ibs] = vmeRead32(&TSp->blockStatus[ibs]);
    }
  blockStatus[3] = vmeRead32(&TSp->adr24);

  TSUNLOCK;

  TSBase = (unsigned long)TSp;

  if(pflag>0)
    {
      printf(" Registers (offset):\n");
      printf("  TSBase     (0x%08lx)\n",TSBase-tsA24Offset);
      printf("  busy           (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->busy) - TSBase, busy);
      printf("  fiber          (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->fiber) - TSBase, fiber);
      printf("  hfbr_tiID[0]   (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->hfbr_tiID[0]) - TSBase, hfbr_tiID[0]);
      printf("  hfbr_tiID[1]   (0x%04lx) = 0x%08x\n", (unsigned long)(&TSp->hfbr_tiID[1]) - TSBase, hfbr_tiID[1]);
      printf("  master_tiID    (0x%04lx) = 0x%08x\t", (unsigned long)(&TSp->master_tiID) - TSBase, master_tiID);

      printf("\n");
    }

  printf("TS Port STATUS Summary\n");
  printf("                                                      Block Status\n");
  printf("Port  ROCID   Connected   TrigSrcEn   Busy Status    Ready / NeedAck\n");
  printf("--------------------------------------------------------------------------------\n");
  /* Master first */
  /* Slot and Port number */
  printf("L     ");

  /* Port Name */
  printf("%5d      ",
	 (master_tiID&TS_ID_CRATEID_MASK)>>8);

  /* Connection Status */
  printf("%s      %s       ",
	 "YES",
	 (trigsrc & TS_TRIGGER_LOOPBACK)?"ENABLED ":"DISABLED");

  /* Busy Status */
  printf("%s       ",
	 (busy & TS_BUSY_MONITOR_LOOPBACK)?"BUSY":"    ");

  /* Block Status */
  nblocksReady   = (blockStatus[3] & TS_BLOCKSTATUS_NBLOCKS_READY1)>>16;
  nblocksNeedAck = (blockStatus[3] & TS_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
  printf("   %3d / %3d",nblocksReady, nblocksNeedAck);
  printf("\n");

  /* Slaves last */
  for(iport=1; iport<2; iport++)
    {
      /* Only continue of this port has been configured as a slave */
      if((tsSlaveMask & (1<<(iport-1)))==0) continue;

      /* Slot and Port number */
      printf("%d     ", iport);

      /* Port Name */
      printf("%5d      ",
	     (hfbr_tiID[iport-1]&TS_ID_CRATEID_MASK)>>8);

      /* Connection Status */
      printf("%s      %s       ",
	     "YES",
	     (hfbr_tiID[iport-1] & TS_ID_TRIGSRC_ENABLE_MASK)?"ENABLED ":"DISABLED");

      /* Busy Status */
      printf("%s       ",
	     (busy & TS_BUSY_MONITOR_FIBER_BUSY(iport))?"BUSY":"    ");

      /* Block Status */
      ifiber=iport-1;
      if( (ifiber % 2) == 0)
	{
	  nblocksReady   = blockStatus[ifiber/2] & TS_BLOCKSTATUS_NBLOCKS_READY0;
	  nblocksNeedAck = (blockStatus[ifiber/2] & TS_BLOCKSTATUS_NBLOCKS_NEEDACK0)>>8;
	}
      else
	{
	  nblocksReady   = (blockStatus[(ifiber-1)/2] & TS_BLOCKSTATUS_NBLOCKS_READY1)>>16;
	  nblocksNeedAck = (blockStatus[(ifiber-1)/2] & TS_BLOCKSTATUS_NBLOCKS_NEEDACK1)>>24;
	}
      printf("   %3d / %3d",nblocksReady, nblocksNeedAck);

      printf("\n");
      slaveCount++;
    }
  printf("\n");
  printf("Total Slaves Added = %d\n",slaveCount);

}

/**
 * @ingroup Status
 * @brief Get the trigger sources enabled bits of the selected port
 *
 * @param  port
 *       - 0 - Self
 *       - 1-2 - Fiber port 1-2
 *
 * @return bitmask of rigger sources enabled if successful, otherwise ERROR
 *         bitmask
 *         - 0 - P0
 *         - 1 - Fiber 1
 *         - 2 - Loopback
 *         - 3 - TRG (FP)
 *         - 4  - VME
 *         - 5 - TS Inputs (FP)
 *         - 6 - TS (rev 2)
 *         - 7 - Internal Pulser
 *
 */
int
tsGetPortTrigSrcEnabled(int port)
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((port<0) || (port>3))
    {
      printf("%s: ERROR: Invalid port (%d)\n",
	     __FUNCTION__,port);
    }

  TSLOCK;
  if(port==0)
    {
      rval = (vmeRead32(&TSp->master_tiID) & TS_ID_TRIGSRC_ENABLE_MASK);
    }
  else
    {
      rval = (vmeRead32(&TSp->hfbr_tiID[port-1]) & TS_ID_TRIGSRC_ENABLE_MASK);
    }
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Returns the mask of fiber channels that report a "connected"
 *     status from a TI has it's trigger source enabled.
 *
 * @return Trigger Source Enabled Mask
 */
int
tsGetTrigSrcEnabledFiberMask()
{
  int rval=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(vmeRead32(&TSp->hfbr_tiID[0]) & TS_ID_TRIGSRC_ENABLE_MASK)
    rval |= (1<<1);

  if(vmeRead32(&TSp->hfbr_tiID[1]) & TS_ID_TRIGSRC_ENABLE_MASK)
    rval |= (1<<2);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the Firmware Version
 *
 * @return Firmware Version if successful, ERROR otherwise
 *
 */
int
tsGetFirmwareVersion()
{
  unsigned int rval=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  /* reset the VME_to_JTAG engine logic */
  vmeWrite32(&TSp->reset,TS_RESET_JTAG);

  /* Reset FPGA JTAG to "reset_idle" state */
  vmeWrite32(&TSp->JTAGFPGABase[(0x003C)>>2],0);

  /* enable the user_code readback */
  vmeWrite32(&TSp->JTAGFPGABase[(0x092C)>>2],0x3c8);

  /* shift in 32-bit to FPGA JTAG */
  vmeWrite32(&TSp->JTAGFPGABase[(0x1F1C)>>2],0);

  /* Readback the firmware version */
  rval = vmeRead32(&TSp->JTAGFPGABase[(0x1F1C)>>2]);
  TSUNLOCK;

  return rval;
}


/**
 * @ingroup Config
 * @brief Reload the firmware on the FPGA
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsReload()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset,TS_RESET_JTAG);
  vmeWrite32(&TSp->JTAGPROMBase[(0x3c)>>2],0);
  vmeWrite32(&TSp->JTAGPROMBase[(0xf2c)>>2],0xEE);
  TSUNLOCK;

  printf ("%s: \n FPGA Re-Load ! \n",__FUNCTION__);
  return OK;

}

/**
 * @ingroup Status
 * @brief Get the Module Serial Number
 *
 * @param rSN  Pointer to string to pass Serial Number
 *
 * @return SerialNumber if successful, ERROR otherwise
 *
 */
unsigned int
tsGetSerialNumber(char **rSN)
{
  unsigned int rval=0;
  char retSN[10];
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset,TS_RESET_JTAG);           /* reset */
  vmeWrite32(&TSp->JTAGPROMBase[(0x3c)>>2],0);     /* Reset_idle */
  vmeWrite32(&TSp->JTAGPROMBase[(0xf2c)>>2],0xFD); /* load the UserCode Enable */
  vmeWrite32(&TSp->JTAGPROMBase[(0x1f1c)>>2],0);   /* shift in 32-bit of data */
  rval = vmeRead32(&TSp->JTAGPROMBase[(0x1f1c)>>2]);
  TSUNLOCK;

  if(rSN!=NULL)
    {
      sprintf(retSN,"TS-%d",rval&0xfff);
      strcpy((char *)rSN,retSN);
    }


  printf("%s: TS Serial Number is %s (0x%08x)\n",
	 __FUNCTION__,retSN,rval);

  return rval;


}

/**
 * @ingroup Config
 * @brief Resync the 250 MHz Clock
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsClockResync()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }


  TSLOCK;
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_AD9510_RESYNC);
  TSUNLOCK;

  printf ("%s: \n\t AD9510 ReSync ! \n",__FUNCTION__);
  return OK;

}

/**
 * @ingroup Config
 * @brief Perform a soft reset of the TS
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsReset()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset,TS_RESET_SOFT);
  TSUNLOCK;
  return OK;
}

/**
 * @ingroup Config
 * @brief Set the crate ID
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetCrateID(unsigned int crateID)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(crateID>0xff)
    {
      printf("%s: ERROR: Invalid crate id (0x%x)\n",__FUNCTION__,crateID);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->boardID,
	   (vmeRead32(&TSp->boardID) & ~TS_BOARDID_CRATEID_MASK)  | crateID);
  tsCrateID = crateID;
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the crate ID of the selected port
 *
 * @param  port
 *       - 0 - Self
 *       - 1-2 - Fiber port 1-2
 *
 * @return port Crate ID if successful, ERROR otherwise
 *
 */
int
tsGetCrateID(int port)
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((port<0) || (port>2))
    {
      printf("%s: ERROR: Invalid port (%d)\n",
	     __FUNCTION__,port);
    }

  TSLOCK;
  if(port==0)
    {
      rval = (vmeRead32(&TSp->master_tiID) & TS_ID_CRATEID_MASK)>>8;
    }
  else
    {
      rval = (vmeRead32(&TSp->hfbr_tiID[port-1]) & TS_ID_CRATEID_MASK)>>8;
    }
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set the number of events per block
 * @param blockLevel Number of events per block
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetBlockLevel(int blockLevel)
{
  return tsBroadcastNextBlockLevel(blockLevel);
}

/**
 * @ingroup Config
 * @brief Broadcast the next block level (to be changed at the end of
 * the next sync event, or during a call to tsSyncReset(1).
 *
 * @see tsSyncReset(1)
 * @param blockLevel block level to broadcats
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsBroadcastNextBlockLevel(int blockLevel)
{
  unsigned int trigger=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (blockLevel>TS_BLOCKLEVEL_MASK) || (blockLevel==0) )
    {
      printf("%s: ERROR: Invalid Block Level (%d)\n",__FUNCTION__,blockLevel);
      return ERROR;
    }

  TSLOCK;
  trigger = vmeRead32(&TSp->trigger);

  /* Turn on the VME trigger, if not enabled */
  if(!(trigger & TS_TRIGGER_VME))
    vmeWrite32(&TSp->trigger, TS_TRIGGER_VME | trigger);

  /* Broadcast using trigger command */
  vmeWrite32(&TSp->triggerCommand, TS_TRIGGERCOMMAND_SET_BLOCKLEVEL | blockLevel);

 /* Turn off the VME trigger, if it was initially disabled */
  if(!(trigger & TS_TRIGGER_VME))
    vmeWrite32(&TSp->trigger, trigger);

  TSUNLOCK;

  tsGetNextBlockLevel();

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the block level that will be updated on the end of the block readout.
 *
 * @return Next Block Level if successful, ERROR otherwise
 *
 */
int
tsGetNextBlockLevel()
{
  unsigned int reg_bl=0;
  int bl=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  reg_bl = vmeRead32(&TSp->blocklevel);
  bl = (reg_bl & TS_BLOCKLEVEL_RECEIVED_MASK)>>24;
  tsNextBlockLevel=bl;

  tsBlockLevel = (reg_bl & TS_BLOCKLEVEL_CURRENT_MASK)>>16;
  TSUNLOCK;

  return bl;
}

/**
 * @ingroup Status
 * @brief Get the current block level
 *
 * @return Next Block Level if successful, ERROR otherwise
 *
 */
int
tsGetCurrentBlockLevel()
{
  unsigned int reg_bl=0;
  int bl=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  reg_bl = vmeRead32(&TSp->blocklevel);
  bl = (reg_bl & TS_BLOCKLEVEL_CURRENT_MASK)>>16;
  tsBlockLevel = bl;

  tsNextBlockLevel = (reg_bl & TS_BLOCKLEVEL_RECEIVED_MASK)>>24;
  TSUNLOCK;

  /* Change Bus Error block termination, based on blocklevel */
  if(tsBlockLevel>2)
    {
      tsEnableBusError();
    }
  else
    {
      tsDisableBusError();
    }

  return bl;
}

/**
 * @ingroup Config
 * @brief Set TS to instantly change blocklevel when broadcast is received.
 *
 * @param enable Option to enable or disable this feature
 *       - 0: Disable
 *        !0: Enable
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetInstantBlockLevelChange(int enable)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(enable)
    vmeWrite32(&TSp->vmeControl,
	       vmeRead32(&TSp->vmeControl) | TS_VMECONTROL_BLOCKLEVEL_UPDATE);
  else
    vmeWrite32(&TSp->vmeControl,
	       vmeRead32(&TSp->vmeControl) & ~TS_VMECONTROL_BLOCKLEVEL_UPDATE);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Get Status of instant blocklevel change when broadcast is received.
 *
 * @return 1 if enabled, 0 if disabled , ERROR otherwise
 *
 */
int
tsGetInstantBlockLevelChange()
{
  int rval=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->vmeControl) & TS_VMECONTROL_BLOCKLEVEL_UPDATE)>>21;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set option to mix FP and GTP inputs
 *      Effectively swaps the FP(16:1) -> GTP'(32:17) and GTP(32:17) -> FP'(16:1)
 *      in both trigger table and data pattern output.
 * @param  enable
 * <pre>
 *    0 - disable
 *   !1 - enable
 * </pre>
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetInputMix(int enable)
{
  int bitset=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(enable)
    bitset = TS_CLOCK_INPUT_MIX_ENABLE;

  TSLOCK;
  vmeWrite32(&TSp->clock,
	     (vmeRead32(&TSp->clock) & ~TS_CLOCK_INPUT_MIX_CONTROL_MASK) | bitset);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup
 * @brief Get option to mix FP and GTP inputs
 *      Effectively swaps the FP(16:1) -> GTP'(32:17) and GTP(32:17) -> FP'(16:1)
 *      in both trigger table and data pattern output.
 * @return 0 if disabled, 1 if enabled, ERROR otherwise
 *
 */
int
tsGetInputMix()
{
  int rval=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->clock) & TS_CLOCK_INPUT_MIX_CONTROL_MASK)>>4;
  TSUNLOCK;

  if((rval == 0) || (rval == 3))
    rval = 0;
  else
    rval = 1;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set which GTP inputs are enabled
 * @param  inputmask -  MASK of which GTP inputs to enable
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetGTPInput(unsigned int inputmask)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->GTPtrigger,inputmask);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set which FP inputs are enabled
 * @param  inputmask -  MASK of which FP inputs (A-D) to enable
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetFPInput(unsigned int inputmask)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpInput,inputmask);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the trigger source
 *
 *     This routine will set a library variable to be set in the TS registers
 *     at a call to tsIntEnable.
 *
 *  @param trig - integer indicating the trigger source
 *    - 5: Random
 *    - 6: GTP/Ext/GTP
 *
 * @return OK if successful, ERROR otherwise
 */
int
tsSetTriggerSource(int trig)
{
  unsigned int trigenable=0;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (trig>7) || (trig<0) )
    {
      printf("%s: ERROR: Invalid Trigger Source (%d).  Must be between 0 and 7.\n",
	     __FUNCTION__,trig);
      return ERROR;
    }


  /* Set VME and Loopback by default */
  trigenable  = TS_TRIGGER_VME;
  trigenable |= TS_TRIGGER_LOOPBACK;

  switch(trig)
    {
    case 5:
      trigenable |= TS_TRIGGER_PULSER;
      break;

    case 6:
      trigenable |= TS_TRIGGER_ENABLE;
      break;

    }

  tsTriggerSource = trigenable;
  printf("%s: INFO: tsTriggerSource = 0x%x\n",__FUNCTION__,tsTriggerSource);

  return OK;
}

/**
 * @ingroup Config
 * @brief Set trigger sources with specified trigmask
 *
 *    This routine is for special use when tsSetTriggerSource(...) does
 *    not set all of the trigger sources that is required by the user.
 *
 * @param trigmask bits:
 *    - 0:  P0
 *    - 1:  HFBR #1
 *    - 2:  TI Master Loopback
 *    - 3:  Front Panel (TRG) Input
 *    - 4:  VME Trigger
 *    - 5:  Front Panel TS Inputs
 *    - 6:  TS (rev 2) Input
 *    - 7:  Random Trigger
 *    - 8:  FP/Ext/GTP
 *    - 9:  P2 Busy
 *
 * @return OK if successful, ERROR otherwise
 */
int
tsSetTriggerSourceMask(int trigmask)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  /* Check input mask */
  if(trigmask>TS_TRIGGER_SOURCEMASK)
    {
      printf("%s: ERROR: Invalid trigger source mask (0x%x).\n",
	     __FUNCTION__,trigmask);
      return ERROR;
    }

  tsTriggerSource = trigmask;

  return OK;
}

/**
 * @ingroup Config
 * @brief Enable trigger sources set by
 *                          tsSetTriggerSource(...) or
 *                          tsSetTriggerSourceMask(...)
 *
 * @sa tsSetTriggerSource tsSetTriggerSourceMask(...)
 *
 * @return OK if successful, ERROR otherwise
 */
int
tsEnableTriggerSource()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tsTriggerSource==0)
    {
      printf("%s: WARN: No Trigger Sources Enabled\n",__FUNCTION__);
    }

  TSLOCK;
  vmeWrite32(&TSp->trigger, tsTriggerSource);
  TSUNLOCK;


  return OK;

}


/**
 * @ingroup Config
 * @brief Disable trigger sources
 *
 * @param fflag
 *  -  0: Disable Triggers
 *  - >0: Disable Triggers and generate enough triggers to fill the current block
 *
 * @return OK if successful, ERROR otherwise
 */
int
tsDisableTriggerSource(int fflag)
{
  unsigned short ntries = 1000;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->trigger,0);
  TSUNLOCK;
  if(fflag)
    {
      tsFillToEndBlock();

      if(tsCurrentBlockFilled(ntries)==ERROR)
	{
	  printf("%s: WARN: Last block not complete after %d tries!\n",
		 __FUNCTION__,ntries);
	}
    }

  return OK;

}

/**
 * @ingroup Config
 * @brief Set the Sync source mask
 *
 * @param sync - MASK indicating the sync source
 *  - 0: P0
 *  - 1: HFBR1
 *  - 2: HFBR5
 *  - 3: Front Panel
 *  - 4: Loopback
 *
 * @return OK if successful, ERROR otherwise
 */
int
tsSetSyncSource(unsigned int sync)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(sync>TS_SYNC_SOURCEMASK)
    {
      printf("%s: ERROR: Invalid Sync Source Mask (%d).\n",
	     __FUNCTION__,sync);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->sync,sync);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the event format
 *
 * @param format - integer number indicating the event format
 *  - 0: 32 bit event number only
 *  - 1: 32 bit event number + 32 bit timestamp
 *  - 2: 32 bit event number + higher 16 bits of timestamp + higher 16 bits of eventnumber
 *  - 3: 32 bit event number + 32 bit timestamp + higher 16 bits of timestamp + higher 16 bits of eventnumber
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSetEventFormat(int format)
{
  unsigned int formatset=0;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (format>3) || (format<0) )
    {
      printf("%s: ERROR: Invalid Event Format (%d).  Must be between 0 and 3.\n",
	     __FUNCTION__,format);
      return ERROR;
    }

  TSLOCK;
  /* preserve the bits not being set */
  formatset = vmeRead32(&TSp->dataFormat) & (~TS_DATAFORMAT_WORDS_MASK);

  switch(format)
    {
    case 0:
      break;

    case 1:
      formatset |= TS_DATAFORMAT_TIMING_WORD;
      break;

    case 2:
      formatset |= TS_DATAFORMAT_HIGHERBITS_WORD;
      break;

    case 3:
      formatset |= (TS_DATAFORMAT_TIMING_WORD | TS_DATAFORMAT_HIGHERBITS_WORD);
      break;

    }

  vmeWrite32(&TSp->dataFormat,formatset);

  printf("%s: 0x%08x\n",__FUNCTION__,vmeRead32(&TSp->dataFormat));

  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set and enable the "software" trigger
 *
 *  @param trigger  trigger type 1 or 2 (playback trigger)
 *  @param nevents  integer number of events to trigger
 *  @param period_inc  period multiplier, depends on range (0-0x7FFF)
 *  @param range
 *     - 0: small period range (min: 120ns, increments of 120ns)
 *     - 1: large period range (min: 120ns, increments of 245.7us)
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsSoftTrig(int trigger, unsigned int nevents, unsigned int period_inc, int range)
{
  unsigned int periodMax=(TS_FIXEDPULSER1_PERIOD_MASK>>16);
  unsigned int reg=0;
  int time=0;

  if(TSp==NULL)
    {
      logMsg("\ntsSoftTrig: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(trigger!=1 && trigger!=2)
    {
      logMsg("\ntsSoftTrig: ERROR: Invalid trigger type %d\n",trigger,2,3,4,5,6);
      return ERROR;
    }

  if(nevents>TS_FIXEDPULSER1_NTRIGGERS_MASK)
    {
      logMsg("\ntsSoftTrig: ERROR: nevents (%d) must be less than %d\n",nevents,
	     TS_FIXEDPULSER1_NTRIGGERS_MASK,3,4,5,6);
      return ERROR;
    }
  if(period_inc>periodMax)
    {
      logMsg("\ntsSoftTrig: ERROR: period_inc (%d) must be less than %d ns\n",
	     period_inc,periodMax,3,4,5,6);
      return ERROR;
    }
  if( (range!=0) && (range!=1) )
    {
      logMsg("\ntsSoftTrig: ERROR: range must be 0 or 1\n",
	     periodMax,2,3,4,5,6);
      return ERROR;
    }

  if(range==0)
    time = 120+120*period_inc;
  if(range==1)
    time = 120+120*period_inc*2048;

  logMsg("\ntsSoftTrig: INFO: Setting software trigger for %d nevents with period of %d\n",
	 nevents,time,3,4,5,6);

  reg = (range<<31)| (period_inc<<16) | (nevents);
  TSLOCK;
  if(trigger==1)
    {
      vmeWrite32(&TSp->fixedPulser1, reg);
    }
  else if(trigger==2)
    {
      vmeWrite32(&TSp->fixedPulser2, reg);
    }
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Config
 * @brief Set the parameters of the random internal trigger
 *
 * @param trigger  - Trigger Selection
 *       -              1: trig1
 *       -              2: trig2
 * @param setting  - frequency prescale from 500MHz
 *
 * @sa tsDisableRandomTrigger
 * @return OK if successful, ERROR otherwise.
 *
 */
int
tsSetRandomTrigger(int trigger, int setting)
{
  double rate;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(trigger!=1 && trigger!=2)
    {
      logMsg("\ntsSetRandomTrigger: ERROR: Invalid trigger type %d\n",trigger,2,3,4,5,6);
      return ERROR;
    }

  if(setting>TS_RANDOMPULSER_TRIG1_RATE_MASK)
    {
      printf("%s: ERROR: setting (%d) must be less than %d\n",
	     __FUNCTION__,setting,TS_RANDOMPULSER_TRIG1_RATE_MASK);
      return ERROR;
    }

  if(setting>0)
    rate = ((double)500000) / ((double) (2<<(setting-1)));
  else
    rate = ((double)500000);

  printf("%s: Enabling random trigger (%d) at rate (kHz) = %.2f\n",
	 __FUNCTION__, trigger, rate);

  TSLOCK;
  if(trigger==1)
    vmeWrite32(&TSp->randomPulser,
	       setting | (setting<<4) | TS_RANDOMPULSER_TRIG1_ENABLE );
  else if (trigger==2)
    vmeWrite32(&TSp->randomPulser,
	       (setting | (setting<<4))<<8 | TS_RANDOMPULSER_TRIG2_ENABLE );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Disable random trigger generation
 * @sa tsSetRandomTrigger
 * @return OK if successful, ERROR otherwise.
 */
int
tsDisableRandomTrigger()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->randomPulser,0);
  TSUNLOCK;
  return OK;
}

/**
 * @ingroup Readout
 * @brief Read a block of events from the TI
 *
 * @param   data  - local memory address to place data
 * @param   nwrds - Max number of words to transfer
 * @param   rflag - Readout Flag
 *   - 0 - programmed I/O from the specified board
 *   - 1 - DMA transfer using Universe/Tempe DMA Engine
 *                    (DMA VME transfer Mode must be setup prior)
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */
int
tsReadBlock(volatile unsigned int *data, int nwrds, int rflag)
{
  int ii, dummy=0;
  int dCnt, retVal, xferCount;
  volatile unsigned int *laddr;
  unsigned int vmeAdr, val;

  if(TSp==NULL)
    {
      logMsg("\ntsReadBlock: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(TSpd==NULL)
    {
      logMsg("\ntsReadBlock: ERROR: TS A32 not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(data==NULL)
    {
      logMsg("\ntsReadBlock: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  TSLOCK;
  if(rflag >= 1)
    { /* Block transfer */
      if(tsBusError==0)
	{
	  logMsg("tsReadBlock: WARN: Bus Error Block Termination was disabled.  Re-enabling\n",
		 1,2,3,4,5,6);
	  TSUNLOCK;
	  tsEnableBusError();
	  TSLOCK;
	}

      /* Assume that the DMA programming is already setup.
	 Don't Bother checking if there is valid data - that should be done prior
	 to calling the read routine */

      /* Check for 8 byte boundary for address - insert dummy word (Slot 0 FADC Dummy DATA)*/
      if((unsigned long) (data)&0x7)
	{
#ifdef VXWORKS
	  *data = (TS_DATAFORMAT_DATA_TYPE_WORD) | (TS_DATAFORMAT_FILLER_WORD_TYPE) | (tsSlotNumber<<22);
#else
	  *data = LSWAP((TS_DATAFORMAT_DATA_TYPE_WORD) | (TS_DATAFORMAT_FILLER_WORD_TYPE) | (tsSlotNumber<<22));
#endif
	  dummy = 1;
	  laddr = (data + 1);
	}
      else
	{
	  dummy = 0;
	  laddr = data;
	}

      vmeAdr = ((unsigned long)(TSpd) - tsA32Offset);

#ifdef VXWORKS
      retVal = sysVmeDmaSend((UINT32)laddr, vmeAdr, (nwrds<<2), 0);
#else
      retVal = vmeDmaSend((unsigned long)laddr, vmeAdr, (nwrds<<2));
#endif
      if(retVal != 0)
	{
	  logMsg("\ntsReadBlock: ERROR in DMA transfer Initialization 0x%x\n",retVal,0,0,0,0,0);
	  TSUNLOCK;
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
#ifdef VXWORKS
	  xferCount = (nwrds - (retVal>>2) + dummy); /* Number of longwords transfered */
#else
	  xferCount = ((retVal>>2) + dummy); /* Number of longwords transfered */
#endif
	  TSUNLOCK;
	  return(xferCount);
	}
      else if (retVal == 0)
	{
#ifdef VXWORKS
	  logMsg("\ntsReadBlock: WARN: DMA transfer terminated by word count 0x%x\n",
		 nwrds,0,0,0,0,0);
#else
	  logMsg("\ntsReadBlock: WARN: DMA transfer returned zero word count 0x%x\n",
		 nwrds,0,0,0,0,0,0);
#endif
	  TSUNLOCK;
	  return(nwrds);
	}
      else
	{  /* Error in DMA */
#ifdef VXWORKS
	  logMsg("\ntsReadBlock: ERROR: sysVmeDmaDone returned an Error\n",
		 0,0,0,0,0,0);
#else
	  logMsg("\ntsReadBlock: ERROR: vmeDmaDone returned an Error\n",
		 0,0,0,0,0,0);
#endif
	  TSUNLOCK;
	  return(retVal>>2);

	}
    }
  else
    { /* Programmed IO */
      if(tsBusError==1)
	{
	  logMsg("tsReadBlock: WARN: Bus Error Block Termination was enabled.  Disabling\n",
		 1,2,3,4,5,6);
	  TSUNLOCK;
	  tsDisableBusError();
	  TSLOCK;
	}

      dCnt = 0;
      ii=0;

      while(ii<nwrds)
	{
	  val = (unsigned int) *TSpd;
#ifndef VXWORKS
	  val = LSWAP(val);
#endif
	  if(val == (TS_DATAFORMAT_DATA_TYPE_WORD | (TS_DATAFORMAT_TYPE_BLOCK_TRAILER<<27)
		     | (tsSlotNumber<<22) | (ii+1)) )
	    {
#ifndef VXWORKS
	      val = LSWAP(val);
#endif
	      data[ii] = val;

	      if(((ii+1)%2)!=0)
		{
		  /* Read out an extra word (filler) in the fifo */
		  val = (unsigned int) *TSpd;
#ifndef VXWORKS
		  val = LSWAP(val);
#endif
		  if(((val & TS_DATAFORMAT_DATA_TYPE_WORD) != TS_DATAFORMAT_DATA_TYPE_WORD) ||
		     ((val & TS_DATAFORMAT_TYPE_MASK) != TS_DATAFORMAT_FILLER_WORD_TYPE))
		    {
		      logMsg("\ntiReadBlock: ERROR: Unexpected word after block trailer (0x%08x)\n",
			     val,2,3,4,5,6);
		    }
		}

	      break;
	    }
#ifndef VXWORKS
	  val = LSWAP(val);
#endif
	  data[ii] = val;
	  ii++;
	}
      ii++;
      dCnt += ii;

      TSUNLOCK;
      return(dCnt);
    }

  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Readout
 * @brief Read a block from the TS and form it into a CODA Trigger Bank
 *
 * @param   data  - local memory address to place data
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */
int
tsReadTriggerBlock(volatile unsigned int *data)
{
  int rval=0, nwrds=0, rflag=0;
  int iword=0;
  unsigned int word=0;
  int iblkhead=-1, iblktrl=-1;


  if(data==NULL)
    {
      logMsg("\ntsReadTriggerBlock: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  /* Determine the maximum number of words to expect, from the block level */
  /* 4 words minimum.. up to 8 extra words per event + Up to 8 padding words */
  nwrds = (tsBlockLevel*(4+8)) + 8;

  /* Optimize the transfer type based on the blocklevel */
  if(tsBlockLevel>2)
    { /* Use DMA */
      rflag = 1;
    }
  else
    { /* Use programmed I/O (Single cycle reads) */
      rflag = 0;
    }

  /* Obtain the trigger bank by just making a call the tiReadBlock */
  rval = tsReadBlock(data, nwrds, rflag);
  if(rval < 0)
    {
      /* Error occurred */
      return ERROR;
    }
  else if (rval == 0)
    {
      /* No data returned */
      return 0;
    }

  /* Work down to find index of block header */
  while(iword<rval)
    {

      word = data[iword];
#ifndef VXWORKS
      word = LSWAP(word);
#endif
      if(word & TS_DATAFORMAT_DATA_TYPE_WORD)
	{
	  if(((word & TS_DATAFORMAT_TYPE_MASK)>>27) == TS_DATAFORMAT_TYPE_BLOCK_HEADER)
	    {
	      iblkhead = iword;
	      break;
	    }
	}
      iword++;
    }

  /* Check if the index is valid */
  if(iblkhead == -1)
    {
      logMsg("tsReadTriggerBlock: ERROR: Failed to find TS Block Header\n",
	     1,2,3,4,5,6);
      return ERROR;
    }
  if(iblkhead != 0)
    {
      logMsg("tsReadTriggerBlock: WARN: Invalid index (%d) for the TS Block header.\n",
	     iblkhead,2,3,4,5,6);
    }

  /* Work up to find index of block trailer */
  iword=rval-1;
  while(iword>=0)
    {

      word = data[iword];
#ifndef VXWORKS
      word = LSWAP(word);
#endif
      if(word & TS_DATAFORMAT_DATA_TYPE_WORD)
	{
	  if(((word & TS_DATAFORMAT_TYPE_MASK)>>27) == TS_DATAFORMAT_TYPE_BLOCK_TRAILER)
	    {
#ifdef CDEBUG
	      printf("%s: block trailer? 0x%08x\n",
		     __FUNCTION__,word);
#endif
	      iblktrl = iword;
	      break;
	    }
	}
      iword--;
    }

  /* Check if the index is valid */
  if(iblktrl == -1)
    {
      logMsg("tsReadTriggerBlock: ERROR: Failed to find TS Block Trailer\n",
	     1,2,3,4,5,6);
      return ERROR;
    }

  /* Get the block trailer, and check the number of words contained in it */
  word = data[iblktrl];
#ifndef VXWORKS
  word = LSWAP(word);
#endif
  if((iblktrl - iblkhead + 1) != (word & 0x3fffff))
    {
      logMsg("tsReadTriggerBlock: Number of words inconsistent (index count = %d, block trailer count = %d\n",
	     (iblktrl - iblkhead + 1), word & 0x3fffff,3,4,5,6);
      return ERROR;
    }

  /* Modify the total words returned */
  rval = iblktrl - iblkhead;

  /* Write in the Trigger Bank Length */
#ifdef VXWORKS
  data[iblkhead] = rval-1;
#else
  data[iblkhead] = LSWAP(rval-1);
#endif

  if(tsSwapTriggerBlock==1)
    {
      for(iword=iblkhead; iword<rval; iword++)
	{
	  word = data[iword];
	  data[iword] = LSWAP(word);
	}
    }

  return rval;

}

/**
 * @ingroup Readout
 * @brief Readout input scalers
 *   Returned data:
 *    bit 31
 *       0: As stored
 *       1: Shifted by 7 bits (must multiply by 2**7)
 *
 * @param data  - local memory address to place scaler data
 * @param choice
 *   - 1-4: Scaler set (1-4)
 *
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */
int
tsReadScalers(volatile unsigned int *data, int choice)
{
  int iscal=0, ichan=0;
  int banks=0;
  int nwrds=0;
  unsigned int tmpData=0;
  volatile struct ScalerStruct *scalers[4];

  if(TSp==NULL)
    {
      logMsg("\ntsReadScalers: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(data==NULL)
    {
      logMsg("\ntsReadScalers: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  scalers[0] = (struct ScalerStruct *)(&TSp->Scalers1);
  scalers[1] = (struct ScalerStruct *)(&TSp->Scalers2);
  scalers[2] = (struct ScalerStruct *)(&TSp->Scalers3);
  scalers[3] = (struct ScalerStruct *)(&TSp->Scalers4);

  TSLOCK;
  switch(choice)
    {
    case 1: /* GTP */
      banks = 8;
      for(iscal=0; iscal<4; iscal++)
	{
	  for(ichan=0; ichan<banks; ichan++)
	    {
	      tmpData = vmeRead32(&scalers[iscal]->GTP[ichan]);
	      if(tmpData & TS_SCALER_SCALE_HI)
		{
		  data[nwrds] = TS_SCALER_SCALE_HI |
		    ((tmpData & TS_SCALER_SCALE_HI_LSB_MASK) >> 7) |
		    ((tmpData & TS_SCALER_SCALE_HI_MSB_MASK) << 24);
		}
	      else
		data[nwrds] = tmpData;

	      nwrds++;
	    }
	}
      break;

    case 2: /* FP */
      banks = 4;
      for(iscal=0; iscal<4; iscal++)
	{
	  for(ichan=0; ichan<banks; ichan++)
	    {
	      tmpData = vmeRead32(&scalers[iscal]->fp[ichan]);
	      if(tmpData & TS_SCALER_SCALE_HI)
		{
		  data[nwrds] = TS_SCALER_SCALE_HI |
		    ((tmpData & TS_SCALER_SCALE_HI_LSB_MASK) >> 7) |
		    ((tmpData & TS_SCALER_SCALE_HI_MSB_MASK) << 24);
		}
	      else
		data[nwrds] = tmpData;

	      nwrds++;
	    }
	}
      break;

    case 3: /* Gen */
      banks = 8;
      for(iscal=0; iscal<4; iscal++)
	{
	  for(ichan=0; ichan<banks; ichan++)
	    {
	      tmpData = vmeRead32(&scalers[iscal]->gen[ichan]);
	      if(tmpData & TS_SCALER_SCALE_HI)
		{
		  data[nwrds] = TS_SCALER_SCALE_HI |
		    ((tmpData & TS_SCALER_SCALE_HI_LSB_MASK) >> 7) |
		    ((tmpData & TS_SCALER_SCALE_HI_MSB_MASK) << 24);
		}
	      else
		data[nwrds] = tmpData;

	      nwrds++;
	    }
	}
      break;

    }
  TSUNLOCK;

  return nwrds;
}

/**
 * @ingroup Readout
 * @brief Print input scalers to standard out
 *
 * @param choice
 *   - 1-4: Scaler set (1-4)
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */
int
tsPrintScalers(int choice)
{
  int ichan=0, nwrds=0;
  volatile unsigned int data[64];

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  switch(choice)
    {
    case 1: /* GTP */
      printf("GTP Scalers:\n");
      break;

    case 2: /* FP */
      printf("FP Scalers:\n");
      break;

    case 3: /* Ext */
      printf("Ext Scalers:\n");
      break;

    }

  nwrds = tsReadScalers(data,choice);
  for(ichan=0; ichan<nwrds; ichan++)
    {
      if((ichan%4)==0) printf("\n%2d: ",ichan);

      if(data[ichan] & TS_SCALER_SCALE_HI)
	printf("%16d ", (data[ichan] & ~TS_SCALER_SCALE_HI)*(2^7));
      else
	printf("%16d ", data[ichan]);
    }

  printf("\n\n");

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the busy source with a given sourcemask sourcemask bits:
 *
 * @param sourcemask
 *  - 0: SWA
 *  - 1: SWB
 *  - 2: P2
 *  - 3: FP-FTDC
 *  - 4: FP-FADC
 *  - 5: FP
 *  - 6: Unused
 *  - 7: Loopack
 *  - 8-9: Fiber 1-2
 *
 * @param rFlag - decision to reset the global source flags
 *  - 0: Keep prior busy source settings and set new "sourcemask"
 *  - 1: Reset, using only that specified with "sourcemask"
 *
 * @return OK if successful, ERROR otherwise.
 */
int
tsSetBusySource(unsigned int sourcemask, int rFlag)
{
  unsigned int busybits=0;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(sourcemask>TS_BUSY_SOURCEMASK)
    {
      printf("%s: ERROR: Invalid value for sourcemask (0x%x)\n",
	     __FUNCTION__, sourcemask);
      return ERROR;
    }

  if(sourcemask & TS_BUSY_P2_TRIGGER_INPUT)
    {
      printf("%s: ERROR: Do not use this routine to set P2 Busy as a trigger input.\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(rFlag)
    {
      /* Read in the previous value , resetting previous BUSYs*/
      busybits = vmeRead32(&TSp->busy) & ~(TS_BUSY_SOURCEMASK);
    }
  else
    {
      /* Read in the previous value , keeping previous BUSYs*/
      busybits = vmeRead32(&TSp->busy);
    }

  busybits |= sourcemask;

  vmeWrite32(&TSp->busy, busybits);
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Config
 * @brief Enable Bus Errors to terminate Block Reads
 * @sa tsDisableBusError
 * @return OK if successful, otherwise ERROR
 */
void
tsEnableBusError()
{

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->vmeControl,
	   vmeRead32(&TSp->vmeControl) | (TS_VMECONTROL_BERR) );
  tsBusError=1;
  TSUNLOCK;

}

/**
 * @ingroup Config
 * @brief Disable Bus Errors to terminate Block Reads
 * @sa tsEnableBusError
 * @return OK if successful, otherwise ERROR
 */
void
tsDisableBusError()
{

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->vmeControl,
	   vmeRead32(&TSp->vmeControl) & ~(TS_VMECONTROL_BERR) );
  tsBusError=0;
  TSUNLOCK;

}

/**
 * @ingroup Deprec
 * @brief Routine to return the VME slot, provided the VXS payload port
 * @param payloadport Payload port
 * @return Vme Slot
 */
int
tsPayloadPort2VMESlot(int payloadport)
{
  int rval=0;
  int islot;
  if(payloadport<1 || payloadport>18)
    {
      printf("%s: ERROR: Invalid payloadport %d\n",
	     __FUNCTION__,payloadport);
      return ERROR;
    }

  for(islot=1;islot<MAX_VME_SLOTS;islot++)
    {
      if(payloadport == PayloadPort[islot])
	{
	  rval = islot;
	  break;
	}
    }

  if(rval==0)
    {
      printf("%s: ERROR: Unable to find VME Slot from Payload Port %d\n",
	     __FUNCTION__,payloadport);
      rval=ERROR;
    }

  return rval;
}

/**
 * @ingroup Deprec
 * @brief Routine to return the VXS payload port, provided the VME Slot
 * @param vmeslot Vme Slot
 * @return Payload Port
 */
int
tsVMESlot2PayloadPort(int vmeslot)
{
  int rval=0;
  if(vmeslot<1 || vmeslot>MAX_VME_SLOTS)
    {
      printf("%s: ERROR: Invalid VME slot %d\n",
	     __FUNCTION__,vmeslot);
      return ERROR;
    }

  rval = (int)PayloadPort[vmeslot];

  if(rval==0)
    {
      printf("%s: ERROR: Unable to find Payload Port from VME Slot %d\n",
	     __FUNCTION__,vmeslot);
      rval=ERROR;
    }

  return rval;

}

/**
 *  @ingroup Config
 *  @brief Set the prescale factor for the external trigger
 *
 *  @param   prescale Factor for prescale.
 *               Max {prescale} available is 65535
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
tsSetPrescale(int prescale)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(prescale<0 || prescale>0xffff)
    {
      printf("%s: ERROR: Invalid prescale (%d).  Must be between 0 and 65535.",
	     __FUNCTION__,prescale);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->trig1Prescale, prescale);
  TSUNLOCK;

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Get the current prescale factor
 *  @return Current prescale factor, otherwise ERROR.
 */
int
tsGetPrescale()
{
  int rval;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->trig1Prescale);
  TSUNLOCK;

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Set the prescale for specified type and channel
 *  @param type  Type of input
 *    - 1: GTP
 *    - 2: FP
 *  @param chan  Channel of specified type
 *  @return Set prescale factor, otherwise ERROR.
 */
int
tsSetTriggerPrescale(int type, int chan, unsigned int prescale)
{
  int rval=OK;
  int bank=0,bitshift=0,chanmask=0xFFFF;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((type<1) || (type>2))
    {
      printf("%s: ERROR: Invalid Trigger Prescale type %d\n",
	     __FUNCTION__,type);
      return ERROR;
    }

  if(prescale>0xF)
    {
      printf("%s: ERROR: Invalid Trigger Prescale value 0x%x\n",
	     __FUNCTION__,prescale);
      return ERROR;
    }

  if(chan>32)
    {
      printf("%s: ERROR: Invalid Prescale Channel %d\n",
	     __FUNCTION__,chan);
      rval = ERROR;
    }

  TSLOCK;
  bank = (int) (chan / 8);
  bitshift = (4 * (int) (chan % 8));
  chanmask = (0XF) << bitshift;
  prescale = prescale << bitshift;

  switch(type)
    {
    case 1: /* GTP */
      vmeWrite32(&TSp->GTPprescale[bank],
		 (vmeRead32(&TSp->GTPprescale[bank]) & ~chanmask) |
		 (prescale & chanmask));
      break;

    case 2: /* FP */
      vmeWrite32(&TSp->fpInputPrescale[bank],
		 (vmeRead32(&TSp->fpInputPrescale[bank]) & ~chanmask) |
		 (prescale & chanmask));
      break;

    }
  TSUNLOCK;

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Get the prescale for specified type and channel
 *  @param type  Type of input
 *    - 1: GTP
 *    - 2: FP
 *  @param chan  Channel of specified type
 *  @return Channel's prescale factor, otherwise ERROR.
 *
*/
int
tsGetTriggerPrescale(int type, int chan)
{
  int rval=OK;
  int bank=0,bitshift=0,chanmask=0xFFFF;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((type<1) || (type>2))
    {
      printf("%s: ERROR: Invalid Trigger Prescale type %d\n",
	     __FUNCTION__,type);
      return ERROR;
    }

  if(chan>32)
    {
      printf("%s: ERROR: Invalid Prescale Channel %d\n",
	     __FUNCTION__,chan);
      rval = ERROR;
    }

  TSLOCK;
  bank = (int) (chan / 8);
  bitshift = (4 * (int) (chan % 8));
  chanmask = (0XF) << bitshift;

  switch(type)
    {
    case 1: /* GTP */
      rval = vmeRead32(&TSp->GTPprescale[bank]) & chanmask;
      break;

    case 2: /* FP */
      rval = vmeRead32(&TSp->fpInputPrescale[bank]) & chanmask;
      break;
    }

  rval = rval >> bitshift;
  TSUNLOCK;

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Set the characteristics of a specified trigger
 *
 *  @param trigger
 *           - 1: set for trigger 1
 *           - 2: set for trigger 2 (playback trigger)
 *  @param delay    delay in units of 4ns
 *  @param width    pulse width in units of 4ns
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetTriggerPulse(int trigger, int delay, int width)
{
  unsigned int rval=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(trigger<1 || trigger>2)
    {
      printf("%s: ERROR: Invalid trigger (%d).  Must be 1 or 2.\n",
	     __FUNCTION__,trigger);
      return ERROR;
    }
  if(delay<0 || delay>TS_TRIGDELAY_TRIG1_DELAY_MASK)
    {
      printf("%s: ERROR: Invalid delay (%d).  Must be less than %d\n",
	     __FUNCTION__,delay,TS_TRIGDELAY_TRIG1_DELAY_MASK);
      return ERROR;
    }
  if(width<0 || width>TS_TRIGDELAY_TRIG1_WIDTH_MASK)
    {
      printf("%s: ERROR: Invalid width (%d).  Must be less than %d\n",
	     __FUNCTION__,width,TS_TRIGDELAY_TRIG1_WIDTH_MASK);
    }

  TSLOCK;
  if(trigger==1)
    {
      rval = vmeRead32(&TSp->trigDelay) &
	~(TS_TRIGDELAY_TRIG1_DELAY_MASK | TS_TRIGDELAY_TRIG1_WIDTH_MASK) ;
      rval |= ( (delay) | (width<<8) );
      vmeWrite32(&TSp->trigDelay, rval);
    }
  if(trigger==2)
    {
      rval = vmeRead32(&TSp->trigDelay) &
	~(TS_TRIGDELAY_TRIG2_DELAY_MASK | TS_TRIGDELAY_TRIG2_WIDTH_MASK) ;
      rval |= ( (delay<<16) | (width<<24) );
      vmeWrite32(&TSp->trigDelay, rval);
    }
  TSUNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the delay time and width of the Sync signal
 *
 * @param delay  the delay (latency) set in units of 4ns.
 * @param width  the width set in units of 4ns.
 * @param twidth  if this is non-zero, set width in units of 32ns.
 *
 */
void
tsSetSyncDelayWidth(unsigned int delay, unsigned int width, int widthstep)
{
  int twidth=0, tdelay=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  if(delay>TS_SYNCDELAY_MASK)
    {
      printf("%s: ERROR: Invalid delay (%d)\n",__FUNCTION__,delay);
      return;
    }

  if(width>TS_SYNCWIDTH_MASK)
    {
      printf("%s: WARN: Invalid width (%d).\n",__FUNCTION__,width);
      return;
    }

  if(widthstep)
    width |= TS_SYNCWIDTH_LONGWIDTH_ENABLE;

  tdelay = delay*4;
  if(widthstep)
    twidth = (width&TS_SYNCWIDTH_MASK)*32;
  else
    twidth = width*4;

  printf("%s: Setting Sync delay = %d (ns)   width = %d (ns)\n",
	 __FUNCTION__,tdelay,twidth);

  TSLOCK;
  vmeWrite32(&TSp->syncDelay,delay);
  vmeWrite32(&TSp->syncWidth,width);
  TSUNLOCK;

}

/**
 * @ingroup Config
 * @brief Reset the trigger link.
 */
void
tsTrigLinkReset()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_TRIGGERLINK_DISABLE);
  taskDelay(1);

  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_TRIGGERLINK_DISABLE);
  taskDelay(1);

  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_TRIGGERLINK_ENABLE);
  taskDelay(1);
  TSUNLOCK;

  printf ("%s: Trigger Data Link was reset.\n",__FUNCTION__);
}

/**
 * @ingroup Config
 * @brief Disable the trigger link.
 */
void
tsTrigLinkDisable()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_TRIGGERLINK_DISABLE);
  taskDelay(1);

  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_TRIGGERLINK_DISABLE);
  taskDelay(1);

  TSUNLOCK;

  printf ("%s: Trigger Data Link was Disabled.\n",__FUNCTION__);
}

/**
 * @ingroup Config
 * @brief Set type of SyncReset to send to TI Slaves
 *
 * @param type Sync Reset Type
 *    - 0: User programmed width in each TI
 *    - !0: Fixed 4 microsecond width in each TI
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetSyncResetType(int type)
{

  if(type)
    tsSyncResetType=TS_SYNCCOMMAND_SYNCRESET_4US;
  else
    tsSyncResetType=TS_SYNCCOMMAND_SYNCRESET;

  return OK;
}

/**
 * @ingroup Config
 * @brief Generate a Sync Reset signal.
 *
 *  @param blflag Option to change block level, after SyncReset issued
 *       -   0: Do not change block level
 *       -  >0: Broadcast block level to all connected slaves (including self)
 *            BlockLevel broadcasted will be set to library value
 *            (Set with tsSetBlockLevel)
 *
 */
void
tsSyncReset(int blflag)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,tsSyncResetType);
  taskDelay(1);
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_RESET_EVNUM);
  taskDelay(1);
  TSUNLOCK;

  if(blflag) /* Set the block level from "Next" to Current */
    {
      printf("%s: INFO: Broadcasting Block Level %d, Buffer Level %d\n",
	     __FUNCTION__,
	     tsNextBlockLevel, tsBlockBufferLevel);
      tsBroadcastNextBlockLevel(tsNextBlockLevel);
      tsSetBlockBufferLevel(tsBlockBufferLevel);
    }

}

/**
 * @ingroup Config
 * @brief Generate a Sync Reset signal that resets the event buffer
 *
 */
void
tsResetEB()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_RESET_EVNUM);
  taskDelay(1);
  TSUNLOCK;

}

/**
 * @ingroup Config
 * @brief Generate a Sync Reset Resync signal.
 *
 *     This type of Sync Reset will NOT reset event numbers
 *
 */
void
tsSyncResetResync()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,tsSyncResetType);
  taskDelay(1);
  TSUNLOCK;

}

/**
 * @ingroup Config
 * @brief Generate a Clock Reset signal.  This signal is sent to the loopback and
 *    all configured TI Slaves.
 *
 */
void
tsClockReset()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_CLK250_RESYNC);
  TSUNLOCK;

}

/**
 * @ingroup Config
 * @brief Control level of the SyncReset signal
 * @sa tsSetUserSyncResetReceive
 * @param enable
 *   - >0: High
 *   -  0: Low
 */
void
tsUserSyncReset(int enable)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  if(enable)
    vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_SYNCRESET_HIGH);
  else
    vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_SYNCRESET_LOW);

  taskDelay(2);
  TSUNLOCK;

  printf("%s: User Sync Reset ",__FUNCTION__);
  if(enable)
    printf("HIGH\n");
  else
    printf("LOW\n");

}

/**
 * @ingroup Config
 * @brief Enable/Disable operation of User SyncReset
 * @sa tsUserSyncReset
 * @param enable
 *   - >0: Enable
 *   - 0: Disable
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetUserSyncResetReceive(int enable)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(enable)
    vmeWrite32(&TSp->sync, (vmeRead32(&TSp->sync) & TS_SYNC_SOURCEMASK) |
	       TS_SYNC_USER_SYNCRESET_ENABLED);
  else
    vmeWrite32(&TSp->sync, (vmeRead32(&TSp->sync) & TS_SYNC_SOURCEMASK) &
	       ~TS_SYNC_USER_SYNCRESET_ENABLED);
  TSUNLOCK;

  return OK;
}


/**
 * @ingroup Config
 * @brief Reset the registers that record the triggers enabled status of TI Slaves.
 *
 */
void
tsTriggerReadyReset()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,TS_SYNCCOMMAND_TRIGGER_READY_RESET);
  TSUNLOCK;


}

/**
 * @ingroup Config
 * @brief Reset the registers that record bit errors recorded on the trigger link.
 *
 */
void
tsTriggerLinkErrorReset()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncCommand,
	     TS_SYNCCOMMAND_GTP_STATUSB_RESET);
  TSUNLOCK;
}

/**
 * @ingroup Status
 * @brief Get the error status bits for the trigger links from specified TDs
 *
 * @param pflag
 *  - !0: Print to standard out
 *
 * @return Trigger Link bits if successful, ERROR otherwise
 */
unsigned int
tsGetTriggerLinkStatus(int pflag)
{
  unsigned int rval = 0, bitflags = 0;
  int ibit = 0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->GTPStatusB);
  TSUNLOCK;

  if(pflag)
    {
      printf("STATUS for Trigger Links\n");

      printf("      Connected    RX Data Error      Disparity    NON 8b/10b Data\n");
      printf("     (12345678)      (12345678)      (12345678)      (12345678)\n");
      printf("--------------------------------------------------------------------------------\n");

      printf("      ");
      bitflags = rval & TS_GTPSTATUSB_CHANNEL_BONDING_MASK;
      for(ibit = 0; ibit < 8; ibit++)
	{
	  if( (1<<ibit) & bitflags )
	    printf("%d", ibit+1);
	  else
	    printf("-");
	}

      printf("        ");
      bitflags = (rval & TS_GTPSTATUSB_DATA_ERROR_MASK) >> 8;
      for(ibit = 0; ibit < 8; ibit++)
	{
	  if( (1<<ibit) & bitflags )
	    printf("%d", ibit+1);
	  else
	    printf("-");
	}

      printf("        ");
      bitflags = (rval & TS_GTPSTATUSB_DISPARITY_ERROR_MASK) >> 16;
      for(ibit = 0; ibit < 8; ibit++)
	{
	  if( (1<<ibit) & bitflags )
	    printf("%d", ibit+1);
	  else
	    printf("-");
	}
      printf("        ");

      bitflags = (rval & TS_GTPSTATUSB_DATA_NOT_IN_TABLE_ERROR_MASK) >> 24;
      for(ibit = 0; ibit < 8; ibit++)
	{
	  if( (1<<ibit) & bitflags )
	    printf("%d", ibit+1);
	  else
	    printf("-");
	}

      printf("\n");
    }

  return rval;
}

/**
 * @ingroup Config
 * @brief Routine to set the A32 Base
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetAdr32(unsigned int a32base)
{
  unsigned long laddr=0;
  int res=0,a32Enabled=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(a32base<0x00800000)
    {
      printf("%s: ERROR: a32base out of range (0x%08x)\n",
	     __FUNCTION__,a32base);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->adr32,
	     (a32base & TS_ADR32_BASE_MASK) );

  vmeWrite32(&TSp->vmeControl,
	     vmeRead32(&TSp->vmeControl) | TS_VMECONTROL_A32);

  a32Enabled = vmeRead32(&TSp->vmeControl)&(TS_VMECONTROL_A32);
  if(!a32Enabled)
    {
      printf("%s: ERROR: Failed to enable A32 Address\n",__FUNCTION__);
      TSUNLOCK;
      return ERROR;
    }

#ifdef VXWORKS
  res = sysBusToLocalAdrs(0x09,(char *)a32base,(char **)&laddr);
  if (res != 0)
    {
      printf("%s: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",
	     __FUNCTION__,a32base);
      TSUNLOCK;
      return(ERROR);
    }
#else
  res = vmeBusToLocalAdrs(0x09,(char *)(unsigned long)a32base,(char **)&laddr);
  if (res != 0)
    {
      printf("%s: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n",
	     __FUNCTION__,a32base);
      TSUNLOCK;
      return(ERROR);
    }
#endif

  tsA32Base = a32base;
  tsA32Offset = laddr - tsA32Base;
  TSpd = (unsigned int *)(laddr);  /* Set a pointer to the FIFO */
  TSUNLOCK;

  printf("%s: A32 Base address set to 0x%08x\n",
	 __FUNCTION__,tsA32Base);

  return OK;
}

/**
 * @ingroup Config
 * @brief Reset the L1A counter, as incremented by the TS.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsResetEventCounter()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset, TS_RESET_RESET_L1A_NUMBER);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Returns the event counter (48 bit)
 *
 * @return Number of accepted events if successful, otherwise ERROR
 */
unsigned long long int
tsGetEventCounter()
{
  unsigned long long int rval=0;
  unsigned int lo=0, hi=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  lo = vmeRead32(&TSp->eventNumber_lo);
  hi = (vmeRead32(&TSp->eventNumber_hi) & TS_EVENTNUMBER_HI_MASK)>>16;

  rval = lo | ((unsigned long long)hi<<32);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set the block number at which triggers will be disabled automatically
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetBlockLimit(unsigned int limit)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->blocklimit,limit);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Returns the value that is currently programmed as the block limit
 *
 * @return Current Block Limit if successful, otherwise ERROR
 */
unsigned int
tsGetBlockLimit()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->blocklimit);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Get the current status of the block limit
 *
 * @return 1 if block limit has been reached, 0 if not, otherwise ERROR;
 *
 */
int
tsGetBlockLimitStatus()
{
  unsigned int reg=0, rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  reg = vmeRead32(&TSp->blockBuffer) & TS_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT;
  if(reg)
    rval = 1;
  else
    rval = 0;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set whether or not the latched pattern of GTP Inputs in block readout
 *
 * @param enable
 *    - 0: Disable
 *    - >0: Enable
 *
 * @return OK if successful, otherwise ERROR
 *
 */
int
tsSetGTPInputReadout(int enable)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(enable)
    vmeWrite32(&TSp->dataFormat,
	       vmeRead32(&TSp->dataFormat) | TS_DATAFORMAT_GTPINPUT_READOUT);
  else
    vmeWrite32(&TSp->dataFormat,
	       vmeRead32(&TSp->dataFormat) & ~TS_DATAFORMAT_GTPINPUT_READOUT);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set whether or not the latched pattern of FP Inputs in block readout
 *
 * @param enable
 *    - 0: Disable
 *    - >0: Enable
 *
 * @return OK if successful, otherwise ERROR
 *
 */
int
tsSetFPInputReadout(int enable)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(enable)
    vmeWrite32(&TSp->dataFormat,
	       vmeRead32(&TSp->dataFormat) | TS_DATAFORMAT_FPINPUT_READOUT);
  else
    vmeWrite32(&TSp->dataFormat,
	       vmeRead32(&TSp->dataFormat) & ~TS_DATAFORMAT_FPINPUT_READOUT);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Enable readout of FP (0-15) + GTP (0-15) before prescale latch pattern
 *
 * @param enable
 *    - 0: Disable
 *    - >0: Enable
 *
 * @return OK if successful, otherwise ERROR
 *
 */
int
tsSetBeforePrescaleReadout(int enable)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(enable)
    vmeWrite32(&TSp->dataFormat,
               vmeRead32(&TSp->dataFormat) | TS_DATAFORMAT_BEFORE_PRESCALE_READOUT);
  else
    vmeWrite32(&TSp->dataFormat,
               vmeRead32(&TSp->dataFormat) & ~TS_DATAFORMAT_BEFORE_PRESCALE_READOUT);
  TSUNLOCK;

  return OK;
}

/*************************************************************

 TS Interrupt/Polling routines

*************************************************************/

/**
 * @ingroup Status
 * @brief Return current readout count
 */
unsigned int
tsGetIntCount()
{
  return(tsIntCount);
}

/*************************************************************
 Interrupt/Polling routines
*************************************************************/

/*******************************************************************************
 *
 *  tsInt
 *  - Default interrupt handler
 *    Handles the TS interrupt.  Calls a user defined routine,
 *    if it was connected with tsIntConnect()
 *
 */

static void
tsInt(void)
{
  tsIntCount++;

  INTLOCK;

  if (tsIntRoutine != NULL)	/* call user routine */
    (*tsIntRoutine) (tsIntArg);

  /* Acknowledge trigger */
  if(tsDoAck==1)
    {
      tsIntAck();
    }
  INTUNLOCK;

}

/*******************************************************************************
 *
 *  tsPoll
 *  - Default Polling Server Thread
 *    Handles the polling of latched triggers.  Calls a user
 *    defined routine if was connected with tsIntConnect.
 *
 */
#ifndef VXWORKS
static void
tsPoll(void)
{
  int tsdata;

  int policy=0;
  struct sched_param sp;

  /* Set scheduler and priority for this thread */
  policy=SCHED_FIFO;
  sp.sched_priority=40;
  printf("%s: Entering polling loop...\n",__FUNCTION__);
  pthread_setschedparam(pthread_self(),policy,&sp);
  pthread_getschedparam(pthread_self(),&policy,&sp);
  printf ("%s: INFO: Running at %s/%d\n",__FUNCTION__,
	  (policy == SCHED_FIFO ? "FIFO"
	   : (policy == SCHED_RR ? "RR"
	      : (policy == SCHED_OTHER ? "OTHER"
		 : "unknown"))), sp.sched_priority);
  prctl(PR_SET_NAME,"tsPoll");

  while(1)
    {

      pthread_testcancel();

      /* If still need Ack, don't test the Trigger Status */
      if(tsNeedAck>0)
	{
	  continue;
	}

      tsdata = 0;

      tsdata = tsBReady();
      if(tsdata == ERROR)
	{
	  printf("%s: ERROR: tsIntPoll returned ERROR.\n",__FUNCTION__);
	  break;
	}

      if(tsdata && tsIntRunning)
	{
	  INTLOCK;
	  tsDaqCount = tsdata;
	  tsIntCount++;

	  if (tsIntRoutine != NULL)	/* call user routine */
	    (*tsIntRoutine) (tsIntArg);

	  /* Write to TS to Acknowledge Interrupt */
	  if(tsDoAck==1)
	    {
	      tsIntAck();
	    }
	  INTUNLOCK;
	}

    }
  printf("%s: Read ERROR: Exiting Thread\n",__FUNCTION__);
  pthread_exit(0);

}
#endif


/*******************************************************************************
 *
 *  tsStartPollingThread
 *  - Routine that launches tsPoll in its own thread
 *
 */
#ifndef VXWORKS
static void
tsStartPollingThread(void)
{
  int pts_status;

  pts_status =
    pthread_create(&tspollthread,
		   NULL,
		   (void*(*)(void *)) tsPoll,
		   (void *)NULL);
  if(pts_status!=0)
    {
      printf("%s: ERROR: TS Polling Thread could not be started.\n",
	     __FUNCTION__);
      printf("\t pthread_create returned: %d\n",pts_status);
    }

}
#endif

/**
 * @ingroup IntPoll
 * @brief Connect a user routine to the TS Interrupt or
 *    latched trigger, if polling.
 *
 * @param vector VME Interrupt Vector
 * @param routine Routine to call if block is available
 * @param arg argument to pass to routine
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsIntConnect(unsigned int vector, VOIDFUNCPTR routine, unsigned int arg)
{
#ifndef VXWORKS
  int status;
#endif

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return(ERROR);
    }


#ifdef VXWORKS
  /* Disconnect any current interrupts */
  if((intDisconnect(tsIntVec) !=0))
    printf("%s: Error disconnecting Interrupt\n",__FUNCTION__);
#endif

  tsResetEventCounter();
  tsIntCount = 0;
  tsAckCount = 0;
  tsDoAck = 1;

  /* Set Vector and Level */
  if((vector < 0xFF)&&(vector > 0x40))
    {
      tsIntVec = vector;
    }
  else
    {
      tsIntVec = TS_INT_VEC;
    }

  TSLOCK;
  vmeWrite32(&TSp->intsetup, (tsIntLevel<<8) | tsIntVec );
  TSUNLOCK;

  switch (tsReadoutMode)
    {
    case TS_READOUT_EXT_POLL:
      break;

    case TS_READOUT_EXT_INT:
#ifdef VXWORKS
      intConnect(INUM_TO_IVEC(tsIntVec),tsInt,arg);
#else
      status = vmeIntConnect (tsIntVec, tsIntLevel,
			      tsInt,arg);
      if (status != OK)
	{
	  printf("%s: vmeIntConnect failed with status = 0x%08x\n",
		 __FUNCTION__,status);
	  return(ERROR);
	}
#endif
      break;

    default:
      printf("%s: ERROR: TS Mode not defined (%d)\n",
	     __FUNCTION__,tsReadoutMode);
      return ERROR;
    }

  printf("%s: INFO: Interrupt Vector = 0x%x  Level = %d\n",
	 __FUNCTION__,tsIntVec,tsIntLevel);

  if(routine)
    {
      tsIntRoutine = routine;
      tsIntArg = arg;
    }
  else
    {
      tsIntRoutine = NULL;
      tsIntArg = 0;
    }

  return(OK);

}

/**
 * @ingroup IntPoll
 * @brief Disable interrupts or kill the polling service thread
 *
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsIntDisconnect()
{
#ifndef VXWORKS
  int status;
  void *res;
#endif

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tsIntRunning)
    {
      logMsg("tsIntDisconnect: ERROR: TS is Enabled - Call tsIntDisable() first\n",
	     1,2,3,4,5,6);
      return ERROR;
    }

  INTLOCK;

  switch (tsReadoutMode)
    {
    case TS_READOUT_EXT_INT:

#ifdef VXWORKS
      /* Disconnect any current interrupts */
      sysIntDisable(tsIntLevel);
      if((intDisconnect(tsIntVec) !=0))
	printf("%s: Error disconnecting Interrupt\n",__FUNCTION__);
#else
      status = vmeIntDisconnect(tsIntLevel);
      if (status != OK)
	{
	  printf("vmeIntDisconnect failed\n");
	}
#endif
      break;

    case TS_READOUT_EXT_POLL:
#ifndef VXWORKS
      if(tspollthread)
	{
	  if(pthread_cancel(tspollthread)<0)
	    perror("pthread_cancel");
	  if(pthread_join(tspollthread,&res)<0)
	    perror("pthread_join");
	  if (res == PTHREAD_CANCELED)
	    printf("%s: Polling thread canceled\n",__FUNCTION__);
	  else
	    printf("%s: ERROR: Polling thread NOT canceled\n",__FUNCTION__);
	}
#endif
      break;
    default:
      break;
    }

  INTUNLOCK;

  printf("%s: Disconnected\n",__FUNCTION__);

  return OK;

}

/**
 * @ingroup IntPoll
 * @brief Connect a user routine to be executed instead of the default
 *  TS interrupt/trigger latching acknowledge prescription
 *
 * @param routine Routine to call
 * @param arg argument to pass to routine
 * @return OK if successful, otherwise ERROR
 */
int
tsAckConnect(VOIDFUNCPTR routine, unsigned int arg)
{
  if(routine)
    {
      tsAckRoutine = routine;
      tsAckArg = arg;
    }
  else
    {
      printf("%s: WARN: routine undefined.\n",__FUNCTION__);
      tsAckRoutine = NULL;
      tsAckArg = 0;
      return ERROR;
    }
  return OK;
}

/**
 * @ingroup IntPoll
 * @brief Acknowledge an interrupt or latched trigger.  This "should" effectively
 *  release the "Busy" state of the TS.
 *
 *  Execute a user defined routine, if it is defined.  Otherwise, use
 *  a default prescription.
 */
void
tsIntAck()
{
  int resetbits=0;
  if(TSp == NULL) {
    logMsg("tsIntAck: ERROR: TS not initialized\n",0,0,0,0,0,0);
    return;
  }

  if (tsAckRoutine != NULL)
    {
      /* Execute user defined Acknowlege, if it was defined */
      TSLOCK;
      (*tsAckRoutine) (tsAckArg);
      TSUNLOCK;
    }
  else
    {
      TSLOCK;
      tsDoAck = 1;
      tsAckCount++;
      resetbits = TS_RESET_BUSYACK;

      if(tsDoSyncResetRequest)
	{
	  resetbits |= TS_RESET_SYNCRESET_REQUEST;
	  tsDoSyncResetRequest=0;
	}

      vmeWrite32(&TSp->reset,resetbits);
      TSUNLOCK;
    }

}

/**
 * @ingroup IntPoll
 * @brief Enable interrupts or latching triggers (depending on set TS mode)
 *
 * @param iflag if = 1, trigger counter will be reset
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsIntEnable(int iflag)
{
#ifdef VXWORKS
  int lock_key=0;
#endif

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return(-1);
    }

  if(iflag == 1)
    {
      tsResetEventCounter();
      tsIntCount = 0;
      tsAckCount = 0;
    }

  TSLOCK;
  tsIntRunning = 1;
  tsDoAck      = 1;
  tsNeedAck    = 0;

  switch (tsReadoutMode)
    {
#ifndef VXWORKS
    case TS_READOUT_EXT_POLL:
      tsStartPollingThread();
      break;
#endif

    case TS_READOUT_EXT_INT:
#ifdef VXWORKS
      lock_key = intLock();
      sysIntEnable(tsIntLevel);
#endif
      vmeWrite32(&TSp->intsetup,
	       vmeRead32(&TSp->intsetup) | TS_INTSETUP_ENABLE );
      break;

    default:
      tsIntRunning = 0;
#ifdef VXWORKS
      if(lock_key)
	intUnlock(lock_key);
#endif
      printf("%s: ERROR: TS Readout Mode not defined %d\n",
	     __FUNCTION__,tsReadoutMode);
      TSUNLOCK;
      return(ERROR);

    }

  TSUNLOCK; /* Locks performed in tsEnableTriggerSource() */

  taskDelay(30);
  tsEnableTriggerSource();

#ifdef VXWORKS
  if(lock_key)
    intUnlock(lock_key);
#endif

  return(OK);

}

/**
 * @ingroup IntPoll
 * @brief Disable interrupts or latching triggers
 *
*/
void
tsIntDisable()
{

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  tsDisableTriggerSource(0);

  TSLOCK;
  vmeWrite32(&TSp->intsetup,
	     vmeRead32(&TSp->intsetup) & ~(TS_INTSETUP_ENABLE));
  tsIntRunning = 0;
  TSUNLOCK;
}

/**
 * @ingroup Readout
 * @brief Returns the number of Blocks available for readout
 *
 * @return Number of blocks available for readout if successful, otherwise ERROR
 *
 */
unsigned int
tsBReady()
{
  unsigned int blockBuffer=0, readyInt=0, rval=0;

  if(TSp == NULL)
    {
      logMsg("tsBReady: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return 0;
    }

  TSLOCK;
  blockBuffer = vmeRead32(&TSp->blockBuffer);
  rval        = (blockBuffer&TS_BLOCKBUFFER_BLOCKS_READY_MASK)>>8;
  readyInt    = (blockBuffer&TS_BLOCKBUFFER_BREADY_INT_MASK)>>24;
  tsSyncEventReceived  = (blockBuffer&TS_BLOCKBUFFER_SYNCEVENT)>>31;

  if( (readyInt==1) && (tsSyncEventReceived) )
    tsSyncEventFlag = 1;
  else
    tsSyncEventFlag = 0;

  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Readout
 * @brief Return the value of the Synchronization flag, obtained from tsBReady.
 *   i.e. Return the value of the SyncFlag for the current readout block.
 *
 * @sa tsBReady
 * @return
 *   -  1: if current readout block contains a Sync Event.
 *   -  0: Otherwise
 *
 */
int
tsGetSyncEventFlag()
{
  int rval=0;

  TSLOCK;
  rval = tsSyncEventFlag;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Readout
 * @brief Return the value of whether or not the sync event has been received
 *
 * @return
 *     - 1: if sync event received
 *     - 0: Otherwise
 *
 */
int
tsGetSyncEventReceived()
{
  int rval=0;

  TSLOCK;
  rval = tsSyncEventReceived;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Config
 * @brief Set the block buffer level for the number of blocks in the system
 *     that need to be read out. This value is also broadcasted to TI-Slaves
 *
 *     If this buffer level is full, the TS will go BUSY.
 *     The BUSY is released as soon as the number of buffers in the system
 *     drops below this level.
 *
 *  @param     level
 *    -  0:  No Buffer Limit -  Pipeline mode
 *    -  1:  One Block Limit - "ROC LOCK" mode
 *    -  2-65535:  "Buffered" mode.
 *
 * @return OK if successful, otherwise ERROR
 *
 */

int
tsSetBlockBufferLevel(unsigned int level)
{
  unsigned int trigger = 0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(level>TS_BLOCKBUFFER_BUFFERLEVEL_MASK)
    {
      printf("%s: ERROR: Invalid value for level (%d)\n",
	     __FUNCTION__,level);
      return ERROR;
    }

  TSLOCK;
  /* Set local buffer level */
  vmeWrite32(&TSp->blockBuffer, level);

  tsBlockBufferLevel = level;

  /* Broadcast buffer level to TI-slaves */
  trigger = vmeRead32(&TSp->trigger);

  /* Turn on the VME trigger, if not enabled */
  if(!(trigger & TS_TRIGGER_VME))
    vmeWrite32(&TSp->trigger, TS_TRIGGER_VME | trigger);

  /* Broadcast using trigger command */
  vmeWrite32(&TSp->triggerCommand, TS_TRIGGERCOMMAND_SET_BUFFERLEVEL | level);

 /* Turn off the VME trigger, if it was initially disabled */
  if(!(trigger & TS_TRIGGER_VME))
    vmeWrite32(&TSp->trigger, trigger);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set (or unset) high level for the user controllable output ports on the front panel.
 *
 * @param         set1  OUT #3
 * @param         set2  OUT #4
 * @param         set3  OUT #5
 * @param         set4  OUT #6
 * @param         set5  OUT #11
 * @param         set6  OUT #12
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetOutputPort(unsigned int set1, unsigned int set2, unsigned int set3,
		unsigned int set4, unsigned int set5, unsigned int set6)
{
  unsigned int bits=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(set1)
    bits |= TS_OUTPUT_FP_1;
  if(set2)
    bits |= TS_OUTPUT_FP_2;
  if(set3)
    bits |= TS_OUTPUT_FP_3;
  if(set4)
    bits |= TS_OUTPUT_FP_4;
  if(set5)
    bits |= TS_OUTPUT_FP_5;
  if(set6)
    bits |= TS_OUTPUT_FP_6;

  TSLOCK;
  vmeWrite32(&TSp->output, bits);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the clock to the specified source.
 *
 * @param   source
 *  - 0:  Onboard clock
 *  - 1:  External clock (FP input)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetClockSource(unsigned int source)
{
  unsigned int clkset=0;
  char sClock[20] = "";

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  switch(source)
    {
    case 0: /* ONBOARD */
      clkset = TS_CLOCK_INTERNAL;
      sprintf(sClock,"ONBOARD (%d)",source);
      break;
    case 1: /* EXTERNAL (FP) */
      clkset = TS_CLOCK_EXTERNAL;
      sprintf(sClock,"EXTERNAL (%d)",source);
      break;
    default:
      printf("%s: ERROR: Invalid Clock Souce (%d)\n",__FUNCTION__,source);
      return ERROR;
    }

  printf("%s: Setting clock source to %s\n",__FUNCTION__,sClock);


  TSLOCK;
  vmeWrite32(&TSp->clock, clkset);
  /* Reset DCM (Digital Clock Manager) - 250/200MHz */
  vmeWrite32(&TSp->reset,TS_RESET_CLK250);
  taskDelay(1);
  /* Reset DCM (Digital Clock Manager) - 125MHz */
  vmeWrite32(&TSp->reset,TS_RESET_CLK125);
  taskDelay(1);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Reset the IO Delay on the TS
 *
 */
void
tsResetIODelay()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset,TS_RESET_IODELAY);
  taskDelay(10);
  TSUNLOCK;
}

int
tsGetIODelayControlReady(int pflag)
{
  int rval = 0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->GTPtriggerBufferLength)
	  & TS_GTPTRIGGERBUFFERLENGTH_IO_DELAY_CTRL_READY) ? 1 : 0;
  TSUNLOCK;

  if(pflag)
    {
      printf("%s: %s\n", __func__,
	     (rval) ? "Ready" : "Not Ready");
    }

  return rval;
}

/**
 * @ingroup Config
 * @brief Reset the configuration of TI Slaves on the TS.
 *      This routine removes all slaves and resets the fiber port busy's.
 *
 * @return OK if successful, ERROR otherwise
 *
 */
int
tsResetSlaveConfig()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  tsSlaveMask = 0;
  vmeWrite32(&TSp->busy, (vmeRead32(&TSp->busy) & ~TS_BUSY_HFBR_MASK));
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Config
 * @brief Add and configurate a TI Slave.
 *
 *      This routine should be used by the TS to configure
 *      HFBR port and BUSY sources.
 *
 * @param    fiber  The fiber port of the TS that is connected to the slave
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsAddSlave(unsigned int fiber)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fiber<1) || (fiber>2) )
    {
      printf("%s: ERROR: Invalid value for fiber (%d)\n",
	     __FUNCTION__,fiber);
      return ERROR;
    }

  /* Add this slave to the global slave mask */
  tsSlaveMask |= (1<<(fiber-1));

  /* Add this fiber as a busy source */
  switch(fiber)
    {
    case 1:
      if(tsSetBusySource(TS_BUSY_TI_A,0)!=OK)
	return ERROR;
      break;

    case 2:
    default:
      if(tsSetBusySource(TS_BUSY_TI_B,0)!=OK)
	return ERROR;
    }

  return OK;

}

/**
 *  @ingroup Config
 *  @brief Remove a TI Slave for the TS.
 *  @param  fiber  The fiber port of the TS to remove.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsRemoveSlave(unsigned int fiber)
{
  unsigned int busybits;

  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fiber<1) || (fiber>2) )
    {
      printf("%s: ERROR: Invalid value for fiber (%d)\n",
	     __FUNCTION__,fiber);
      return ERROR;
    }

  /* Remove this slave to the global slave mask */
  tsSlaveMask &= ~(1<<(fiber-1));

  /* Remove this fiber as a busy source (use first fiber macro as the base) */
  TSLOCK;
  /* Read in previous values, keeping current busy's */
  busybits = vmeRead32(&TSp->busy);

  /* Turn off busy to the fiber in question */
  busybits &= ~(1<<(TS_BUSY_TI_A-1+fiber));

  /* Write the new mask */
  vmeWrite32(&TSp->busy, busybits);
  TSUNLOCK;

  /* Keep the fiber enabled: No call to tdEnableFiber(..) */

  return OK;
}

static int tsTriggerRuleClockPrescale[3][4] =
  {
    {4, 4, 8, 16}, // 250 MHz ref
    {16, 32, 64, 128}, // 33.3 MHz ref
    {16, 32, 64, 128} // 33.3 MHz ref prescaled by 32
  };

/**
 * @ingroup Config
 * @brief Set the value for a specified trigger rule.
 *
 * @param   rule  the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @param   value  the specified time period (in steps of timestep)
 * @param timestep Timestep that is dependent on the trigger rule selected
 *<pre>
 *                           rule
 *    timestep    1       2       3       4
 *    --------  ------ ------- ------- --------
 *       0        16ns    16ns    32ns     64ns
 *       1       480ns   960ns  1920ns   3840ns
 *       2     15360ns 30720ns 61440ns 122880ns
 *</pre>
 *
 * @return OK if successful, otherwise ERROR.
 *
 */
int
tsSetTriggerHoldoff(int rule, unsigned int value, int timestep)
{
  unsigned int wval = 0, rval = 0;
  unsigned int maxvalue = 0x7f;
  unsigned int vmeControl = 0;
  static int slow_clock_previously_switched = 0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (rule<1) || (rule>5) )
    {
      printf("%s: ERROR: Invalid value for rule (%d).  Must be 1-4\n",
	     __FUNCTION__,rule);
      return ERROR;
    }
  if(value>maxvalue)
    {
      printf("%s: ERROR: Invalid value (%d). Must be less than %d.\n",
	     __FUNCTION__,value,maxvalue);
      return ERROR;
    }

  if(timestep)
    value |= (1<<7);

  /* Read the previous values */
  TSLOCK;
  rval = vmeRead32(&TSp->triggerRule);
  vmeControl = vmeRead32(&TSp->vmeControl);

  switch(rule)
    {
    case 1:
      wval = value | (rval & ~TS_TRIGGERRULE_RULE1_MASK);
      break;
    case 2:
      wval = (value<<8) | (rval & ~TS_TRIGGERRULE_RULE2_MASK);
      break;
    case 3:
      wval = (value<<16) | (rval & ~TS_TRIGGERRULE_RULE3_MASK);
      break;
    case 4:
      wval = (value<<24) | (rval & ~TS_TRIGGERRULE_RULE4_MASK);
      break;
    }

  vmeWrite32(&TSp->triggerRule,wval);

  if(timestep == 2)
    {
      if(!(vmeControl & TS_VMECONTROL_SLOWER_TRIGGER_RULES))
	{
	  if(slow_clock_previously_switched == 1)
	    {
	      printf("%s: WARNING: Using slower clock for trigger rules.\n",
		     __FUNCTION__);
	      printf("\tThis may affect previously set rules!\n");
	    }
	  vmeWrite32(&TSp->vmeControl,
		     vmeControl | TS_VMECONTROL_SLOWER_TRIGGER_RULES);
	  slow_clock_previously_switched = 1;
	}
    }
  else
    {
      if(vmeControl & TS_VMECONTROL_SLOWER_TRIGGER_RULES)
	{
	  if(slow_clock_previously_switched == 1)
	    {
	      printf("%s: WARNING: Using faster clock for trigger rules.\n",
		     __FUNCTION__);
	      printf("\tThis may affect previously set rules!\n");
	    }
	  vmeWrite32(&TSp->vmeControl,
		     vmeControl & ~TS_VMECONTROL_SLOWER_TRIGGER_RULES);
	  slow_clock_previously_switched = 1;
	}
    }
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the value for a specified trigger rule.
 *
 * @param   rule   the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @return If successful, returns the value (in steps of 16ns)
 *            for the specified rule. ERROR, otherwise.
 *
 */
int
tsGetTriggerHoldoff(int rule)
{
  unsigned int rval=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(rule<1 || rule>5)
    {
      printf("%s: ERROR: Invalid value for rule (%d).  Must be 1 or 2.\n",
	     __FUNCTION__,rule);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->triggerRule);
  TSUNLOCK;

  switch(rule)
    {
    case 1:
      rval = (rval & TS_TRIGGERRULE_RULE1_MASK);
      break;

    case 2:
      rval = (rval & TS_TRIGGERRULE_RULE2_MASK)>>8;
      break;

    case 3:
      rval = (rval & TS_TRIGGERRULE_RULE3_MASK)>>16;
      break;

    case 4:
      rval = (rval & TS_TRIGGERRULE_RULE4_MASK)>>24;
      break;
    }

  return rval;

}

int
tsPrintTriggerHoldoff(int dflag)
{
  unsigned long TSBase = 0;
  unsigned int triggerRule = 0, triggerRuleMin = 0, vmeControl = 0;
  int irule = 0, slowclock = 0, clockticks = 0, timestep = 0, minticks = 0;
  float clock[3] = {250, 33.3, 33.3/32.}, stepsize = 0., time = 0., min = 0.;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  triggerRule    = vmeRead32(&TSp->triggerRule);
  triggerRuleMin = vmeRead32(&TSp->part1.triggerRuleMin);
  vmeControl     = vmeRead32(&TSp->vmeControl);
  TSUNLOCK;

  if(dflag)
    {
      printf("  Registers:\n");
      TSBase = (unsigned long)TSp;
      printf("   triggerRule    (0x%04lx) = 0x%08x\t",
	     (unsigned long)(&TSp->triggerRule) - TSBase, triggerRule);
      printf(" triggerRuleMin (0x%04lx) = 0x%08x\n",
	     (unsigned long)(&TSp->part1.triggerRuleMin) - TSBase, triggerRuleMin);
    }

  printf("\n");
  printf("    Rule   Timesteps    + Up to     Minimum  ");
  if(dflag)
    printf("  ticks   clock   prescale\n");
  else
    printf("\n");
  printf("    ----   ---[ns]---  ---[ns]---  ---[ns]---");
  if(dflag)
    printf("  -----  -[MHz]-  --------\n");
  else
    printf("\n");

  slowclock = (vmeControl & (1 << 31)) >> 31;
  for(irule = 0; irule < 4; irule++)
    {
      clockticks = (triggerRule >> (irule*8)) & 0x7F;
      timestep   = ((triggerRule >> (irule*8)) >> 7) & 0x1;
      if((triggerRuleMin >> (irule*8)) & 0x80)
	minticks = (triggerRuleMin >> (irule*8)) & 0x7F;
      else
	minticks = 0;

      if((timestep == 1) && (slowclock == 1))
	{
	  timestep = 2;
	}

      stepsize = ((float) tsTriggerRuleClockPrescale[timestep][irule] /
		  (float) clock[timestep]);

      time = (float)clockticks * stepsize;

      min = (float) minticks * stepsize;

      printf("    %4d     %8.1f    %8.1f    %8.1f ",
	     irule + 1, 1E3 * time, 1E3 * stepsize, min);

      if(dflag)
	printf("   %3d    %5.1f       %3d\n",
	       clockticks, clock[timestep],
	       tsTriggerRuleClockPrescale[timestep][irule]);
	printf("\n");

    }
  printf("\n");

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the value for the minimum time of specified trigger rule.
 *
 * @param   rule  the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @param   value  the specified time period (in steps of timestep)
 *<pre>
 *       	 	      rule
 *    		         2      3      4
 *    		       ----- ------ ------
 *    		        16ns  480ns  480ns
 *</pre>
 *
 * @return OK if successful, otherwise ERROR.
 *
 */
int
tsSetTriggerHoldoffMin(int rule, unsigned int value)
{
  unsigned int mask=0, enable=0, shift=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(rule<2 || rule>5)
    {
      printf("%s: ERROR: Invalid rule (%d).  Must be 2-4.\n",
	     __FUNCTION__,rule);
      return ERROR;
    }

  if(value > 0x7f)
    {
      printf("%s: ERROR: Invalid value (%d). Must be less than %d.\n",
	     __FUNCTION__,value,0x7f);
      return ERROR;
    }

  switch(rule)
    {
    case 2:
      mask = ~(TS_TRIGGERRULEMIN_MIN2_MASK | TS_TRIGGERRULEMIN_MIN2_EN);
      enable = TS_TRIGGERRULEMIN_MIN2_EN;
      shift = 8;
      break;
    case 3:
      mask = ~(TS_TRIGGERRULEMIN_MIN3_MASK | TS_TRIGGERRULEMIN_MIN3_EN);
      enable = TS_TRIGGERRULEMIN_MIN3_EN;
      shift = 16;
      break;
    case 4:
      mask = ~(TS_TRIGGERRULEMIN_MIN4_MASK | TS_TRIGGERRULEMIN_MIN4_EN);
      enable = TS_TRIGGERRULEMIN_MIN4_EN;
      shift = 24;
      break;
    }

  TSLOCK;
  vmeWrite32(&TSp->part1.triggerRuleMin,
	     (vmeRead32(&TSp->part1.triggerRuleMin) & mask) |
	     enable |
	     (value << shift) );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Get the value for a specified trigger rule minimum busy.
 *
 * @param   rule   the number of triggers within some time period..
 *            e.g. rule=1: No more than ONE trigger within the
 *                         specified time period
 *
 * @param  pflag  if not 0, print the setting to standard out.
 *
 * @return If successful, returns the value
 *          (in steps of 16ns for rule 2, 480ns otherwise)
 *            for the specified rule. ERROR, otherwise.
 *
 */
int
tsGetTriggerHoldoffMin(int rule, int pflag)
{
  unsigned int rval=0;
  unsigned int enable=0, val=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(rule<2 || rule>5)
    {
      printf("%s: ERROR: Invalid rule (%d).  Must be 2-4.\n",
	     __FUNCTION__,rule);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->part1.triggerRuleMin);
  TSUNLOCK;

  switch(rule)
    {
    case 2:
      val = (rval & TS_TRIGGERRULEMIN_MIN2_MASK >> 8);
      enable = TS_TRIGGERRULEMIN_MIN2_EN;
      break;
    case 3:
      val = (rval & TS_TRIGGERRULEMIN_MIN3_MASK >> 16);
      enable = TS_TRIGGERRULEMIN_MIN3_EN;
      break;
    case 4:
      val = (rval & TS_TRIGGERRULEMIN_MIN4_MASK >> 24);
      enable = TS_TRIGGERRULEMIN_MIN4_EN;
      break;
    }


  if(pflag)
    {
      printf("%s: Trigger rule %d  minimum busy = %d - %s\n",
	     __FUNCTION__,rule,
	     val,
	     (enable)?"ENABLED":"DISABLED");
    }

  return val;
}


/**
 * @ingroup Config
 * @brief Configure trigger table to be loaded with a user provided array.
 *
 * @param itable Input Table (8x256 Array of 4byte words)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsTriggerTableConfig(unsigned int **itable)
{
  int imem=0, ielement=0;

  if(itable==NULL)
    {
      printf("%s: ERROR: Invalid input table address\n",
	     __FUNCTION__);
      return ERROR;
    }

  for(imem=0; imem<8; imem++)
    for(ielement=0; ielement<256; ielement++)
      tsTrigPatternData[imem][ielement] = itable[imem][ielement];

  return OK;
}

/**
 * @ingroup Config
 * @brief Get the current trigger table stored in local memory (not necessarily on TS).
 *
 * @param otable Output Table (8x256 Array of 4byte words, user must allocate memory)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsGetTriggerTable(unsigned int **otable)
{
  int imem=0, ielement=0;

  if(otable==NULL)
    {
      printf("%s: ERROR: Invalid output table address\n",
	     __FUNCTION__);
      return ERROR;
    }

  for(imem=0; imem<8; imem++)
    for(ielement=0; ielement<16; ielement++)
      otable[imem][ielement] = tsTrigPatternData[imem][ielement];

  return OK;
}

/**
 * @ingroup Config
 * @brief Configure trigger table to be loaded with a predefined
 * trigger table (mapping GTP and FP inputs to trigger types).
 *
 */
void
tsTriggerTableDefault()
{
  unsigned int imem=0, iword=0;

  /* Fill in the single bit patterns with "single trigger" patterns */
  for (imem=0; imem<8; imem++) /* 0-3: GTP tables, 4-7: FP Tables */
    {
      /* Start by initializing all bit patterns to their numerical event types,
	 and setting them all to be "multiple trigger" patterns */
      for (iword=0; iword<256; iword++)
	{
	  /* set bit(8) to 1 (hw trig1), and bit(11:10) to 3 for multi-bit trigger */
	  tsTrigPatternData[imem][iword] = 0xD00 + iword;
	}

      /* Zero inputs, No triggers */
      tsTrigPatternData[imem][0] = 0;

      for (iword=0; iword<8; iword++)
	{
	  /* set bit(8) to 1 (hw trig1), and bit(10) to 1 for single-bit trigger */
	  tsTrigPatternData[imem][((1<<iword)%0xff)] = 0x500 + iword + 1 + imem*8;
	}
    }

}

/**
 * @ingroup Config
 * @brief Define a specific trigger pattern as a hardware trigger (trig1/trig2/syncevent)
 * and Event Type
 *
 * @param inputType The Input Type
 *      0: GTP
 *      1: Front Panel
 * @param trigMask Trigger Pattern
 *    - TS inputs defining the pattern.  The current firmware limits each pattern
 *      to be within each subgroup inputs:
 *   GTP: 1-8, 9-16, 17-24, 25-32
 *    FP: 1-8, 9-16, 17-24, 25-32
 *       The routine will look for the first subgroup with a bit pattern.
 *       ALL OTHER PATTERNS WILL BE IGNORED (with WARN to standard out)
 * @param hwTrig Hardware trigger type (must be less than 3)
 *      0:  no trigger
 *      1:  Trig1 (event trigger)
 *      2:  Trig2 (playback trigger)
 *      3:  SyncEvent
 * @param evType Event Type (must be less than 256)
 *
 * @return OK if successful, otherwise ERROR
 */

int
tsDefineEventType(int inputType, unsigned int trigMask, int hwTrig, int evType)
{
  int ibyte=0, foundPattern=0;
  int mem=0;
  unsigned int pattern=0;

  const char *subgroup[4] =
    {
      " 1- 8",
      " 9-16",
      "17-24",
      "25-32"
    };

  if((inputType>2) || (inputType<0))
    {
      printf("%s: ERROR: Invalid inputType (%d)\n",
	     __FUNCTION__, inputType);
      return ERROR;
    }

  if(hwTrig>3)
    {
      printf("%s: ERROR: Invalid hwTrig (%d)\n",
	     __FUNCTION__, hwTrig);
      return ERROR;
    }

  if(evType>0xFF)
    {
      printf("%s: ERROR: Invalid evType (%d)\n",
	     __FUNCTION__, evType);
      return ERROR;
    }

  /* Find the first non-zero pattern subgroup */
  for(ibyte=0; ibyte<4; ibyte++)
    {
      pattern = (trigMask>>(ibyte*8)) & 0xFF;

      if(pattern==0)
	continue;
      else if(foundPattern==1)
	{
	  printf("%s: WARN: Pattern 0x%02x for %s subgroup %s ignored.\n",
		 __FUNCTION__,pattern, (inputType==0)?"GTP":"FP", subgroup[ibyte]);
	  printf("          Pattern was already found in provided trigMask (0x%08x).\n",
		 trigMask);
	  continue;
	}
      else
	{
	  mem = ibyte + inputType*4;

	  /* Write this as a single trigger, so that the event Type is preserved */
	  tsTrigPatternData[mem][pattern] = (1<<10) | (hwTrig<<8) | evType;
	  foundPattern=1;
	}

    }

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the trigger type for the specified special trigger
 *
 * @param trigOpt Trigger Option
 *   0:  Software (default = 253)
 *   1:  Pulser   (default = 254)
 *   2:  Multiple GTP or FP Hits (default = 250)
 *   3:  Combined GTP and FP Hits (default = 251)
 *
 * @param evType Event Type
 *   - Must be 1-255
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsDefineSpecialEventType(int trigOpt, int evType)
{
  unsigned int reg=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((trigOpt<0) || (trigOpt>3))
    {
      printf("%s: ERROR: Invalid trigOpt (%d)\n",
	     __FUNCTION__,trigOpt);
      return ERROR;
    }

  if((evType<0) || (evType>0xFF))
    {
      printf("%s: ERROR: Invalid evType (%d)\n",
	     __FUNCTION__,evType);
      return ERROR;
    }

  TSLOCK;
  reg = vmeRead32(&TSp->specialEvTypes);

  switch(trigOpt)
    {
    case 0: /* Software */
      reg = (reg &~ TS_SPECIALEVTYPES_VME_MASK) | (evType<<16);
      break;

    case 1: /* Pulser */
      reg = (reg &~ TS_SPECIALEVTYPES_PULSER_MASK) | (evType<<24);
      break;

    case 2: /* Multiple GTP or FP */
      reg = (reg &~ TS_SPECIALEVTYPES_MULT_GTP_OR_FP_MASK) | (evType<<0);
      break;

    case 3: /* Combined GTP and FP */
      reg = (reg &~ TS_SPECIALEVTYPES_GTP_AND_FP_MASK) | (evType<<8);
      break;
    }

  vmeWrite32(&TSp->specialEvTypes, reg);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Get the trigger type for the specified special trigger
 *
 * @param trigOpt Trigger Option
 *   0:  Software (default = 253)
 *   1:  Pulser   (default = 254)
 *   2:  Multiple GTP or FP Hits (default = 250)
 *   3:  Combined GTP and FP Hits (default = 251)
 *
 * @return Event Type if successful, otherwise ERROR
 */
int
tsGetSpecialEventType(int trigOpt)
{
  unsigned int rval=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((trigOpt<0) || (trigOpt>3))
    {
      printf("%s: ERROR: Invalid trigOpt (%d)\n",
	     __FUNCTION__,trigOpt);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->specialEvTypes);

  switch(trigOpt)
    {
    case 0: /* Software */
      rval = (rval & TS_SPECIALEVTYPES_VME_MASK)>>16;
      break;

    case 1: /* Pulser */
      rval = (rval & TS_SPECIALEVTYPES_PULSER_MASK)>>24;
      break;

    case 2: /* Multiple GTP or FP */
      rval = (rval & TS_SPECIALEVTYPES_MULT_GTP_OR_FP_MASK);
      break;

    case 3: /* Combined GTP and FP */
      rval = (rval & TS_SPECIALEVTYPES_GTP_AND_FP_MASK)>>8;
      break;
    }

  TSUNLOCK;

  return rval;
}



static void
SBRAMLoad(volatile unsigned int *reg, unsigned int *Wdata)
{
  unsigned int i;

  TSLOCK;
  /* Reset the write address first */
  vmeWrite32(&TSp->reset,TS_RESET_RAM_WRITE);
  taskDelay(1);

  for (i=0; i<256; i++)
    {
      vmeWrite32(reg, Wdata[i]);
    }
  TSUNLOCK;

}


static void
STRGTableLoad()
{
  int imem=0;

  for (imem=0; imem<8; imem++)
    {
      if(imem<4)   /* GTP Table Load */
	SBRAMLoad(&TSp->GTPTriggerTable[imem], (unsigned int*)&tsTrigPatternData[imem]);
      else         /* FP Table Load */
	SBRAMLoad(&TSp->FPTriggerTable[imem-4], (unsigned int*)&tsTrigPatternData[imem]);
    }

}


static void
STRGSubTableLoad()
{
  unsigned int MemData[256];
  unsigned int its, iword;

  /* loop over the four TS partitions */
  for (its=0; its<4; its++)
  {
    /* initialize the data buffer */
    for (iword=0; iword<256; iword++)
      {
	/* set bit(8) to 1, and bit(9) to 1 for multi-bit trigger */
	MemData[iword] = 0xF0a;
      }

    /* initialize the proper word */
    MemData[0] = 0;
    for (iword=0; iword<8; iword++)
      {
	/* set bit(8) to 1 */
	MemData[((1<<iword)&0xff)] = 0x509;
      }

    /* load the GTP TSpartition input table */
    SBRAMLoad(&TSp->PartTrigTable[its].GTPTriggerTable, (unsigned int*)&MemData);
    /* load the EXT TSpartition input table */
    SBRAMLoad(&TSp->PartTrigTable[its].FPTriggerTable, (unsigned int*)&MemData);
  }
}

/**
 * @ingroup Config
 * @brief Load up the default trigger lookup table for the TS
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsLoadTriggerTable()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  STRGTableLoad();

  printf("%s: Loaded.\n",__FUNCTION__);

  return OK;
}

/**
 * @ingroup Status
 * @brief Print trigger table to standard out.
 *
 * @param inputType Input Type
 *     0: GTP
 *     1: FP
 * @param subGroup Each input type is grouped into 8 channels.
 *     0: 1-8
 *     1: 9-16
 *     2: 17-24
 *     3: 25-32
 * @param showbits Show trigger bit pattern, instead of hex
 *
 */
void
tsPrintTriggerTable(int inputType, int subGroup, int showbits)
{
  int ielement=0, ipattern=0, multi1=0, multi2=0;
  int hwTrig1=0, evType1=0, hwTrig2=0, evType2=0;
  const char* input[2] =
    {
      "GTP", " FP"
    };
  const char *subgroup[4] =
    {
      " 1- 8",
      " 9-16",
      "17-24",
      "25-32"
    };

  if((inputType>2) || (inputType<0))
    {
      printf("%s: ERROR: Invalid inputType (%d)\n",
	     __FUNCTION__, inputType);
      return;
    }

  if((subGroup>3) || (subGroup<0))
    {
      printf("%s: ERROR: Invalid subGroup (%d)\n",
	     __FUNCTION__, inputType);
      return;
    }

  for(ielement = 0; ielement<256; ielement+=16)
    {
      if(showbits)
	{
	  printf("--------%s INPUT------                   --------%s INPUT------\n"
		 ,input[inputType], input[inputType]);
	  printf("%2d %2d %2d %2d %2d %2d %2d %2d  HW evType        %2d %2d %2d %2d %2d %2d %2d %2d  HW evType\n",
		 subGroup*8+1,
		 subGroup*8+2,
		 subGroup*8+3,
		 subGroup*8+4,
		 subGroup*8+5,
		 subGroup*8+6,
		 subGroup*8+7,
		 subGroup*8+8,
		 subGroup*8+1,
		 subGroup*8+2,
		 subGroup*8+3,
		 subGroup*8+4,
		 subGroup*8+5,
		 subGroup*8+6,
		 subGroup*8+7,
		 subGroup*8+8);
	}
      else
	{
	  printf("%s Pattern                %s Pattern\n"
		 ,input[inputType],input[inputType]);
	  printf("  %s       HW evType      %s      HW evType\n"
		 ,subgroup[subGroup],subgroup[subGroup]);
	}

      for(ipattern=ielement; ipattern<ielement+8; ipattern++)
	{

	  hwTrig1 = ((tsTrigPatternData[inputType*4+subGroup][ipattern]) & 0x300)>>8;
	  evType1 = (tsTrigPatternData[inputType*4+subGroup][ipattern]) & 0xFF;
	  multi1  = ((tsTrigPatternData[inputType*4+subGroup][ipattern]) & 0xC00)==0xC00;

	  if(multi1)
	    evType1 = 250;

	  hwTrig2 = ((tsTrigPatternData[inputType*4+subGroup][ipattern+8]) & 0x300)>>8;
	  evType2 = (tsTrigPatternData[inputType*4+subGroup][ipattern+8]) & 0xFF;
	  multi2  = ((tsTrigPatternData[inputType*4+subGroup][ipattern+8]) & 0xC00)==0xC00;

	  if(multi2)
	    evType2 = 250;

	  if(showbits)
	    {
	      printf(" %d  %d  %d  %d  %d  %d  %d  %d  %d   %3d           %d  %d  %d  %d  %d  %d  %d  %d  %d   %3d\n",
		     ((ipattern) & (1<<0))?1:0,
		     ((ipattern) & (1<<1))?1:0,
		     ((ipattern) & (1<<2))?1:0,
		     ((ipattern) & (1<<3))?1:0,
		     ((ipattern) & (1<<4))?1:0,
		     ((ipattern) & (1<<5))?1:0,
		     ((ipattern) & (1<<6))?1:0,
		     ((ipattern) & (1<<7))?1:0,
		     hwTrig1, evType1,
		     ((ipattern+8) & (1<<0))?1:0,
		     ((ipattern+8) & (1<<1))?1:0,
		     ((ipattern+8) & (1<<2))?1:0,
		     ((ipattern+8) & (1<<3))?1:0,
		     ((ipattern+8) & (1<<4))?1:0,
		     ((ipattern+8) & (1<<5))?1:0,
		     ((ipattern+8) & (1<<6))?1:0,
		     ((ipattern) & (1<<7))?1:0,
		     hwTrig2, evType2);
	    }
	  else
	    {
	      printf("   0x%02x        %d  %3d         0x%02x       %d  %3d\n",
		     ipattern,hwTrig1, evType1,
		     ipattern+8,hwTrig2, evType2);
	    }
	}
      printf("\n");

    }


}

/**
 *  @ingroup Config
 *  @brief Latch the Busy and Live Timers.
 *
 *     This routine should be called prior to a call to tsGetLiveTime and tsGetBusyTime
 *
 *  @sa tsGetLiveTime
 *  @sa tsGetBusyTime
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsLatchTimers()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset, TS_RESET_LATCH_TIMERS);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Return the current "live" time of the module
 *
 * @returns The current live time in units of 7.68 us
 *
 */
unsigned int
tsGetLiveTime()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->livetime);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Return the current "busy" time of the module
 *
 * @returns The current live time in units of 7.68 us
 *
 */
unsigned int
tsGetBusyTime()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->busytime);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Return the current "live" time of the module
 *
 * @returns The current live time in units of 7.68 us
 *
 */
unsigned int
tsGetLiveTime_InputHigh()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->part1.hel_livetime);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Return the current "busy" time of the module
 *
 * @returns The current live time in units of 7.68 us
 *
 */
unsigned int
tsGetBusyTime_InputHigh()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->part1.hel_busytime);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Calculate the live time (percentage) from the live and busy time scalers
 *
 * @param sflag if > 0, then returns the integrated live time
 *
 * @return live time as a 3 digit integer % (e.g. 987 = 98.7%)
 *
 */
int
tsLive(int sflag)
{
  int rval=0;
  float fval=0;
  unsigned int newBusy=0, newLive=0, newTotal=0;
  unsigned int live=0, total=0;
  static unsigned int oldLive=0, oldTotal=0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset,TS_RESET_LATCH_TIMERS);
  newLive = vmeRead32(&TSp->livetime);
  newBusy = vmeRead32(&TSp->busytime);

  newTotal = newLive+newBusy;

  if((sflag==0) && (oldTotal<newTotal))
    { /* Differential */
      live  = newLive - oldLive;
      total = newTotal - oldTotal;
    }
  else
    { /* Integrated */
      live = newLive;
      total = newTotal;
    }

  oldLive = newLive;
  oldTotal = newTotal;

  if(total>0)
    fval = 1000*(((float) live)/((float) total));

  rval = (int) fval;

  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Show block Status of specified fiber
 * @param fiber  Fiber port to show
 * @param pflag  Whether or not to print to standard out
 * @return 0
 */
unsigned int
tsBlockStatus(int fiber, int pflag)
{
  unsigned int rval=0;
  char name[50];
  unsigned int nblocksReady, nblocksNeedAck;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(fiber>2)
    {
      printf("%s: ERROR: Invalid value (%d) for fiber\n",__FUNCTION__,fiber);
      return ERROR;

    }

  switch(fiber)
    {
    case 0:
      rval = (vmeRead32(&TSp->adr24) & 0xFFFF0000)>>16;
      break;

    case 1:
      rval = (vmeRead32(&TSp->blockStatus[(fiber-1)/2]) & 0xFFFF);
      break;

    case 2:
      rval = ( vmeRead32(&TSp->blockStatus[(fiber/2)-1]) & 0xFFFF0000 )>>16;
      break;
    }

  if(pflag)
    {
      nblocksReady   = rval & TS_BLOCKSTATUS_NBLOCKS_READY0;
      nblocksNeedAck = (rval & TS_BLOCKSTATUS_NBLOCKS_NEEDACK0)>>8;

      if(fiber==0)
	sprintf(name,"Loopback");
      else
	sprintf(name,"Fiber %d",fiber);

      printf("%s: %s : Blocks ready / need acknowledge: %d / %d\n",
	     __FUNCTION__, name,
	     nblocksReady, nblocksNeedAck);

    }

  return rval;
}


/**
 * @ingroup Status
 * @brief Returns the bits that are contributing to the current busy state
 *
 */

int
tsGetBusyStatus(int pflag)
{
  unsigned int busy=0, setbusy=0, isbusy=0, easybusy=0;
  int busyFound=0;
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  busy = vmeRead32(&TSp->busy);
  TSUNLOCK;

  isbusy = (busy&TS_BUSY_MONITOR_MASK)>>16;
  setbusy = (busy&TS_BUSY_SOURCEMASK);

  /* These are the "easy" bits... i.e. one to one */
  easybusy = isbusy & setbusy;
  if(easybusy) busyFound=1;
  rval = easybusy;

  if(pflag)
    {
      printf("%s: TS Busy from:\n",__FUNCTION__);

      if(easybusy & TS_BUSY_SWA)
	printf("   Switch Slot A\n");
      if(easybusy & TS_BUSY_SWB)
	printf("   Switch Slot A\n");
      if(easybusy & TS_BUSY_P2)
	printf("   P2 Input\n");
      if(easybusy & TS_BUSY_FP_FTDC)
	printf("   Front Panel TDC\n");
      if(easybusy & TS_BUSY_FP_FADC)
	printf("   Front Panel ADC\n");
      if(easybusy & TS_BUSY_FP)
	printf("   Front Panel\n");
      if(easybusy & TS_BUSY_LOOPBACK)
	printf("   Loopback (Block buffer level)\n");
      if(easybusy & TS_BUSY_TI_A)
	printf("   Fiber 1 (Block buffer level)\n");
      if(easybusy & TS_BUSY_TI_B)
	printf("   Fiber 2 (Block buffer level)\n");
      if(easybusy & TS_BUSY_INT)
	printf("   Too many available unread blocks\n");

    }

  /* These are the more detailed bits */
  isbusy = isbusy<<16;
  if((setbusy & TS_BUSY_LOOPBACK) && (isbusy & TS_BUSY_MONITOR_TS))
    {
      rval |= TS_BUSY_MONITOR_TS;
      if(pflag)
	printf("   TS (data buffer, etc)\n");
      busyFound=1;
    }

  if((setbusy & TS_BUSY_TI_A) && (isbusy & TS_BUSY_MONITOR_TI_A))
    {
      rval |= TS_BUSY_MONITOR_TI_A;
      if(pflag)
	printf("   Fiber 1 (crate busy)\n");
      busyFound=1;
    }

  if((setbusy & TS_BUSY_TI_B) && (isbusy & TS_BUSY_MONITOR_TI_B))
    {
      rval |= TS_BUSY_MONITOR_TI_B;
      if(pflag)
	printf("   Fiber 2 (crate busy)\n");
      busyFound=1;
    }

  if(pflag)
    if(!busyFound)
      printf("   No Sources\n");

  return rval;
}

/**
 * @ingroup Config
 * @brief Set the value of the syncronization event interval
 *
 *
 * @param  blk_interval
 *      Sync Event will occur in the last event of the set blk_interval (number of blocks)
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSetSyncEventInterval(int blk_interval)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(blk_interval>TS_SYNCEVENTCTRL_NBLOCKS_MASK)
    {
      printf("%s: WARN: Value for blk_interval (%d) too large.  Setting to %d\n",
	     __FUNCTION__,blk_interval,TS_SYNCEVENTCTRL_NBLOCKS_MASK);
      blk_interval = TS_SYNCEVENTCTRL_NBLOCKS_MASK;
    }

  TSLOCK;
  vmeWrite32(&TSp->syncEventCtrl, blk_interval);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Readout
 * @brief Force a sync event (type = 0).
 * @return OK if successful, otherwise ERROR
 */
int
tsForceSyncEvent()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset, TS_RESET_FORCE_SYNCEVENT);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Readout
 * @brief Sync Reset Request is sent to TI-Master or TS.
 *
 *    This option is available for multicrate systems when the
 *    synchronization is suspect.  It should be exercised only during
 *    "sync events" where the requested sync reset will immediately
 *    follow all ROCs concluding their readout.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsSyncResetRequest()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  tsDoSyncResetRequest=1;
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Readout
 * @brief Determine if a TI has requested a Sync Reset
 *
 * @return 1 if requested received, 0 if not, otherwise ERROR
 */
int
tsGetSyncResetRequest()
{
  int request=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  request = (vmeRead32(&TSp->blockBuffer) & TS_BLOCKBUFFER_SYNCRESET_REQUESTED)>>30;
  TSUNLOCK;

  return request;
}

/**
 * @ingroup Readout
 * @brief Generate non-physics triggers until the current block is filled.
 *    This feature is useful for "end of run" situations.
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsFillToEndBlock()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset, TS_RESET_FILL_TO_END_BLOCK);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Poll the TS to determine status of current block, for specified number of times.
 *    Return immediately when block has been filed, or when timeout has occurred.
 *
 * @param npoll Number of times to poll TS, before timeout declared
 *
 * @return OK if Block Is Filled, otherwise ERROR
 */
int
tsCurrentBlockFilled(unsigned short npoll)
{
  int rval=OK, ipoll=0;
  unsigned int bl, nevents;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  bl = tsGetCurrentBlockLevel();

  TSLOCK;
  for(ipoll=0; ipoll<npoll; ipoll++)
    {
      nevents = (vmeRead32(&TSp->nblocks) & TS_NBLOCKS_EVENTS_IN_BLOCK_MASK)>>24;
      if(nevents==bl)
	break;
    }
  TSUNLOCK;

  return rval;
}


/**
 * @ingroup Config
 * @brief Reset the MGT
 * @return OK if successful, otherwise ERROR
 */
int
tsResetMGT()
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->reset, TS_RESET_MGT);
  TSUNLOCK;
  taskDelay(1);

  return OK;
}

/**
 * @ingroup Config
 * @brief Set the input delay for teh specified front panel input (0-31)
 * @param chan Front Panel Input Channel (0-31)
 * @param delay Delay in units of 4ns (0=8ns)
 * @return OK if successful, otherwise ERROR
 */
int
tsSetFPDelay(int chan, int delay)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((chan<0) || (chan>31))
    {
      printf("%s: ERROR: Invalid chan (%d)\n",__FUNCTION__,
	     chan);
      return ERROR;
    }

  if((delay<0) || (delay>0x1ff))
    {
      printf("%s: ERROR: Invalid delay (%d)\n",__FUNCTION__,
	     delay);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpDelay[chan/3],
	     (vmeRead32(&TSp->fpDelay[chan/3]) & ~TS_FPDELAY_MASK(chan))
	     | delay<<(10*(chan%3)));
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Get the input delay for teh specified front panel input (0-31)
 * @param chan Front Panel Input Channel (0-31)
 * @return Channel delay (units of 4ns) if successful, otherwise ERROR
 */
int
tsGetFPDelay(int chan)
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((chan<0) || (chan>31))
    {
      printf("%s: ERROR: Invalid chan (%d)\n",__FUNCTION__,
	     chan);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpDelay[chan/3]) & TS_FPDELAY_MASK(chan))>>(10*(chan%3));
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Print Front Panel Channel Delays to Standard Out
 * @return OK if successful, otherwise ERROR
 */
int
tsPrintFPDelay()
{
  unsigned int reg[11];
  int ireg=0, ichan=0, delay=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  for(ireg=0; ireg<11; ireg++)
    reg[ireg] = vmeRead32(&TSp->fpDelay[ireg]);
  TSUNLOCK;

  printf("%s: Front panel delays:", __FUNCTION__);
  for(ichan=0;ichan<31;ichan++)
    {
      delay = reg[ichan/3] & TS_FPDELAY_MASK(ichan)>>(10*(ichan%3));
      if((ichan%4)==0)
	{
	  printf("\n");
	}
      printf("Chan %2d: %5d   ",ichan,delay);
    }
  printf("\n");

  return OK;
}

/**
 * @ingroup Config
 * @brief Enable/Disable the FPGA drive to the TSIO.
 * @param enable Enable/Disable
 *   0: Disable
 *  >0: Enable
 * @return OK if successful, otherwise ERROR
 */
int
tsSetTSIODrive(int enable)
{
  unsigned int reg=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  reg = vmeRead32(&TSp->vmeControl);
  if(enable)
    {
      vmeWrite32(&TSp->vmeControl, reg | TS_VMECONTROL_DRIVE_TSIO_EN);
    }
  else
    {
      vmeWrite32(&TSp->vmeControl, reg & ~TS_VMECONTROL_DRIVE_TSIO_EN);
    }
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Status
 * @brief Return the Enable/Disable status the FPGA drive to the TSIO.
 * @return 1 if enabled, 0 if disabled, otherwise ERROR
 */
int
tsGetTSIODrive()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->vmeControl) & TS_VMECONTROL_DRIVE_TSIO_EN) ? 1 : 0;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Status
 * @brief Return the Firmware Version that is Supported by the Library
 * @return Firmware Version
 */
int
tsGetDriverSupportedVersion()
{
  return TS_SUPPORTED_FIRMWARE;
}

/**
 * @ingroup Readout
 * @brief Readout input scalers
 *   Returned data:
 *    bit 31
 *       0: As stored
 *       1: Shifted by 7 bits (must multiply by 2**7)
 *
 * @param data  - local memory address to place scaler data
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */

int
tsReadScalersMon(volatile unsigned int *data)
{

  int iscal = 0, ichan = 0;
  int banks = 0;
  int nwrds = 0;
  unsigned int tmpData=0;
  volatile struct ScalerStruct *scalers[4];

  if(TSp==NULL)
    {
      logMsg("\ntsReadScalers: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(data==NULL)
    {
      logMsg("\ntsReadScalers: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  scalers[0] = (struct ScalerStruct *)(&TSp->Scalers1);
  scalers[1] = (struct ScalerStruct *)(&TSp->Scalers2);
  scalers[2] = (struct ScalerStruct *)(&TSp->Scalers3);
  scalers[3] = (struct ScalerStruct *)(&TSp->Scalers4);

  TSLOCK;

  /* Latch Timers */
  vmeWrite32(&TSp->reset, TS_RESET_LATCH_TIMERS);


  /* GTP */
  banks = 8;
  for(iscal = 0; iscal < 4; iscal++)
    {
      for(ichan = 0; ichan < banks; ichan++)
	{
	  tmpData = vmeRead32(&scalers[iscal]->GTP[ichan]);
	  if(tmpData & TS_SCALER_SCALE_HI)
	    {
	      data[nwrds] = TS_SCALER_SCALE_HI |
		((tmpData & TS_SCALER_SCALE_HI_LSB_MASK) >> 7) |
		((tmpData & TS_SCALER_SCALE_HI_MSB_MASK) << 24);
	    }
	  else
	    data[nwrds] = tmpData;

	  nwrds++;
	}
    }

  /* Gen */
  banks = 8;
  for(iscal = 0; iscal < 4; iscal++)
    {
      for(ichan=0; ichan<banks; ichan++)
	{
	  tmpData = vmeRead32(&scalers[iscal]->gen[ichan]);
	  if(tmpData & TS_SCALER_SCALE_HI)
	    {
	      data[nwrds] = TS_SCALER_SCALE_HI |
		((tmpData & TS_SCALER_SCALE_HI_LSB_MASK) >> 7) |
		((tmpData & TS_SCALER_SCALE_HI_MSB_MASK) << 24);
	    }
	  else
	    data[nwrds] = tmpData;

	  nwrds++;
	}
    }

  /* LiveTime */
  data[64] = vmeRead32(&TSp->livetime);
  nwrds++;
  /* BusyTime */
  data[65] = vmeRead32(&TSp->busytime);
  nwrds++;

  TSUNLOCK;

  return nwrds;
}


/**
 * @ingroup Config
 * @brief Set the trigger coincidence window
 * @param size Size of the coincidence window in units of 4ns
 * @return OK if successful, otherwise ERROR
 */

int
tsSetTrigCoinWindow(unsigned int size)
{
  unsigned int maxvalue = 0xFF;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( (size > maxvalue) || (size == 0) )
    {
      printf("%s: ERROR: Invalid window size (%d). Must be less than %d.\n",
	     __FUNCTION__,size,maxvalue);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->triggerWindow,
	     (vmeRead32(&TSp->triggerWindow) &~ TS_TRIGGERWINDOW_COINC_MASK)
	     | size);
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the trigger coincidence window
 * @return Size of the coincidence window in units of 4ns if successful,
 *         otherwise ERROR
 */

int
tsGetTrigCoinWindow()
{
  unsigned int rval = 0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->triggerWindow) & TS_TRIGGERWINDOW_COINC_MASK;
  TSUNLOCK;

  return rval;

}

/**
 * @ingroup Config
 * @brief Set the trigger inhibit window
 * @param size Size of the inhibit window in units of 4ns
 * @return OK if successful, otherwise ERROR
 */

int
tsSetTrigInhibitWindow(unsigned int size)
{
  unsigned int maxvalue = 0xFF;

  if( (size > maxvalue) || (size == 0) )
    {
      printf("%s: ERROR: Invalid inhibit window size (%d). Must be less than %d.\n",
	     __FUNCTION__,size,maxvalue);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->triggerWindow,
	     (vmeRead32(&TSp->triggerWindow) &~ TS_TRIGGERWINDOW_INHIBIT_MASK)
	     | (size<<8));
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Status
 * @brief Get the trigger inhibit window
 * @return Size of the inhibit window in units of 4ns if successful,
 *         otherwise ERROR
 */

int
tsGetTrigInhibitWindow()
{
  unsigned int rval = 0;

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->triggerWindow) & TS_TRIGGERWINDOW_INHIBIT_MASK)>>8;
  TSUNLOCK;

  return rval;

}

/*************************************************************

 TS Partition routines

*************************************************************/

/**
 * @ingroup Part
 * @brief Initialize a TS partition
 * @param pID The partition identification number (1-4)
 * @param tAddr
 *  - A24 VME Address of the TS
 *  - Slot number of TS (1 - 21)
 * @param mode What mode to signal data ready from the partition
 *  - 0: Interrupt
 *  - 2: Polling
 * @param iFlag Initialization bit mask
 *  - 0: No TS Initialization (map pointer only)
 * @return OK if successful, otherwise ERROR
 */
int
tsPartInit(int pID, unsigned int tAddr, unsigned int mode, int iFlag)
{
  unsigned long laddr;
  unsigned int rval, boardID;
  unsigned int firmwareInfo;
  int stat;
  int noBoardInit=0;
  int tsType=0;

  /* Check VME address */
  if(tAddr<0 || tAddr>0xffffff)
    {
      printf("%s: ERROR: Invalid VME Address (%d)\n",__FUNCTION__,
	     tAddr);
    }
  if(tAddr==0)
    {
      /* Assume 0 means to use default from GEO (slot 20 or 21, whichever = MAX_VME_SLOTS) */
      tAddr=(MAX_VME_SLOTS)<<19;
    }

  /* Check pID */
  if(pID<1 || pID>4)
    {
      printf("%s: Invalid Partition ID (%d).  Must be 1-4.\n",
	     __FUNCTION__,pID);
      return ERROR;
    }

  noBoardInit = iFlag&(0x1);

  /* Form VME base address from slot number */
#ifdef VXWORKS
  stat = sysBusToLocalAdrs(0x39,(char *)tAddr,(char **)&laddr);
  if (stat != 0)
    {
      printf("%s: ERROR: Error in sysBusToLocalAdrs res=%d \n",__FUNCTION__,stat);
      return ERROR;
    }
  else
    {
      printf("TS address = 0x%x\n",laddr);
    }
#else
  stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)tAddr,(char **)&laddr);
  if (stat != 0)
    {
      printf("%s: ERROR: Error in vmeBusToLocalAdrs res=%d \n",__FUNCTION__,stat);
      return ERROR;
    }
  else
    {
      if(!noBoardInit)
	printf("TS VME (Local) address = 0x%08x (0x%lx)\n",
	       tAddr, laddr);
    }
#endif
  tsA24Offset = laddr-tAddr;

  /* Set Up pointer */
  TSp = (struct TS_A24RegStruct *)laddr;

  /* Check if TS board is readable */
#ifdef VXWORKS
  stat = vxMemProbe((char *)(&TSp->boardID),0,4,(char *)&rval);
#else
  stat = vmeMemProbe((char *)(&TSp->boardID),4,(char *)&rval);
#endif

  if (stat != 0)
    {
      printf("%s: ERROR: TS card not addressable\n",__FUNCTION__);
      TSp=NULL;
      return(-1);
    }
  else
    {
      /* Check that it is a TS */
      if(((rval&TS_BOARDID_TYPE_MASK)>>16) != TS_BOARDID_TYPE_TS)
	{
	  printf("%s: ERROR: Invalid Board ID: 0x%x (rval = 0x%08x)\n",
		 __FUNCTION__,
		 (rval&TS_BOARDID_TYPE_MASK)>>16,rval);
	  TSp=NULL;
	  return(ERROR);
	}
      /* Check if this is board has a valid slot number */
      boardID =  (rval&TS_BOARDID_GEOADR_MASK)>>8;
      if((boardID <= 0)||(boardID >21))
	{
	  printf("%s: ERROR: Board Slot ID is not in range: %d\n",
		 __FUNCTION__,boardID);
	  TSp=NULL;
	  return(ERROR);
	}
    }

  /* Check if we should exit here, or initialize some board defaults */
  if(noBoardInit)
    {
      return OK;
    }

  /* Get the Firmware Information and print out some details */
  firmwareInfo = tsGetFirmwareVersion();
  if(firmwareInfo>0)
    {
      printf("  User ID: 0x%x \tFirmware (type - revision): 0x%X - 0x%x.0x%02x\n",
	     (firmwareInfo&TS_FIRMWARE_ID_MASK)>>16,
	     (firmwareInfo&TS_FIRMWARE_TYPE_MASK)>>12,
	     (firmwareInfo&TS_FIRMWARE_MAJOR_VERSION_MASK)>>4,
	     firmwareInfo&TS_FIRWMARE_MINOR_VERSION_MASK);

      tsVersion = firmwareInfo&0xFFF;
      tsType    = (firmwareInfo&TS_FIRMWARE_TYPE_MASK)>>12;
      if((tsVersion < TS_SUPPORTED_FIRMWARE) || (tsType!=TS_SUPPORTED_TYPE))
	{
	  printf("%s: ERROR: Type %x Firmware version (0x%x) not supported by this driver.\n  Supported Type %x version 0x%x\n",
		 __FUNCTION__,
		 tsType,tsVersion,TS_SUPPORTED_TYPE,TS_SUPPORTED_FIRMWARE);
	  TSp=NULL;
	  return ERROR;
	}
    }
  else
    {
      printf("%s:  ERROR: Invalid firmware 0x%08x\n",
	     __FUNCTION__,firmwareInfo);
      return ERROR;
    }

  /* Set some defaults, dependent on Master/Slave status */
  tsReadoutMode = mode;
  switch(mode)
    {
    case TS_READOUT_EXT_INT:
    case TS_READOUT_EXT_POLL:
      /* BUSY from Loopback and Switch Slot B */
      tsSetBusySource(TS_BUSY_LOOPBACK | TS_BUSY_SWB,1);
      /* Onboard Clock Source */
      tsSetClockSource(TS_CLOCK_INTERNAL);
      /* Loopback Sync Source */
      tsSetSyncSource(TS_SYNC_LOOPBACK);
      break;

    default:
      printf("%s: ERROR: Invalid TS Mode %d\n",
	     __FUNCTION__,mode);
      return ERROR;
    }
  tsReadoutMode = mode;

  tsPartitionID = pID;

  switch(tsPartitionID)
    {
    case 2:
      TSpart = (struct PartitionStruct *)(&TSp->part2);
      break;

    case 3:
      TSpart = (struct PartitionStruct *)(&TSp->part3);
      break;

    case 4:
      TSpart = (struct PartitionStruct *)(&TSp->part4);
      break;

    case 1:
    default:
      TSpart = (struct PartitionStruct *)(&TSp->part1);
      break;
    }

  return OK;

}

/**
 * @ingroup Part
 * @brief Set the busy source for this partition
 *
 * @param   busysrc
 *    - 1: Switch slot B
 *    - 2: Front Panel
 *    - 3: Fiber TI-A
 *    - 4: Fiber TI-B
 *
 * @return OK if successful, otherwise ERROR.
 */
int
tsPartSetBusySource(int busysrc)
{
  unsigned int busybits=0;
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(busysrc > 4)
    {
      printf("%s: ERROR: Invalid busysrc (%d)\n",__FUNCTION__,busysrc);
      return ERROR;
    }

  switch(busysrc)
    {
    case 0: /* Switch Slot B */
      busybits = TS_PART_BUSYCONFIG_SWB;
      break;

    case 1: /* Front Panel */
      busybits = TS_PART_BUSYCONFIG_FP;
      break;

    case 2: /* Fiber TI-A */
      busybits = TS_PART_BUSYCONFIG_TI_A;
      break;

    case 3: /* Fiber TI-B */
    default:
      busybits = TS_PART_BUSYCONFIG_TI_B;

    }

  TSLOCK;
  vmeWrite32(&TSpart->busyConfig,
	     (vmeRead32(&TSpart->busyConfig) & ~TS_PART_BUSYCONFIG_BUSYSRC_MASK) |
	     busybits);
  TSUNLOCK;

  return OK;
}


/**
 * @ingroup Part
 * @brief Set up the Block Buffer Level
 * @param bufferlevel How many unacknowledged blocks in the system before busy
 * @return OK if successful, otherwise ERROR
 */
int
tsPartSetBlockBufferLevel(unsigned int bufferlevel)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(bufferlevel>0xff)
    {
      printf("%s: ERROR: Invalid bufferlevel (%d).\n"
	     ,__FUNCTION__,bufferlevel);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSpart->busyConfig,
	     ((vmeRead32(&TSpart->busyConfig) & ~(TS_PART_BUSYCONFIG_BUFFERLEVEL_MASK))) |
	     (bufferlevel<<24) |
	     TS_PART_BUSYCONFIG_BUFFERLEVEL_ENABLE | TS_PART_BUSYCONFIG_ALMOSTFULL_ENABLE);
  printf("%s: 0x%08x\n",
	 __FUNCTION__,vmeRead32(&TSpart->busyConfig));
  TSUNLOCK;

  return OK;

}

/**
 * @ingroup Part
 * @brief Select the input that has the TD Busy
 * @param tdinput Input selection
 * @return OK if successful, otherwise ERROR
 */
int
tsPartSetTDInput(unsigned int tdinput)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tdinput>0xff)
    {
      printf("%s: ERROR: Invalid tdinput (%d).\n"
	     ,__FUNCTION__,tdinput);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSpart->blockBuffer,
	     (vmeRead32(&TSpart->blockBuffer) & ~(TS_PART_BUSYCONFIG_TD_INPUT_MASK)) |
	     (tdinput));
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Part
 * @brief Set the FP Inputs for this partition
 * @param input1
 * @param input2
 * @param input3
 * @return OK if successful, otherwise ERROR
 */
int
tsPartSetFPInput(unsigned short input1,
		 unsigned short input2,
		 unsigned short input3)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((input1>0x3F) || (input2>0x3F) || (input3>0x3F))
    {
      printf("%s: ERROR: Input out of range.  Must be 0-63.\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSpart->fpConfig,
	     input1 |
	     (input2<<6) |
	     (input3<<12));
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Part
 * @brief Set the GTP Inputs for this partition
 * @param input1
 * @param input2
 * @param input3
 * @param input4
 * @param input5
 * @return OK if successful, otherwise ERROR
 */
int
tsPartSetGTPInput(unsigned short input1,
		  unsigned short input2,
		  unsigned short input3,
		  unsigned short input4,
		  unsigned short input5)
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((input1>0x3F) || (input2>0x3F) || (input3>0x3F) || (input4>0x3F) || (input5>0x3F))
    {
      printf("%s: ERROR: Input out of range.  Must be 0-63.\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSpart->gtpConfig,
	     input1 |
	     (input2<<6) |
	     (input3<<12) |
	     (input4<<18) |
	     (input5<<24) );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Part
 * @brief Load the trigger table for current partition
 * @return OK if successful, otherwise ERROR
 */
int
tsPartLoadTriggerTable()
{
  if(TSp==NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  STRGSubTableLoad();

  return OK;
}

/**
 * @ingroup Part
 * @brief Read a block of events from the current TS partition
 *
 * @param data  - local memory address to place data
 * @param nwrds - Max number of words to transfer
 *
 * @return Number of words transferred to data if successful, ERROR otherwise
 *
 */

int
tsPartReadBlock(volatile unsigned int *data, int nwrds)
{
  int ii=0, dCnt=0;
  unsigned int val;

  if(TSp==NULL)
    {
      logMsg("\ntsPartReadBlock: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      logMsg("\ntsPartReadBlock: ERROR: TS Partition not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(data==NULL)
    {
      logMsg("\ntsPartReadBlock: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  while(ii<nwrds)
    {
      val = (unsigned int) TSpart->data;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if((val==TS_EMPTY_FIFO) || (val==-1))
	break;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      data[ii] = val;
      ii++;

      /* Check if this is the block trailer */
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if(val & TS_DATAFORMAT_DATA_TYPE_WORD)
	{
	  if(((val & TS_DATAFORMAT_TYPE_MASK)>>27)==TS_DATAFORMAT_TYPE_BLOCK_TRAILER)
	    {
	      break;
	    }
	}}

  dCnt += ii;

  TSUNLOCK;
  return(dCnt);
}

/**
 * @ingroup Part
 * @brief Returns the number of Blocks available for readout for this partition
 *
 * @return Number of blocks available for readout if successful, otherwise ERROR
 *
 */
unsigned int
tsPartBReady()
{
  unsigned int rval=0;
  if(TSp==NULL)
    {
      logMsg("\ntsPartBReady(): ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      logMsg("\ntsPartBReady(): ERROR: TS Partition not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSpart->blockBuffer) & TS_PART_BLOCKBUFFER_BLOCKS_READY_MASK;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Part
 * @brief Acknowledge an interrupt or latched trigger for this partition
 */
void
tsPartIntAck()
{
  if(TSp == NULL)
    {
      logMsg("tsPartIntAck: ERROR: TS not initialized\n",0,0,0,0,0,0);
      return;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      logMsg("\ntsPartReadBlock: ERROR: TS Partition not initialized\n",1,2,3,4,5,6);
      return;
    }

  TSLOCK;
  tsDoAck = 1;
  tsAckCount++;
  vmeWrite32(&TSp->reset,TS_RESET_PART_ACK(tsPartitionID));
  TSUNLOCK;

}

#ifndef VXWORKS
static void
tsPartPoll(void)
{
  int tsdata;

  prctl(PR_SET_NAME,__FUNCTION__);

  while(1)
    {

      pthread_testcancel();

      /* If still need Ack, don't test the Trigger Status */
      if(tsNeedAck>0)
	{
	  continue;
	}

      tsdata = tsPartBReady();

      if(tsdata && tsIntRunning)
	{
	  INTLOCK;
	  tsDaqCount = tsdata;
	  tsIntCount++;

	  if (tsIntRoutine != NULL)	/* call user routine */
	    (*tsIntRoutine) (tsIntArg);

	  /* Write to TS to Acknowledge Interrupt */
	  if(tsDoAck==1)
	    {
	      tsPartIntAck();
	    }
	  INTUNLOCK;
	}

    }
  printf("%s: Read ERROR: Exiting Thread\n",__FUNCTION__);
  pthread_exit(0);

}
#endif


#ifndef VXWORKS
static void
tsPartStartPollingThread(void)
{
  int pts_status;

  pts_status =
    pthread_create(&tspollthread,
		   NULL,
		   (void*(*)(void *)) tsPartPoll,
		   (void *)NULL);
  if(pts_status!=0)
    {
      printf("%s: ERROR: TS Partition Polling Thread could not be started.\n",
	     __FUNCTION__);
      printf("\t pthread_create returned: %d\n",pts_status);
    }

}

/**
 * @ingroup Part
 * @brief Enable interrupts or latching triggers for this partition
 * @param iflag if = 1, trigger counter will be reset
 *
 * @return OK if successful, otherwise ERROR
 */
int
tsPartIntEnable(int iflag)
{

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  if(iflag == 1)
    {
      tsIntCount = 0;
      tsAckCount = 0;
    }

  tsIntRunning = 1;
  tsDoAck      = 1;
  tsNeedAck    = 0;

  switch (tsReadoutMode)
    {
    case TS_READOUT_EXT_POLL:
      tsPartStartPollingThread();
      break;

    default:
      tsIntRunning = 0;
      printf("%s: ERROR: TS Readout Mode not defined %d\n",
	     __FUNCTION__,tsReadoutMode);
      TSUNLOCK;
      return ERROR;

    }

  taskDelay(30); /* maybe replace with a condition variable? */

  vmeBusLock(); /* Make sure things don't change while we're doing this */
  /* Enable the bits we need */
  vmeWrite32(&TSp->trigger,
	     vmeRead32(&TSp->trigger) |
	     TS_TRIGGER_ENABLE |
	     TS_TRIGGER_PART(tsPartitionID));
  vmeBusUnlock();
  TSUNLOCK;

  return(OK);

}

/**
 * @ingroup Part
 * @brief Disable interrupts or latching triggers for this partition
 */
void
tsPartIntDisable()
{

  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return;
    }

  TSLOCK;
  vmeBusLock();
  vmeWrite32(&TSp->trigger,
	     vmeRead32(&TSp->trigger) & ~(TS_TRIGGER_PART(tsPartitionID)));
  vmeBusUnlock();

  tsIntRunning = 0;
  TSUNLOCK;
}


/**
 * @ingroup Part
 * @brief Connect a user routine to the TS Interrupt or
 *    latched trigger, if polling.
 *
 * @param routine Routine to call if block is available
 * @param arg argument to pass to routine
 *
 * @return OK if successful, otherwise ERROR

 */
int
tsPartIntConnect(VOIDFUNCPTR routine, unsigned int arg)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      printf("%s: ERROR: TS Partition not initialized\n",__FUNCTION__);
      return ERROR;
    }

  tsIntCount = 0;
  tsAckCount = 0;
  tsDoAck = 1;


  if(routine)
    {
      tsIntRoutine = routine;
      tsIntArg = arg;
    }
  else
    {
      tsIntRoutine = NULL;
      tsIntArg = 0;
    }

  return OK;

}

/**
 * @ingroup Part
 * @brief Disable interrupts or kill the polling service thread
 *
 * @return OK if successful, otherwise ERROR

 */
int
tsPartIntDisconnect()
{
  void *res;

  if(TSp == NULL)
    {
      logMsg("tsPartIntDisconnect: ERROR: TS not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((tsPartitionID==0) || (TSpart==NULL))
    {
      logMsg("tsPartIntDisconnect: ERROR: TS Partition not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if(tsIntRunning)
    {
      logMsg("tsPartIntDisconnect: ERROR: TS is Enabled - Call tsPartIntDisable() first\n",
	     1,2,3,4,5,6);
      return ERROR;
    }

  INTLOCK;

  if(tspollthread)
    {
      if(pthread_cancel(tspollthread)<0)
	perror("pthread_cancel");
      if(pthread_join(tspollthread,&res)<0)
	perror("pthread_join");
      if (res == PTHREAD_CANCELED)
	printf("%s: Polling thread canceled\n",__FUNCTION__);
      else
	printf("%s: ERROR: Polling thread NOT canceled\n",__FUNCTION__);
    }

  INTUNLOCK;

  printf("%s: Disconnected\n",__FUNCTION__);

  return OK;

}
#endif

/**
 * @ingroup Dupl
 * @brief Enable/Disable Duplication Mode
 *
 * @param  set Enable/Disable setting
 *       - 0 - Disable
 *       - !0 - Enable
 *
 * @return OK if successful, otherwise ERROR;
 */

int
tsDuplMode(int set)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  TSLOCK;

  tsDuplicationMode = (set)?1:0;
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */

int
tsDuplSetBranchEnable(int b1, int b2, int b3, int b4)
{
  unsigned int bmask=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  if(b1)
    bmask |= TS_BUSY_BRANCH1;
  if(b2)
    bmask |= TS_BUSY_BRANCH2;
  if(b3)
    bmask |= TS_BUSY_BRANCH3;
  if(b4)
    bmask |= TS_BUSY_BRANCH4;

  bmask |= TS_BUSY_ALL_BRANCHES;

  return tsSetBusySource(bmask, 0);
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetLocalTrigComboMask(unsigned int mask)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(mask&0x1)
    {
      printf("%s: WARN: Invalid trigger combination mask (0x%08x). Masking off 0x1\n",
	     __FUNCTION__,mask);
      mask = mask &~ 0x1;
    }


  TSLOCK;
  vmeWrite32(&TSp->fpInputPrescale[2], mask);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
unsigned int
tsDuplGetLocalTrigComboMask()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->fpInputPrescale[2]);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetLocalTrigCombo(unsigned int mask, int set)
{
  int ibit=0, trigbit=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if((mask>0x3E) || (mask&0x1))
    {
      printf("%s: ERROR: Invalid trigger combination mask (0x%02x)\n",
	     __FUNCTION__,mask);
      return ERROR;
    }

  /* Determine the bit in enable/disable */
  for(ibit=1; ibit<6; ibit++)
    {
      if(mask&(1<<ibit))
	trigbit+=ibit;
    }

  TSLOCK;
  if(set)
    {
      vmeWrite32(&TSp->fpInputPrescale[2],
		 vmeRead32(&TSp->fpInputPrescale[2]) | (1<<trigbit));
    }
  else
    {
      vmeWrite32(&TSp->fpInputPrescale[2],
		 vmeRead32(&TSp->fpInputPrescale[2]) & ~(1<<trigbit));
    }
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetTriggerHoldoff(unsigned int value)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(value>0x7F)
    {
      printf("%s: ERROR: Invalid value (%d)\n",
	     __FUNCTION__, value);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpInputPrescale[3],
	     (vmeRead32(&TSp->fpInputPrescale[3]) &~ TS_DUPL_LOCAL_TRIG_RULE_MASK) |
	     value);
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetTriggerHoldoff()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->fpInputPrescale[3]) & TS_DUPL_LOCAL_TRIG_RULE_MASK;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetLocalTriggerWidth(int width)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(width>0xFF)
    {
      printf("%s: ERROR: Invalid width (%d)\n",
	     __FUNCTION__, width);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpInputPrescale[3],
	     (vmeRead32(&TSp->fpInputPrescale[3]) &~ TS_DUPL_LOCAL_TRIG_WIDTH_MASK) |
	     (width)<<8 );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetLocalTriggerWidth()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpInputPrescale[3]) & TS_DUPL_LOCAL_TRIG_WIDTH_MASK)>>8;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetFastClearWidth(int width)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(width>0xFF)
    {
      printf("%s: ERROR: Invalid width (%d)\n",
	     __FUNCTION__, width);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpInputPrescale[3],
	     (vmeRead32(&TSp->fpInputPrescale[3]) &~ TS_DUPL_FAST_CLEAR_WIDTH_MASK) |
	     (width)<<16 );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetFastClearWidth()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpInputPrescale[3]) & TS_DUPL_FAST_CLEAR_WIDTH_MASK)>>16;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetFastClearDelay(int delay)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(delay>0x1FF)
    {
      printf("%s: ERROR: Invalid delay (%d)\n",
	     __FUNCTION__, delay);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpDelay[5],
	     (vmeRead32(&TSp->fpDelay[5]) &~ TS_DUPL_FAST_CLEAR_DELAY_MASK) |
	     (delay)<<10 );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetFastClearDelay()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpDelay[5]) & TS_DUPL_FAST_CLEAR_DELAY_MASK)>>10;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetFastClearVetoWidth(int width)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(width>0xFF)
    {
      printf("%s: ERROR: Invalid width (%d)\n",
	     __FUNCTION__, width);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpInputPrescale[3],
	     (vmeRead32(&TSp->fpInputPrescale[3]) &~ TS_DUPL_FAST_CLEAR_VETO_WIDTH_MASK) |
	     (width)<<24 );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetFastClearVetoWidth()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpInputPrescale[3]) & TS_DUPL_FAST_CLEAR_VETO_WIDTH_MASK)>>24;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetLocalTrigBusy(int value)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(value>0x3FF)
    {
      printf("%s: ERROR: Invalid value (%d)\n",
	     __FUNCTION__, value);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpDelay[6],
	     (vmeRead32(&TSp->fpDelay[6]) &~ TS_DUPL_LOCAL_TRIG_BUSY_MASK) |
	     (value) );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetLocalTrigBusy()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpDelay[6]) & TS_DUPL_LOCAL_TRIG_BUSY_MASK);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplSetFastClearBusy(int value)
{
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }
  if(value>0x3FF)
    {
      printf("%s: ERROR: Invalid value (%d)\n",
	     __FUNCTION__, value);
      return ERROR;
    }

  TSLOCK;
  vmeWrite32(&TSp->fpDelay[6],
	     (vmeRead32(&TSp->fpDelay[6]) &~ TS_DUPL_FAST_CLEAR_BUSY_MASK) |
	     (value)<<20 );
  TSUNLOCK;

  return OK;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetFastClearBusy()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = (vmeRead32(&TSp->fpDelay[6]) & TS_DUPL_FAST_CLEAR_BUSY_MASK)>>20;
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplGetBusyTime()
{
  int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->duplBusyTime);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
unsigned int
tsDuplGetBusyStatus()
{
  unsigned int rval=0;
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  TSLOCK;
  rval = vmeRead32(&TSp->duplBusyStatus);
  TSUNLOCK;

  return rval;
}

/**
 * @ingroup Dupl
 * @brief
 *
 * @param
 *
 * @return OK if successful, otherwise ERROR;
 */
int
tsDuplPrintBusyStatus()
{
  unsigned int status=0;
  int ibranch;
  int en[4], alt[4], afc[4], fe[4], oa[4];
  if(TSp == NULL)
    {
      printf("%s: ERROR: TS not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(tsDuplicationMode!=1)
    {
      printf("%s: ERROR: TS Library not configured for Duplication Mode\n",
	     __FUNCTION__);
      return ERROR;
    }

  status = tsDuplGetBusyStatus();

  for(ibranch=0; ibranch<4; ibranch++)
    {
      en[ibranch]  = (status & (1<<(ibranch)))?1:0;
      alt[ibranch] = (status & (1<<(4+ibranch)))?1:0;
      afc[ibranch] = (status & (1<<(8+ibranch)))?1:0;
      fe[ibranch]  = (status & (1<<(12+ibranch)))?1:0;
      oa[ibranch]  = (status & (1<<(20+ibranch)))?1:0;
    }

  printf("                       TS Duplication Mode Busy Status\n\n");

  printf(" All Branches      : %s\n\n", (status & (1<<17))?"BUSY":"Not Busy");

  printf(" Local Trigger Rule: %s\n\n", (status & (1<<16))?"BUSY":"Not Busy");

  printf("                  After Local   After Fast                             \n");
  printf("Branch  Enabled    Trigger        Clear       FrontEnd      Overall \n");
  printf("--------------------------------------------------------------------------------\n");
  for(ibranch=0; ibranch<4; ibranch++)
    {
      printf(" %d       ",ibranch);
      printf("%s       ",en[ibranch]?"YES":"NO ");
      if(en[ibranch])
	{
	  printf("%s          ",alt[ibranch]?"BUSY":"----");
	  printf("%s          ",afc[ibranch]?"BUSY":"----");
	  printf("%s          ",fe[ibranch]?"BUSY":"----");
	  printf("%s          ",oa[ibranch]?"BUSY":"----");
	}
      printf("\n");
    }

  return OK;
}
