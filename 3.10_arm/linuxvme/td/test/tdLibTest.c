/*
 * File:
 *    tdLibTest.c
 *
 * Description:
 *    Test TD Library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tdLib.h"
/* #include "remexLib.h" */

int
main(int argc, char *argv[])
{

  int stat;

  printf("\nJLAB TD Library Test\n");
  printf("----------------------------\n");

  vmeOpenDefaultWindows();
  stat = tdInit(0, 0, 0, TD_INIT_SKIP_FIRMWARE_CHECK| TD_INIT_NO_INIT);

  if(stat == ERROR)
    goto CLOSE;

  stat = tdCheckAddresses();

  if(stat == ERROR)
    goto CLOSE;

  tdAddSlaveMask(0, 0xFF);

  tdGStatus(0);

  tdGFiberBusyStatus(1);

  tdResetFiber(0);

  /* tdSlaveStatus(0, 1); */

  /* tdPrintFiberFifo(0, 1); */
 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k -B tdLibTest"
  End:
 */
