/*
 * File:
 *    f1GFirmwareUpdate.c
 *
 * Description:
 *    JLab f1TDC firmware updating program for all found modules in crate.
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
extern int nf1tdc;

int
main(int argc, char *argv[]) 
{
  int iflag=0, stat=0;
  int inputchar=10;
  unsigned int f1_address=(3<<19);
  char *rbf_filename;
  int file_rev=0;

  printf("\nJLAB f1TDC Firmware Update\n");
  printf("----------------------------\n");
  
  progName = argv[0];
  
  if(argc<2)
    {
      printf(" ERROR: Must specify one argument\n");
      Usage();
      exit(-1);
    }
  else
    {
      rbf_filename = argv[1];
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

  f1Init(f1_address,(1<<19),18,iflag);
  if(nf1tdc < 1)
    {
      printf(" ERROR: No f1TDCs found\n");
      goto CLOSE;
    }

  int if1=0, rev=0, rev1=0;
  int revError=0;
  unsigned int cfw=0;
  extern int f1Rev[(F1_MAX_BOARDS+1)];

  printf("\n");
  for(if1=0; if1<nf1tdc; if1++)
    {
      cfw = f1GetFirmwareVersion(f1Slot(if1),0);
      rev = (f1Rev[f1Slot(if1)] & F1_VERSION_BOARDREV_MASK)>>8;
      printf(" %2d: V%d FPGA Firmware Version: 0x%04x\n",f1Slot(if1),rev,cfw);

      if(if1==0)
	rev1=rev;
      else
	if(rev!=rev1) revError=1;
      
    }
  printf("\n");

  if(revError)
    {
      printf(" ERROR: Inconsistent f1TDC revisions!\n");
      printf(" Quitting without update.\n");
      goto CLOSE;
    }

  if(rev1!=file_rev)
    {
      printf(" ERROR: f1TDC Version Type (%d) not consistent with File Version Type (%d)!\n",
	     rev1,file_rev);
      printf(" Quitting without update.\n");
      goto CLOSE;
    }

  printf(" Will update firmware with file: \n   %s\n",rbf_filename);

  printf(" <ENTER> to continue... or q and <ENTER> to quit without update.\n");

  inputchar = getchar();

  if((inputchar == 113) ||
     (inputchar == 81))
    {
      printf(" Quitting without update.\n");
      goto CLOSE;
    }

  vmeBusLock();
  stat = f1FirmwareGEraseEPROM();
  if(stat != OK)
    {
      vmeBusUnlock();
      goto CLOSE;
    }

  stat = f1FirmwareGDownloadConfigData();
  if(stat != OK)
    {
      vmeBusUnlock();
      goto CLOSE;
    }
  stat = f1FirmwareGVerifyDownload();
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
  printf("%s <firmware rbf file>\n",progName);
  printf("\n");

}
