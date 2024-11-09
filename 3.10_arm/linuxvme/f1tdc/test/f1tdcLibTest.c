/*
 * File:
 *    f1tdcLibTest.c
 *
 * Description:
 *    Test JLab F1 TDC with GEFANUC Linux Driver
 *    and f1tdc library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tiLib.h"
#include "f1tdcLib.h"
#include "sdLib.h"
/* #include "f1tm.h" */
#include "remexLib.h"

DMA_MEM_ID vmeIN,vmeOUT;
extern DMANODE *the_event;
extern unsigned int *dma_dabufp;

extern int tiA32Base;

#define BLOCKLEVEL 1

#define DO_READOUT

extern int f1tdcA32Base;
extern int f1tdcA32Offset;
extern int nf1tdc;

/* access tirLib global variables */
int F1_SLOT;

#define F1TDC_ADDR (14<<19)
#define F1TM_ADDR  0xe100

/* Interrupt Service routine */
void mytiISR(int arg);

int 
main(int argc, char *argv[]) 
{
  int stat;
  extern int f1ID[20];
  int iflag;
/*   int status; */
  int itdc;
  int inputchar=10;

  printf("\nJLAB f1tdc Lib Tests\n");
  printf("----------------------------\n");
  
  if(vmeOpenDefaultWindows() != OK) {
    goto CLOSE;
  }


  /* Set up remex */
  /* remexSetCmsgServer("dafarm28"); */
  /* remexInit("dafarm42",0); */
  
  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1);

  /* INIT dmaPList */

  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",20244,100,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);
    
  dmaPStatsAll();

  dmaPReInitAll();


  //////////////////////////////
  // TI SETUP
  //////////////////////////////
  tiA32Base=0x08000000;
  tiInit(0,TI_READOUT_EXT_POLL,TI_INIT_SKIP_FIRMWARE_CHECK);
  tiCheckAddresses();

  char mySN[20];
  printf("0x%08x\n",tiGetSerialNumber((char **)&mySN));
  printf("mySN = %s\n",mySN);

  tiLoadTriggerTable(0);
    
  tiSetTriggerHoldoff(1,4,0);
  tiSetTriggerHoldoff(2,4,0);

  tiSetPrescale(0);
  tiSetBlockLevel(BLOCKLEVEL);

  tiSetSyncEventInterval(10);

  stat = tiIntConnect(TI_INT_VEC, mytiISR, 0);
  if (stat != OK) 
    {
      printf("ERROR: tiIntConnect failed \n");
      goto CLOSE;
    } 
  else 
    {
      printf("INFO: Attached TI Interrupt\n");
    }

  /* tiSetTriggerSource(TI_TRIGGER_TSINPUTS); */
  tiSetTriggerSource(TI_TRIGGER_PULSER);
  tiEnableTSInput(TI_TSINPUT_1);

/*   tiSetBusySource(TI_BUSY_LOOPBACK,1); */

  tiSetBlockBufferLevel(1);

  /* tiSetFiberDelay(1,2); */
  /* tiSetSyncDelayWidth(1,0x3f,1); */
  tiEnableVXSSignals();

  sdInit(1);
  /* sdGetSerialNumber(NULL); */

/*   f1tmInit(F1TM_ADDR,0); */
/*   f1tmSetPulserPeriod(0xffff); */
/*   f1tmSetTriggerDelay(50);  */
/*   f1tmSetInputMode(F1TM_HITSRC_INT, F1TM_TRIGSRC_INT); */
/*   f1tmEnablePulser(); */
/*   f1tmStatus(); */

  //////////////////////////////
  // F1 SETUP
  //////////////////////////////
  /* f1ConfigReadFile("../fromEd/cfg_v2_0hs_3125.dat"); */
/*   f1ConfigReadFile("../fromEd/cfg_v3_0ns_3125.dat"); */
  
  iflag  = 0;
  iflag |= F1_SRSRC_VXS;   // SyncReset from VXS
  iflag |= F1_TRIGSRC_VXS; // Trigger from VXS
  iflag |= F1_CLKSRC_VXS;  // Clock from VXS
  iflag |= F1_IFLAG_NOFWCHECK;
  /* printf("iflag = 0x%x\n",iflag); */

  f1tdcA32Base   = 0x09000000;
/*   f1tdcA32Offset = 0x08000000; */

  f1Init(F1TDC_ADDR,(1<<19),4,iflag);
  F1_SLOT = f1ID[0];

  if(nf1tdc>1)
    f1EnableMultiBlock(1);

  sdSetActiveVmeSlots(f1ScanMask()); /* Use the fadcSlotMask to configure the SD */
  /* sdStatus(1); */

  f1GStatus(0);
  f1ChipStatus(F1_SLOT,1);

/*   goto CLOSE; */
/*   f1ConfigShow(F1_SLOT,0); */

/*   // Setup F1TDC */
/*   printf(" Write new stuff here \n"); */
/*   for(ii=0; ii<nf1tdc; ii++) */
/*     { */
/*       f1SetWindow(f1Slot(ii),2000,6000,F1_ALL_CHIPS); */
/* /\*       f1SetConfig(f1Slot(ii),3,F1_ALL_CHIPS); *\/ */
/*     } */

  f1GSetBlockLevel(BLOCKLEVEL);
  f1GEnableBusError();

  // Setup 1 microsec window and latency
