/*************************************************************************
 *
 *  fadc_sd_ctp_list.c - Library of routines for readout and buffering of 
 *                events using a JLAB Trigger Interface (TI) with 
 *                a Linux VME controller.
 *
 *      This readout list for use with a crate of fADC250-V2s, Switch Slot
 *      modules: CTP, SD.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     400
#define MAX_EVENT_LENGTH   (66000<<2)      /* Size in Bytes */

#define TI_SLAVE
#define TI_READOUT TI_READOUT_TS_POLL  /* Poll for available data, TS/TImaster triggers */
#define TI_ADDR    (21<<19)          /* GEO slot 21 */

#define FIBER_LATENCY_OFFSET 0x30

#include <linux/prctl.h>
#include "dmaBankTools.h"
#include "tiprimary_list.c" /* source required for CODA */
#include "fadcLib.h"
#include "sdLib.h"
#include "ctpLib.h"

unsigned int blockLevel=1;
#define BUFFERLEVEL 3

/* TI Globals */
unsigned int MAXTIWORDS=0;
extern unsigned int tiTriggerSource;

/* CTP Defaults/Globals */
#define CTP_THRESHOLD    0xbb

/* FADC Defaults/Globals */
#define FADC_DAC_LEVEL    3250
#define FADC_THRESHOLD      20
#define FADC_WINDOW_LAT    375
#define FADC_WINDOW_WIDTH   24
int FADC_NPULSES =           4;
#define FADC_MODE           10
extern int fadcA32Base;
extern int nfadc;

#define NFADC 3
/* Address of first fADC250 */
#define FADC_ADDR 0x500000
/* Increment address to find next fADC250 */
#define FADC_INCR 0x080000

unsigned int fadcSlotMask=0;

/* for the calculation of maximum data words in the block transfer */
unsigned int MAXFADCWORDS=0;

unsigned int ctp_threshold=CTP_THRESHOLD, fadc_threshold=FADC_THRESHOLD;
unsigned int fadc_window_lat=FADC_WINDOW_LAT, fadc_window_width=FADC_WINDOW_WIDTH;

