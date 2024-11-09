/*************************************************************************
 *
 * f1tdc_list.c - Library of routines for the user to write for 
 *                readout and buffering of events from JLab F1TDC
 *                using a Linux VME controller
 *
 *
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

#include "linuxvme_list.c"  /* source required for CODA */
#include "tirLib.h"         /* library of TI routines */
#include "f1tdcLib.h"       /* library of f1tdc routines */

/* F1TDC Specifics */
extern int f1tdcA32Base;
int F1_SLOT;
extern int f1ID[20];
#define F1_ADDR 0xed0000

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
  vmeDmaConfig(2,3,0); 

  printf("rocDownload: User Download Executed\n");

}

void
rocPrestart()
{
  unsigned short iflag;
  int stat;

  /* Program/Init VME Modules Here */
  f1ConfigReadFile("cfg_norm.dat");
  
  /*   iflag = 0x0ee0; /* SDC Board address */ 
  iflag = 0x0; /* no SDC */
  iflag |= 4;  /* read from file */
  /*   iflag |= 2;  /* Normal Resolution, Trigger matching */
  printf("iflag = 0x%x\n",iflag);

  f1Init(F1_ADDR,0x0,1,iflag);
  F1_SLOT = f1ID[0];

  /* Setup F1TDC */
  /*   f1Clear(F1_SLOT); */
  /*   f1SetConfig(F1_SLOT,2,0xff); */
  f1EnableData(F1_SLOT,0xff);
  f1SetBlockLevel(F1_SLOT,1);
  /*   f1DisableBusError(F1_SLOT); */
  f1EnableBusError(F1_SLOT);

  /* Setup 1 microsec window and latency */
  /*   f1SetWindow(F1_SLOT,500,500,0xff); */
  
  f1Clear(F1_SLOT);

  /* lock the resolution using the TIR output bit */
  tirIntOutput(1<<3);
  tirIntOutput(0);

  /* wait for resolution to lock */
  usleep(50000);

  f1Status(F1_SLOT,0);

  printf("rocPrestart: User Prestart Executed\n");

}

void
rocGo()
{
  
  /* lock the resolution by using the SDC */
  /*   f1SDC_Sync(); */

}

void
rocEnd()
{

  /* F1TDC Event status - Is all data read out */
  f1Status(F1_SLOT,0);

  f1Reset(F1_SLOT,0);

  printf("rocEnd: Ended after %d events\n",tirGetIntCount());
  
}

void
rocTrigger(int arg)
{
  int ii, status, dma, count;
  int nwords;
  unsigned int datascan, tirval, vme_addr;
  int length,size;

  tirIntOutput(2);

  /* Insert trigger count  - Make sure bytes are ordered little-endian (LSWAP)*/
  *dma_dabufp++ = LSWAP(tirGetIntCount());

  /* Check for valid data here */
  for(ii=0;ii<100;ii++) 
    {
      datascan = f1Dready(F1_SLOT);
      if (datascan>0) 
	{
	  break;
	}
    }
  
  if(datascan>0) 
    {
      nwords = f1ReadEvent(F1_SLOT,dma_dabufp,500,1);

      if(nwords < 0) 
	{
	  printf("ERROR: in transfer (event = %d), status = 0x%x\n", tirGetIntCount(),nwords);
	  *dma_dabufp++ = LSWAP(0xda000bad);
	} 
      else 
	{
	  dma_dabufp += nwords;
	}
    } 
  else 
    {
      printf("ERROR: Data not ready in event %d\n",tirGetIntCount());
      *dma_dabufp++ = LSWAP(0xda000bad);
    }
  *dma_dabufp++ = LSWAP(0xda0000ff); /* Event EOB */

  tirIntOutput(0);

}
 
