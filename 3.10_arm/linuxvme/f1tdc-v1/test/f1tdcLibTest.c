/*
 * File:
 *    f1tdcLibTest.c
 *
 * Description:
 *    Test JLab F1 TDC with GEFANUC Linux Driver
 *    and f1tdc library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "f1tdcLib.h"

int
main(int argc, char *argv[])
{
  extern int nf1tdc;

  printf("\nJLAB f1tdc Lib Tests\n");
  printf("----------------------------\n");

  if (vmeOpenDefaultWindows() != OK)
    {
      goto CLOSE;
    }

  f1ConfigReadFile("f1_cfg_righHRS_HighResSync.dat");
  f1Init(0xee1000,0,1,4);
  
  usleep(30000);
  f1Clear(0);

  f1GStatus(1);
  f1Status(0,0);

CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k -B f1tdcLibTest"
  End:
 */
