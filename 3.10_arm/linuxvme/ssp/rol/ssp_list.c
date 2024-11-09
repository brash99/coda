/*************************************************************************
 *
 *  ssp_list.c      - Library of routines for readout and buffering of
 *                    events using a JLAB Trigger Interface V3 (TI) with
 *                    a Linux VME controller in CODA 3.0.
 *
 *                    This is for a TI in Slave Mode controlled by a
 *                    Master TI or Trigger Supervisor and hosts one
 *                    or more SubSystem Processors (SSPs)
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
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
#include "sspLib.h"
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
  int stat=0, iFlag=0;

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

  /* Set Trigger Buffer Level */
  tiSetBlockBufferLevel(BUFFERLEVEL);

  /* Init the SD library so we can get status info */
  stat = sdInit(0);
  if(stat==0)
    {
      sdSetActiveVmeSlots(0);
      sdStatus(0);
    }
  tiStatus(0);

  /*****************
   *   SSP SETUP
   *****************/
  iFlag = SSP_INIT_MODE_DISABLED; /* Disabled, initially */

  sspInit(0, 0, 0, iFlag); /* Scan for, and initialize all SSPs in crate */

  sspGStatus(0);

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  unsigned short iFlag;
  int issp;

  tiStatus(0);

  /* Configure the SSP modules */
  iFlag  = SSP_INIT_MODE_VXS;
  iFlag |= SSP_INIT_FIBER_ENABLE_MASK;     /* Enable all fiber ports */
  iFlag |= SSP_INIT_GTP_FIBER_ENABLE_MASK; /* Enable all fiber port data to GTP */
  for(issp=0; issp<nSSP; issp++)
    {
      sspSetMode(sspSlot(issp),iFlag,0);
      /* Direct SSP Internal "Trigger 0" to LVDS0 Output on Front Panel */
      sspSetIOSrc(sspSlot(issp), SD_SRC_LVDSOUT0, SD_SRC_SEL_TRIGGER0);
    }


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

  /* Determine which ports are connected, and only enable them */
  for(issp=0; issp<nSSP; issp++)
    {
      connected_ports = sspGetConnectedFiberMask(sspSlot(issp));
      iFlag  = SSP_INIT_SKIP_SOURCE_SETUP; /* Already set in rocPrestart */
      iFlag |= connected_ports<<16;
      iFlag |= connected_ports<<24;
      sspSetMode(sspSlot(issp),iFlag,1);
    }

  sspGStatus(0);

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  tiStatus(0);

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int dCnt;

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

  printf("%s: Reset all Modules\n",__FUNCTION__);

}
