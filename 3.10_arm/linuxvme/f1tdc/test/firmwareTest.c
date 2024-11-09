/*
 * File:
 *    firmwareTest.c
 *
 * Description:
 *    Test Firmware routines for the f1TDC
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "f1tdcLib.h"

#define F1TDC_ADDR 0xe70000

int
main(int argc, char *argv[]) 
{
  int iflag=0;
  int stat=0;
  int F1_SLOT=0;

  vmeOpenDefaultWindows();

  iflag |= F1_SRSRC_SOFT;
  iflag |= F1_TRIGSRC_SOFT;
  iflag |= F1_CLKSRC_INT;
  iflag |= F1_IFLAG_NOINIT;
  iflag |= F1_IFLAG_NOFWCHECK;

  stat = f1Init(F1TDC_ADDR,0x060000,2,iflag);
  if(stat != OK)
    goto CLOSE;

  F1_SLOT = f1Slot(0);

  f1GStatus(1);

  stat = f1FirmwareReadFile("../firmware/f1tdc_v3_11.rbf");
  if(stat != OK)
    goto CLOSE;

  stat = f1FirmwareGEraseEPROM();
  if(stat != OK)
    goto CLOSE;

  stat = f1FirmwareGDownloadConfigData();
  if(stat != OK)
    goto CLOSE;

  stat = f1FirmwareGVerifyDownload();
  if(stat != OK)
    goto CLOSE;

 CLOSE:

  vmeCloseDefaultWindows();

  return OK;
}
