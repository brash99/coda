/**
 * @mainpage
 * <pre>
 *----------------------------------------------------------------------------*
 *  Copyright (c) 2010        Southeastern Universities Research Association, *
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
 *     Status and Control library for the JLAB Signal Distribution
 *     (SD) module using an i2c interface from the JLAB Trigger
 *     Interface/Supervisor (TI/TS) module.
 *
 *----------------------------------------------------------------------------
 * <pre> */

#include <stdio.h>
#ifdef VXWORKS
#include <vxWorks.h>
#include <sysLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <vxLib.h>
#endif
#include "jvme.h"
#include "sdLib.h"
#include "tiLib.h"
#include "tsLib.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include  <pthread.h>

#define DEVEL

/* Mutex to guard TI read/writes */
pthread_mutex_t   sdMutex = PTHREAD_MUTEX_INITIALIZER;
#define SDLOCK     if(pthread_mutex_lock(&sdMutex)<0) perror("pthread_mutex_lock");
#define SDUNLOCK   if(pthread_mutex_unlock(&sdMutex)<0) perror("pthread_mutex_unlock");

/* This is the SD base relative to the TI base VME address */
#define SDBASE 0x40000

/* Global Variables */
volatile struct SDStruct  *SDp=NULL;    /* pointer to SD memory map */
static int sdTestMode=0;                /* 1 if SD Jumper set to "test" mode */

/* Firmware updating variables */
static unsigned char *progFirmware=NULL;
static size_t progFirmwareSize=0;
/* Maximum firmware size = 1 MB */
#define SD_MAX_FIRMWARE_SIZE 1024*1024

/* External TI/TS Local Pointers */
extern volatile struct TI_A24RegStruct *TIp;
extern volatile struct TS_A24RegStruct *TSp;


/**
 * @defgroup Config Initialization/Configuration
 * @defgroup Status Status
 * @defgroup FWUpdate Firmware Update
 * @defgroup Test Signal testing
 * @defgroup Deprec Deprecated - To be removed
 */

/**
  @ingroup Config
  @brief Initialize the Signal Distribution Module

  @param flag Initialization bitmask
    - bit0 : Ignore module version

  @return OK if successful, otherwise ERROR
*/
int
sdInit(int flag)
{
  unsigned long tiBase=0, sdBase=0;
  unsigned int version=0;
  int testStatus=0;
  int fIgnoreVersion=0;

  /* First check if we're using a TI */
  if(TIp==NULL)
    {
      /* Now check to see if we're using a TS */
      if(TSp==NULL)
	{
	  printf("%s: ERROR: TI or TS not initialized.  Unable to initialize SD.\n",__FUNCTION__);
	  return ERROR;
	}
      else
	{
	  tiBase = (unsigned long)TSp;
	  sdBase = (unsigned long)&(TSp->SWB[0]);
	}
    }
  else
    {
      tiBase = (unsigned long)TIp;
      sdBase = (unsigned long)&(TIp->SWB[0]);
    }

  if(flag & SD_INIT_IGNORE_VERSION)
    {
      fIgnoreVersion=1;
      printf("%s: INFO: Initialization without respecting Library-Firmware version\n",
	     __FUNCTION__);
    }

  /* Do something here to verify that we've got good i2c to the SD..
     - maybe just check the status of the clk A and B */
  /* Verify that the SD registers are in the correct space for the TI I2C */

  if( (sdBase-tiBase) != SDBASE)
    {
      printf("%s: ERROR: SD memory structure not in correct VME Space!\n",
	     __FUNCTION__);
      printf("   current base = 0x%lx   expected base = 0x%lx\n",
	     sdBase-tiBase, (unsigned long)SDBASE);
      return ERROR;
    }

  SDp = (struct SDStruct *)sdBase;

  SDLOCK;
  version = vmeRead32(&SDp->version);
  testStatus = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_TEST_RESET)>>15;
  SDUNLOCK;

  if(version == 0xffff)
    {
      printf("%s: ERROR: Unable to read SD version (returned 0x%x)\n",
	     __FUNCTION__,version);

      SDp = NULL;
      return ERROR;
    }

  if(version < SD_SUPPORTED_FIRMWARE)
    {
      if(fIgnoreVersion)
	{
	  printf("%s: WARN: SD Firmware Version (0x%x) not supported by this driver.\n",
		 __FUNCTION__,version);
	  printf("           Firmware version 0x%x required. (Ignored)\n",SD_SUPPORTED_FIRMWARE);
	}
      else
	{
	  printf("%s: ERROR: SD Firmware Version (0x%x) not supported by this driver.\n",
		 __FUNCTION__,version);
	  printf("           Firmware version 0x%x required.\n",SD_SUPPORTED_FIRMWARE);
	  SDp = NULL;
	  return ERROR;
	}
    }

  printf("%s: SD (version 0x%x) initialized at Local Base address 0x%lx\n",
	 __FUNCTION__,version,(unsigned long) SDp);
  if(testStatus)
    {
      sdTestMode=1;
      printf("  INFO: **** TEST JUMPER IS INSTALLED ****\n");
    }
  else
    sdTestMode=0;

  return OK;
}

