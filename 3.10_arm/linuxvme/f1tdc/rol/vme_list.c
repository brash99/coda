/*************************************************************************
 *
 *  vme_list.c - Library of routines for readout and buffering of
 *                events using a JLAB Trigger Interface V3 (TI) with
 *                a Linux VME controller.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
#define MAX_EVENT_LENGTH   1024*60      /* Size in Bytes */

/* Define Interrupt source and address */
#define TI_MASTER
#define TI_READOUT TI_READOUT_EXT_POLL  /* Poll for available data, external triggers */
#define TI_ADDR    (21<<19)          /* GEO slot 21 */

#define FIBER_LATENCY_OFFSET 0x4A  /* measured longest fiber length */

#include "dmaBankTools.h"
#include "tiprimary_list.c" /* source required for CODA */
#include "f1tdcLib.h"
#include "sdLib.h"

#define BLOCKLEVEL 1
#define BUFFERLEVEL 3

extern int f1tdcA32Base;
extern int f1tdcA32Offset;
extern int nf1tdc;

#define F1TDC_ADDR (3<<19)

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
   *   TI SETUP
   *****************/
#ifdef TI_MASTER
  tiSetTriggerSource(TI_TRIGGER_TSINPUTS);

  /* Set needed TS input bits */
  tiEnableTSInput( TI_TSINPUT_3 );

  /* Load the trigger table that associates
     pins 21/22 | 23/24 | 25/26 : trigger1
     pins 29/30 | 31/32 | 33/34 : trigger2
  */
  tiLoadTriggerTable(0);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

  /* Set number of events per block */
  tiSetBlockLevel(BLOCKLEVEL);

  tiSetBlockBufferLevel(BUFFERLEVEL);
#endif
  tiStatus(0);

  /*****************
   *    F1 Setup
   *****************/
  int iflag  = 0;
  iflag |= F1_SRSRC_VXS;   // SyncReset from VXS
  iflag |= F1_TRIGSRC_VXS; // Trigger from VXS
  iflag |= F1_CLKSRC_VXS;  // Clock from VXS

  f1tdcA32Base   = 0x09000000;

  f1Init(F1TDC_ADDR,(1<<19),18,iflag);

  f1GSetBlockLevel(BLOCKLEVEL);
  f1GEnableBusError();

  if(nf1tdc>1)
    f1EnableMultiBlock(1);

  sdInit(0);
  sdSetActiveVmeSlots(f1ScanMask()); /* Use the fadcSlotMask to configure the SD */
  sdStatus();

  f1GStatus(0);

  f1ConfigShow(f1Slot(0),0);

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{

  f1GStatus(0);
  tiStatus(0);


  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{

  /* Print out the Run Number and Run Type (config id) */
  printf("rocGo: Activating Run Number %d, Config id = %d\n",
	 rol->runNumber,rol->runType);

  /* Get the broadcasted Block and Buffer Levels from TS or TI Master */
  blockLevel = tiGetCurrentBlockLevel();
  bufferLevel = tiGetBroadcastBlockBufferLevel();
  printf("rocGo: Block Level set to %d  Buffer Level set to %d\n",blockLevel,bufferLevel);

  /* In case of slave, set TI busy to be enabled for full buffer level */
  tiUseBroadcastBufferLevel(1);

  /* Enable modules, if needed, here */
  f1GSoftReset();

  f1ResetToken(f1Slot(0));

  f1GEnableData(0xff);
  f1GEnable();

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

  f1GDisable();
  f1GStatus(0);
  tiStatus(0);

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int evntno)
{
  int ii;
  int stat, dCnt;

  tiSetOutputPort(1,0,0,0);

  /* Readout the trigger block from the TI
     Trigger Block MUST be readout first */
  dCnt = tiReadTriggerBlock(dma_dabufp);

  if(dCnt<=0)
    {
      printf("No TI Trigger data or error.  dCnt = %d\n",dCnt);
    }
  else
    { /* TI Data is already in a bank structure.  Bump the pointer */
      dma_dabufp += dCnt;
    }

  BANKOPEN(7,BT_UI4,0);
  unsigned int scanmask = f1ScanMask();
  /* f1 readout */
  for(ii=0;ii<100;ii++)
    {
      stat = f1GBready();
      if (stat==scanmask)
	{
	  break;
	}
    }

  if(stat==scanmask)
    {
      dCnt = f1ReadBlock(f1Slot(0),dma_dabufp,4000,2);
      if(dCnt<=0)
	{
	  printf("No f1TDC data or error.  dCnt = %d\n",dCnt);
	}
      else
	{
	  dma_dabufp += dCnt;
	}
    }
  else
    {
      printf("%8d: Data not ready in f1TDC (stat = 0x%x, scanmask = 0x%x)\n",
	     tiGetIntCount(),stat,scanmask);
    }

  for(ii=0; ii<nf1tdc; ii++)
    f1ResetToken(f1Slot(ii));

  BANKCLOSE;

  tiSetOutputPort(0,0,0,0);

}

void
rocCleanup()
{
  int islot=0;

  printf("%s: Reset all F1TDCs\n",__FUNCTION__);
  for(islot=0; islot<nf1tdc; islot++)
    {
      f1Reset(f1Slot(islot),0);
    }

}
