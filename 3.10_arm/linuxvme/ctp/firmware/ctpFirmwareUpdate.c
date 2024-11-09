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
 *     Firmware update for the Crate Trigger Processor (CTP) module.
 *
 *----------------------------------------------------------------------------*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef VXWORKS
#include "vxCompat.h"
#else
#include "jvme.h"
#endif
#include "tiLib.h"
#include "ctpLib.h"


char *programName;
enum ifpga {U1, U3, U24, NFPGA};
const char *fpga_names[NFPGA] = {
    "U1",
    "U3",
    "U24"
};

#ifndef VXWORKS
static void ctpFirmwareUsage();
#endif

int
#ifdef VXWORKS
ctpFirmwareUpdate(unsigned int arg_vmeAddr, char *arg_filename)
#else
main(int argc, char *argv[])
#endif
{
  int stat;
  int ifpga;
  char *filename;
  char inputchar[16];
  int  inputint;
  unsigned int vme_addr=0;
  int reboot=1;
  
  printf("\nCTP firmware update via VME\n");
  printf("----------------------------\n");

#ifdef VXWORKS
  programName = __FUNCTION__;

  vme_addr = arg_vmeAddr;
  filename = arg_filename;
#else
  programName = argv[0];

  if(argc!=3)
    {
      printf(" ERROR: Must specify two arguments\n");
      ctpFirmwareUsage();
      return(-1);
    }
  else
    {
      vme_addr = (unsigned int) strtoll(argv[1],NULL,16)&0xffffffff;
      filename = argv[2];
    }

  vmeSetQuietFlag(1);
  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    goto CLOSE;
#endif

  /* Read in firmware file */
  stat = ctpReadFirmwareFile(filename);
  if(stat==ERROR)
    {
      printf(" Error reading firmware file: %s\n",filename);
      goto CLOSE;
    }

  ifpga = ctpFirmwareChipFromFpgaID(0);
  if( ifpga == ERROR )
    {
      printf(" ERROR: Did not obtain FPGA type from firmware file.\n");
      printf("        Please specific FPGA type\n");
      for(ifpga=U1; ifpga<NFPGA; ifpga++)
	{
	  printf("\t\t%d - %s\n",ifpga,fpga_names[ifpga]);
	}
      printf("        or q and <ENTER> to quit without update\n");
      printf("\n");

    REPEAT_FPGA:
      printf(" (#/q): ");
      scanf("%s",(char *)&inputchar);

      inputint = (int) strtol(inputchar,NULL,10);

      if((strcmp(inputchar,"q")==0) || (strcmp(inputchar,"Q")==0))
	{
	  printf("--- Exiting without update ---\n");
	  goto CLOSE;
	}
      else if((inputint==U1)||(inputint==U3)||(inputint==U24))
	{
	  ifpga = inputint;
	}
      else
	{
	  goto REPEAT_FPGA;
	}
    }

  stat = tiInit(vme_addr,0,1);
  if(stat != OK)
    {
      printf("\n");
      printf("*** Failed to initialize TI ***\n");
      goto CLOSE;
    }

  stat = ctpInit(CTP_INIT_IGNORE_VERSION);
  if(stat != OK)
    {
      printf("\n");
      printf("*** Failed to initialize CTP ***\n");
      goto CLOSE;
    }

  char ctpSN[20];
  int  fwversion=0;

  stat = ctpGetSerialNumber((char **)&ctpSN);
  if(stat>0)
    printf(" CTP Serial Number : %s\n",ctpSN);
  else
    printf("ERROR getting serial number (returned %d)\n",stat);

  fwversion = ctpGetFirmwareVersion(ifpga);
  printf(" %s: Firmware Version = 0x%04x\n",
	 fpga_names[ifpga],fwversion);

  printf(" Update with file: %s ",filename);
  if(ctpFirmwareChipFromFpgaID(0)!=ERROR)
    printf(" (Rev = 0x%04x)\n\n",ctpFirmwareRevFromFpgaID(0));
  else
    printf("\n\n");


  printf("Proceed with the CTP Firmware update?\n");
 REPEAT:
  printf(" (y/n): ");
    scanf("%s",(char *)inputchar);

    if((strcmp(inputchar,"q")==0) || (strcmp(inputchar,"Q")==0) ||
       (strcmp(inputchar,"n")==0) || (strcmp(inputchar,"N")==0) )
      {
	printf("--- Exiting without update ---\n");
	goto CLOSE;
      }
    else if((strcmp(inputchar,"y")==0) || (strcmp(inputchar,"Y")==0))
      {
	printf("--- Continuing with update ---\n");
      }
    else
      goto REPEAT;
  
  stat = ctpFirmwareUpload(ifpga,reboot);
  if(stat != OK)
    {
      printf("\n");
      printf("*** CTP Firmware Update Failed ***\n");
      goto CLOSE;
    }
  else
    {
      printf("\n");
      printf("--- CTP Firmware Update Succeeded ---\n");
    }
 CLOSE:

#ifndef VXWORKS
  vmeCloseDefaultWindows();
#endif
  printf("\n");

  return OK;
}

#ifndef VXWORKS
static void
ctpFirmwareUsage()
{
  printf("\n");
  printf("%s <TI VME Address (A24)> <firmware mcs file>>\n",programName);
  printf("\n");

}
#endif
