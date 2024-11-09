/*
 * File:
 *    testPP.c
 *
 * Description:
 *    Test PayloadPort/VmeSlots routines
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"

int 
main(int argc, char *argv[]) 
{
  int i;


  printf("20 Slot crate\n");
  vmeSetMaximumVMESlots(20);

  for(i=1; i<=18; i++)
    printf("PP %2d = VME %2d\n",i,vxsPayloadPort2vmeSlot(i));

  for(i=1; i<=20; i++)
    printf("VME %2d = PP %2d\n",i,vmeSlot2vxsPayloadPort(i));


  printf("21 Slot crate\n");
  vmeSetMaximumVMESlots(21);

  for(i=1; i<=18; i++)
    printf("PP %2d = VME %2d\n",i,vxsPayloadPort2vmeSlot(i));

  for(i=1; i<=21; i++)
    printf("VME %2d = PP %2d\n",i,vmeSlot2vxsPayloadPort(i));

  exit(0);
}