/**
   @ingroup Status
   @brief  Display status of SD registers

   @param rflag Registers flag
     if >0, Show raw register output

   @returns OK if successful, otherwise ERROR
*/
int
sdStatus(int rflag)
{
  unsigned int system, status, payloadPorts, tokenPorts,
    busyoutPorts, trigoutPorts,
    busyoutStatus, trigoutStatus;
  unsigned int version, csrTest;
  unsigned long tiBase, sdBase;
  int ii=0, ibegin=0, iend=0;
  int showVMEslots=1;
  unsigned int vmeslotmask;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  system        = vmeRead32(&SDp->system);
  status        = vmeRead32(&SDp->status);
  payloadPorts  = vmeRead32(&SDp->payloadPorts);
  tokenPorts    = vmeRead32(&SDp->tokenPorts);
  busyoutPorts  = vmeRead32(&SDp->busyoutPorts);
  trigoutPorts  = vmeRead32(&SDp->trigoutPorts);
#ifdef OLDMAP
  status2       = vmeRead32(&SDp->status2);
#endif
  busyoutStatus = vmeRead32(&SDp->busyoutStatus);
  trigoutStatus = vmeRead32(&SDp->trigoutStatus);
  version       = vmeRead32(&SDp->version);
  csrTest       = vmeRead32(&SDp->csrTest);

  /* First check if we're using a TI */
  if(TIp==NULL)
    {
      /* Now check to see if we're using a TS */
      if(TSp==NULL)
	{
	  printf("%s: ERROR: TI or TS not initialized.\n",__FUNCTION__);
	  return ERROR;
	}
      else
	{
	  tiBase = (unsigned long)TSp;
	  sdBase = (unsigned long)&(TSp->SWB[0]);
	}
    }
  else
    {
      tiBase = (unsigned long)TIp;
      sdBase = (unsigned long)&(TIp->SWB[0]);
    }

  SDUNLOCK;

  if(showVMEslots==1)
    {
      ibegin=3; iend=21;
    }
  else
    {
      ibegin=1; iend=17;
    }

  printf("\n");
  printf("STATUS for SD at TI (Local) base address 0x%08lx (0x%08lx) \n",sdBase-tiBase,sdBase);
  printf("--------------------------------------------------------------------------------\n");
  printf("  Firmware version = 0x%x\n",version);

  if(rflag)
    {
      printf("  System Register  = 0x%04x\n",system);
      printf("  Status Register  = 0x%04x\n",status);
      printf("  CSR Test Reg     = 0x%04x\n",csrTest);
      printf("\n");
  }

  if(system & SD_SYSTEM_TI_LINK_ENABLE)
    printf("  TI Fast Link ACTIVE\n");
  else
    printf("  TI Fast Link INACTIVE\n");

  if(version>=0xa5)
    {
      if( (status & SD_STATUS_POWER_FAULT)==0 )
	{
	  printf("  *** Power Fault Detected ***\n");
	}
    }
  else /* Version 0xA4 */
    {
      if( status & SD_STATUS_POWER_FAULT )
	{
	  printf("  *** Power Fault Detected ***\n");
	}
    }
  printf("\n");

  printf("  Clock settings:  \n");
  printf("    A: ");
  if(system & SD_SYSTEM_CLKA_BYPASS_MODE)
    printf("Bypass Mode\n");
  else
    {
      printf("PLL set for ");

      switch( (system&SD_SYSTEM_CLKA_FREQUENCY_MASK)>>2 )
	{
	case 0:
	  printf("Undefined");
	  break;
	case 1:
	  printf("31.25 MHz");
	  break;
	case 2:
	  printf("125.00 MHz");
	  break;
	case 3:
	  printf("250.00 MHz");
	  break;
	}
      printf("\n");
    }
  printf("    B: ");
  if(system & SD_SYSTEM_CLKB_BYPASS_MODE)
    printf("Bypass Mode\n");
  else
    {
      printf("PLL set for ");

      switch( (system&SD_SYSTEM_CLKB_FREQUENCY_MASK)>>6 )
	{
	case 0:
	  printf("Undefined");
	  break;
	case 1:
	  printf("31.25 MHz");
	  break;
	case 2:
	  printf("125.00 MHz");
	  break;
	case 3:
	  printf("250.00 MHz");
	  break;
	}
      printf("\n");
    }
  printf("\n");

  printf("  Detected Clock:  \n");
  printf("    A: ");
  switch( status&SD_STATUS_CLKA_DETECTED_MASK )
    {
    case SD_STATUS_CLKA_DETECTED_UNKNOWN:
      printf("UNKNOWN");
      break;
    case SD_STATUS_CLKA_DETECTED_31_25:
      printf("31.25 MHz");
      break;
    case SD_STATUS_CLKA_DETECTED_125:
      printf("125.00 MHz");
      break;
    case SD_STATUS_CLKA_DETECTED_250:
      printf("250.00 MHz");
      break;
    }
  printf("\n");
  printf("    B: ");
  switch( (status&SD_STATUS_CLKB_DETECTED_MASK) )
    {
    case SD_STATUS_CLKB_DETECTED_UNKNOWN:
      printf("UNKNOWN");
      break;
    case SD_STATUS_CLKB_DETECTED_31_25:
      printf("31.25 MHz");
      break;
    case SD_STATUS_CLKB_DETECTED_125:
      printf("125.00 MHz");
      break;
    case SD_STATUS_CLKB_DETECTED_250:
      printf("250.00 MHz");
      break;
    }
  printf("\n\n");

  printf("  Clock STATUS:  \n");
  printf("    A: ");
  switch( status& (SD_STATUS_CLKA_LOSS_OF_SIGNAL | SD_STATUS_CLKA_LOSS_OF_LOCK) )
    {
    case 0:
      printf("Normal");
      break;
    case SD_STATUS_CLKA_LOSS_OF_SIGNAL:
      printf("*** Loss of Signal ***");
      break;
    case SD_STATUS_CLKA_LOSS_OF_LOCK:
      printf("*** Loss of Lock ***");
      break;
    case (SD_STATUS_CLKA_LOSS_OF_SIGNAL|SD_STATUS_CLKA_LOSS_OF_LOCK):
      printf("*** Loss of Signal and Lock ***");
      break;
    }
  printf("\n");
  printf("    B: ");
  switch( status& (SD_STATUS_CLKB_LOSS_OF_SIGNAL | SD_STATUS_CLKB_LOSS_OF_LOCK) )
    {
    case 0:
      printf("Normal");
      break;
    case SD_STATUS_CLKB_LOSS_OF_SIGNAL:
      printf("*** Loss of Signal ***");
      break;
    case SD_STATUS_CLKB_LOSS_OF_LOCK:
      printf("*** Loss of Lock ***");
      break;
    case (SD_STATUS_CLKB_LOSS_OF_SIGNAL|SD_STATUS_CLKB_LOSS_OF_LOCK):
      printf("*** Loss of Signal and Lock ***");
      break;
    }
  printf("\n\n");

  if(rflag)
    {
      printf("  Payload Boards Mask = 0x%04x   Token Passing Boards Mask = 0x%04x\n",
	     payloadPorts,tokenPorts);
      printf("  BusyOut Boards Mask = 0x%04x   TrigOut Boards Mask       = 0x%04x\n",
	     busyoutPorts,trigoutPorts);
      printf("\n");
    }

  if(showVMEslots==1)
    printf("  VME Slots Enabled: \n\t");
  else
    printf("  Payload ports Enabled: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(payloadPorts);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(showVMEslots==1)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(payloadPorts & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n");

  if(showVMEslots==1)
    printf("  Token VME Slots Enabled: \n\t");
  else
    printf("  Token Payload ports Enabled: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(tokenPorts);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(showVMEslots==1)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(tokenPorts & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n");

  if(showVMEslots==1)
    printf("  BusyOut VME Slots Enabled: \n\t");
  else
    printf("  BusyOut Payload ports Enabled: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(busyoutPorts);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(showVMEslots==1)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(busyoutPorts & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n");

  if(showVMEslots==1)
    printf("  Trigout VME Slots Enabled: \n\t");
  else
    printf("  Trigout Payload ports Enabled: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(trigoutPorts);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(showVMEslots==1)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(trigoutPorts & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n\n");

  if(rflag)
    {
      printf("  Busyout State Mask  = 0x%04x   TrigOut State Mask        = 0x%04x\n",
	     busyoutStatus,trigoutStatus);
      printf("\n");
    }

  if(status & SD_STATUS_BUSYOUT)
    {
      printf("  At least one module has asserted BUSY since last read\n");
    }
  if(showVMEslots==1)
    printf("  VME Slots Busy Status High: \n\t");
  else
    printf("  Payload ports Busy Status High: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(busyoutStatus);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(showVMEslots==1)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(busyoutStatus & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n\n");

  if(status & SD_STATUS_TRIGOUT)
    {
      printf("  At least one module has asserted TrigOut since last read\n");
    }
  if(showVMEslots==1)
    printf("  VME Slots TrigOut Status High : \n\t");
  else
    printf("  Payload ports TrigOut Status High: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(trigoutStatus);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(showVMEslots==1)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(trigoutStatus & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n\n");


  printf("--------------------------------------------------------------------------------\n");
  printf("\n\n");
  return OK;
}

