/*************************************************************************
 *
 *  vme_list.c - Library of routines for readout and buffering of 
 *                events using a JLAB Trigger Interface (TI) with 
 *                a Linux VME controller.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     400
#define MAX_EVENT_LENGTH   1024*10      /* Size in Bytes */

/* Define Interrupt source and address */
#define TIR_SOURCE
#define TIR_ADDR 0x0ed0
/* TIR_MODE:  0 : interrupt on trigger,
              1 : interrupt from Trigger Supervisor signal
              2 : polling for trigger
              3 : polling for Trigger Supervisor signal  */
#define TIR_MODE 2

#include "linuxvme_list.c" /* source required for CODA */

/* function prototype */
void rocTrigger(int arg);

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

  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  unsigned short iflag;
  int stat;

  /* Program/Init VME Modules Here */

  printf("rocPrestart: User Prestart Executed\n");

}

void
rocGo()
{
  /* Enable modules, if needed, here */

  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
rocEnd()
{

  printf("rocEnd: Ended after %d events\n",tirGetIntCount());
  
}

void
rocTrigger(int arg)
{
  int ii;
  unsigned int event_ty=0, event_no=0;

  event_ty = EVTYPE;
  event_no = EVNUM;

  /* Open an event bank of banks with event type EVTYPE (obtained from TI) */
  EVENTOPEN(event_ty, BT_BANK);

  /* Example: Raise the 0th (1<<0) and 2nd (1<<2) output level on the TI */
  tirIntOutput(1<<0 | 1<<2);

  /* Open Bank number 5 (for example), Bank of 32bit unsigned integers */
  BANKOPEN(5,BT_UI4,0);
  /* Insert some data here - Make sure bytes are ordered little-endian (LSWAP)*/
  *dma_dabufp++ = LSWAP(0xda000022);
  for(ii=0; ii<20; ii++) 
    {
      *dma_dabufp++ = LSWAP(ii);
    }
  *dma_dabufp++ = LSWAP(0xda0000ff);

  BANKCLOSE;

  EVENTCLOSE;
  /* Drop all output levels on the TI */
  tirIntOutput(0);

}

void
rocCleanup()
{
  /* Put any clean up code that should be done before the next readout list is downloaded */

}
