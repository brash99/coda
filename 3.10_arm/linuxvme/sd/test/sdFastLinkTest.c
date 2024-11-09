/*
 * File:
 *    sdFastLinkTest.c
 *
 * Description:
 *    Quick program to Test the SD library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tiLib.h"
#include "tsLib.h"
#include "sdLib.h"

extern volatile struct TI_A24RegStruct  *TIp;
extern int tiA24Offset;

struct thisSD
{
  /* 0x0000 */          unsigned int blankSD0[(0x3C00-0x0000)/4];
  volatile unsigned int reg[20];
};


struct fastSD
{
  volatile unsigned int reg[20];
};

int
main(int argc, char *argv[])
{

  int stat;
  int iloop, ireg;
  volatile struct thisSD *i2cSD = NULL;
  volatile struct fastSD *fastSD = NULL;
  unsigned long base=0;

  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    {
      goto CLOSE;
    }

  /* Set the TI structure pointer */
  tiInit((21<<19),TI_READOUT_EXT_POLL,1);
  sdInit(0);
  sdStatus();

  base = (unsigned long) &TIp->boardID;

  i2cSD  = (struct thisSD *)(&TIp->SWB[0]);
  fastSD = (struct fastSD *)(&TIp->SWB_status[0]);

  /* Start it up */
  printf("Starting up fast link (writing 1 to bit 9, reg 0)\n");
  sdSetTiFastLink(1);
  taskDelay(60);
  printf("Stopping fast link (writing 0 to bit 9, reg 0)\n");
  sdSetTiFastLink(0);
  taskDelay(60);

  for(iloop=0; iloop < 1; iloop++)
    {

      /* Loop over all regs */
      printf("Reading registers\n");
      for(ireg = 0 ; ireg <= 0x17; ireg++)
	{
	  printf("0x%02x:  fast (0x%06lx)= 0x%04x\n",
		 ireg,
		 ((ireg%2)==0)?
		 ((unsigned long)&fastSD->reg[ireg])-base:
		 ((unsigned long)&fastSD->reg[ireg/2])-base+2,
		 ((ireg%2)==0)?
		 vmeRead32(&fastSD->reg[ireg/2]) & 0xFFFF:
		 vmeRead32(&fastSD->reg[ireg/2])>>16 & 0xFFFF);
/* 	  printf("0x%02x:  i2c (0x%06x)= 0x%04x    fast (0x%06x)= 0x%04x\n", */
/* 		 ireg,  */
/* 		 ((unsigned int)&i2cSD->reg[ireg])-base, */
/* 		 vmeRead32(&i2cSD->reg[ireg]), */
/* 		 ((ireg%2)==0)? */
/* 		 ((unsigned int)&fastSD->reg[ireg/2])-base: */
/* 		 ((unsigned int)&fastSD->reg[ireg/2])-base+2, */
/* 		 ((ireg%2)==0)? */
/* 		 vmeRead32(&fastSD->reg[ireg/2]) & 0xFFFF: */
/* 		 vmeRead32(&fastSD->reg[ireg/2])>>16 & 0xFFFF); */
	}

    }



 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}