/**
   @ingroup Status
   @brief Return and optionally display the SD firmware version.

   @param pflag Print Flag
     If >0, will print firmware version to standard out

   @returns Firmware version if successful, otherwise ERROR.
 */

int
sdGetFirmwareVersion(int pflag)
{
  int version=0;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  version = vmeRead32(&SDp->version) & 0xFFFF;
  SDUNLOCK;

  if(pflag)
    {
      printf("%s: Firmware Version 0x%x\n",
	     __FUNCTION__,version);
    }

  return version;
}

/**
   @ingroup Config
   @brief Enable or Disable the Fast link to the TI

   @param enable Enable flag
     0 : Disable
     >0 : Enable

   @return OK if successful, otherwise ERROR
 */

int
sdSetTiFastLink(int enable)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  if(enable)
    {
      vmeWrite32(&SDp->system,
		 vmeRead32(&SDp->system) | SD_SYSTEM_TI_LINK_ENABLE);
    }
  else
    {
      vmeWrite32(&SDp->system,
		 vmeRead32(&SDp->system) & ~SD_SYSTEM_TI_LINK_ENABLE);
    }
  SDUNLOCK;
  return OK;
}

/**
   @ingroup Status
   @brief Return the state of the fast link to the TI

   @return 1 if enabled, 0 if disabled, otherwise ERROR
 */

int
sdGetTiFastLink()
{
  int bitval=0,rval=0;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  bitval = vmeRead32(&SDp->system) & SD_SYSTEM_TI_LINK_ENABLE;
  if(bitval)
    rval=1;
  else
    rval=0;
  SDUNLOCK;

  return rval;
}

/**
  @ingroup Config
  @brief Set the PLL Clock Frequency of A/B/Both
  @param iclk Which side of crate to activate PLL
   - 0 for A (LHS of crate)
   - 1 for B (RHS of crate)
   - 2 for Both
   @param ifreq Frequency to lock on.
   - 1 for 31.25 MHz
   - 2 for 125.00 MHz
   - 3 for 250.00 MHz

   @return OK if successul, otherwise ERROR.
*/

int
sdSetPLLClockFrequency(int iclk, int ifreq)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(iclk<0 || iclk>2)
    {
      printf("%s: ERROR: Invalid value of iclk (%d).  Must be 0, 1, or 2.\n",
	     __FUNCTION__,iclk);
      return ERROR;
    }
  if(ifreq<1 || ifreq>3)
    {
      printf("%s: ERROR: Invalid value of ifreq (%d).  Must be 1, 2, or 3.\n",
	     __FUNCTION__,ifreq);
      return ERROR;
    }

  SDLOCK;
  if(iclk==0 || iclk==2)
    vmeWrite32(&SDp->system,
	     (vmeRead32(&SDp->system) & ~(SD_SYSTEM_CLKA_FREQUENCY_MASK)) |
	     (ifreq<<2) );

  if(iclk==1 || iclk==2)
    vmeWrite32(&SDp->system,
	     (vmeRead32(&SDp->system) & ~(SD_SYSTEM_CLKB_FREQUENCY_MASK)) |
	     (ifreq<<6) );
  SDUNLOCK;

  return OK;
}

/**
  @ingroup Status
  @brief Return the PLL clock frequency for the selected iclk
  @param iclk Clock selection
    - 0 for A (LHS of crate)
    - 1 for B (RHS of crate)

  @return
    - 1 for 31.25 MHz
    - 2 for 125.00 MHz
    - 3 for 250.00 MHz
*/

int
sdGetPLLClockFrequency(int iclk)
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(iclk<0 || iclk>1)
    {
      printf("%s: ERROR: Invalid value of iclk (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,iclk);
      return ERROR;
    }

  SDLOCK;
  if(iclk==0)
    rval = (vmeRead32(&SDp->system) & (SD_SYSTEM_CLKA_FREQUENCY_MASK))>>2;
  else
    rval = (vmeRead32(&SDp->system) & (SD_SYSTEM_CLKB_FREQUENCY_MASK))>>6;

  SDUNLOCK;

  return rval;
}

/**
  @ingroup Status
  @brief Return the detected clock frequency for the selected iclk
  @param iclk Clock selection
     - 0 for A (LHS of crate)
     - 1 for B (RHS of crate)

  @return
     - 0 for undefined
     - 1 for 31.25 MHz
     - 2 for 125.00 MHz
     - 3 for 250.00 MHz

*/

