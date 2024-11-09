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
  int stat, i=0;
  int *dump=NULL;

  vmeBusCreateLockShm();
  dump = malloc(2*1024*1024);

  printf("Grab mutex lock\n");

  vmeBusLock();
  printf("crash it\n");
/*   for(i=0; i<10000000; i--) */
  while(1)
    {
      i++;
      stat = dump[i] = 0x2901;
    }
  printf("no crash\n");
/*   vmeBusKillLockShm(0); */

  exit(0);
}
