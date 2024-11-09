/*************************************************************************
 *
 *  c792_linux_list.c - Library of routines for the user to write for
 *                      readout and buffering of events using
 *                      a Linux VME controller.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     400
#define MAX_EVENT_LENGTH   1024*10      /* Size in Bytes */

/* Define Interrupt source and address */
#define TIR_SOURCE
#define TIR_ADDR 0x0ed0
#define TIR_MODE TIR_EXT_POLL

#define ADC_ID 0
#define MAX_ADC_DATA 34

#include "linuxvme_list.c"
#include "c792Lib.h"

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
  vmeDmaConfig(1,3,0); 

  c792Init(0xa10000,0,1,0);

  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  unsigned short iflag;
  int stat;

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

  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
rocEnd()
{
  int status, count;
  
  printf("rocEnd: Ended after %d events\n",tirGetIntCount());
  
}

void
rocTrigger(int arg)
{
  int ii, status, dma, count;
  int nwords;
  unsigned int datascan, tirval, vme_addr;
  int length,size;
  int itimeout=0;

/*   tirIntOutput(2); */

  *dma_dabufp++ = LSWAP(tirGetIntCount()); /* Insert Event Number */

  /* Check if an Event is available */

  while(itimeout<1000)
    {
      itimeout++;
      status = c792Dready(ADC_ID);
      if(status>0) break;
    }
  if(status > 0) 
    {
      if(tirGetIntCount() %1000==0)
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

/*   tirIntOutput(0); */

}