int
sdGetClockFrequency(int iclk, int pflag)
{
  int rval;
#ifndef UNSUPPORTED_A5
  printf("%s: This feature not supported at this time.\n",__FUNCTION__);
  return ERROR;
#endif
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(iclk<0 || iclk>1)
    {
      printf("%s: ERROR: Invalid value of iclk (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,iclk);
      return ERROR;
    }

  SDLOCK;
  if(iclk==0)
    rval = (vmeRead32(&SDp->status) & (SD_STATUS_CLKA_DETECTED_MASK))>>8;
  else
    rval = (vmeRead32(&SDp->status) & (SD_STATUS_CLKB_DETECTED_MASK))>>10;
  SDUNLOCK;

  if(pflag)
    {
      printf("%s: Detected Clock Frequency = ",__FUNCTION__);
      switch(rval)
	{
	case 1:
	  printf(" 31.25 MHz\n");
	  break;
	case 2:
	  printf(" 125 MHz\n");
	  break;
	case 3:
	  printf(" 250 MHz\n");
	  break;
	case 0:
	default:
	  printf(" UNKNOWN\n");
	}
    }

  return rval;
}

/**
  @ingroup Config
  @brief Select whether the Clock fanned out will be jitter attenuated (PLL on) or
    as received from the TI
  @param iclk Clock Selection
     - 0 for A (LHS of crate)
     - 1 for B (RHS of crate)
  @param imode Mode selection
     - 0 for Bypass mode
     - 1 for PLL mode
  @return OK if successful, otherwise ERROR
*/

int
sdSetClockMode(int iclk, int imode)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(iclk<0 || iclk>1)
    {
      printf("%s: ERROR: Invalid value of iclk (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,iclk);
      return ERROR;
    }
  if(imode<0 || imode>1)
    {
      printf("%s: ERROR: Invalid value of imode (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,imode);
      return ERROR;
    }

  SDLOCK;
  if(iclk==0)
    vmeWrite32(&SDp->system,
	     (vmeRead32(&SDp->system) & ~(SD_SYSTEM_CLKA_BYPASS_MODE)) |
	     (imode<<0) );
  else
    vmeWrite32(&SDp->system,
	     (vmeRead32(&SDp->system) & ~(SD_SYSTEM_CLKB_BYPASS_MODE)) |
	     (imode<<4) );
  SDUNLOCK;

  return OK;
}

/**
  @ingroup Status
  @brief Return whether the Clock fanned out will be jitter attenuated (PLL)
  or as received from the TI(D)
  @param iclk Clock Selection
    - 0 for A (LHS of crate)
    - 1 for B (RHS of crate)

  @return
    - 0 for Bypass mode
    - 1 for PLL mode
*/

int
sdGetClockMode(int iclk)
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(iclk<0 || iclk>1)
    {
      printf("%s: ERROR: Invalid value of iclk (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,iclk);
      return ERROR;
    }

  SDLOCK;
  if(iclk==0)
    rval = (vmeRead32(&SDp->system) & (SD_SYSTEM_CLKA_BYPASS_MODE));
  else
    rval = (vmeRead32(&SDp->system) & (SD_SYSTEM_CLKB_BYPASS_MODE))>>4;
  SDUNLOCK;

  return rval;
}

/**
  @ingroup Config
  @brief Reset the SD

  @return OK if successful, otherwise ERROR
*/

int
sdReset()
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  vmeWrite32(&SDp->system,
	     (vmeRead32(&SDp->system) & ~(SD_SYSTEM_TEST_RESET)) |
	     SD_SYSTEM_TEST_RESET );
  vmeWrite32(&SDp->system,
	     (vmeRead32(&SDp->system) & ~(SD_SYSTEM_TEST_RESET)));
  SDUNLOCK;

  return OK;
}

/**
  @ingroup Config
  @brief Routine for user to define the Payload Ports that participate in
  Trigger Out, Busy Out, Token, and Status communication.

  @param imask Payload port mask

  @return OK if successful, otherwise ERROR.
*/
int
sdSetActivePayloadPorts(unsigned int imask)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(imask<0 || imask>0xffff)
    {
      printf("%s: ERROR: Invalid imask 0x%x\n",
	     __FUNCTION__,imask);
      return ERROR;
    }

  SDLOCK;
  vmeWrite32(&SDp->payloadPorts, imask);
  vmeWrite32(&SDp->tokenPorts, imask);
  vmeWrite32(&SDp->busyoutPorts, imask);
  vmeWrite32(&SDp->trigoutPorts, imask);
  SDUNLOCK;

  return OK;
}

/**
  @ingroup Config
  @brief Routine for user to define the Vme Slots that participate in
  Trigger Out, Busy Out, Token, and Status communication.

  @param vmemask VME Slot mask
    - bit  0: Vme Slot 0 (non-existant)
    - bit  1: Vme Slot 1 (controller slot)
    - bit  2: Vme Slot 2 (not used by CTP)
    - bit  3: Vme Slot 3 (First slot on the LHS of crate that is used by CTP)
      ..
    - bit 20: Vme Slot 20 (Last slot that is used by the CTP)
    - bit 21: Vme Slot 21 (Slot for the TID)

  @return OK if successful, otherwise ERROR.
*/
int
sdSetActiveVmeSlots(unsigned int vmemask)
{
  unsigned int payloadmask=0;
  unsigned int slot=0, payloadport=0, ibit=0;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  /* Check the input mask */
  if( vmemask & 0xFFE00007 )
    {
      printf("%s: ERROR: Invalid vmemask (0x%08x)\n",
	     __FUNCTION__,vmemask);
      return ERROR;
    }

  /* Convert the vmemask to the payload port mask */
  for (ibit=0; ibit<32; ibit++)
    {
      if(vmemask & (1<<ibit))
	{
	  slot = ibit;
	  payloadport  = vmeSlot2vxsPayloadPort(slot);
	  payloadmask |= (1<<(payloadport-1));
	}
    }

  sdSetActivePayloadPorts(payloadmask);

  return OK;


}

/**
  @ingroup Status
  @brief Routine to return the currently defined Payload Ports that participate in
    Trigger Out, Busy Out, Token, and Status communication.

  @return Active Payload Port Mask if successful, otherwise ERROR
*/
int
sdGetActivePayloadPorts()
{
  int rval;
  unsigned int payloadPorts, tokenPorts, busyoutPorts, trigoutPorts;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  payloadPorts = vmeRead32(&SDp->payloadPorts);
  tokenPorts   = vmeRead32(&SDp->tokenPorts);
  busyoutPorts = vmeRead32(&SDp->busyoutPorts);
  trigoutPorts = vmeRead32(&SDp->trigoutPorts);
  SDUNLOCK;

  rval = payloadPorts;

  /* Simple check for consistency, warn if there's not */
  if((payloadPorts != tokenPorts) ||
     (payloadPorts != busyoutPorts) ||
     (payloadPorts != trigoutPorts) )
    {
      printf("%s: WARNING: Inconsistent payload slot masks..",__FUNCTION__);
      printf("    payloadPorts = 0x%08x\n",payloadPorts);
      printf("    tokenPorts   = 0x%08x\n",tokenPorts);
      printf("    busyoutPorts = 0x%08x\n",busyoutPorts);
      printf("    trigoutPorts = 0x%08x\n",trigoutPorts);
    }

  return rval;
}

/*
  @ingroup Config
  @brief Routine for user to define the Payload Ports that participate in
  Busy Out communication.


  @param  imask - mask of payload ports to enable busy out
          reset - decision to reset and use only the user defined imask

  @return OK if successful, otherwise ERROR

*/
int
sdSetBusyVmeSlots(unsigned int vmemask, int reset)
{
  int ibit, slot;
  unsigned int payloadmask = 0, payloadport = 0,
    wpayload_mask = 0, wbusy_mask = 0;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  /* Check the input mask */
  if( vmemask & 0xFFE00007 )
    {
      printf("%s: ERROR: Invalid vmemask (0x%08x)\n",
	     __FUNCTION__,vmemask);
      return ERROR;
    }

  /* Convert the vmemask to the payload port mask */
  for (ibit=0; ibit<32; ibit++)
    {
      if(vmemask & (1<<ibit))
	{
	  slot = ibit;
	  payloadport  = vmeSlot2vxsPayloadPort(slot);
	  payloadmask |= (1<<(payloadport-1));
	}
    }

  SDLOCK;
  if(reset == 0) /* Maintain the current payload configuration */
    {
      wpayload_mask = vmeRead32(&SDp->payloadPorts);
      wbusy_mask = vmeRead32(&SDp->busyoutPorts);
    }

  wpayload_mask |= payloadmask;
  wbusy_mask |= payloadmask;

  vmeWrite32(&SDp->payloadPorts, wpayload_mask);
  vmeWrite32(&SDp->busyoutPorts, wbusy_mask);
  SDUNLOCK;

  return OK;
}

