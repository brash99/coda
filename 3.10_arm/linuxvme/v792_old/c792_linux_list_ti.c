/*************************************************************************
 *
 *  c792_linux_list.c - Library of routines for the user to write for
 *                      readout and buffering of events using
 *                      a Linux VME controller.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
#define MAX_EVENT_LENGTH   1024*60      /* Size in Bytes */

/* Define Interrupt source and address */
#define TI_MASTER
#define TI_READOUT TI_READOUT_EXT_POLL  /* Poll for available data, external triggers */
#define TI_ADDR    (21<<19)          /* GEO slot 21 */

#define ADC_ID 0
#define MAX_ADC_DATA 34


#include "c792Lib.h"
#define TI_DATA_READOUT

#define FIBER_LATENCY_OFFSET 0x4A  /* measured longest fiber length */

#include "dmaBankTools.h"
#include "tiprimary_list.c" /* source required for CODA */

/* Default block level */
unsigned int BLOCKLEVEL=1;
#define BUFFERLEVEL 3

/* Redefine tsCrate according to TI_MASTER or TI_SLAVE */
#ifdef TI_SLAVE
int tsCrate=0;
#else
#ifdef TI_MASTER
int tsCrate=1;
#endif
#endif

/* function prototype */
void rocTrigger(int arg);

void
rocDownload()
{
  int dmaMode;

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  //vmeDmaConfig(1,3,0); 
  vmeDmaConfig(2,5,1); 

  //c792Init(0xa10000,0,1,0);
  c792Init(0x00100000,0,1,0);
  c792Status(ADC_ID,0,0);

 /*****************
   *   TI SETUP
   *****************/
  int overall_offset=0x80;

#ifndef TI_DATA_READOUT
  /* Disable data readout */
  tiDisableDataReadout();
  /* Disable A32... where that data would have been stored on the TI */
  tiDisableA32();
#endif

  /* Set crate ID */
  tiSetCrateID(0x01); /* ROC 1 */

  tiSetTriggerSource(TI_TRIGGER_TSINPUTS);

  /* Set needed TS input bits */
  tiEnableTSInput( TI_TSINPUT_1 );

  /* Load the trigger table that associates 
     pins 21/22 | 23/24 | 25/26 : trigger1
     pins 29/30 | 31/32 | 33/34 : trigger2
  */
  tiLoadTriggerTable(0);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

/*   /\* Set the sync delay width to 0x40*32 = 2.048us *\/ */
  tiSetSyncDelayWidth(0x54, 0x40, 1);

  /* Set the busy source to non-default value (no Switch Slot B busy) */
  tiSetBusySource(TI_BUSY_LOOPBACK,1);

/*   tiSetFiberDelay(10,0xcf); */

#ifdef TI_MASTER
  /* Set number of events per block */
  tiSetBlockLevel(BLOCKLEVEL);
#endif

  tiSetEventFormat(1);

  tiSetBlockBufferLevel(BUFFERLEVEL);


  tiStatus();

  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  unsigned short iflag;
  int stat;
  tiStatus();

  /* Program/Init VME Modules Here */
  /* Setup ADCs (no sparcification, enable berr for block reads) */
  c792Sparse(ADC_ID,0,0);
  c792Clear(ADC_ID);
  c792EnableBerr(ADC_ID);

  c792Status(ADC_ID,0,0);
  

  printf("rocPrestart: User Prestart Executed\n");

}

void
rocGo()
{
  /* Enable modules, if needed, here */
  /* Get the current block level */
  BLOCKLEVEL = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n",
	 __FUNCTION__,BLOCKLEVEL);

  /* Use this info to change block level is all modules */

  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
rocEnd()
{
  int status, count;
  tiStatus();
  
  printf("rocEnd: Ended after %d events\n",tiGetIntCount());
  
}

void
rocTrigger(int arg)
{
  int ii, status, dma, count;
  int nwords, dCnt;
  unsigned int datascan, tirval, vme_addr;
  int length,size;
  int itimeout=0;

 tiSetOutputPort(1,0,0,0);

  BANKOPEN(5,BT_UI4,0);
  *dma_dabufp++ = LSWAP(tiGetIntCount());
  *dma_dabufp++ = LSWAP(0xdead);
  *dma_dabufp++ = LSWAP(0xcebaf111);
  BANKCLOSE;

#ifdef TI_DATA_READOUT
  BANKOPEN(4,BT_UI4,0);

  vmeDmaConfig(2,5,1); 
  dCnt = tiReadBlock(dma_dabufp,8+(3*BLOCKLEVEL),1);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
      dma_dabufp += dCnt;
    }

  BANKCLOSE;
#endif

   *dma_dabufp++ = LSWAP(tiGetIntCount()); /* Insert Event Number */

  /* Check if an Event is available */

  while(itimeout<1000)
    {
      itimeout++;
      status = c792Dready(ADC_ID);
      if(status>0) break;
    }
  if(status > 0) 
    {
      if(tiGetIntCount() %1000==0)
	{
	  printf("itimeout = %d\n",itimeout);
	  c792PrintEvent(ADC_ID,0);
	}
      else
	{
/* 	  nwords = c792ReadEvent(ADC_ID,dma_dabufp); */
	  nwords = c792ReadBlock(ADC_ID,dma_dabufp,MAX_ADC_DATA);
	  if(nwords<=0) 
	    {
	      logMsg("ERROR: ADC Read Failed - Status 0x%x\n",nwords,0,0,0,0,0);
	      *dma_dabufp++ = 0xda000bad;
	      c792Clear(ADC_ID);
	    } 
	  else 
	    {
	      dma_dabufp += nwords;
	    }
	}
    }
  else
    {
      logMsg("ERROR: NO data in ADC  datascan = 0x%x, itimeout=%d\n",status,itimeout,0,0,0,0);
      c792Clear(ADC_ID);
    }

  *dma_dabufp++ = LSWAP(0xd00dd00d); /* Event EOB */

  tiSetOutputPort(0,0,0,0);


}
