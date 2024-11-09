/*************************************************************************
 *
 *  gt_list.c       - Library of routines for readout and buffering of
 *                    events using a JLAB Trigger Interface V3 (TI) with
 *                    a Linux VME controller in CODA 3.0.
 *
 *                    This is for a TI in Slave Mode controlled by a
 *                    Master TI or Trigger Supervisor and hosts one
 *                    or more SubSystem Processors (SSPs) and a
 *                    Global Trigger Processor (GTP)
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     20
#define MAX_EVENT_LENGTH   1024*64      /* Size in Bytes */

/* Define TI Type (TI_MASTER or TI_SLAVE) */
#define TI_SLAVE
/* TS Fiber Link trigger source (from TI Master, TD, or TS), POLL for available data */
#define TI_READOUT TI_READOUT_TS_POLL
/* TI VME address, or 0 for Auto Initialize (search for TI by slot) */
#define TI_ADDR  0

/* Measured longest fiber length in system */
#define FIBER_LATENCY_OFFSET 0x4A

#include "dmaBankTools.h"   /* Macros for handling CODA banks */
#include "tiprimary_list.c" /* Source required for CODA readout lists using the TI */
#include "sspLib.h"         /* Include SSP library stuff */
#include "gtpLib.h"
#include "sdLib.h"

/* Define buffering level */
#define BUFFERLEVEL 10

/* Extern global variables */
extern int nSSP;   /* Number of SSPs found with sspInit(..) */


/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  int stat=0, iFlag=0, iport=0;

  /* Setup Address and data modes for DMA transfers
   *
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */

  vmeDmaConfig(2,5,1);

  /* Define BLock Level variable to a default */
  blockLevel = 1;


  /*****************
   *   TI SETUP
   *****************/

  /* Set the sync delay width to 0x40*32 = 2.048us */
  tiSetSyncDelayWidth(0x54, 0x40, 1);

  /* Set Trigger Buffer Level */
  tiSetBlockBufferLevel(BUFFERLEVEL);

  /* Init the SD library so we can get status info */
  stat = sdInit(0);
  if(stat==0)
    {
      tiSetBusySource(TI_BUSY_SWB,1);
      sdSetActiveVmeSlots(0);
      sdStatus(0);
    }
  else
    { /* No SD or the TI is not in the Proper slot */
      tiSetBusySource(TI_BUSY_LOOPBACK,1);
    }

  tiStatus(0);

  /*****************
   *   SSP SETUP
   *****************/
  iFlag = SSP_INIT_MODE_DISABLED; /* Disabled, initially */

  sspInit((9<<19), 0, 1, iFlag); /* Initialize the SSP in slot 9 */

  sspGStatus(0);

  /*****************
   *   GTP SETUP
   *****************/

  /* Initialize GTP.  NULL (arg 2) specifies library to access GTP through TI (I2C)
     to determine it's network hostname */
  gtpInit(0,NULL);

  /* Clock source is Internal before prestart */
  gtpSetClockSource(GTP_CLK_CTRL_INT);
  gtpSetSyncSource(GTP_SD_SRC_SEL_SYNC);
  gtpSetTrig1Source(GTP_SD_SRC_SEL_TRIG1);

  gtpEnableVmeSlotMask(sspSlotMask());
  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  unsigned short iFlag;
  int stat;
  int issp, iport;
  int portconnected_mask = 0;

  /* Check the health of the vmeBus Mutex.. re-init if necessary */
  vmeCheckMutexHealth(10);

  /* Set GTP to use the global clock */
  gtpSetClockSource(GTP_CLK_CTRL_P0);

  tiStatus(0);

  /* Configure the SSP modules */
  iFlag  = SSP_INIT_MODE_VXS;
  iFlag |= SSP_INIT_FIBER0_ENABLE;     /* Enable fiber port 0 */
  iFlag |= TRG_CTRL_GTPSRC_FIBER0; /* Enable fiber port 0 data to GTP */
  for(issp=0; issp<nSSP; issp++)
    {
      sspSetMode(sspSlot(issp),iFlag,0);

      /* Reset crate IDs on all ports */
      for(iport = 0; iport < 8; iport++)
	sspResetCrateId(0,iport);
    }

  gtpEnableVmeSlotMask(sspSlotMask());

  /* Set up trigger bit 0 out - TOF N Hits logic*/
  gtpSetTriggerBitEnableMask(0, GTP_TRIGBIT_CTRL_ENABLE | GTP_TRIGBIT_CTRL_TOF_NHITS_EN);

  /* Set trigbit0 latency, width */
  gtpSetTrigoutLatencyWidth(0, 0, 4);

  /* Equation for TOF_hitpattern
    TOFMasked=TOFHitPattern&TOFHitPattern_Mask
    TOFhori = TOFMasked(15:0)
    TOFvert = TOFMasked(31:16)
    (bit_cnt(TOFMasked) > TOFNHits_Thr) &&
    vec_or(TOFhori) &&
    vec_or(TOFvert)
  */

  /* Set trigbit 0 equation parameters */
  gtpSetTrigoutParameter(0, GTP_TOF_MASK, 0xFFFFFFFF); /* Everything hit */
  gtpSetTrigoutParameter(0, GTP_TOF_HITCOUNT_THRESHOLD, 0);

  sspGStatus(0);
  gtpStatus(0);

  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{
  int issp, iFlag=0;
  int connected_ports=0;

  /* Get the broadcasted Block Level from TS or TI Master */
  blockLevel = tiGetCurrentBlockLevel();
  printf("rocGo: Block Level set to %d\n",blockLevel);

  sspGStatus(0);
  gtpStatus(0);

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{
  int islot;

  tiStatus(0);

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int ii, islot;
  int stat, dCnt, len=0, idata;
  unsigned int val;
  unsigned int *start;

  /* Set TI output 1 high for diagnostics */
  tiSetOutputPort(1,0,0,0);

  /* Readout the trigger block from the TI
     Trigger Block MUST be reaodut first */
  dCnt = tiReadTriggerBlock(dma_dabufp);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    { /* TI Data is already in a bank structure.  Bump the pointer */
      dma_dabufp += dCnt;
    }


  /* EXAMPLE: How to open a bank (type=5) and add data words by hand */
  BANKOPEN(5,BT_UI4,blockLevel);
  *dma_dabufp++ = tiGetIntCount();
  *dma_dabufp++ = 0xdead;
  *dma_dabufp++ = 0xcebaf111;
  *dma_dabufp++ = 0xcebaf222;
  BANKCLOSE;

  /* Set TI output 0 low */
  tiSetOutputPort(0,0,0,0);

}

void
rocCleanup()
{
  int islot=0;

  printf("%s: Reset all Modules\n",__FUNCTION__);

}
