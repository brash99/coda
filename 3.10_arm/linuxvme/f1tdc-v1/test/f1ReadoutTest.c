/*
 * File:
 *    f1tdcLibTest.c
 *
 * Description:
 *    Test JLab F1 TDC with GEFANUC Linux Driver
 *    and f1tdc library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tirLib.h"
#include "f1tdcLib.h"


void faGClearError();
extern int f1tdcA32Base;
extern int f1tdcA32Offset;

/* access tirLib global variables */
extern unsigned int tirIntCount;
int F1_SLOT;

#define F1TDC_ADDR 0xed0000

/* Interrupt Service routine */
void
mytirISR(int arg)
{
  unsigned short reg;
  int ii,stat;
  volatile unsigned int data[128];
  int nwrds;

  reg = tirReadData();
  if(tirIntCount%100 == 0) {
    printf("Got %d interrupts (Data = 0x%x)\n",tirIntCount,reg);
    for(ii=0;ii<100;ii++) {
      stat = f1Dready(F1_SLOT);
      if (stat>0) {
	break;
      }
    }
    if(stat>0) {
      f1PrintEvent(F1_SLOT,1);
    } else {
      printf (" no events\n");
    }
    
  } else {
    nwrds = f1ReadEvent(F1_SLOT,(unsigned int *)&data,128-2,0);
    if(nwrds>128 || nwrds<1)

      printf("nwrds = %d\n",nwrds);
  }
  tirIntOutput(1);
  tirIntOutput(0);
}

int 
main(int argc, char *argv[]) 
{
  int stat;
  extern int f1ID[20];
  unsigned short iflag;
  int status;

  printf("\nJLAB f1tdc Lib Tests\n");
  printf("----------------------------\n");
  
  if(vmeOpenDefaultWindows() != OK) {
    goto CLOSE;
  }
  
  tirIntInit(0,TIR_EXT_INT,1);
  tirIntStatus(1);

/*   f1ConfigReadFile("cfg_norm.dat"); */
  
  iflag = 0x0ee0; // SDC Board address
/*   iflag = 0; */
/*   iflag |= 4;  // read from file */
  iflag |= 2;  // Normal Resolution, Trigger matching
  printf("iflag = 0x%x\n",iflag);

  f1tdcA32Base   = 0x08000000;
  f1tdcA32Offset = 0x08000000;

  f1Init(F1TDC_ADDR,0x0,1,iflag);
  F1_SLOT = f1ID[0];

  // Setup F1TDC
  f1Clear(F1_SLOT);
  f1SetConfig(F1_SLOT,2,0xff);
  f1GEnableData(0xff);
  f1GSetBlockLevel(0);
/*   f1EnableBusError(F1_SLOT); */

  // Setup 1 microsec window and latency
  f1SetWindow(F1_SLOT,500,500,0xff);
  
  f1GClear();

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

  // lock the resolution using the TIR output bit
/*   tirIntOutput(1); */
/*   tirIntOutput(0); */

/*    lock the resolution by using the SDC */
  f1SDC_Sync();
  
  usleep(50000);

  f1GStatus(0);
  printf("Hit any key to enable Triggers...\n");
  getchar();


  /* Enable the TIR and clear the trigger counter */
  tirIntEnable(TIR_CLEAR_COUNT);

  printf("Hit any key to Disable TIR and exit.\n");
  getchar();

  tirIntDisable();

  tirIntDisconnect();

/* F1TDC Event status - Is all data read out */
  f1Status(F1_SLOT,0);

  f1Reset(F1_SLOT,0);
 CLOSE:

  tirIntStatus(1);

  vmeCloseDefaultWindows();

  exit(0);
}

