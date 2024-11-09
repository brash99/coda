/*
 * File:
 *    tdTest.c
 *
 * Description:
 *    Test JLab Vme Driver
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

  printf("wait 60..\n");
  for(i=0; i<1; i++)
    taskDelay(60);
  printf("Done\n");

/*   printf("start sleep\n"); */
/*   sleep(1); */
/*   printf("Done\n"); */
  

  exit(0);
}

