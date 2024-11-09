/*
 * File:
 *    writeSlave.c
 *
 * Description:
 *    Perform a DMA to the Slave created with testSlave.
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

#define DATASIZE (128+(60/4))

unsigned int *data;
extern GEF_VME_BUS_HDL vmeHdl;    
GEF_VME_ADDR dest_addr =
  {
    .upper = 0x00000000,
    .lower = 0x18000000,
    .addr_space = GEF_VME_ADDR_SPACE_A32,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_2eSST,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_320,
    .broadcast_id = 0,
    .flags = 0
  };

int 
main(int argc, char *argv[]) 
{

  int status;
  unsigned int addr=0x18000000;
  GEF_VME_DMA_HDL dma_hdl;
  GEF_MAP_PTR map_ptr;
  int startnum =0;

  vmeOpenDefaultWindows();
  status = (int)gefVmeAllocDmaBuf(vmeHdl,DATASIZE<<2,&dma_hdl,(GEF_MAP_PTR)&map_ptr);
  printf("gefVmeAllocDmaBuf status = 0x%08x\n",status);

  data = (unsigned int *)map_ptr;

  int idata=0;
  int inputchar=10;
  while(1 && (inputchar==10))
    {
      startnum=rand()/32768;
      for(idata=0; idata<DATASIZE; idata++)
	{
	  data[idata+(60/4)] = startnum+(idata<<8);
	}
      for(idata=0; idata<32; idata++)
	{
	  if((idata%4)==0) printf("\n");
	  printf("%5d: 0x%08x ",idata,(unsigned int)data[idata+(60/4)]);
	}
      printf("\n\n");

      status = (int)gefVmeWriteDmaBuf (dma_hdl,
				       0,
				       &dest_addr,
				       DATASIZE<<2);

      printf("gefVmeWriteDmaBuf status = 0x%08x\n",status);
      printf("<Enter> for next trigger, 1 and <Enter> to End\n");
      inputchar = getchar();
    }
  gefVmeFreeDmaBuf(dma_hdl);
  vmeCloseDefaultWindows();
  
  exit(0);
}

