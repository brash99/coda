/*
 * File:
 *    tsPartTest.c
 *
 * Description:
 *    Test the partitioning aspects of the TS Library
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tsLib.h"

DMA_MEM_ID vmeIN,vmeOUT;
extern DMANODE *the_event;
extern unsigned int *dma_dabufp;

/* Interrupt Service routine */
void
mytsISR(int arg)
{
  volatile unsigned short reg;
  int dCnt, len=0,idata;
  DMANODE *outEvent;
  int tsbready=0, timeout=0;
  int prescale=1;

  unsigned int tsIntCount = tsGetIntCount();

/*   tsPrintScalers(1); */
/*   printf(" tsIntCount = %d\n", tsIntCount); */
/*   tsPrintScalers(2); */
/*   tsPrintScalers(3); */

  GETEVENT(vmeIN,tsIntCount);

  dCnt = tsPartReadBlock(dma_dabufp,900>>2);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
      dma_dabufp += dCnt;
/*       printf("dCnt = %d\n",dCnt); */
    
    }
  PUTEVENT(vmeOUT);
  
  outEvent = dmaPGetItem(vmeOUT);
#define READOUT
#ifdef READOUT
  if(tsIntCount%prescale==0)
    {
      printf("Received %d triggers...\n",
	     tsIntCount);

      len = outEvent->length;
      
      for(idata=0;idata<len;idata++)
	{
	  if((idata%5)==0) printf("\n\t");
	  printf("  0x%08x ",(unsigned int)LSWAP(outEvent->data[idata]));
	}
      printf("\n\n");
    }
#endif
  dmaPFreeItem(outEvent);
/*   tsStatus(); */
/*   sleep(1); */
}


int 
main(int argc, char *argv[]) {

    int stat;

    printf("\nJLAB TS Tests\n");
    printf("----------------------------\n");

    vmeOpenDefaultWindows();

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
    vmeIN  = dmaPCreate("vmeIN",1024,500,0);
    vmeOUT = dmaPCreate("vmeOUT",0,0,0);
    
    dmaPStatsAll();

    dmaPReInitAll();

    tsReload();

/*     gefVmeSetDebugFlags(vmeHdl,0x0); */
    /* Set the TS structure pointer */
    tsPartInit(1,(21<<19),TS_READOUT_EXT_POLL,0);
    if(tsCheckAddresses()==ERROR)
      goto CLOSE;

    tsPartLoadTriggerTable();
    tsPartSetBlockBufferLevel(10);    

    tsSetBlockLevel(1);

    stat = tsPartIntConnect(mytsISR, 0);
    if (stat != OK) 
      {
	printf("ERROR: tsIntConnect failed \n");
	goto CLOSE;
      } 
    else 
      {
	printf("INFO: Attached TS Interrupt\n");
      }

    tsSetTriggerSource(6); // Pulser = 5, GTP/Ext/FP = 6

    tsSetFPInput(0);
    tsSetGenInput(0xffff);
    tsSetGTPInput(0x0);

    tsPartSetFPInput(1,2,3);
    tsPartSetExtInput(1,2,3,4,5);
    tsPartSetGTPInput(1,2,3,4,5);

/*     tsSetBusySource(TS_BUSY_LOOPBACK,1); */
    tsSetBusySource(0,1);

/*     tsSetBlockBufferLevel(1); */

    tsClockReset();
    taskDelay(1);
    tsTrigLinkReset();
    taskDelay(1);
    tsSyncReset();

    taskDelay(1);
    
    tsStatus();

    printf("Hit enter to start triggers\n");
    getchar();

    tsPartIntEnable(0);
    tsStatus();

/* #define SOFTTRIG */
#ifdef SOFTTRIG
    tsSetRandomTrigger(1,0x7);
    taskDelay(10);
    tsSoftTrig(1,0x1,0x700,0);
#endif

    printf("Hit any key to Disable TID and exit.\n");
    getchar();
    tsStatus();
    tsPrintScalers(1);
    tsPrintScalers(2);
    tsPrintScalers(3);

#ifdef SOFTTRIG
    /* No more soft triggers */
/*     tidSoftTrig(0x0,0x8888,0); */
    tsSoftTrig(1,0,0x700,0);
    tsDisableRandomTrigger();
#endif

    tsPartIntDisable();

    tsPartIntDisconnect();


 CLOSE:

    vmeCloseDefaultWindows();

    exit(0);
}