/*
  sdGetBusyoutCounter
  - Return the value of the Busyout Counter for a specified payload board
    Value of the counter is reset after read

  @return Busy counter of specified payload port if successful, otherwise ERROR.
*/

int
sdGetBusyoutCounter(int ipayload)
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(ipayload<1 || ipayload>16)
    {
      printf("%s: ERROR: Invalid ipayload = %d.  Must be 1-16\n",
	     __FUNCTION__,ipayload);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->busyoutCounter[ipayload-1]);
  SDUNLOCK;

  return rval;

}

/**
  @ingroup Status
  @brief Display (to standard out) values of the busy counters for all VME Slots.

  @return OK if successful, otherwise ERROR.
*/

int
sdPrintBusyoutCounters()
{
  unsigned int counter=0;
  int islot;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  printf("-----------------\n");
  printf("Slot:  Busy Count\n");
  for(islot=3; islot<21; islot++)
    {
      if((islot==11) || (islot==12)) /* Skip the Switch Slots */
	continue;

      counter = sdGetBusyoutCounter(vmeSlot2vxsPayloadPort(islot));
      printf("  %2d:  %d\n",islot,counter);
    }
  printf("-----------------\n");
  printf("\n");

  return OK;
}

/**
  @ingroup Status
  @brief Return the mask value of payload ports that are currently BUSY

  @param pflag Print flag
     - >0 : Print to standard out

  @return Payload port busy mask
*/

int
sdGetBusyoutStatus(int pflag)
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->busyoutStatus) & 0xFFFF;
  SDUNLOCK;

  if(pflag)
    {
      printf("%s: Busyout Status = 0x%04x\n",
	     __FUNCTION__,rval);
    }

  return rval;
}

/**
  @ingroup Status
  @brief Return the value of the Trigout Counter for a specified payload board
    Value of the counter is reset after read

  @param ipayload Selected Payload Port

  @return Trigout Counter for selected payload port if successful, otherwise ERROR

*/

int
sdGetTrigoutCounter(int ipayload)
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(ipayload<1 || ipayload>16)
    {
      printf("%s: ERROR: Invalid ipayload = %d.  Must be 1-16\n",
	     __FUNCTION__,ipayload);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->trigoutCounter[ipayload-1]);
  SDUNLOCK;

  return rval;

}

/**
  @ingroup Status
  @brief Display (to standard out) values of the trigout counters for all VME Slots.

  @return OK if successful, otherwise ERROR.
*/

int
sdPrintTrigoutCounters()
{
  unsigned int counter=0;
  int islot;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  printf("--------------------\n");
  printf("Slot:  Trigout Count\n");
  for(islot=3; islot<21; islot++)
    {
      if((islot==11) || (islot==12)) /* Skip the Switch Slots */
	continue;

      counter = sdGetTrigoutCounter(vmeSlot2vxsPayloadPort(islot));
      printf("  %2d:  %d\n",islot,counter);
    }
  printf("--------------------\n");
  printf("\n");

  return OK;
}



/*************************************************************
 *  SD FIRMWARE UPDATING ROUTINES
 ************************************************************/
static int
sdFirmwareWaitCmdDone(int wait)
{
  int i;
  unsigned int data_out;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  for(i = 0; i < wait*75; i++)
    {
      if((i%100)==0)
	{
	  printf(".");
	  fflush(stdout);
	}

      SDLOCK;
      data_out = vmeRead32(&SDp->memCheckStatus);
      SDUNLOCK;

      fflush(stdout);
      if (!(data_out & 0x100))
	{
	  return data_out & 0xFF;
	}
    }

  printf("%s: ERROR: Timeout\n",__FUNCTION__);

  return 0;

}

/**
   @ingroup FWUpdate
   @brief Flush the FIFO used in write data to the ROM
   @return OK if successful, otherwise ERROR;
 */

int
sdFirmwareFlushFifo()
{
  int i;
  unsigned int data_out;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  for(i = 0; i < 100; i++)
    {
      data_out = vmeRead32(&SDp->memReadCtrl);

      if(data_out & 0x200)
	break;
    }
  SDUNLOCK;

  if(i == 100)
    {
      printf("%s: ERROR: config read init buffer error\n",__FUNCTION__);
      return ERROR;
    }

  else
    printf("%s: INFO: i = %d   data_out = 0x%0x\n",__FUNCTION__,i, data_out);

  return OK;
}

/**
   @ingroup FWUpdate
   @brief Load the specified file containing the firmware, into local memory.
   @param filename Name of file
   @return OK if successful, otherwise ERROR;
 */
int
sdFirmwareLoadFile(char *filename)
{
  FILE *progFile;
/* #define DEBUGFILE */
#ifdef DEBUGFILE
  int ibyte=0;
#endif

  if(filename==NULL)
    {
      printf("%s: Error: Invalid filename\n",__FUNCTION__);
      return ERROR;
    }

  /* Open the file containing the firmware */
  progFile=fopen(filename,"r");
  if(progFile==NULL)
    {
      printf("%s: ERROR opening file (%s) for reading\n",
	     __FUNCTION__,filename);
      return ERROR;
    }

  /* Allocate memory to store in locally */
  progFirmware = (unsigned char *)malloc(SD_MAX_FIRMWARE_SIZE*sizeof(unsigned char));
  if(progFirmware==NULL)
    {
      printf("%s: ERROR: Unable to allocate memory for firmware\n",__FUNCTION__);
      fclose(progFile);
      return ERROR;
    }

  /* Initialize this local memory with 0xff's */
  memset(progFirmware, 0xff, SD_MAX_FIRMWARE_SIZE);

  /* Read the file into memory */
  progFirmwareSize = fread(progFirmware, 1, SD_MAX_FIRMWARE_SIZE, progFile);
  printf("%s: Firmware Size = %d (0x%x)\n",
	 __FUNCTION__, (int)progFirmwareSize, (int)progFirmwareSize);

/*   progFirmwareSize=0x20000; */

  fclose(progFile);

#ifdef DEBUGFILE
  for(ibyte = 0; ibyte<progFirmwareSize; ibyte++)
    {
      printf("%02x ",progFirmware[ibyte]);
      if((ibyte+1)%64==0) printf("\n");
    }
  printf("\n");
#endif

  return OK;
}

/**
   @ingroup FWUpdate
   @brief Free the memory allocated to store the firmware.
   @return OK if successful, otherwise ERROR;
 */

void
sdFirmwareFreeMemory()
{
  if(progFirmware!=NULL)
    {
      free(progFirmware);
    }
}

/**
   @ingroup FWUpdate
   @brief Verify the page of data starting a specified memory address
   @param mem_addr Address of memory to check
   @return OK if successful, otherwise ERROR;
 */

