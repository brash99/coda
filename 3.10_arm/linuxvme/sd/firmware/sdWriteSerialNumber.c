/*----------------------------------------------------------------------------*
 *  Copyright (c) 2013        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Update serial number info for the Signal Distribution (SD) module.
 *
 *----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "unistd.h"
#ifdef VXWORKS
#include "vxCompat.h"
#else
#include "jvme.h"
#endif
#include "tiLib.h"
#include "tsLib.h"
#include "sdLib.h"

extern void sdFirmwarePrintSpecs();
int
main(int argc, char *argv[])
{
  int res=0;
  unsigned int serial_number=0;
  unsigned int hall_board_version=0;
  unsigned int firmware_version=0;
  int inputchar=10;
  char line[32];

  printf("\nJLAB Signal Distribution (SD) Serial Number Update\n");
  printf("-----------------------------------------------------------\n");
  fflush(stdout);

  firmware_version = 0xA9;
  vmeSetQuietFlag(1);
  res = vmeOpenDefaultWindows();
  if(res!=OK)
    goto CLOSE;

  res = tiInit(0,0,1|TI_INIT_SKIP_FIRMWARE_CHECK);
  if(res!=OK)
    {
      goto CLOSE;
    }

  res = sdInit(SD_INIT_IGNORE_VERSION);
  if(res!=OK)
    goto CLOSE;

  printf("-----------------------------------------------------------\n\n");

  printf("Serial Number (decimal from 1 to 255):\n");
  fflush(stdout);
  fgets(line, 32, stdin);
  serial_number = (unsigned int) strtoll(line,NULL,10)&0xffffffff;


  if(serial_number<1 || serial_number>255)
    {
      printf(" ERROR: Invalid Serial Number (%d).  Must be 1-255\n",serial_number);
      exit(-1);
    }


  printf("Hall/Board Version (hex from 0x01 to 0xFF):\n");
  fflush(stdout);
  fgets(line, 32, stdin);
  hall_board_version = (unsigned int) strtoll(line,NULL,16)&0xffffffff;

  if(hall_board_version<0x1 || hall_board_version>0xFF)
    {
      printf(" ERROR: Invalid Assigned Hall and Board Version (0x%x).\n  Must be 0x01-0xFF\n"
	     ,hall_board_version);
      exit(-1);
    }

  printf("\n");
  printf("\n");
  printf("-----------------------------------------------------------\n");
  printf("Serial Number (dec)             = %4d\n",serial_number);
  printf("Assigned Hall and Board Version = 0x%02X\n",hall_board_version);
  printf("Initial Firmware Version        = 0x%02X\n",firmware_version);
  printf("-----------------------------------------------------------\n");
  printf("\n");

  sdGetSerialNumber(NULL);

  printf(" Please verify these items before continuing... \n");
  printf("\n");
  fflush(stdout);

  printf(" <ENTER> to continue... or q and <ENTER> to quit without update\n");
  fflush(stdout);
  inputchar = getchar();

  if((inputchar == 113) ||
     (inputchar == 81))
    {
      printf(" Exiting without update\n");
      res=1;
      goto CLOSE;
    }

  sdFirmwareFlushFifo();
  fflush(stdout);
  sdFirmwareWriteSpecs(0x7F0000,serial_number,hall_board_version,firmware_version);
  sleep(3);
  fflush(stdout);
  sdFirmwarePrintSpecs();
  fflush(stdout);
  sdFirmwareFreeMemory();
  fflush(stdout);

 CLOSE:

  vmeCloseDefaultWindows();

  printf("\n");
  if(res==ERROR)
    printf(" ******** SD Serial number Update ended in ERROR! ******** \n");
  else if (res==OK)
    {
      printf(" ++++++++ SD Serial Number Update Successful! ++++++++\n");
    }


  printf("\n");
  return OK;
}
