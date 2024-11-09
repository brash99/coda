/*
 * File:
 *    jvmeTest.c
 *
 * Description:
 *    Test JLab Vme Driver
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "jvme.h"

int 
main(int argc, char *argv[])
{
  uint32_t vmeAddr = 0x01000000 | (6<<19);
  
  vmeOpenDefaultWindows();
  
  printf("0x%08x\n",vmeBusRead32(0x09, vmeAddr));
  vmeBusWrite32(0x89, vmeAddr, 0x10);
  vmeBusWrite32(0x89, vmeAddr, 0x10);
  vmeBusWrite32(0x89, vmeAddr, 0x10);
  vmeBusWrite32(0x89, vmeAddr, 0x10);
  
  vmeCloseDefaultWindows();
  
  exit(0);
}

