/*
 * File:
 *    sspFirmwareUpdate.c
 *
 * Description:
 *    JLab SSP firmware updating
 *     Single board
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "sspLib.h"

char *progName;

void
Usage();

int 
main(int argc, char *argv[]) {

    int status;
    int stat=0;
    char *fw_filename;
    int inputchar=10;
    unsigned int ssp_address=0;

    printf("\nJLAB SSP firmware update\n");
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
	fw_filename = argv[1];
	ssp_address = (unsigned int) strtoll(argv[2],NULL,16)&0xffffffff;
      }

    vmeSetQuietFlag(1);
    status = vmeOpenDefaultWindows();
    if(status<0)
      {
	printf(" Unable to initialize VME driver\n");
	exit(-1);
      }

    stat = sspInit(ssp_address, 0, 1, 
		   SSP_INIT_SKIP_SOURCE_SETUP | SSP_INIT_SKIP_FIRMWARE_CHECK);
    if(stat<0)
      {
	printf(" Unable to initialize SSP.\n");
	goto CLOSE;
      }
    
    unsigned int cfw = sspGetFirmwareVersion(sspSlot(0));
    printf("%2d: Current Firmware Version: 0x%x\n",
	   sspSlot(0),cfw);

    printf(" Will update firmware");

    printf(" with file: \n   %s\n",fw_filename);
    printf(" for SSP with VME address = 0x%08x\n",ssp_address);

    printf(" <ENTER> to continue... or q and <ENTER> to quit without update\n");

    inputchar = getchar();

    if((inputchar == 113) ||
       (inputchar == 81))
      {
	printf(" Exitting without update\n");
	goto CLOSE;
      }

    sspFirmwareUpdateVerify(sspSlot(0), fw_filename);

    goto CLOSE;

 CLOSE:


    status = vmeCloseDefaultWindows();
    if (status != GEF_SUCCESS)
    {
      printf("vmeCloseDefaultWindows failed: code 0x%08x\n",status);
      return -1;
    }

    exit(0);
}


void
Usage()
{
  printf("\n");
  printf("%s <firmware .bin file> <SSP VME ADDRESS>\n",progName);
  printf("\n");

}
