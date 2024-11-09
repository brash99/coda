/*
 * File:
 *    remexLibTest.c
 *
 * Description:
 *    Receiver (executer) side test
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
/* #include "jvme.h" */
#include "remexLib.h"

int 
main(int argc, char *argv[]) 
{
  printf("\n REMEX receiver test \n");
  remexAddName("robot");
/*   remexSetCmsgServer(""); */
  remexSetRedirect(0);

  remexSetCmsgPassword("DAQDEVEL");
  remexSetCmsgServer("dubhe");
  remexInit("megrez",1);


  printf("Press Enter to Close\n");
  getchar();

  remexClose();

  return 0;
}