/* function prototype */
void rocTrigger(int arg);

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  int iFlag, ifa;

  /*****************
   *   TI SETUP
   *****************/
  if(tsCrate)
    {
      tiSetBlockBufferLevel(BUFFERLEVEL);
      tiSetBlockLevel(blockLevel);

      tiSetTriggerSource(TI_TRIGGER_TSINPUTS);
      
      /* Set needed TS input bits */
      tiEnableTSInput( TI_TSINPUT_1 );
      
      /* Load the trigger table that associates 
	 All TS inputs as a physics trigger
      */
      tiLoadTriggerTable(3);
      
      tiSetTriggerHoldoff(1,10,0);
      tiSetTriggerHoldoff(2,10,0);
      tiSetTriggerHoldoff(3,10,0);
      tiSetTriggerHoldoff(4,10,0);

      /* Sync event every 1000 blocks */
      tiSetSyncEventInterval(1000);

      /* Add a TI Slave connected to fiber port 1 */
      tiAddSlave(1);

      /* Set L1A prescale ... rate/(x+1) */
      tiSetPrescale(0);

      /* Set TS input #1 prescale rate/(2^(x-1) + 1)*/
      tiSetInputPrescale(1, 0);
    }

  /* Add trigger latch pattern to datastream */
  tiSetFPInputReadout(1);
  
  /* Set ROC ID to 1 */
  tiSetCrateID(1);

  /*******************
   *   FADC250 SETUP
   *******************/
  fadcA32Base=0x09000000;
  iFlag = 0;
  iFlag |= FA_INIT_EXT_SYNCRESET; /* External (VXS) SyncReset*/
  iFlag |= FA_INIT_VXS_TRIG;      /* VXS Input Trigger */
  iFlag |= FA_INIT_INT_CLKSRC;    /* Internal Clock Source (Will switch later) */

  vmeSetQuietFlag(1);
  faInit(FADC_ADDR, FADC_INCR, NFADC, iFlag);
  vmeSetQuietFlag(0);

  if(nfadc>1)
    faEnableMultiBlock(1);

  fadcSlotMask=faScanMask();

  for(ifa = 0; ifa < nfadc; ifa++) 
    {
      faEnableBusError(faSlot(ifa));

      /* Set the internal DAC level */
      faSetDAC(faSlot(ifa), FADC_DAC_LEVEL, 0);

      /* Set the threshold for data readout */
      faSetThreshold(faSlot(ifa), fadc_threshold, 0);
	
      int ichan;
      for(ichan=0; ichan<16; ichan++)
	{
	  faSetChannelPedestal(faSlot(ifa),ichan,0);
	}


      /*********************************************************************************
       * faSetProcMode(int id, int pmode, unsigned int PL, unsigned int PTW, 
       *    int NSB, unsigned int NSA, unsigned int NP, 
       *    unsigned int NPED, unsigned int MAXPED, unsigned int NSAT);
       *
       *  id    : fADC250 Slot number
       *  pmode : Processing Mode
       *          9 - Pulse Parameter (ped, sum, time)
       *         10 - Debug Mode (9 + Raw Samples)
       *    PL : Window Latency
       *   PTW : Window Width
       *   NSB : Number of samples before pulse over threshold
       *   NSA : Number of samples after pulse over threshold
       *    NP : Number of pulses processed per window
       *  NPED : Number of samples to sum for pedestal
       *MAXPED : Maximum value of sample to be included in pedestal sum
       *  NSAT : Number of consecutive samples over threshold for valid pulse
       */
      faSetProcMode(faSlot(ifa),
		    FADC_MODE,
		    FADC_WINDOW_LAT,
		    FADC_WINDOW_WIDTH,
		    3,   /* NSB */
		    6,   /* NSA */
		    1,   /* NP */
		    4,   /* NPED */
		    250, /* MAXPED */
		    2);  /* NSAT */

    }	
  faGStatus(0);

  /*****************
   *   SD SETUP
   *****************/
  sdInit(0);
  sdSetActiveVmeSlots(fadcSlotMask);
  sdStatus(1);

  /*****************
   *   CTP SETUP
   *****************/
  ctpInit(0);

  ctpSetVmeSlotEnableMask(fadcSlotMask);
  ctpSetFinalSumThreshold(ctp_threshold, 0);

  int iwait=0;
  int allchanup=0;
  while(allchanup  != (0x7) )
    {
      iwait++;
      allchanup = ctpGetAllChanUp(0);
      if(iwait>1000)
	{
	  printf("ERROR: Timeout waiting for CTP Channel up - 0x%x\n",allchanup);
	  break;
	}
    }
  ctpStatus(1);

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  int ifa;

  /* Program/Init VME Modules Here */
  for(ifa=0;ifa<nfadc;ifa++) 
    {
      faSetClockSource(faSlot(ifa),FA_REF_CLK_P0);
      faSoftReset(faSlot(ifa),0);
      faResetToken(faSlot(ifa));
      faResetTriggerCount(faSlot(ifa));
    }

  tiStatus(0);

  ctpAlignAtSyncReset(1);

  for(ifa=0;ifa<nfadc;ifa++) 
    {
      /*  Enable FADC */
      faChanDisable(faSlot(ifa),0xffff);
      faSetMGTTestMode(faSlot(ifa),0);
      faEnable(faSlot(ifa),0,0);
    }

  ctpStatus(1);
  ctpResetScalers();

  printf("rocPrestart: User Prestart Executed\n");
}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{
  int ifa;

  /* Enable modules, if needed, here */
  ctpAlignAtSyncReset(0);
  ctpGetAlignmentStatus(1,10);

  for(ifa=0;ifa<nfadc;ifa++)
    {
      faChanDisable(faSlot(ifa),0x0);
      faSetMGTTestMode(faSlot(ifa),1);
    }

  faGDisable(0);
  /* Get the current block level */
  blockLevel = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n",
	 __FUNCTION__,blockLevel);

  faGSetBlockLevel(blockLevel);

  if(FADC_MODE == 9)
    MAXFADCWORDS = nfadc * (2 + 4 + blockLevel * 8);
  else /* FADC_MODE == 10 */
    MAXFADCWORDS = nfadc * (2 + 4 + blockLevel * (8 + FADC_WINDOW_WIDTH/2));


  faGEnable(0, 0);
  /* Interrupts/Polling enabled after conclusion of rocGo() */

}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{
  faGDisable(0);
  faGStatus(0);
  
  tiStatus(0);
  sdStatus(1);
  ctpStatus(1);

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());
}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int ifa, stat, nwords;
  unsigned int datascan, scanmask;
  int roCount = 0, blockError = 0;

  roCount = tiGetIntCount();

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1); 