int
sdFirmwareVerifyPage(unsigned int mem_addr)
{
  unsigned int data;
  unsigned int ibyte;
  int n_err=0;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

/*   printf("%s: Verifying loaded firmware with current firmware\n", */
/* 	 __FUNCTION__); */

  /* Loop over each byte in the firmware */
  for(ibyte = mem_addr; ibyte<mem_addr+256; ibyte++)
    {
      if((ibyte%0x10000) == 0) printf("Verifying firmware to memory address 0x%06x\n",ibyte);
      /* Write to set the memory address we're accessing */
      vmeWrite32(&SDp->memAddrLSB, (ibyte & 0xFFFF) );
      vmeWrite32(&SDp->memAddrMSB, (ibyte & 0xFF0000)>>16 );

      vmeWrite32(&SDp->memReadCtrl, 0xB00);

      data = vmeRead32(&SDp->memReadCtrl);
      if(data < 0)
	{
	  printf("%s: ERROR: page program timeout error\n", __FUNCTION__);
	  SDUNLOCK;
	  return ERROR;
	}
      else
	{
	  if(progFirmware[ibyte] != (data & 0xFF))
	    {
	      n_err++;
	      if(n_err<400)
		{
		  printf("0x%06x (%8d): 0x%02x != 0x%02x    ***** \n",
			 ibyte, ibyte, progFirmware[ibyte], (data & 0xFF));
		}

	    }
	}

    }

  if(n_err)
    {
      printf("%s: Total errors: %d\n",__FUNCTION__,n_err);
      return ERROR;
    }

  return OK;


}

/**
   @ingroup FWUpdate
   @brief Verify the page of data starting a specified memory address, is zero
   @param mem_addr Address of memory to check
   @return OK if successful, otherwise ERROR;
 */

int
sdFirmwareVerifyPageZero(unsigned int mem_addr)
{
  unsigned int data;
  unsigned int ibyte;
  int n_err=0;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

/*   printf("%s: Verifying loaded firmware with current firmware\n", */
/* 	 __FUNCTION__); */

  /* Loop over each byte in the firmware */
/* #define DEBUGERASE */
#ifdef DEBUGERASE
  printf("Verifying erase to memory address 0x%06x\n",mem_addr);
#endif
  for(ibyte = mem_addr; ibyte<mem_addr+256; ibyte++)
    {
      /* Write to set the memory address we're accessing */
      vmeWrite32(&SDp->memAddrLSB, (ibyte & 0xFFFF) );
      vmeWrite32(&SDp->memAddrMSB, (ibyte & 0xFF0000)>>16 );

      vmeWrite32(&SDp->memReadCtrl, 0xB00);

      data = vmeRead32(&SDp->memReadCtrl);
      if(data < 0)
	{
	  printf("%s: ERROR: page program timeout error\n", __FUNCTION__);
	  SDUNLOCK;
	  return ERROR;
	}
      else
	{
	  if(0xff != (data & 0xFF))
	    {
	      n_err++;
#ifdef DEBUGERASE
	      if(n_err<400)
		{
		  printf("0x%06x (%8d): 0x%02x != 0x%02x    ***** \n",
			 ibyte, ibyte, 0xff, (data & 0xFF));
		}
#endif

	    }
	}

    }

  if(n_err)
    {
      printf("%s: Total errors: %d\n",__FUNCTION__,n_err);
      return ERROR;
    }

  return OK;


}

/**
   @ingroup FWUpdate
   @brief Write a page of data starting a specified memory address
   @param mem_addr Address of memory to write
   @return OK if successful, otherwise ERROR;
 */

void
sdFirmwareWritePage(unsigned int mem_addr)
{
  int ibyte=0;
  unsigned int prog=0;
  unsigned int memCommand=0, mem_write=0;

  vmeWrite32(&SDp->memAddrLSB, (mem_addr & 0xFFFF) );
  vmeWrite32(&SDp->memAddrMSB, (mem_addr & 0xFF0000)>>16 );

  memCommand=0x0600;
  for(ibyte = mem_addr; ibyte < mem_addr + 256; ibyte++)
    {

      prog = (progFirmware[ibyte]) & 0xFF;

      if(ibyte>=progFirmwareSize)
	mem_write = (memCommand | 0xFF);
      else
	mem_write = (memCommand | prog);

      vmeWrite32(&SDp->memWriteCtrl, mem_write );

      if(ibyte==(mem_addr+255))
	{
	  memCommand=0x300;
	  mem_write = (memCommand | prog);

	  vmeWrite32(&SDp->memWriteCtrl, mem_write );
	}

    }
  vmeWrite32(&SDp->memWriteCtrl, 0x0300 | prog);

#ifdef VXWORKSPPC
  taskDelay(1);
#else
  usleep(7000);
#endif
}

/**
   @ingroup FWUpdate
   @brief Write entire firmware to SD's ROM
   @return OK if successful, otherwise ERROR;
 */

int
sdFirmwareWriteToMemory()
{
  unsigned int mem_addr=0;
  int page_count=0;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((progFirmware==NULL) || (progFirmwareSize==0))
    {
      printf("%s: Error: Firmware file not loaded into memory\n",
	     __FUNCTION__);
      return ERROR;
    }

  /* Loop over each byte in the firmware */
  for(mem_addr=0; mem_addr<progFirmwareSize; mem_addr+=256)
    {

      if( (mem_addr % 0x10000) == 0) /* Erase current sector */
	{
	  SDLOCK;
	  vmeWrite32(&SDp->memAddrLSB, (mem_addr & 0xFFFF) );
	  vmeWrite32(&SDp->memAddrMSB, (mem_addr & 0xFF0000)>>16 );

	  vmeWrite32(&SDp->memWriteCtrl, (0x1200) );

	  sleep(3);
	  SDUNLOCK;
	  if(sdFirmwareWaitCmdDone(3300)<0)
	    {
	      printf("%s: ERROR: sector erase timeout error\n",__FUNCTION__);
	      return ERROR;
	    }

	}

      /* Write to set the memory address we're accessing */
      if((mem_addr%0x10000) == 0) printf("Writing firmware to memory address 0x%06x\n",mem_addr);
      SDLOCK;

      if(sdFirmwareVerifyPageZero(mem_addr)==ERROR)
	{
	  SDUNLOCK;
	  printf("%s: Too many errors in current page (%d)\n",__FUNCTION__,page_count);
	  return ERROR;
	}

      sdFirmwareWritePage(mem_addr);

      if(sdFirmwareVerifyPage(mem_addr)==ERROR)
	{
	    SDUNLOCK;
	    printf("%s: Too many errors in current page (%d)\n",__FUNCTION__,page_count);
	    return ERROR;
	}

      page_count++;

#ifdef VXWORKSPPC
      taskDelay(1);
#else
      usleep(5000);
#endif
      SDUNLOCK;

    }
  SDUNLOCK;

  printf("%s: pages written = %d\n",__FUNCTION__,page_count);
  return OK;
}


/**
   @ingroup FWUpdate
   @brief Verify that SD's ROM contains exactly the contents of the firmware
   @return OK if successful, otherwise ERROR;
 */

