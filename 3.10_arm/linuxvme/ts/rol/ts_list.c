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
#define BUFFERLEVEL 10

extern unsigned int tsTriggerSource;
int usePulser=0;

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
  int overall_offset=0x80;

  if(usePulser)
    tsSetTriggerSource(5);
  else
    tsSetTriggerSource(6);

  /* Enable/Disable specific inputs */
  tsSetFPInput(0x0);
  tsSetGenInput(0x0);
  tsSetGTPInput(0xffff);

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
  tsSetTriggerHoldoff(2,20,0);
  tsSetTriggerHoldoff(3,20,0);
  tsSetTriggerHoldoff(4,20,0);

  /* Set the sync delay width to 0x40*32 = 2.048us */
  tsSetSyncDelayWidth(0x30, 0x40, 1);

  /* 
   * Set the Block Buffer Level
   *  0:  Pipeline mode
   *  1:  One Block per readout - "ROC LOCK" mode
   *  2-255:  "Buffered" mode.
   */
  tsSetBlockBufferLevel(BUFFERLEVEL);

  /* Override the busy source set in tsInit (only if TS crate running alone) */
/*   tsSetBusySource(TS_BUSY_LOOPBACK,1); */

#define USETD
#ifdef USETD
  /* Setup TDs */
  tdInit((17<<19),0,1,0);

  tdGSetBlockLevel(BLOCKLEVEL);
  tdGSetBlockBufferLevel(BUFFERLEVEL);

  tdAddSlave(17,2); // TI Slave - Bottom Crate (payload)
  tdAddSlave(17,5); // TI Slave - Bench (GTP)

  tdGStatus(0);

  sdInit();
  sdSetActiveVmeSlots( 1<<17 );

  sdStatus();
#else
  tsSetBusySource(TS_BUSY_LOOPBACK,1);
  tsAddSlave(1);
#endif

/*   tsSetPrescale(0); */

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

  tsStatus();

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
  tsStatus();

  if(usePulser)
    tsSetRandomTrigger(1,0x7);

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  int islot;

  if(usePulser)
    tsDisableRandomTrigger();

  tsStatus();

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

  remexClose();

}
