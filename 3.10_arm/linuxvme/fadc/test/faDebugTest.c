/*
 * File:
 *    faDebugTest
 *
 * Description:
 *    Test new debug routines
 *
 *
 */


#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "jvme.h"
#include "fadcLib.h"

#define FADC_ADDR (4<<19)

int
main(int argc, char *argv[])
{

  int status;

  vmeSetQuietFlag(1);
  status = vmeOpenDefaultWindows();
  if(status != OK)
    goto CLOSE;

  status = faInit((unsigned int)(FADC_ADDR), 1<< 19,2,FA_INIT_SKIP | FA_INIT_SKIP_FIRMWARE_CHECK);
  if(status != OK)
    goto CLOSE;

  faPrintDebugOutput(0);
 CLOSE:


  vmeCloseDefaultWindows();
  printf("\n");
  printf("--------------------------------------------------------------------------------\n");


  exit(0);
}

/*
  Local Variables:
  compile-command: "make -k -B faDebugTest"
  End:
 */
