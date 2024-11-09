/*
 * File:
 *    gtpLibTest.c
 *
 * Description:
 *    Quick program to Test the GTP library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tiLib.h"
#include "sdLib.h"
#include "sspLib.h"
#include "gtpLib.h"

int 
main(int argc, char *argv[]) 
{

  int stat, iFlag=0;
  unsigned int val=0;

  vmeSetQuietFlag(1);
  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    {
      goto CLOSE;
    }

  /* Set the TI structure pointer */
  tiInit(0,TI_READOUT_EXT_POLL,0);
  
  /* Set the sync delay width to 0x40*32 = 2.048us */
  tiSetSyncDelayWidth(0x54, 0x40, 1);
  /* Init the SD library so we can get status info */

  stat = sdInit(0);
  tiSetBusySource(TI_BUSY_SWB,1);
  sdSetActiveVmeSlots(0);
  sdStatus(0);

  iFlag = SSP_INIT_MODE_DISABLED; /* Disabled, initially */

  sspInit(8<<19, 0, 1, iFlag); /* Scan for, and initialize all SSPs in crate */

  sspGStatus(0);


  if(gtpInit(1,NULL)!=OK)
    goto CLOSE;
  gtpCheckAddresses();
  gtpSetClockSource(GTP_CLK_CTRL_INT);
  gtpSetSyncSource(GTP_SD_SRC_SEL_SYNC);
  gtpSetTrig1Source(GTP_SD_SRC_SEL_TRIG1);
  gtpEnablePayloadPortMask(1<<(vmeSlot2vxsPayloadPort(8)-1));
  gtpStatus(GTP_STATUS_SHOW_PAYLOAD_INPUT);

  printf("<Enter> to \"Prestart\"");
  getchar();

  tiClockReset();
  taskDelay(2);
  tiTrigLinkReset();
  tiEnableVXSSignals();

  /* Check the health of the vmeBus Mutex.. re-init if necessary */
  vmeCheckMutexHealth(10);

  gtpSetClockSource(1);

  /* Configure the SSP modules */
  iFlag  = SSP_INIT_MODE_VXS;
  iFlag |= SSP_INIT_FIBER_ENABLE_MASK;     /* Enable all fiber ports */
  iFlag |= SSP_INIT_GTP_FIBER_ENABLE_MASK; /* Enable all fiber port data to GTP */
  int issp;
  extern int nSSP;
  for(issp=0; issp<nSSP; issp++)
    {
      sspSetMode(sspSlot(issp),iFlag,1);
      /* Direct SSP Internal "Trigger 0" to LVDS0 Output on Front Panel */
      sspSetIOSrc(sspSlot(issp), SD_SRC_LVDSOUT0, SD_SRC_SEL_TRIGGER0);
    }
  gtpEnablePayloadPortMask(1<<(vmeSlot2vxsPayloadPort(8)-1));

  sspGStatus(0);
  gtpStatus(GTP_STATUS_SHOW_PAYLOAD_INPUT);

  printf("%s: Sending sync as TI master\n",__FUNCTION__);
  sleep(1);
  tiSyncReset(1); /* set the Block Level */
  taskDelay(2);
  tiSetBlockLevel(1);

  printf("<Enter> to \"Go\"");
  getchar();
  int blockLevel = tiGetCurrentBlockLevel();
  printf("rocGo: Block Level set to %d\n",blockLevel);

  sspGStatus(0);
  gtpStatus(GTP_STATUS_SHOW_PAYLOAD_INPUT);

 CLOSE:

  gtpSocketClose();
  vmeCloseDefaultWindows();

  exit(0);
}

