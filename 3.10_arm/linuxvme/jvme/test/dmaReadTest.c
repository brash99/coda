/*
 * File:
 *    dmaReadTest.c
 *
 * Description:
 *    Test routine to perform DMA from "slave" module
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

#define BUFFER_SIZE 1024*20
#define NBUFFER     200

#define NBLOCKS  1

#define VME_ADDR 0x09000000

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

    printf("\nDMA Read Tests\n");
    printf("----------------------------\n");


    status = vmeOpenDefaultWindows();

    vmeSetDebugFlags(0x0);

    if(status != OK) {
      printf("vmeOpenDefaultWindows failed with code = 0x%x\n",status);
      goto CLOSE;
    }

    /* INIT tempeDma */
  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
    vmeDmaConfig(2,4,0);

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

    printf("Press return to perform DMA read from 0x%x\n",VME_ADDR);
    getchar();
    GETEVENT(vmeIN[0],0);

    // Perform read here 
    int retval;
    int nbytes = (64<<2)+4;
    printf("nbytes = %d\n",nbytes);
    retval = vmeDmaSend((unsigned long)dma_dabufp,VME_ADDR,nbytes);
    printf("retval = %d\n",retval);
    if(retval != 0)
      printf("bad\n");

    retval = vmeDmaDone();
    printf("retval = %d\n",retval);

    dma_dabufp += retval>>2;

    *dma_dabufp++ = LSWAP(0xd00dd00d);

    PUTEVENT(vmeOUT);


    int len, ii;
    DMANODE *outEvent;
      
    outEvent = dmaPGetItem(vmeOUT);
#ifdef DEBUG
    printf("outEvent->dmaHdl = 0x%x\n",(unsigned int)(outEvent->dmaHdl));
#endif
    if(outEvent != NULL) {
      len = outEvent->length;
/*       if(tirIntCount%100 == 0) { */
/* 	printf("Got %d interrupts (TIRdata = 0x%x) (len = %d)\n",tirIntCount,tirdata,len); */
	for(ii=0; ii<len; ii++) {
	  if((ii%5) == 0) printf("\n    ");
	  printf(" 0x%08x ",(unsigned int)LSWAP(outEvent->data[ii]));
	}
	printf("\n\n");
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

