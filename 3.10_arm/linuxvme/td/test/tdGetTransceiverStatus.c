/*
 * File:
 *    tdGetTransceiverStatus.c
 *
 * Description:
 *    Test routines that get the transceiver status.
 *    I like good program names.
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tdLib.h"

int
main(int argc, char *argv[])
{

  int slot, stat;

  if (argc > 1)
    {
      slot = atoi(argv[1]);
      if (slot < 1 || slot > 22)
	{
	  printf("invalid slot... using 21");
	  slot = 21;
	}
    }
  else
    slot = 21;

  printf("\nJLAB TD.  Get Transceiver Status for slot = %d\n", slot);
  printf
    ("\n--------------------------------------------------------------------------------\n");
  vmeOpenDefaultWindows();


  vmeBusLock();
  stat = tdInit(slot, 0, 1, TD_INIT_SKIP_FIRMWARE_CHECK | TD_INIT_NO_INIT);
  if (stat != OK)
    goto CLOSE;

  tdCheckAddresses();

  tdGStatus(0);

  tdPrintTranceiverStatus(slot);

CLOSE:

  vmeBusUnlock();
  vmeCloseDefaultWindows();
  printf
    ("\n--------------------------------------------------------------------------------\n");

  exit(0);
}


/*
  Local Variables:
  compile-command: "make -k -B tdGetTransceiverStatus"
  End:
 */
