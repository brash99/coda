/*
 * File:
 *    tirTDCTest.c
 *
 * Description:
 *    Readout of vfTDC using TIR as a trigger source
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tirLib.h"
#include "vfTDCLib.h"

DMA_MEM_ID vmeIN, vmeOUT;
extern DMANODE *the_event;
extern unsigned int *dma_dabufp;

/* access tirLib global variables */
extern unsigned int tirIntCount;

/* Interrupt Service routine */
void
mytirISR(int arg)
{
  int dCnt, len = 0, idata, blkReady = 0, timeout = 0;
  int printout = 1000;

  tirIntOutput(1);


  GETEVENT(vmeIN, tirIntCount);

  /* Readout vfTDC data */
  blkReady = vfTDCBReady(0);
  if (blkReady == 0 && timeout < 100)
    {
      blkReady = vfTDCBReady(0);
      timeout++;
    }

  if (timeout >= 100)
    {
      printf("%s: Data not ready in vfTDC.\n", __FUNCTION__);
    }
  else
    {
      dCnt = vfTDCReadBlock(0, dma_dabufp, (10 * 192 + 10), 1);
      if (dCnt <= 0)
	{
	  printf("%s: No vfTDC data or error.  dCnt = %d\n", __FUNCTION__,
		 dCnt);
	}
      else
	{
	  dma_dabufp += dCnt;
	}

    }

  PUTEVENT(vmeOUT);

  DMANODE *outEvent = dmaPGetItem(vmeOUT);

  if (tirIntCount % printout == 0)
    {
      printf("Received %d triggers...\r", tirIntCount);

      len = outEvent->length;

      for (idata = 0; idata < len; idata++)
	{
	  vfTDCDataDecode(outEvent->data[idata]);
	}
      printf("\n\n");
    }

  dmaPFreeItem(outEvent);


  tirIntOutput(0);

}


int
main(int argc, char *argv[])
{
  int stat;

  printf("\nJLAB Trigger Interface Tests (0x%x)\n", SSWAP(0x1234));
  printf("----------------------------\n");

  vmeOpenDefaultWindows();
  vmeDmaConfig(2, 5, 1);

  dmaPFreeAll();
  vmeIN = dmaPCreate("vmeIN", 10 * 192 + 40, 1, 0);
  vmeOUT = dmaPCreate("vmeOUT", 0, 0, 0);

  dmaPStatsAll();

  dmaPReInitAll();

  /* Set the TIR structure pointer */
  tirIntInit((unsigned int) (TIR_DEFAULT_ADDR), TIR_EXT_POLL, 1);

  stat = tirIntConnect(TIR_INT_VEC, mytirISR, 0);
  if (stat != OK)
    {
      printf("ERROR: tirIntConnect failed \n");
      goto CLOSE;
    }
  else
    {
      printf("INFO: Attached TIR Interrupt\n");
    }

  /*************************************************************/
  /* VFTDC initialization                                      */
  /*************************************************************/
  extern unsigned int vfTDCA32Base;

  vfTDCA32Base = 0x09000000;
  vfTDCInit(14 << 19, 1 << 19, 1,
	    VFTDC_INIT_VXS_SYNCRESET |
	    VFTDC_INIT_VXS_TRIG | VFTDC_INIT_VXS_CLKSRC);

  int window_width = 250;	/* 250 = 250*4ns = 1000ns */
  int window_latency = 100;	/* 100 = 100*4ns =  400ns */
  vfTDCSetWindowParamters(0, window_latency, window_width);
  vfTDCSetBlockLevel(0, 1);

  vfTDCStatus(0, 1);

  printf("Hit any key to enable Triggers...\n");
  getchar();

  /* Enable the TIR and clear the trigger counter */
  tirIntEnable(TIR_CLEAR_COUNT);

  printf("Hit any key to Disable TIR and exit.\n");
  getchar();

  tirIntDisable();

  tirIntDisconnect();

CLOSE:
  vfTDCStatus(0, 1);
  tirIntStatus(1);

  vmeCloseDefaultWindows();

  exit(0);
}


/*
  Local Variables:
  compile-command: "make -k -B tirTDCtest"
  End:
 */
