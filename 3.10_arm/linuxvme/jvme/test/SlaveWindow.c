/*
 * File:
 *    SlaveWindow.c
 *
 * Description:
 *    Test JLab Vme Driver
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../jvme.h"
#include "common_struct.h"

volatile struct vmecpumem *vmecpu;
extern void *a32slave_window;

int 
main(int argc, char *argv[]) 
{
  unsigned int laddr = 0x18000000;
  unsigned int taddr = 0x0ed0;
  int stat;
  int inputchar=10;

  vmeOpenDefaultWindows();

  vmeSetDebugFlags(0xffffffff);

  stat = vmeOpenSlaveA32(0x18000000,0x40000);
  if(stat != GEF_STATUS_SUCCESS)
    {
      goto CLOSE;
    }

  printf("Read: 0x%08x\n", vmeBusRead32(0x09, laddr));
  vmeBusWrite32(0x09, laddr, 0x44442222);
  printf("Read: 0x%08x\n", vmeBusRead32(0x09, laddr));
  vmeWrite32(&a32slave_window, 0x12345678);
  printf("   Read: 0x%08x\n", vmeRead32(&a32slave_window));
  
  stat = vmeCloseA32Slave();

 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

