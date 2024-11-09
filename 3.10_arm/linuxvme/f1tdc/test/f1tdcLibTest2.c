/*
 * File:
 *    f1tdcLibTest2.c
 *
 * Description:
 *    Test JLab F1 TDC with GEFANUC Linux Driver
 *    and f1tdc library
 *  This one tests the DMA functions
 *
 *
 */
#define _GNU_SOURCE

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tirLib.h"
#include "f1tdcLib.h"

void faGClearError();

extern int f1tdcA32Base;

/* access tirLib global variables */
extern unsigned int tirIntCount;
int F1_SLOT;

#define F1TDC_ADDR 0xed0000

unsigned long long int rdtsc(void)
{
/*    unsigned long long int x; */
   unsigned a, d;
   
   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((unsigned long long)a) | (((unsigned long long)d) << 32);;
}

// definitions for dmaPList 
#define MAX_NUM_EVENTS    1
#define MAX_SIZE_EVENTS   1024*100      /* Size in Bytes */
#define MAX_DMA_WORDS     1024*100/4       /* Must be less than MAX_SIZE_EVENTS/4 */

unsigned long long starttime=0,endtime=0;
unsigned long long time_now=0,time_prev=0;
double rate=0.; 
double rate_avg=0.;
double dma_rate=0.;
double dma_rate_avg=0.;
extern int f1ID[20];
unsigned short iflag;
int ievent=0;
int nevents=10;
unsigned long long time1=0,time2=0;
unsigned long long clockcalib=0;
DMA_MEM_ID vmeIN,vmeOUT;

void mytirISR(int arg);

int 
main(int argc, char *argv[]) 
{
  int status;
  GEF_UINT32 srcAddr=0x08000000;
  int nwrods=MAX_DMA_WORDS, dmaMode;
  int stat;
  extern int f1ID[20];
  unsigned short iflag;

  printf("\nJLAB f1tdc Lib Tests\n");
  printf("----------------------------\n");
  
  time1 = rdtsc();
  sleep(1);
  time2 = rdtsc();
  clockcalib=time2-time1;

  if(vmeOpenDefaultWindows() != OK) {
    goto CLOSE;
  }

  vmeDisableBERRIrq();
  
  tirIntInit(0,0,1);
  tirIntStatus(1);

  f1ConfigReadFile("cfg_norm.dat");
  
  iflag = 0x0ee0; // SDC Board address
/*   iflag = 0x0; // no SDC */
  iflag |= 4;  // read from file
/*   iflag |= 2;  // Normal Resolution, Trigger matching */
  printf("iflag = 0x%x\n",iflag);


  f1tdcA32Base   = 0x10000000;

  f1Init(F1TDC_ADDR,0x0,1,iflag);
  F1_SLOT = f1ID[0];

  // Setup F1TDC
/*   f1Clear(F1_SLOT); */
/*   f1SetConfig(F1_SLOT,2,0xff); */
  f1EnableData(F1_SLOT,0xff);
  f1SetBlockLevel(F1_SLOT,1);
/*   f1DisableBusError(F1_SLOT); */
  f1EnableBusError(F1_SLOT);

/*   // Setup 1 microsec window and latency */
  f1SetWindow(F1_SLOT,500,500,0xff);
  
  f1Clear(F1_SLOT);

  // lock the resolution using the TIR output bit
  tirIntOutput(1);
  tirIntOutput(0);

  f1SDC_Sync();

  usleep(50000);


/*   f1ConfigShow(F1_SLOT,0); */

  stat = tirIntConnect(TIR_INT_VEC, mytirISR, 0);
  if (stat != OK) {
    printf("ERROR: tirIntConnect failed: \n");
    goto CLOSE;
  } else {
    printf("INFO: Attached TIR Interrupt\n");
  }

/*   f1Status(F1_SLOT,0); */
/*   f1ChipStatus(F1_SLOT,0); */
  f1Status(F1_SLOT,0);



  dmaPartInit();

  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,3,0); 

  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",MAX_SIZE_EVENTS,MAX_NUM_EVENTS,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);

  dmaPStatsAll();

  dmaPReInitAll();

  printf("Hit any key to enable Triggers...\n");
  getchar();

/*   // lock the resolution by using the SDC */
/*   f1SDC_Sync(); */
  

  /* Enable the TIR and clear the trigger counter */
  tirIntEnable(TIR_CLEAR_COUNT);

  printf("Hit any key to Disable TIR and exit.\n");
  getchar();

  tirIntDisable();

  tirIntDisconnect();

/* F1TDC Event status - Is all data read out */
  f1Status(F1_SLOT,0);

  f1Reset(F1_SLOT,0);
     
  printf("************************************************************\n");
    
  dmaPFreeAll();
  dmaPStatsAll();

