/*
 * File:
 *    sdLibTest.c
 *
 * Description:
 *    Quick program to Test the CTP library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tiLib.h"
#include "ctpLib.h"

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
  tiInit(0,TI_READOUT_EXT_POLL,TI_INIT_SKIP_FIRMWARE_CHECK);
  ctpInit(1);
  ctpStatus(1);

 CLOSE:

  vmeCloseDefaultWindows();

  exit(0);
}

