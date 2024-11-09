/*
 * File:
 *    f1FirmwareUpdate.c
 *
 * Description:
 *    JLab f1TDC firmware updating program for a single board.
 *
 *
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "f1tdcLib.h"

char *progName;
void Usage();

int
main(int argc, char *argv[]) 
{
  int iflag=0, stat=0, F1_SLOT=0;
  int inputchar=10;
  unsigned int f1_address=0;
  char *rbf_filename;
  int file_rev=0;

  printf("\nJLAB f1TDC Firmware Update\n");
  printf("----------------------------\n");
  
  progName = argv[0];
  
  if(argc<3)
    {
      printf(" ERROR: Must specify two arguments\n");
      Usage();
      exit(-1);
    }
  else
    {
      rbf_filename = argv[1];
      f1_address = (unsigned int) strtoll(argv[2],NULL,16)&0xffffffff;
    }

  stat = f1FirmwareReadFile(rbf_filename);
  if(stat != OK)
    exit(-1);

  /* Get the version type from the filename */
  if(strcasestr(rbf_filename,"v2")!=NULL)
    {
      file_rev = 2;
    }
  else if(strcasestr(rbf_filename,"v3")!=NULL)
    {
      file_rev = 3;
    }
  else
    {
      printf(" ERROR: Unable to determine f1TDC Version from rbf filename (%s)!\n",
	     rbf_filename);
      printf(" Quitting without update.\n");
      exit(-1);
    }

  vmeSetQuietFlag(1);
  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    goto CLOSE;

  iflag |= F1_SRSRC_SOFT;
  iflag |= F1_TRIGSRC_SOFT;
  iflag |= F1_CLKSRC_INT;
  iflag |= F1_IFLAG_NOINIT;
  iflag |= F1_IFLAG_NOFWCHECK;

  stat = f1Init(f1_address,0,1,iflag);
  if(stat != OK)
    goto CLOSE;

  F1_SLOT = f1Slot(0);

  int if1=0, rev1=0;
  unsigned int cfw=0;
  extern int nf1tdc;
  extern int f1Rev[(F1_MAX_BOARDS+1)];

  printf("\n");
  for(if1=0; if1<nf1tdc; if1++)
    {
      cfw = f1GetFirmwareVersion(f1Slot(if1),0);
      rev1 = (f1Rev[f1Slot(if1)] & F1_VERSION_BOARDREV_MASK)>>8;
      printf(" %2d: V%d FPGA Firmware Version: 0x%04x\n",f1Slot(if1),rev1,cfw);
    }
  printf("\n");

  if(rev1!=file_rev)
    {
      printf(" ERROR: f1TDC Version Type (%d) not consistent with File Version Type (%d)!\n",
	     rev1,file_rev);
      printf(" Quitting without update.\n");
      goto CLOSE;
    }

  printf(" Will update firmware with file: \n   %s\n",rbf_filename);
  printf(" for f1TDC with VME adderss = 0x%08x\n", f1_address);

  printf(" <ENTER> to continue... or q and <ENTER> to quit without update.\n");

  inputchar = getchar();

  if((inputchar == 113) ||
     (inputchar == 81))
    {
      printf(" Quitting without update.\n");
      goto CLOSE;
    }

  vmeBusLock();

  stat = f1FirmwareEraseEPROM(F1_SLOT);
  if(stat != OK)
    {
      vmeBusUnlock();
      goto CLOSE;
    }

  stat = f1FirmwareDownloadConfigData(F1_SLOT,1);
  if(stat != OK)
    {
      vmeBusUnlock();
      goto CLOSE;
    }

  stat = f1FirmwareVerifyDownload(F1_SLOT,1);
  if(stat != OK)
    {
      vmeBusUnlock();
      goto CLOSE;
    }

  vmeBusUnlock();

 CLOSE:

  vmeCloseDefaultWindows();

  return OK;
}


void
Usage()
{
  printf("\n");
  printf("%s <firmware rbf file> <f1TDC VME ADDRESS>\n",progName);
  printf("\n");

}