#define CODA2
#ifdef CODA2
  int ev_type;
  
  BANKOPEN(1,BT_UI4,0);
  nwords = tiReadBlock(dma_dabufp, 3 + 5*blockLevel, 1);
  if(nwords <= 0) 
    {
      printf("ERROR: Event %d: No TI data or error.  nwords = %d\n",
	     roCount, nwords);
    }
  else
    {
      /* Get the event type from the TI data */
      ev_type = tiDecodeTriggerType(dma_dabufp, nwords, 1);
      if(ev_type <= 0)
	{
	  /* Could not find trigger type */
	  ev_type = 1;
	}
      
      /* CODA 2.x only allows for 4 bits of trigger type */
      ev_type &= 0xF; 

      the_event->type = ev_type;

      dma_dabufp += nwords;
    }

  BANKCLOSE;
#else // CODA 3
  dCnt = tiReadTriggerBlock(dma_dabufp);
  if(dCnt<=0) 
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
      dma_dabufp += dCnt;
    }
#endif

  /* fADC250 Readout */
  BANKOPEN(3,BT_UI4,0);

  /* Mask of initialized modules */
  scanmask = faScanMask();
  /* Check scanmask for block ready up to 100 times */
  datascan = faGBlockReady(scanmask, 100); 
  stat = (datascan == scanmask);

  if(stat) 
    {
      nwords = faReadBlock(faSlot(ifa), dma_dabufp, MAXFADCWORDS, 2);

      /* Check for ERROR in block read */
      blockError = faGetBlockError(1);

      if(blockError) 
	{
	  printf("ERROR: in transfer (event = %d), nwords = 0x%x\n",
		 roCount, nwords);
	  for(ifa = 0; ifa < nfadc; ifa++)
	    faResetToken(faSlot(ifa));

	  if(nwords > 0)
	    dma_dabufp += nwords;
	} 
      else 
	{
	  dma_dabufp += nwords;
	  faResetToken(faSlot(0));
	}
    }
  else 
    {
      printf("ERROR: Event %d: Datascan != Scanmask  (0x%08x != 0x%08x)\n",
	     roCount, datascan, scanmask);
    }

  BANKCLOSE;

  /* Check for SYNC Event */
  if(tiGetSyncEventFlag() == 1)
    {
      /* Check for data available */
      int davail = tiBReady();
      if(davail > 0)
	{
	  printf("%s: ERROR: TI Data available (%d) after readout in SYNC event \n",
		 __func__, davail);
	  
	  while(tiBReady())
	    {
	      vmeDmaFlush(tiGetAdr32());
	    }
	}
      
      davail = faGBready();
      if(davail > 0)
	{
	  printf("%s: ERROR: fADC250 Data available after readout in SYNC event \n",
		 __func__, davail);
	  
	  while(faGBready())
	    {
	      vmeDmaFlush(faGetA32M());
	    }
	}
    }
  
}

void
rocCleanup()
{

  printf("%s: Reset all FADCs\n",__FUNCTION__);
  faGReset(1);
  
}

