/*
 * File:
 *    tirLibTest.c
 *
 * Description:
 *    Test Vme TI interrupts with GEFANUC Linux Driver
 *    and TIR library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tirLib.h"

/* access tirLib global variables */
extern unsigned int tirIntCount;

/* Interrupt Service routine */
void
mytirISR(int arg)
{
  volatile unsigned short reg;
  

/*   if(tirIntCount%1000 == 0) */
/*     printf("Got %d interrupts (Data = 0x%x)\n",tirIntCount,reg); */

  tirIntOutput(1);
  int i;
/*   for(i=0;i<10000;i++) */
/*     { */
/*       reg = tirReadData(); */
/* /\*       tirIntOutput(1); *\/ */
/*     } */

/*   tirIntOutput(0); */

  /* tirIntOutput(1); */
/*   for(i=0;i<100;i++) */
/*     { */
/*       reg = tirReadData(); */
/* /\*       tirIntOutput(0); *\/ */
/* /\*       tirIntOutput(1); *\/ */
/*     } */

  tirIntOutput(0);

  if(tirIntCount%1==0)
    printf("Received %d triggers\n",tirIntCount);

}


int 
main(int argc, char *argv[]) {

    int stat;

    printf("\nJLAB Trigger Interface Tests (0x%x)\n",SSWAP(0x1234));
    printf("----------------------------\n");

    vmeOpenDefaultWindows();

/*     gefVmeSetDebugFlags(vmeHdl,0x0); */
    /* Set the TIR structure pointer */
    tirIntInit((unsigned int)(TIR_DEFAULT_ADDR),TIR_EXT_POLL,1);

    stat = tirIntConnect(TIR_INT_VEC, mytirISR, 0);
    if (stat != OK) {
      printf("ERROR: tirIntConnect failed \n");
      goto CLOSE;
    } else {
      printf("INFO: Attached TIR Interrupt\n");
    }


    printf("Hit any key to enable Triggers...\n");
    getchar();

    /* Enable the TIR and clear the trigger counter */
    tirIntEnable(TIR_CLEAR_COUNT);

    printf("Hit any key to Disable TIR and exit.\n");
    getchar();

    tirIntDisable();

    tirIntDisconnect();

 CLOSE:
    tirIntStatus(1);

    vmeCloseDefaultWindows();

    exit(0);
}

