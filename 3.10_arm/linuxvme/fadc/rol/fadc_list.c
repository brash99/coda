/*************************************************************************
 *
 *  fadc_list.c - Library of routines for the user to write for
 *                readout and buffering of events from JLab FADC using
 *                a JLab pipeline TI module and Linux VME controller.
 *  
 *                In this example, clock, syncreset, and trigger are
 *                output from the TI then distributed using a Front Panel SDC.
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
#include "fadcLib.h"        /* library of FADC250 routines */

unsigned int blockLevel=1;
#define BUFFERLEVEL 3

/* FADC Library Variables */
extern int fadcA32Base, nfadc;
#define NFADC     3
/* Address of first fADC250 */
#define FADC_ADDR 0x500000
/* Increment address to find next fADC250 */
#define FADC_INCR 0x080000

#define FADC_WINDOW_LAT    375
#define FADC_WINDOW_WIDTH   24
#define FADC_MODE           10

/* for the calculation of maximum data words in the block transfer */
unsigned int MAXFADCWORDS=0;

/* function prototype */
void rocTrigger(int arg);

void
rocDownload()
{
  unsigned short iflag;
  int ifa, stat;

  /* Configure TI */
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
  
  
  /* Program/Init FADC Modules Here */
  iflag = 0xea00; /* SDC Board address */
  iflag |= FA_INIT_EXT_SYNCRESET;  /* Front panel sync-reset */
  iflag |= FA_INIT_FP_TRIG;  /* Front Panel Input trigger source */
  iflag |= FA_INIT_FP_CLKSRC;  /* Internal 250MHz Clock source */

  fadcA32Base = 0x09000000;

  vmeSetQuietFlag(1);
  faInit(FADC_ADDR, FADC_INCR, NFADC, iflag);
  vmeSetQuietFlag(0);

  for(ifa = 0; ifa < nfadc; ifa++)
    {
      faEnableBusError(faSlot(ifa));

      /* Set input DAC level */
      faSetDAC(faSlot(ifa), 3250, 0);

      /*  Set All channel thresholds to 150 */
      faSetThreshold(faSlot(ifa), 150, 0xffff);
  
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
		    FADC_MODE,   /* Processing Mode */
		    FADC_WINDOW_LAT, /* PL */
		    FADC_WINDOW_WIDTH,  /* PTW */
		    3,   /* NSB */
		    6,   /* NSA */
		    1,   /* NP */
		    4,   /* NPED */
		    250, /* MAXPED */
		    2);  /* NSAT */
      
    }

  faSDC_Status(0);
  faGStatus(0);
  
  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  int ifa;

  /* Program/Init VME Modules Here */
  for(ifa=0; ifa < nfadc; ifa++) 
    {
      faSoftReset(faSlot(ifa),0);
      faResetTriggerCount(faSlot(ifa));
    }

  tiStatus(0);
  faGStatus(0);

  printf("rocPrestart: User Prestart Executed\n");

}

void
rocGo()
{
  /* Get the current block level */
  blockLevel = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n",
	 __FUNCTION__,blockLevel);

  faGSetBlockLevel(blockLevel);

  if(FADC_MODE == 9)
    MAXFADCWORDS = 2 + 4 + blockLevel * 8;
  else /* FADC_MODE == 10 */
    MAXFADCWORDS = 2 + 4 + blockLevel * (8 + FADC_WINDOW_WIDTH/2);
  
  /*  Enable FADC */
  faGEnable(0, 0);

  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
rocEnd()
{

  /* FADC Disable */
  faGDisable(0);

  /* FADC Event status - Is all data read out */
  faGStatus(0);

  tiStatus(0);

  faGReset(0);

  printf("rocEnd: Ended after %d events\n",tiGetIntCount());
  
}

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
      for(ifa = 0; ifa < nfadc; ifa++)
	{
	  nwords = faReadBlock(faSlot(ifa), dma_dabufp, MAXFADCWORDS, 1);
	  
	  /* Check for ERROR in block read */
	  blockError = faGetBlockError(1);
	  
	  if(blockError) 
	    {
	      printf("ERROR: Slot %d: in transfer (event = %d), nwords = 0x%x\n",
		     faSlot(ifa), roCount, nwords);

	      if(nwords > 0)
		dma_dabufp += nwords;
	    } 
	  else 
	    {
	      dma_dabufp += nwords;
	    }
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

      for(ifa = 0; ifa < nfadc; ifa++)
	{
	  davail = faBready(faSlot(ifa));
	  if(davail > 0)
	    {
	      printf("%s: ERROR: fADC250 Data available after readout in SYNC event \n",
		     __func__, davail);
	      
	      while(faBready(faSlot(ifa)))
		{
		  vmeDmaFlush(faGetA32(faSlot(ifa)));
		}
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

