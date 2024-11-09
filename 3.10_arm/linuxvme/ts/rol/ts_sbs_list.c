/*************************************************************************
 *
 *  ts_list.c - Library of routines for readout and buffering of 
 *                events using a JLAB Pipeline Trigger Supervisor (TS) with 
 *                a Linux VME controller.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
#define MAX_EVENT_LENGTH   1024*60      /* Size in Bytes */

/* Define Interrupt source and address */
#define TS_READOUT TS_READOUT_EXT_POLL  /* Poll for available data, external triggers */
#define TS_ADDR    (21<<19)          /* GEO slot 3 */


#include "dmaBankTools.h"
#include "tsprimary_list.c" /* source required for CODA */
#include "tdLib.h"

#define BLOCKLEVEL 1
#define BUFFERLEVEL 1

extern unsigned int tsTriggerSource;

/* function prototype */
void rocTrigger(int arg);

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1);

  /*****************
   *   TS SETUP
   *****************/

  /* Setup Trigger Source to be FrontPanel and/or GTP inputs */
  tsSetTriggerSource(6);

  /* Enable/Disable specific inputs */
  tsSetFPInput(0xFF00); /* Disable UL/UR for duplication mode */
  tsSetGTPInput(0x0);   /* Disabled */

  /* Sync Event Interval (units of blocks) */
  tsSetSyncEventInterval(1000);

  /* Set number of events per block */
  tsSetBlockLevel(BLOCKLEVEL);

  /* Load the default trigger table */
  tsLoadTriggerTable();
  

  /* 
   * Trigger Holdoff rules
   */
  tsSetTriggerHoldoff(1,20,0);

  /* 
   * Set the Block Buffer Level
   *  0:  Pipeline mode
   *  1:  One Block per readout - "ROC LOCK" mode
   *  2-255:  "Buffered" mode.
   */
  tsSetBlockBufferLevel(BUFFERLEVEL);

  /*****************
   *   TD SETUP
   *****************/
#ifdef USETD
  /* Setup TDs */
  tdInit((17<<19),0,1,0); /* TD in Slot 17 */

  tdGSetBlockLevel(BLOCKLEVEL);
  tdGSetBlockBufferLevel(BUFFERLEVEL);

  tdAddSlave(17,2); // TI Slave - Connected to Fiber port 2
  tdAddSlave(17,5); // TI Slave - Connected to Fiber port 5

  tdGStatus(0);

  sdInit();
  sdSetActiveVmeSlots( tdSlotMask() );

  sdStatus();
  
  tsSetBusySource(TS_BUSY_LOOPBACK | TS_BUSY_SWB,1);
#else
  tsSetBusySource(TS_BUSY_LOOPBACK,1);
  tsAddSlave(1); /* Enable TS Fiber TI_A */
  tsAddSlave(2); /* Enable TS Fiber TI_B */
#endif

  /*******************************
   *   TS DUPLICATION MODE SETUP
   *   This is an EXAMPLE setup
   *******************************/

 /* Enable Duplication Mode */
  tsDuplMode(1);

  /* Enable Branch 1 and 2, disable 3 and 4 */
  tsDuplSetBranchEnable(1,1,0,0);

  /* Setup the Local Trigger Table 
     (Default: 0xAAAAAAAA == Whenever input #1 -> local trigger) */
  tsDuplSetLocalTrigComboMask(0xAAAAAAAA);

  /* Include local trigger for simultaneous #2 + #3 hit (exclusive) */
  tsDuplSetLocalTrigCombo((1<<2) | (1<<3), 1);

  /* No more than 1 local trigger within 4*(30+2) = 128 ns */
  tsDuplSetTriggerHoldoff(30);

  /* Local trigger output width = 4*(10+2) = 48 ns */
  tsDuplSetLocalTriggerWidth(10);

  /* Fast Clear Width = 4*(23+2) = 100 ns */
  tsDuplSetFastClearWidth(23);

  /* Fast Clear Delay = 4*(25) = 100 ns */
  tsDuplSetFastClearDelay(25);

  /* Fast Clear Veto Width = 4*(23+2) = 100 ns */
  tsDuplSetFastClearVetoWidth(23);

  /* Busy after local trigger = 4*(40) = 160 ns */
  tsDuplSetLocalTrigBusy(40);

  /* Busy after fast clear 4*(40) = 160 ns */
  tsDuplSetFastClearBusy(40);

  printf("rocDownload: User Download Executed\n");


}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  unsigned short iflag;
  int stat;
  int islot;

  tsStatus(0);

  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{
  int islot;
  /* Enable modules, if needed, here */
  tsStatus(0);

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  int islot;

  tsStatus(0);

  printf("rocEnd: Ended after %d blocks\n",tsGetIntCount());
  
}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int ii, islot;
  int stat, dCnt, len=0, idata;
  int timeout;
  int syncFlag=0;
  static unsigned int roEventNumber=0;


  roEventNumber++;
  syncFlag = tsGetSyncEventFlag();

  if(syncFlag)
    {
      printf("%s: Sync Flag Received at readout event %d\n",
	     __FUNCTION__,roEventNumber);
    }

  BANKOPEN(5,BT_UI4,0);
  *dma_dabufp++ = LSWAP(tsGetIntCount());
  *dma_dabufp++ = LSWAP(0xdead);
  *dma_dabufp++ = LSWAP(0xcebaf111);
  BANKCLOSE;

  BANKOPEN(4,BT_UI4,0);

  stat = tsBReady();

  timeout=0;

  while((stat==0) && (timeout<100))
    {
      stat=tsBReady();
      timeout++;
    }

  if(timeout>=100)
    {
      logMsg("tsBReady TIMEOUT\n",1,2,3,4,5,6);
    }
  else
    {
      dCnt = tsReadBlock(dma_dabufp,900>>2,1);
      if(dCnt<=0)
	{
	  logMsg("No data or error.  dCnt = %d\n",dCnt);
	}
      else
	{
	  dma_dabufp += dCnt;
	}
    }

  BANKCLOSE;

}

void
rocCleanup()
{
  int islot=0;
  tsDisableTriggerSource(0);
}
