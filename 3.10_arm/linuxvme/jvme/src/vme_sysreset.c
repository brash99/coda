/*
 * File:
 *    vme_sysreset.c
 *
 * Description:
 *    Assert a SYSRESET on the VME Bus.
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
  
  int stat;

  vmeOpenDefaultWindows();

  vmeBusLock();
  printf("Press return to assert SYSRESET\n");
  getchar();
  stat = vmeSysReset();
  printf("vmeSysReset returned 0x%x\n",stat);
  vmeBusUnlock();

  vmeCloseDefaultWindows();

  exit(0);
}

