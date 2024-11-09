/*
 * File:
 *    tsCrateInit.c
 *
 * Description:
 *    Initialization the TS Crate (TS + TDs)
 *
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include "jvme.h"
#include "tsLib.h"
#include "tdLib.h"
#include "sdLib.h"

int
main(int argc, char *argv[])
{

  printf("\nJLAB TS Crate Init\n");
  printf("----------------------------\n");

  vmeSetQuietFlag(1);
  vmeOpenDefaultWindows();

  vmeBusLock();

  /* Initialize the TS */
  if (tsInit(0, 0, TS_INIT_SKIP_FIRMWARE_CHECK) != OK)
    {
      printf("ERROR in initialization of TS.  Quitting\n");
      goto CLOSE;
    }

  tsStatus(1);

  /* Initialize the TDs */
  if (tdInit( (3 << 19), (1 << 19), 18, TD_INIT_SKIP_FIRMWARE_CHECK) != OK)
    {
      printf("ERROR in initialization of TDs.  Quitting\n");
      goto CLOSE;
    }

  tdGStatus(1);

  /* Initialize the SD */
  if (sdInit (SD_INIT_IGNORE_VERSION) != OK)
    {
      printf("ERROR in initialization of TDs.  Quitting\n");
      goto CLOSE;
    }

  sdSetActiveVmeSlots(tdSlotMask());

  sdStatus(1);

CLOSE:
  vmeBusUnlock();

  vmeCloseDefaultWindows();

  exit(0);
}


/*
  Local Variables:
  compile-command: "make -k -B tsCrateInit"
  End:
 */