int
sdFirmwareVerifyMemory()
{
  unsigned int mem_addr=0, data;
  int n_err=0;

  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  printf("%s: Verifying loaded firmware with current firmware\n",
	 __FUNCTION__);

  /* Loop over each byte in the firmware */
  SDLOCK;
  for(mem_addr=0; mem_addr<progFirmwareSize; mem_addr++)
    {
      if((mem_addr%0x10000) == 0) printf("Verifying firmware to memory address 0x%06x\n",mem_addr);
      /* Write to set the memory address we're accessing */
      vmeWrite32(&SDp->memAddrLSB, (mem_addr & 0xFFFF) );
      vmeWrite32(&SDp->memAddrMSB, (mem_addr & 0xFF0000)>>16 );

      vmeWrite32(&SDp->memReadCtrl, 0xB00);

      data = vmeRead32(&SDp->memReadCtrl);
      if(data < 0)
	{
	  printf("%s: ERROR: page program timeout error\n", __FUNCTION__);
	  SDUNLOCK;
	  return ERROR;
	}
      else
	{
	  if(progFirmware[mem_addr] != (data & 0xFF))
	    {
	      n_err++;
	      if(n_err<400)
		{
		  printf("0x%06x (%8d): 0x%02x != 0x%02x    ***** \n",
			 mem_addr, mem_addr, progFirmware[mem_addr], (data & 0xFF));
		}

	    }
	}

    }
  SDUNLOCK;

  printf("%s: Total errors: %d\n",__FUNCTION__,n_err);

  if(n_err)
    return ERROR;

  return OK;

}

/**
   @ingroup FWUpdate
   @brief Check the read status of the FIFO
   @return OK if successful, otherwise ERROR;
 */

int
sdFirmwareReadStatus()
{
  unsigned int status_out;
  int i;

  SDLOCK;
  for(i = 0; i < 3; i++)
    {
      vmeWrite32(&SDp->memReadCtrl, 0x0400);
/*       write_i2c (0x0400, 0x48); */
      status_out = vmeRead32(&SDp->memCheckStatus);
/*       status_out = read_i2c(0x49); */
/*       if ((i % 2) == 0) */
/* 	{	printf("{%04X}", status_out);} */

      if ((status_out & 0x4))
	{
/* 	  printf("%s: INFO: read status complete\n",__FUNCTION__); */
/* 	  printf("{%04X}", status_out); */
/* 	  printf("\n"); */
	  SDUNLOCK;
	  return status_out & 0xFF;
	}
      taskDelay(1);
    }
  SDUNLOCK;
  printf("%s: ERROR: Timeout\n",__FUNCTION__);
  return -1;
}

/**
   @cond NoDoc
   Don't include this routine in the documentation
*/
void
sdFirmwareWriteSpecs(unsigned int addr, unsigned int serial_number,
		     unsigned int hall_board_version, unsigned int firmware_version)
{
  int i;

  SDLOCK;
  vmeWrite32(&SDp->memAddrLSB, (addr & 0xFFFF) );
  vmeWrite32(&SDp->memAddrMSB, (addr & 0xFF0000)>>16 );

  vmeWrite32(&SDp->memWriteCtrl, (0x2200) );

  sleep(3);
  SDUNLOCK;
  if(sdFirmwareWaitCmdDone(3300)<0)
    {
      printf("%s: ERROR: sector erase timeout error\n",__FUNCTION__);
      return;
    }

  SDLOCK;
  vmeWrite32(&SDp->memAddrLSB, (addr & 0xFFFF) );
  vmeWrite32(&SDp->memAddrMSB, (addr & 0xFF0000)>>16 );

  vmeWrite32(&SDp->memWriteCtrl, (0x1200) );

  sleep(3);
  SDUNLOCK;
  if(sdFirmwareWaitCmdDone(3300)<0)
    {
      printf("%s: ERROR: sector erase timeout error\n",__FUNCTION__);
      return;
    }

  SDLOCK;

  sdFirmwareVerifyPageZero(addr);

  vmeWrite32(&SDp->memAddrLSB, (addr & 0xFFFF) );
  vmeWrite32(&SDp->memAddrMSB, (addr & 0xFF0000)>>16 );

  /* Write the board specific stuff here */
  vmeWrite32(&SDp->memWriteCtrl, (0x600) | (serial_number&0xFF) );
  vmeWrite32(&SDp->memWriteCtrl, (0x600) | (hall_board_version&0xFF) );
  vmeWrite32(&SDp->memWriteCtrl, (0x600) | (firmware_version&0xFF) );

  for(i = 0; i < 253; i++)
    {
      vmeWrite32(&SDp->memWriteCtrl, 0x6EE);

      if (i == 252)
	{
	  vmeWrite32(&SDp->memWriteCtrl, 0x3EE);
	  break;
	}

    }

  vmeWrite32(&SDp->memWriteCtrl, (0x2220) );

  sleep(2);
  SDUNLOCK;
  if(sdFirmwareWaitCmdDone(3300)<0)
    {
      printf("%s: ERROR: sector protect timeout error\n",__FUNCTION__);
      return;
    }

  printf("%s: INFO: Complete\n",__FUNCTION__);

}
/**
   @endcond
*/

/**
   @ingroup FWUpdate
   @brief  Read a specific address from the ROM

   @return Value at address (8 bits).
*/
int
sdFirmwareReadAddr(unsigned int addr)
{
  unsigned int data_out;

  SDLOCK;
  vmeWrite32(&SDp->memAddrLSB, (addr & 0xFFFF) );
  vmeWrite32(&SDp->memAddrMSB, (addr & 0xFF0000)>>16 );

  vmeWrite32(&SDp->memReadCtrl, 0xB00);

  taskDelay(1);

  data_out = vmeRead32(&SDp->memReadCtrl);
  SDUNLOCK;

  return data_out & 0xFF;

}

/**
   @ingroup FWUpdate
   @brief  Read protected information from the ROM.

      Prints to standard out
      - Serial Number
      - Assigned Hall and Hardware Version
      - Initial Firmware Version

*/
void
sdFirmwarePrintSpecs()
{
  printf("%s:\n",__FUNCTION__);
  printf("\tSerial Number            = %4d\n", sdFirmwareReadAddr(0x7F0000));
  printf("\tAssigned Hall & Version  = 0x%02X\n", sdFirmwareReadAddr(0x7F0001));
  printf("\tInitial Firmware Version = 0x%02X\n", sdFirmwareReadAddr(0x7F0002));
}

/**
   @ingroup Status
   @brief  Return the serial number of the SD.

   @param rSN Local Memory address to store Serial Number in string format.

   @return Number value of the serial number

*/
unsigned int
sdGetSerialNumber(char *rSN)
{
  unsigned int sn;
  char retSN[10];
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }

  sn = sdFirmwareReadAddr(0x7F0000);

  sprintf(retSN,"SD-%03d",sn&0xffff);
  if(rSN!=NULL)
    {
      strcpy((char *)rSN,retSN);
    }


  printf("%s: SD Serial Number is %s (0x%08x)\n",
	 __FUNCTION__,retSN,sn);

  return sn;

}

