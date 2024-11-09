/*
 * File:
 *    dmaReadTest.c
 *
 * Description:
 *    Test routine to test the dmaPList library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

#define BUFFER_SIZE 1024*600
#define NBUFFER     400

#define NBLOCKS  1

#define MAX_DATA    37               /* Max number of words in event */

#define PRINTOUT

DMA_MEM_ID vmeIN[NBLOCKS],vmeOUT;

/*! Buffer node pointer */
extern DMANODE *the_event;
/*! Data pointer */
extern unsigned int *dma_dabufp;

int 
main(int argc, char *argv[]) {

    int status;
    unsigned int vmeaddress;
    int stat;

    printf("\ndmaPList library Tests\n");
    printf("----------------------------\n");

    printf("Size of DMANODE = %d\n",sizeof(DMANODE));

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

    
    long ii, length,size;
    printf("GETEVENT\n");
    for(iblock=0; iblock<200; iblock++)
      {
	GETEVENT(vmeIN[0],0);
	printf("dma_dabufp0 = 0x%lx\n",(long)dma_dabufp);
	
	/* Insert some data here */
	*dma_dabufp++ = LSWAP(0xda000022);
	for(ii=0; ii<MAX_DATA; ii++) 
	  {
	    *dma_dabufp++ = LSWAP(ii);
	  }
	*dma_dabufp++ = LSWAP(0xda0000ff);
	PUTEVENT(vmeOUT);
      }

    /* Check if the event length is larger than expected */
    length = (((long)(dma_dabufp) - (long)(&the_event->length))) -4 ;
    size = the_event->part->size  - sizeof(DMANODE);
    if(length>size) 
      {
	printf("rocLib: ERROR: Event length > Buffer size (%ld > %ld).  Event %d\n",
	       length,size,(int)the_event->nevent);
      }

    int first,second;
    
    printf("length = 0x%lx   size = 0x%lx\n", length,size);
    printf("sizeof = 0x%lx\n",sizeof(DMA_MEM_PART));
    printf("sizeof = 0x%lx\n",sizeof(DMANODE));
    printf("dma_dabufp0 = 0x%lx\n",(long)dma_dabufp);
    first = (long)(&the_event->length);
    printf("the_event->length = 0x%lx\n",(long)(the_event->length));
    
/*     GETEVENT(vmeIN[0],1); */
/*     second = (long)(&the_event->length); */
/*     printf("dma_dabufp1 = 0x%lx\n",(long)dma_dabufp); */
/*     printf("the_event->length = 0x%lx\n",(long)(the_event->length)); */
/*     printf("diff = 0x%x\n",second-first); */
/*     printf("PUTEVENT\n"); */
/*     PUTEVENT(vmeOUT); */
    dmaPStatsAll();

    long len;
    DMANODE *outEvent;
      

    outEvent = dmaPGetItem(vmeOUT);
    goto SKIP;
#ifdef DEBUG
    printf("outEvent->dmaHdl = 0x%x\n",(unsigned int)(outEvent->dmaHdl));
#endif
    if(outEvent != NULL) {
      len = outEvent->length;
/*       if(tirIntCount%100 == 0) { */
/* 	printf("Got %d interrupts (TIRdata = 0x%x) (len = %d)\n",tirIntCount,tirdata,len); */
#ifdef PRINTOUT
      printf("len = 0x%lx (%ld)\n",len, len);
	for(ii=0; ii<len; ii++) {
	  if((ii%5) == 0) printf("\n    ");
	  printf(" 0x%08x ",(unsigned int)LSWAP(outEvent->data[ii]));
	}
	printf("\n\n");
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

 SKIP:

    printf("going to free\n");
    int iemp=0;
    while(!dmaPEmpty(vmeOUT))
      {
	iemp++;
	printf("Empty it\n");
	dmaPFreeItem(dmaPGetItem(vmeOUT));
	if(iemp>NBUFFER)
	  break;
      }
    dmaPStatsAll();
    printf("iemp = %d\n",iemp);

    dmaPFreeAll();
    printf("going to stats\n");
    dmaPStatsAll();
    

    goto CLOSE;


 CLOSE:


    printf("going to close\n");
    status = vmeCloseDefaultWindows();
    if (status != OK)
    {
      printf("vmeCloseDefaultWindows failed: code 0x%08x\n",status);
      return -1;
    }

    exit(0);
}

