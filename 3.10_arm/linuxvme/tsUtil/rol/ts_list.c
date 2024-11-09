/*************************************************************************
 *
 *  ts_list.c - Library of routines for the user to write for
 *                readout and buffering of events using
 *                a Linux VME controller.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     400
#define MAX_EVENT_LENGTH   1024*10      /* Size in Bytes */

/* Define Interrupt source and address */
#define TS_SOURCE
#define TS_MODE TS_POLL
#define TS_ADDR 0xed0000

/*  Type 0xff10 is RAW trigger No timestamps
    Type 0xff11 is RAW trigger with timestamps (64 bits) */
int trigBankType = 0xff11;

#include "dmaBankTools.h"   /* Macros for handling CODA banks */
#include "linuxvme_list.c"

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

  tsStatus(1);
  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{
  /* Set number of events per block (must be the same for each ROC) */
  blockLevel = 1;
  printf("rocPrestart: Block Level set to %d\n",blockLevel);

  /* Program Trigger Supervisor and Configure VME Modules Here */
  tsReset(0);

  tsCsr2Set(TS_CSR2_LOCK_ALL);  /* ROC Lock mode on all BRANCHES */
  tsEnableInput(0xfff,0);       /* Enable all Trigger inputs in non-strobe mode */
  tsRoc(0,0,0,0);               /* Enable ACK 0,1 on BRANCH 1  and ACK 2 on Branch 2*/

  /* Synchronization programming */
  tsSync(100);                      /* schedule Sync every 100th physics trigger */
  tsCsr2Set(TS_CSR2_ENABLE_SYNC);   /* Enable Scheduled syncs */

  /* Set nominal Level 2/3 timer values */
  tsTimerWrite(TS_TIMER_L2A,0x05);   /* Level 2 Timer 40ns/count */
  tsTimerWrite(TS_TIMER_L3A,0x05);   /* Level 3 Timer 40ns/count */

  /* Front End Busy timer 40ns/count */
  tsTimerWrite(TS_TIMER_FB,250);         /* 250 = 10 microsec */
  tsCsr2Set(TS_CSR2_ENABLE_FB_TIMER);    /* Enable FEB Timer */

  /*  Construct TS memory data  ---  in the following model, all trigger patterns
      that form the memory address are assigned to trigger class 1.  For those
      trigger patterns with a single hit, the ROC code is set to be the trigger
      input number.  Otherwise, the ROC code is set to 0xE.  All LEVEL 1 ACCEPT
      signals are asserted for every pattern.  */

  tsMemInit();

  /* Fix special cases - both inputs 1 and 2 firing - type 13 (0xd)
     all L1 accept outputs firing 0xff03 */
  tsMemWrite(3,0xdff03);

  /* Set specific input prescale factors */
  tsPrescale(1,0);
  tsPrescale(2,0);

  /* EXAMPLE: User bank of banks added to prestart event */
  UEOPEN(500,BT_BANK,0);

  /* EXAMPLE: Bank of data in User Bank 500 */
  CBOPEN(1,BT_UI4,0);
  *rol->dabufp++ = 0x11112222;
  *rol->dabufp++ = 0x55556666;
  *rol->dabufp++ = 0xaabbccdd;
  CBCLOSE;

  UECLOSE;

  tsStatus(1);

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

  /* Enable modules, if needed, here */

  /* Interrupts/Polling enabled after return of rocGo() */
}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{
  /* Disable modules here, if needed */
  tsStatus(1);

  printf("rocEnd: Ended after %d events\n",tsGetIntCount());
}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int evnum, int ev_type, int sync_flag)
{

  /* Insert dummy trigger bank for CODA 3 */
  InsertDummyTriggerBank(trigBankType, evnum, ev_type, blockLevel);

  /* EXAMPLE: How to open a bank (name=5, type=ui4) and add data words by hand */
  BANKOPEN(5,BT_UI4,blockLevel);
  *dma_dabufp++ = evnum;
  *dma_dabufp++ = 0xdead;
  *dma_dabufp++ = 0xcebaf111;
  *dma_dabufp++ = 0xcebaf222;
  BANKCLOSE;


}

void
rocCleanup()
{

  printf("%s: Reset all Modules\n",__FUNCTION__);

}