/**
   @ingroup Test
   @brief Return the mask of busy's asserted from the payload ports.

   @return Payload Port Mask of asserted busy's if successful, otherwise ERROR.
 */

int
sdTestGetBusyout()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->busyoutTest);
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Return the asserted level from the TI over the SDlink.

   @return Value stored at sdLinkTest register if successful, otherwise ERROR.
 */

int
sdTestGetSdLink()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->sdLinkTest);
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Return payload port mask of those payload modules asserting TokenIN.

   @return Payload Port Mask of asserted TokenIN's if successful, otherwise ERROR.
 */

int
sdTestGetTokenIn()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->tokenInTest);
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Return payload port mask of those payload modules asserting TrigOUT.

   @return Payload Port Mask of asserted TrigOUT's if successful, otherwise ERROR.
 */

int
sdTestGetTrigOut()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->trigOutTest);
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Set TokenOUT levels HI to those payload ports specified in mask

   @param mask Mask to set TokenOUT levels HI
 */

void
sdTestSetTokenOutMask(int mask)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return;
    }
  if(mask>0xffff)
    {
      printf("%s: ERROR: Mask out of range (0x%x)\n",__FUNCTION__,mask);
      return;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return;
    }

  SDLOCK;
  vmeWrite32(&SDp->tokenOutTest,mask);
  SDUNLOCK;

}

/**
   @ingroup Test
   @brief Set StatBitB levels HI to those payload ports specified in mask

   @param mask Mask to set StatBitB levels HI
 */

void
sdTestSetStatBitBMask(int mask)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return;
    }
  if(mask>0xffff)
    {
      printf("%s: ERROR: Mask out of range (0x%x)\n",__FUNCTION__,mask);
      return;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return;
    }

  SDLOCK;
  vmeWrite32(&SDp->statBitBTest,mask);
  SDUNLOCK;

}

/**
   @ingroup Test
   @brief Set the PLL mode for Clock A (LHS of crate).

   @param mode PLL mode
     - 0 : Bypass Mode
     - 1 : PLL mode
 */

void
sdTestSetClkAPLL(int mode)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return;
    }

  if(mode>=1) mode=1;
  else mode=0;

  SDLOCK;
  vmeWrite32(&SDp->csrTest,mode |
	     SD_CSRTEST_CLKA_FREQ |
	     SD_CSRTEST_CLKB_FREQ |
	     SD_CSRTEST_TEST_RESET);
  SDUNLOCK;

}

/**
   @ingroup Test
   @brief Return the amount of clocks ticks that occurred for a two second period after
    the Clock A Mode was set.

   @return Clock count if successful, otherwise ERROR.
*/

int
sdTestGetClockAStatus()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_CLKA_TEST_STATUS)>>1;
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Return the Clock A Frequency.
       This test was bypassed during acceptance testing.

   @return  if successful, otherwise ERROR.
*/

int
sdTestGetClockAFreq()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_CLKA_FREQ)>>2;
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Set the PLL mode for Clock B (LHS of crate).

   @param mode PLL mode
     - 0 : Bypass Mode
     - 1 : PLL mode
 */

void
sdTestSetClkBPLL(int mode)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return;
    }

  if(mode>=1) mode=1;
  else mode=0;

  SDLOCK;
  vmeWrite32(&SDp->csrTest,(mode<<4) | SD_CSRTEST_TEST_RESET);
  SDUNLOCK;

}

/**
   @ingroup Test
   @brief Return the amount of clocks ticks that occurred for a two second period after
    the Clock A Mode was set.

   @return Clock count if successful, otherwise ERROR.
*/

int
sdTestGetClockBStatus()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_CLKB_TEST_STATUS)>>5;
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Return the Clock B Frequency.
       This test was bypassed during acceptance testing.

   @return  if successful, otherwise ERROR.
*/

int
sdTestGetClockBFreq()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_CLKB_FREQ)>>6;
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Set the Busy OUT level to the TI
   @param level Busy Out level
     -  0: Lo
     - !0: Hi
*/

void
sdTestSetTIBusyOut(int level)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return;
    }

  if(level>=1) level=1;
  else level=0;

  SDLOCK;
  if(level)
    vmeWrite32(&SDp->csrTest,SD_CSRTEST_TI_BUSYOUT | SD_CSRTEST_TEST_RESET);
  else
    vmeWrite32(&SDp->csrTest,0 | SD_CSRTEST_TEST_RESET);
  SDUNLOCK;
}

/**
   @ingroup Test
   @brief Get the input level from the TI TokenIN
   @return 1 if Hi, 0 if Lo, otherwise ERROR
*/

int
sdTestGetTITokenIn()
{
  int rval;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_TI_TOKENIN) ? 1 : 0;
  printf("%s: csrTest = 0x%08x  rval = %d\n",
	 __FUNCTION__, vmeRead32(&SDp->csrTest), rval);
  SDUNLOCK;

  return rval;

}

/**
   @ingroup Test
   @brief Set the GTP Link output to the TI
   @param level GTP Link output level
     -  0: Lo
     - !0: Hi
*/

void
sdTestSetTIGTPLink(int level)
{
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return;
    }

  if(level>=1) level=1;
  else level=0;

  SDLOCK;
  if(level)
    vmeWrite32(&SDp->csrTest,SD_CSRTEST_TI_GTPLINK | SD_CSRTEST_TEST_RESET);
  else
    vmeWrite32(&SDp->csrTest,0 | SD_CSRTEST_TEST_RESET);
  SDUNLOCK;
}

/**
   @ingroup Test
   @brief Get the counter associated with the Clock A Frequency during period of 0.1ms.
   @return Counter value if successful, otherwise ERROR
*/

unsigned int
sdTestGetClkACounter()
{
  unsigned int rval=0;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return 0;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->clkACounterTest);
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Get the counter associated with the Clock B Frequency during period of 0.1ms.
   @return Counter value if successful, otherwise ERROR
*/

unsigned int
sdTestGetClkBCounter()
{
  unsigned int rval=0;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return 0;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = vmeRead32(&SDp->clkBCounterTest);
  SDUNLOCK;

  return rval;
}

/**
   @ingroup Test
   @brief Return the bit mask of levels sent from the SWA Module.
       This test was bypassed during acceptance testing.
   @return SWA Bitmask if successful, otherwise ERROR
*/

unsigned int
sdTestGetSWALoopback()
{
  unsigned int rval=0;
  if(SDp==NULL)
    {
      printf("%s: ERROR: SD not initialized\n",__FUNCTION__);
      return 0;
    }
  if(!sdTestMode)
    {
      printf("%s: ERROR: SD Test Mode Jumper not installed\n",__FUNCTION__);
      return ERROR;
    }

  SDLOCK;
  rval = (vmeRead32(&SDp->csrTest) & SD_CSRTEST_SWA_LOOPBACK_MASK);
  SDUNLOCK;

  return rval;
}
