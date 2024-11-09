/*
 * File:
 *    sdLibTest.c
 *
 * Description:
 *    Quick program to Test the SD library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tiLib.h"
#include "tsLib.h"
#include "sdLib.h"

int 
main(int argc, char *argv[]) 
{

  int stat;

  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    {
      goto CLOSE;
    }

  /* Set the TI structure pointer */
  tiInit(0, TI_READOUT_EXT_POLL, TI_INIT_NO_INIT|TI_INIT_SKIP_FIRMWARE_CHECK);
  sdInit(0);
  sdStatus();

 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

