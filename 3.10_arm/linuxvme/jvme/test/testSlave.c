/*
 * File:
 *    testSlave.c
 *
 * Description:
 *    Test Slave window properties
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

#define BUFFER_SIZE 1024*1024*3
#define NBUFFER     1

extern void *a32slave_window;
DMA_MEM_ID vmeIN,vmeOUT;

unsigned int *data;

int 
main(int argc, char *argv[]) 
{

  int status;
  unsigned int addr=0x18000000;
  
  vmeOpenDefaultWindows();
  
  status = vmeOpenSlaveA32(addr,0x00400000);
  if(status != OK)
    {
      printf("%s: ERROR: failed to open slave window (%d)\n",
	     __FUNCTION__,status);
      return ERROR;
    }
  dmaPUseSlaveWindow(1);

  /* INIT dmaPList */
  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",BUFFER_SIZE,NBUFFER,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);

  dmaPStatsAll();
  
  dmaPReInitAll();

  GETEVENT(vmeIN,0);
/*   data = (unsigned int *)a32slave_window; */
  data = (unsigned int*)dma_dabufp;

  printf("a32slavewindow = 0x%08x\n",(unsigned int*)a32slave_window);
  printf("dma_dabufp     = 0x%08x\n",(unsigned int*)dma_dabufp);
  printf("data           = 0x%08x\n",(unsigned int*)data);

  printf("vmeDmaLocalToVmeAdrs(dma_dabufp)  = 0x%08x\n",
	 vmeDmaLocalToVmeAdrs((unsigned int)dma_dabufp));
  printf("vmeDmaLocalToPhysAdrs(dma_dabufp) = 0x%08x\n",
	 vmeDmaLocalToPhysAdrs((unsigned int)dma_dabufp));

  printf("Press <Enter> to look at data\n");
  getchar();
  int idata=0, iter=0;
  int inputchar=10;
  while(1 && (inputchar==10))
    {
      for(idata=0+4*8*iter; idata<4*8*(iter+1); idata++)
	{
	if((idata%4)==0) printf("\n");
	printf("%5d: 0x%08x ",idata,(unsigned int)data[idata]);
	}
      iter++;
      printf("\n\n");

      printf("<Enter> for next trigger, 1 and <Enter> to End\n");
      inputchar = getchar();
      printf("inputchar = %d\n",inputchar);
      if(inputchar==49) break; /* 1 */
      if(inputchar==50) {
	iter=0;
/* 	if(iter<0) iter=0; */
	inputchar=10;
      }
    }
  


  status = vmeCloseA32Slave();
  
  vmeCloseDefaultWindows();
  
  exit(0);
}

