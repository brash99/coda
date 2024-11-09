/*
 * File:
 *    tid_intTest.c
 *
 * Description:
 *    Test Vme TID interrupts with GEFANUC Linux Driver
 *    and TID library
 *
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

#define BLOCKLEVEL 3

/* Interrupt Service routine */
void
mytsISR(int arg)
{
  volatile unsigned short reg;
  int dCnt, len=0,idata;
  DMANODE *outEvent;
  int tsbready=0, timeout=0;
  static int bl=BLOCKLEVEL;
  int printout = 1;
  
  unsigned int tsIntCount = tsGetIntCount();

/*   tsPrintScalers(1); */
/*   printf(" tsIntCount = %d\n", tsIntCount); */
/*   tsPrintScalers(2); */
/*   tsPrintScalers(3); */

  GETEVENT(vmeIN,tsIntCount);

#ifdef DOINT
  tsbready = tsBReady();
  if(tsbready==ERROR)
    {
      printf("%s: ERROR: tsIntPoll returned ERROR.\n",__FUNCTION__);
      return;
    }

  if(tsbready==0 && timeout<100)
    {
      printf("NOT READY!\n");
      tsbready=tsBReady();
      timeout++;
    }

  if(timeout>=100)
    {
      printf("TIMEOUT!\n");
      return;
    }
#endif

  dCnt = tsReadTriggerBlock(dma_dabufp);
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
  if((tsIntCount%printout)==0)
    {
      printf("Received %d triggers...\n",
	     tsIntCount);

      len = outEvent->length;
      
      for(idata=0;idata<len;idata++)
	{
	  if((idata%5)==0) printf("\n\t");
	  printf("  0x%08x ",(unsigned int)(outEvent->data[idata]));
	}
      printf("\n\n");
    }
#endif
  dmaPFreeItem(outEvent);

  if(tsGetSyncEventFlag())
    {
/*       tsSetBlockLevel(bl++); */
      printf("SE: Curr BL = %d\n",tsGetCurrentBlockLevel());
      printf("SE: Next BL = %d\n",tsGetNextBlockLevel());
    }


/*   tsStatus(1); */
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

/*     gefVmeSetDebugFlags(vmeHdl,0x0); */
    /* Set the TS structure pointer */
    tsInit(0, TS_READOUT_EXT_POLL, 0);
    if(tsCheckAddresses()==ERROR)
      goto CLOSE;

    tsSetSyncEventInterval(20);

    tsSetBlockLevel(BLOCKLEVEL);

    
    tsLoadTriggerTable();

    stat = tsIntConnect(TS_INT_VEC, mytsISR, 0);
    if (stat != OK) 
      {
	printf("ERROR: tsIntConnect failed \n");
	goto CLOSE;
      } 
    else 
      {
	printf("INFO: Attached TS Interrupt\n");
      }

    tsSetTriggerSource(TS_TRIGSRC_PULSER);

    tsSetFPInput(0x0);
    tsSetGTPInput(0x0);

    tsSetBusySource(TS_BUSY_LOOPBACK,1);

    tsSetBlockLimit(20);

    tsSetBlockBufferLevel(1);

    /* tsSetGTPInputReadout(1); */
    tsSetFPInputReadout(1);
    tsSetEventFormat(3);
    
    tsTrigLinkReset();
    taskDelay(1);

    tsStatus(1);
    int again=0;

 AGAIN:
    tsSyncReset(1);

    taskDelay(1);

    tsGetCurrentBlockLevel();
    
    tsStatus(1);

    printf("Hit enter to start triggers\n");
    getchar();

    tsIntEnable(0);
    tsStatus(1);
#define SOFTTRIG
#ifdef SOFTTRIG
    tsSetRandomTrigger(1,0x7);
    taskDelay(10);
    /* tsSoftTrig(2,10000,0x7FFF,1); */
#endif

    printf("Hit any key to Disable triggers.\n");
    getchar();
    tsDisableTriggerSource(1);
    tsStatus(1);

#ifdef SOFTTRIG
    /* No more soft triggers */
/*     tidSoftTrig(0x0,0x8888,0); */
/*     tsSoftTrig(1,0,0x700,0); */
    tsSoftTrig(2,0,0x7FFF,1);
    tsDisableRandomTrigger();
#endif

    tsIntDisable();

    tsIntDisconnect();

    tsStatus(1);

    again=0;
    if(again==1)
      {
	again=0;
	goto AGAIN;
      }

 CLOSE:

    vmeCloseDefaultWindows();

    exit(0);
}

