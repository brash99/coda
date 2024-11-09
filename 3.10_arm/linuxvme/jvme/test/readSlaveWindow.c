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
#include "../jlabgef.h"
#include "common_struct.h"

volatile struct vmecpumem *vmecpu;

int 
main(int argc, char *argv[]) 
{
  unsigned int laddr;
  unsigned int taddr = 0x09000000;
  int stat;
  int inputchar=10;
  int res;
  int rdata;

  vmeOpenDefaultWindows();

  res = vmeBusToLocalAdrs(0x09,(char *)taddr,(char **)&laddr);
  if (res != 0) 
    {
      printf("%s: ERROR in vmeBusToLocalAdrs(0x%x,0x%x,&laddr) \n",
	     __FUNCTION__,0x09,laddr);
      return(ERROR);
    }

  vmecpu = (struct vmecpumem *)laddr;
  res = vmeMemProbe((char *) &(vmecpu->id), 4, (char *)&rdata);

  printf("res = %d,  rdata = 0x%x\n", res, LSWAP(rdata));
  vmeCloseDefaultWindows();
  exit(0);

/*   vmeSetDebugFlags(0xffffffff); */

  while(1 && (inputchar==10))
    {
	
      printf("Grab mutex lock\n");
      vmeBusLock();
      printf("Press return to unlock mutex..\n");
      getchar();
      vmeBusUnlock();
	
      printf("I think it's unlocked now... return to kill it\n");
      inputchar = getchar();
    }

 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

