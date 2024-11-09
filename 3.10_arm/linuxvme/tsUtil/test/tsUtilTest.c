/*
 * File:
 *    tsUtilTest.c
 *
 * Description:
 *    Test Vme TS interrupts with jvme Linux Driver
 *    and tsUtil library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tsUtil.h"

#define TS_MODE 0

/* Interrupt Service routine */
void
mytsISR(int arg)
{
  unsigned int reg=0;

  reg = tsIntType();
  
  if((tsGetIntCount()%1000) == 0)
    printf("Got %d interrupts (Data = 0x%x)\n",tsGetIntCount(),reg);

  tsIntAck();
}


int 
main(int argc, char *argv[]) 
{

  int stat;

  printf("\nJLAB Trigger Supervisor Tests\n");
  printf("----------------------------\n");

  vmeOpenDefaultWindows();

  /* Set the TS structure pointer */
  tsInit((unsigned int)(TS_BASE_ADDR),0);

  tsReset(0);

  tsCsr2Set(TS_CSR2_LOCK_ALL);  /* ROC Lock mode on all BRANCHES */
  tsEnableInput(0xfff,0);       /* Enable all Trigger inputs in non-strobe mode */
  tsRoc(0,0,0,0);               /* Enable ACK 0,1 on BRANCH 1  and ACK 2 on Branch 2*/

  /* Synchronization programming */
  tsSync(100);                      /* schedule Sync every 100th physics trigger */
  tsCsr2Set(TS_CSR2_ENABLE_SYNC);   /* Enable Scheduled syncs */ 

  /* Set nominal Level 2/3 timer values */
  tsTimerWrite(TS_TIMER_L2A,0x05);   /* Level 2 Timer 40ns/count */
  tsTimerWrite(TS_TIMER_L3A,0x05);   /* Level 3 Timer 40ns/count */

  /* Front End Busy timer 40ns/count */
  tsTimerWrite(TS_TIMER_FB,250);         /* 250 = 10 microsec */   
  tsCsr2Set(TS_CSR2_ENABLE_FB_TIMER);    /* Enable FEB Timer */

  /*  Construct TS memory data  ---  in the following model, all trigger patterns 
      that form the memory address are assigned to trigger class 1.  For those 
      trigger patterns with a single hit, the ROC code is set to be the trigger 
      input number.  Otherwise, the ROC code is set to 0xE.  All LEVEL 1 ACCEPT 
      signals are asserted for every pattern.  */
   
  tsMemInit();

  /* Fix special cases - both inputs 1 and 2 firing - type 13 (0xd) 
     all L1 accept outputs firing 0xff03 */
  tsMemWrite(3,0xdff03);

  /* Set specific input prescale factors */
  tsPrescale(1,0);
  tsPrescale(2,0);

  tsStatus(1);

  int trig_count = tsScalRead(TS_SCALER_EVENT,0);
  printf("TS Event Scaler Count = %d\n",trig_count);

  stat = tsIntConnect(0,mytsISR,0,TS_MODE);
  if (stat != OK) {
    printf("ERROR: tsIntConnect failed: \n");
    goto CLOSE;
  } else {
    printf("INFO: Attached TS Interrupt\n");
  }

  printf("Hit any key to enable Triggers...\n");
  getchar();

  tsIntEnable(1);
  tsGo(1);  /* Enable TS and L1 */

  printf("Hit any key to Disable TIR and exit.\n");
  getchar();

  tsStop(1);

  trig_count = tsScalRead(TS_SCALER_EVENT,0);
  printf("TS Event Scaler Count = %d\n",trig_count);

  tsIntDisable();

  tsIntDisconnect();


 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