/*   f1SetWindow(F1_SLOT,500,500,0xff); */
  
  printf("Hit enter to start triggers\n");
  getchar();


  tiClockReset();
  taskDelay(1);
  tiTrigLinkReset();


 RESTART:
  f1GSoftReset();

  int ii;
  for(ii=0; ii<nf1tdc; ii++)
    {
      f1ResetToken(f1Slot(ii));
    }

  /* printf(" <ENTER> to continue...and send SyncReset.\n"); */

  /* inputchar = getchar(); */

  taskDelay(1);
  tiSyncReset(1);
  tiResetEventCounter();
  taskDelay(1);


  f1GEnableData(0xff);
  f1GEnable();

  f1GClearStatus(F1_ALL_CHIPS);

  f1GStatus(0);
  f1ChipStatus(F1_SLOT,1);

  tiIntEnable(1);
  tiStatus(1);
#define SOFTTRIG
#ifdef SOFTTRIG
  tiSetRandomTrigger(1,0xF);
  taskDelay(10);
/*   tiSoftTrig(1,0x1,0x700,0); */
#endif

  printf("Hit any key to Disable TID and exit.\n");
  getchar();

#ifdef SOFTTRIG
  /* No more soft triggers */
  /*     tidSoftTrig(0x0,0x8888,0); */
  tiSoftTrig(1,0,0x700,0);
  tiDisableRandomTrigger();
#endif

  tiIntDisable();

  tiIntDisconnect();


  f1GDisable();
  /* f1DisableData(0); */

  tiStatus(1);
  f1GStatus(0);
  f1ChipStatus(F1_SLOT,1);

  /* printf(" <ENTER> to continue... or q and <ENTER> to quit.\n"); */

  /* inputchar = getchar(); */

  /* if((inputchar == 113) || */
  /*    (inputchar == 81)) */
  /*   { */
  /*     printf(" Quitting without update.\n"); */
  /*     goto CLOSE; */
  /*   } */
  /* else */
  /*   goto RESTART; */

  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      F1_SLOT = f1Slot(itdc);
      f1Reset(F1_SLOT,0);
    }

 CLOSE:

  /* remexClose(); */
  dmaPFreeAll();
  vmeCloseDefaultWindows();

  exit(0);
}

void
mytiISR(int arg)
{
/*   volatile unsigned short reg; */
  int dCnt, len=0,idata;
  DMANODE *outEvent;
/*   int tibready=0, timeout=0; */
  int printout = 1/BLOCKLEVEL;
  int stat=0, ii;
  int scanmask=0;

  unsigned int tiIntCount = tiGetIntCount();

  GETEVENT(vmeIN,tiIntCount);

  dCnt = tiReadTriggerBlock(dma_dabufp);
  if(dCnt<=0)
    {
      printf("No TI data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
/*       dma_dabufp += dCnt; */
      /*       printf("dCnt = %d\n",dCnt); */
    
    }

  scanmask = f1ScanMask();
  /* f1 readout */
  for(ii=0;ii<100;ii++) 
    {
      stat = f1GBready();
      if (stat==scanmask) 
	{
	  break;
	}
    }

  F1_SLOT=f1Slot(0);
  
  if((stat==scanmask) && ((tiIntCount % 2) == 0))
    {
      int rflag=2;
      if(nf1tdc<=1) rflag=1;
      dCnt = f1ReadBlock(F1_SLOT,dma_dabufp,4000,rflag);
      if(dCnt<=0)
	{
	  printf("No f1TDC data or error.  dCnt = %d\n",dCnt);
	}
      else
	{
	  if(dCnt <= 30)
	    dma_dabufp += dCnt;
	}
    }
  else
    {
      printf("%8d: Data not ready in f1TDC (stat = 0x%x, scanmask = 0x%x)\n",
	     tiIntCount,stat,scanmask);
    }

  for(ii=0; ii<nf1tdc; ii++)
    f1ResetToken(f1Slot(ii));


  PUTEVENT(vmeOUT);

  outEvent = dmaPGetItem(vmeOUT);

  if(tiIntCount%printout==0)
    {
      len = outEvent->length;
      
      printf("Received %d triggers... len = %d\n",
	     tiIntCount, len);


      unsigned int data=0;
      for(idata=0;idata<len;idata++)
	{
/* 	  if((idata%5)==0) printf("\n\t"); */
/* 	  printf("  0x%08x ",(unsigned int)LSWAP(outEvent->data[idata])); */
	  data = LSWAP(outEvent->data[idata]);
	  f1DataDecode(0,data);
	}
      printf("\n\n");
    }

  dmaPFreeItem(outEvent);

  if(tiGetSyncEventFlag())
    {
      printf("SYNC EVENT\n");

      /* Check for data available */
      int davail = tiBReady();
      if(davail > 0)
	{
	  printf("%s: ERROR: TI Data available (%d) after readout in SYNC event \n",
		 __func__, davail);

	  printf("A32 = 0x%08x\n",
		 tiGetAdr32());
	  while(tiBReady())
	    {
	      printf("tiBReady() = %d  ... Call vmeDmaFlush\n",
		     tiBReady());
	      vmeDmaFlush(tiGetAdr32());
	      printf("tiBReady() = %d\n",
		     tiBReady());
	    }
	}

      davail = f1GBready();
      if(davail > 0)
	{
	  printf("%s: ERROR: F1 Data available (%d) after readout in SYNC event \n",
		 __func__, davail);

	  while(f1GBready())
	    {
	      printf("f1GBready() = %d  ... Call vmeDmaFlush\n",
		     f1GBready());
	      vmeDmaFlush(f1GetA32M());
	      printf("f1GBready() = %d\n",
		     f1GBready());
	      for(ii=0; ii<nf1tdc; ii++)
		f1ResetToken(f1Slot(ii));
	    }
	  

	}
    }

      /* if(tiIntCount%printout==0) */
  /*   printf("intCount = %d\n",tiIntCount ); */
/*     sleep(1); */

}
