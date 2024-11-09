/*
 * File:
 *    testLock.c
 *
 * Description:
 *    Test Bus Locking SHM mechanism 
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
  unsigned int laddr;
  unsigned int taddr = 0x0ed0;
  int stat;
  int inputchar=10;

  vmeBusCreateLockShm();
  vmeCheckMutexHealth(10);
  while(1 && (inputchar==10))
    {
	
      printf("Grab mutex lock\n");
      if(vmeBusTryLock()!=OK)
	{
	  printf("Try again\n");
	  getchar();
	  continue;
	}
      printf("Press return to unlock mutex..\n");
      getchar();
/*       vmeCheckMutexHealth(0); */
      vmeBusUnlock();
	
      printf("I think it's unlocked now... return to kill it\n");
      inputchar = getchar();
    }

 CLOSE:

  vmeBusKillLockShm(0);

  exit(0);
}