CLOSE:

  tirIntStatus(1);

  vmeCloseDefaultWindows();

  printf("average rate = %lf\t average dma rate = %lf\n",rate_avg,dma_rate_avg);

  exit(0);
}

/* Interrupt Service routine */
void
mytirISR(int arg)
{
  unsigned short tirdata;
  int ii,stat;
  int status;
  int pollstatus;

#ifdef DEBUG
  printf("event = %d\n",tirIntCount);
#endif
  ievent = tirIntCount;


  if(time_now) {
    time_prev=time_now;
  }
  time_now = rdtsc();
  if(time_prev) {
    rate = 1./(((double)(time_now-time_prev))/(double)clockcalib);
    rate_avg = (rate_avg*(ievent-1) + rate)/(ievent);
  }

  GETEVENT(vmeIN,ievent);

#ifdef DEBUG
  printf("beginning = 0x%x\t0x%x\n",
	 (unsigned int)dma_dabufp,(unsigned int)&(the_event->length));
  int diff ;
  diff = (int)dma_dabufp-(int)&(the_event->length);
  printf("now = 0x%x \t0x%x \t0x%x\n",
	 (unsigned int)dma_dabufp,(unsigned int)&(the_event->length),diff);
#endif

  *dma_dabufp++; // not sure why I need to increment the pointer first.
  // "Event header"
  *dma_dabufp++ = LSWAP(0xd00dd00d);

  // TIR data
  tirdata = tirReadData();
  *dma_dabufp++ = tirdata;


#ifdef DEBUG
  diff = (int)dma_dabufp-(int)&(the_event->data[0]);

  printf("vmeIN->part[0] = 0x%x\t the_event->part = 0x%x\n",
	 (unsigned int)vmeIN->part[0],(unsigned int)&the_event->part);
  printf("now = 0x%x \t0x%x \t0x%x\n",
	 (unsigned int)dma_dabufp,(unsigned int)&(the_event->length),diff);
  printf("data = 0x%x\n",
	 (unsigned int)&(the_event->data[0]));
  printf("the_event dma_hdl = 0x%x\n",(unsigned int)(the_event->dmaHdl));
      
  printf("Start DMA transfer\n");
#endif

  // check for data ready in the f1tdc
  for(ii=0;ii<100;ii++) {
    stat = f1Dready(F1_SLOT);
    if (stat>0) {
      break;
    }
  }
  if(stat>0) {
    *dma_dabufp++ = LSWAP(stat);
    *dma_dabufp++ = LSWAP(f1ReadCSR(F1_SLOT));
    starttime=rdtsc();

    pollstatus = f1ReadEvent(F1_SLOT,dma_dabufp,200,1);

    endtime=rdtsc();
    dma_rate = 1./(((double)(endtime-starttime))/(double)clockcalib);
    if(ievent>0) 
      dma_rate_avg = (dma_rate_avg*(ievent-1) + dma_rate)/(ievent);
#ifdef DEBUG
    printf("main: dma transfer time = %d\n",endtime-starttime);
    printf("pollstatus = %d\n",pollstatus);
#endif

    if(pollstatus < 0) {
      printf("ERROR: in transfer (event = %d), status = 0x%x\n", tirIntCount,pollstatus);
      *dma_dabufp++ = LSWAP(0xda000bad);
    } else {
      dma_dabufp += pollstatus;
    }
  } else {
    printf("ERROR: Data not ready in event %d\n",tirIntCount);
    *dma_dabufp++ = LSWAP(0xda000bad);
  }
  *dma_dabufp++ = LSWAP(0xd00dd00d);
      

  PUTEVENT(vmeOUT);

#ifdef DEBUG
  dmaPStatsAll();
#endif
  int len;
  DMANODE *outEvent;
      
  outEvent = dmaPGetItem(vmeOUT);
#ifdef DEBUG
  printf("outEvent->dmaHdl = 0x%x\n",(unsigned int)(outEvent->dmaHdl));
#endif
  if(outEvent != NULL) {
    len = outEvent->length;
    if(tirIntCount%10 == 0) {
      printf("Got %d interrupts (TIRdata = 0x%x)\n",tirIntCount,tirdata);
      for(ii=0; ii<len; ii++) {
	if((ii%5) == 0) printf("\n    ");
	printf(" 0x%08x ",(unsigned int)LSWAP(outEvent->data[ii]));
      }
      printf("\n\n");
    }
    dmaPFreeItem(outEvent);
  }else{
    logMsg("Error: no Event in vmeOUT queue\n",0,0,0,0,0,0);
  }
#ifdef DEBUG
  printf("\n\n");

  dmaPStatsAll();
  if(time_prev) 
    printf("rate = %lf\n",rate);
  printf("done with event %d\n\n",ievent);
#endif

  tirIntOutput(1);
  tirIntOutput(0);
}
