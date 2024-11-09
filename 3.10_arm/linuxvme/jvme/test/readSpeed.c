/*
 * File:
 *    readSpeed.c
 *
 * Description:
 *    Test routine to test the read speed of the buffer allocated for DMA
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

#define BUFFER_SIZE 1024*200
#define NBUFFER     1

#define NBLOCKS  1

#define MAX_DATA    20*1024               /* Max number of words in event */


DMA_MEM_ID vmeIN[NBLOCKS],vmeOUT;

unsigned long long int read_time_accum=0;
unsigned int n_read_time=0;

int 
main(int argc, char *argv[]) {

    int status;
    unsigned int vmeaddress;
    int stat;
    volatile unsigned int other_buffer[BUFFER_SIZE];

    printf("\nreadSpeed jvme library Tests\n");
    printf("----------------------------\n");

    vmeSetQuietFlag(1);
    status = vmeOpenDefaultWindows();

    vmeSetDebugFlags(0x0);

    if(status != OK) {
      printf("vmeOpenDefaultWindows failed with code = 0x%x\n",status);
      goto CLOSE;
    }

    /* INIT dmaPList */
    dmaPFreeAll();
    int iblock;
    char blockname[50];
    for(iblock=0; iblock<NBLOCKS; iblock++)
      {
	sprintf(blockname,"vmeIN%d",iblock);
	printf("blockname = %s\n",blockname);
	vmeIN[iblock]  = dmaPCreate(blockname,BUFFER_SIZE,NBUFFER,0);
      }
    vmeOUT = dmaPCreate("vmeOUT",0,0,0);
    
    dmaPStatsAll();

    dmaPReInitAll();

    printf("GETEVENT\n");
    GETEVENT(vmeIN[0],0);

    int ii, length,size;
    /* Insert some data here */
    *dma_dabufp++ = LSWAP(0xda000022);
    for(ii=0; ii<MAX_DATA-2; ii++) 
      {
	*dma_dabufp++ = LSWAP(ii);
      }
    *dma_dabufp++ = LSWAP(0xda0000ff);


    /* Check if the event length is larger than expected */
    length = (((int)(dma_dabufp) - (int)(&the_event->length))) -4 ;
    size = the_event->part->size  - sizeof(DMANODE);
    if(length>size) 
      {
	printf("rocLib: ERROR: Event length > Buffer size (%d > %d).  Event %d\n",
	       length,size,(int)the_event->nevent);
      }

/*     int first,second; */
    
/*     printf("length = 0x%x   size = 0x%x\n", length,size); */
/*     printf("sizeof = 0x%x\n",sizeof(DMA_MEM_PART)); */
/*     printf("sizeof = 0x%x\n",sizeof(DMANODE)); */
/*     printf("dma_dabufp0 = 0x%x\n",(int)dma_dabufp); */
/*     first = (int)(&the_event->length); */
/*     printf("the_event->length = 0x%x\n",(int)(&the_event->length)); */
    
/*     GETEVENT(vmeIN[0],1); */
/*     second = (int)(&the_event->length); */
/*     printf("dma_dabufp1 = 0x%x\n",(int)dma_dabufp); */
/*     printf("the_event->length = 0x%x\n",(int)(&the_event->length)); */
/*     printf("diff = 0x%x\n",second-first); */
/*     printf("PUTEVENT\n"); */
    PUTEVENT(vmeOUT);


    int len;
    DMANODE *outEvent;
    unsigned long long int timer0, timer1;
    timer0=rdtsc();
    sleep(1);
    timer1=rdtsc();
    printf("sleep(1) = %lld\n",timer1-timer0);
      
    outEvent = dmaPGetItem(vmeOUT);
#ifdef DEBUG
    printf("outEvent->dmaHdl = 0x%x\n",(unsigned int)(outEvent->dmaHdl));
#endif
    if(outEvent != NULL) {
      len = outEvent->length;
/*       if(tirIntCount%100 == 0) { */
/* 	printf("Got %d interrupts (TIRdata = 0x%x) (len = %d)\n",tirIntCount,tirdata,len); */
#ifdef PRINTOUT
	for(ii=0; ii<len; ii++) {
	  if((ii%5) == 0) printf("\n    ");
	  printf(" 0x%08x ",(unsigned int)LSWAP(outEvent->data[ii]));
	}
	printf("\n\n");
#else
	timer0=rdtsc();
	for(ii=0;ii<len;ii++)
	  {
	    other_buffer[ii] = (unsigned int)LSWAP(outEvent->data[ii]);
	  }
	timer1=rdtsc();
	read_time_accum = (timer1-timer0);
#endif
/*       } */
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

    printf("read_time_accum = %lld\n",read_time_accum);
    printf("len = %d\n",len);

    dmaPFreeAll();
    dmaPStatsAll();
    

    goto CLOSE;


 CLOSE:


    status = vmeCloseDefaultWindows();
    if (status != OK)
    {
      printf("vmeCloseDefaultWindows failed: code 0x%08x\n",status);
      return -1;
    }

    exit(0);
}

