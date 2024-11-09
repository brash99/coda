/*
 * File:
 *    fadcLibTest.c
 *
 * Description:
 *    Test JLab Flash ADC with GEFANUC Linux Driver
 *    and fadc library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "fadcLib.h"

#define FADC_ADDR (3<<19)

extern int fadcA32Base;
extern int fadcID[FA_MAX_BOARDS];
extern int nfadc;
int 
main(int argc, char *argv[]) {

    GEF_STATUS status;
    int i,FA_SLOT;
    char sn[20];

    printf("\nJLAB fadc Lib Tests\n");
    printf("----------------------------\n");

    status = vmeOpenDefaultWindows();
    vmeSetQuietFlag(1);

    fadcA32Base=0x09000000;
    /* Set the FADC structure pointer */
    faInit((unsigned int)(FADC_ADDR),(1<<19),18, 0x25);
/*     faCheckAddresses(0); */
/*     faStatus(0,0); */

/*     printf("Control : 0x%08x   Proc : 0x%08x\n", */
/* 	   faGetCtrlFPGAData(0), */
/* 	   faGetProcFPGAData(0)); */

    faGetCtrlFPGAVoltage(0, 0, 1);
    faGetCtrlFPGAVoltage(0, 1, 1);

/*     getchar(); */
/*     for(i = 0; i<nfadc; i++) */
/*       { */
/* 	FA_SLOT = faSlot(i); */
/* 	if(faGetSerialNumber(FA_SLOT,(char **)&sn,0)>0) */
/* 	  printf("%2d: Serial Number = %s\n", */
/* 		 FA_SLOT,sn); */
/* 	else */
/* 	  printf("ERROR READING SERIAL NUMBER\n"); */
/* 	faReset(FA_SLOT,0); */

/*       } */

    faSetProcMode(0, 9, 1000, 500,
		  -3, 3, 2,
		  4, 250, 2);
/*     faSetChannelEnableMask(0,0xffff); */

/*     faSetChannelDisable(0,0); */
/*     faSetChannelDisable(0,1); */
    faGStatus(0);

/*     getchar(); */
    /* faStatus(0,0); */

/*     getchar(); */
/*     faStatus(0,0); */


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

