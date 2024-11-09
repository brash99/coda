/*
 * File:
 *    pulserTest.c
 *
 * Description:
 *    Test JLab F1 TDC v3 pulser with the f1TDC library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "f1tdcLib.h"
#include "f1tm.h"

extern int nf1tdc;
extern int f1tdcA32Base;
/* access tirLib global variables */
int F1_SLOT;

#define F1TDC_ADDR (0xed0000)
#define F1TM_ADDR  0xe100

/* Interrupt Service routine */
void mytiISR(int arg);

int 
main(int argc, char *argv[]) 
{
  int stat;
  extern int f1ID[20];
  unsigned short iflag;
/*   int status; */
  int itdc;

  printf("\nJLAB f1tdc pulser Tests\n");
  printf("----------------------------\n");
  
  if(vmeOpenDefaultWindows() != OK) {
    goto CLOSE;
  }


  f1tmInit(F1TM_ADDR,0);
  f1tmSetPulserPeriod(0xffff);
  f1tmSetTriggerDelay(50); 
  f1tmSetInputMode(F1TM_HITSRC_INT, F1TM_TRIGSRC_INT);
  f1tmEnablePulser();
  f1tmStatus();

  //////////////////////////////
  // F1 SETUP
  //////////////////////////////
  f1ConfigReadFile("../fromEd/cfg_v2_0hs_3125.dat");
  
  iflag  = 0;
  iflag |= F1_SRSRC_SOFT;   // SyncReset from Software
  iflag |= F1_TRIGSRC_SOFT; // Trigger from Software/Internal
  iflag |= F1_CLKSRC_VXS;  // Clock from VXS

  printf("iflag = 0x%x\n",iflag);

  f1tdcA32Base   = 0x09000000;
/*   f1tdcA32Offset = 0x08000000; */

  f1Init(F1TDC_ADDR,(1<<19),1,iflag);
  F1_SLOT = f1ID[0];
  f1SyncReset(0);


  f1ResetPulser(0);
  f1SetPulserDAC(0, 3, 400);
  f1SetPulserTriggerDelay(0, 500);
  f1GEnableData(0xff);
  f1GEnable();
  f1GStatus(0);
  f1tmClearPulserScaler(1);
  f1tmClearPulserScaler(2);

  printf(" 1: %d\n",f1tmGetPulserScaler(1));
  printf(" 2: %d\n",f1tmGetPulserScaler(2));

  printf("Hit enter to start pulses\n");
  getchar();

  int i;
  for(i=0; i<1000; i++)
    {
      f1SoftPulser(0, 2);
      taskDelay(1);
      if((i%10)==0) 
	{
	  printf("."); fflush(stdout);
	}
    }
  printf("\n");

  printf(" 1: %d\n",f1tmGetPulserScaler(1));
  printf(" 2: %d\n",f1tmGetPulserScaler(2));
  
  f1GDisable();
  f1GStatus(0);


 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}
