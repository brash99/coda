/*----------------------------------------------------------------------------*
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
 *     Status and Control library for the JLAB Crate Trigger Processor
 *     (CTP) module using an i2c interface from the JLAB Trigger
 *     Interface (TI) module.
 *
 *----------------------------------------------------------------------------*/

#ifdef VXWORKS
#include <vxWorks.h>
#include <sysLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <vxLib.h>
#include "vxCompat.h"
#else
#include "jvme.h"
#endif
#include "ctpLib.h"
#include "tiLib.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

/* Mutex to guard CTP read/writes */
pthread_mutex_t   ctpMutex = PTHREAD_MUTEX_INITIALIZER;
#define CTPLOCK     if(pthread_mutex_lock(&ctpMutex)<0) perror("pthread_mutex_lock");
#define CTPUNLOCK   if(pthread_mutex_unlock(&ctpMutex)<0) perror("pthread_mutex_unlock");

/* This is the CTP base relative to the TI base VME address */
#define CTPBASE 0x30000 

/* Global Variables */
volatile struct CTPStruct  *CTPp=NULL;    /* pointer to CTP memory map */

/* External TI Local Pointer */
extern volatile struct TI_A24RegStruct *TIp;

/* FPGA Channel number to Payload Port Map */
#define NUM_CTP_FPGA 3
#define NUM_FADC_CHANNELS 6 /* 5 for VLX50, 6 for VLX110 */
unsigned int ctpPayloadPort[NUM_CTP_FPGA][NUM_FADC_CHANNELS] =
  {
    /* U1 */
    {  7,  9, 11, 13, 15,  0},  
    /* U3 */
    {  8, 10, 12, 14, 16,  0},
    /* U24 */
    {  3,  1,  5,  2,  4,  6}
  };
enum ifpga {U1, U3, U24, NFPGA};

/* Firmware updating variables/functions */
#define MAX_FIRMWARE_SIZE 8000000
static unsigned int  fw_fpgaid=-1;
static unsigned char fw_data[MAX_FIRMWARE_SIZE];
static unsigned int fw_data_size=0;
static unsigned int my_fw_data_size=0;
static int my_erase_n_sectors=0;
static int fw_file_loaded=0;
static int u24FirmwareVersion=0;


/* Static function prototypes */
static int ctpSROMRead(int addr, int ntries);
static int hex2num(char c);
static int ctpCROMErase(int fpga);
static int ctpWaitForCommandDone(int ntries, int display);
static int ctpWriteFirmwareToSRAM();
static int ctpVerifySRAMData();
static int ctpProgramCROMfromSRAM(int ifpga);
static int ctpWriteCROMToSRAM(int ifpga);
static int ctpRebootAllFPGA();
/*
  ctpInit
  - Initialize the Crate Trigger Processor

  Arguments:
       flag  - Initialization flags
          bits : description
            0  : Ignore module version

*/
int
ctpInit(int flag)
{
  unsigned long tiBase=0, ctpBase=0;
  unsigned int version[NFPGA]={0,0,0};
  char sfpga[NFPGA][4] = {"U1", "U3", "U24"};
  int fIgnoreVersion=0;
  int ifpga=0, versionFail=0;

  if(TIp==NULL)
    {
      printf("%s: ERROR: TI not initialized\n",__FUNCTION__);
      return ERROR;
    }

  /* Do something here to verify that we've got good i2c to the CTP */
  /* Verify that the ctp registers are in the correct space for the TID I2C */
  tiBase = (unsigned long)TIp;
  ctpBase = (unsigned long)&(TIp->SWA[0]);

  if(flag & CTP_INIT_IGNORE_VERSION)
    {
      fIgnoreVersion=1;
      printf("%s: INFO: Initialization without respecting Library-Firmware version\n",
	     __FUNCTION__);
    }
  
  if(ctpBase-tiBase != CTPBASE)
    {
      printf("%s: ERROR: CTP memory structure not in correct VME Space!\n",
	     __FUNCTION__);
      printf("   current base = 0x%lx   expected base = 0x%lx\n",
	     ctpBase-tiBase, (unsigned long)CTPBASE);
      return ERROR;
    }

  CTPp = (struct CTPStruct *)(&TIp->SWA[0]);

  CTPLOCK;
  version[U1] = vmeRead32(&CTPp->fpga1.status2) & CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK;
  version[U3] = vmeRead32(&CTPp->fpga3.status2) & CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK;
  u24FirmwareVersion = version[U24] = 
    vmeRead32(&CTPp->fpga24.status2) & CTP_FPGA24_STATUS2_FIRMWARE_VERSION_MASK;
  CTPUNLOCK;

  /* Check for minimal firmware versions */
  for(ifpga = U1; ifpga<NFPGA; ifpga++)
    {
      if(version[ifpga] & CTP_FIRMWARE_BETA_VERSION_MASK)
	{
	  printf("%s: WARN: %s Firmware Version (0x%x) is a BETA Version!\n",
		 __FUNCTION__,sfpga[ifpga],version[ifpga]);
	}

      if(ifpga == U24)
	{
	  /* Check for U24 compatibility */
	  int supported_u24[CTP_SUPPORTED_U24_FIRMWARE_NUMBER]
	    = {CTP_SUPPORTED_U24_FIRMWARE};
	  int icheck=0, supported=0;

	  for(icheck=0; icheck<CTP_SUPPORTED_U24_FIRMWARE_NUMBER; icheck++)
	    {
	      if(version[ifpga] == supported_u24[icheck])
		supported=1;
	    }
	  if(supported==0)
	    {
	      if(fIgnoreVersion)
		{
		  printf("%s: WARN: %s Firmware Version (0x%x) not supported by this driver.\n",
			 __FUNCTION__,sfpga[ifpga],version[ifpga]);
		}
	      else
		{
		  printf("%s: ERROR: %s Firmware Version (0x%x) not supported by this driver.\n",
			 __FUNCTION__,sfpga[ifpga],version[ifpga]);
		  versionFail=1;
		}
	      printf("           Firmware version ");
	      for(icheck=0; icheck<CTP_SUPPORTED_U24_FIRMWARE_NUMBER; icheck++)
		{
		  printf("0x%x ",supported_u24[icheck]);
		}
		  
	      printf("required.");
	      if(fIgnoreVersion)
		printf(" (Ignored)\n");
	      else
		printf("\n");

	    }
	  
	}
      else
	{
	  int supported_u=0;
	  if(ifpga == U1)
	    supported_u = CTP_SUPPORTED_U1_FIRMWARE;
	  else if(ifpga == U3)
	    supported_u = CTP_SUPPORTED_U3_FIRMWARE;

	  /* Check U1/U3 compatibility */
	  if(version[ifpga] < supported_u)
	    {
	      if(fIgnoreVersion)
		{
		  printf("%s: WARN: %s Firmware Version (0x%x) not supported by this driver.\n",
			 __FUNCTION__,sfpga[ifpga],version[ifpga]);
		  printf("           Firmware version 0x%x required. (Ignored)\n", supported_u);
			 
		}
	      else
		{
		  printf("%s: ERROR: %s Firmware Version (0x%x) not supported by this driver.\n",
			 __FUNCTION__,sfpga[ifpga],version[ifpga]);
		  printf("           Firmware version 0x%x required.\n", supported_u);
		  versionFail=1;
		}
	    }
	}
    }
  
  if(versionFail==1)
    return ERROR;

  printf("%s: CTP (U1: 0x%04x U3: 0x%04x U24: 0x%04x) initialized at Local Base address 0x%lx\n",
	 __FUNCTION__,
	 version[U1], version[U3], version[U24],
	 (unsigned long) CTPp);

  /* Reset the fiber links... this needs to be done after the TI clock switchover, 
     So do it here */
/*   ctpPayloadReset(); */
  ctpFiberReset();

  return OK;

}

/*
  ctpStatus
  - Display the status of the CTP registers 

  ARGs: pflag
     bits:   explaination
        0    Display information in the form of VME slots (as opposed to 
             payload ports)
        1    Display raw register information

*/
int
ctpStatus(int pflag)
{
  struct CTP_FPGA_U1_Struct fpga[NFPGA]; // Array to handle the "common" registers
  char sfpga[NFPGA][4] = {"U1", "U3", "U24"};
  int ichan, ifpga, payloadport, ii, ibegin=0, iend=0;
  int lane0_up[22], lane1_up[22];    /* Stored payload port that has it's "lane up" */
  unsigned int lane0_up_mask=0, lane1_up_mask=0;
  int channel_up[22]; /* Stored payload port that has it's "channel up" */
  unsigned int channel_up_mask=0;
  int firmware_version[NFPGA];
  unsigned int threshold_lsb, threshold_msb;
  unsigned int vmeslotmask=0;
  int displayVme=0, displayRegs=0;
  unsigned int u24config2=0, u24config3=0, u24config4=0;
  int spect=0;
  char spectMode[3][10] = 
    {
      "StCT",
      "PSS",
      "PSCT"
    };

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(pflag & (1<<0))
    displayVme=1;

  if(pflag & (1<<1))
    displayRegs=1;

  CTPLOCK;
  fpga[U1].status0 = vmeRead32(&CTPp->fpga1.status0);
  fpga[U1].status1 = vmeRead32(&CTPp->fpga1.status1);
  fpga[U1].status2 = vmeRead32(&CTPp->fpga1.status2);
  fpga[U3].status0 = vmeRead32(&CTPp->fpga3.status0);
  fpga[U3].status1 = vmeRead32(&CTPp->fpga3.status1);
  fpga[U3].status2 = vmeRead32(&CTPp->fpga3.status2);
  fpga[U24].status0 = vmeRead32(&CTPp->fpga24.status0);
  fpga[U24].status1 = vmeRead32(&CTPp->fpga24.status1);
  fpga[U24].status2 = vmeRead32(&CTPp->fpga24.status2);

  fpga[U1].temp    = vmeRead32(&CTPp->fpga1.temp);
  fpga[U3].temp    = vmeRead32(&CTPp->fpga3.temp);
  fpga[U24].temp    = vmeRead32(&CTPp->fpga24.temp);

  fpga[U1].vint    = vmeRead32(&CTPp->fpga1.vint);
  fpga[U3].vint    = vmeRead32(&CTPp->fpga3.vint);
  fpga[U24].vint    = vmeRead32(&CTPp->fpga24.vint);

  fpga[U1].config0 = vmeRead32(&CTPp->fpga1.config0);
  fpga[U3].config0 = vmeRead32(&CTPp->fpga3.config0);
  fpga[U24].config0 = vmeRead32(&CTPp->fpga24.config0);

  u24config2 = vmeRead32(&CTPp->fpga24.config2);
  u24config3 = vmeRead32(&CTPp->fpga24.config3);
  u24config4 = vmeRead32(&CTPp->fpga24.config4);

  threshold_lsb = vmeRead32(&CTPp->fpga24.sum_threshold_lsb);
  threshold_msb = vmeRead32(&CTPp->fpga24.sum_threshold_msb);
  CTPUNLOCK;

  /* Loop over FPGAs and Channels to get the detailed status info. */
  for(ichan=0; ichan<6; ichan++)
    {
      for(ifpga=U1; ifpga<NFPGA; ifpga++)
	{
	  payloadport = ctpPayloadPort[ifpga][ichan];
	  if(payloadport==0)
	    continue;
	  
	  /* Get MGT Channel Up Status */
	  switch(payloadport)
	    {
	    case 15:
	    case 16:
	    case 4:
	      channel_up[payloadport] = fpga[ifpga].status1 & CTP_FPGA_STATUS1_CHANUP_EXTRA1;
	      break;
	      
	    case 6:
	      channel_up[payloadport] = fpga[ifpga].status1 & CTP_FPGA_STATUS1_CHANUP_EXTRA2;
	      break;
	      
	    default:
	      channel_up[payloadport] = fpga[ifpga].status0 & CTP_FPGA_STATUS0_CHAN_UP(ichan);
	      
	    }

	  /* Get MGT Lane0/1 Up Status */
	  lane0_up[payloadport] = fpga[ifpga].status0 & CTP_FPGA_STATUS0_LANE0_UP(ichan);
	  lane1_up[payloadport] = fpga[ifpga].status0 & CTP_FPGA_STATUS0_LANE1_UP(ichan);

	  if(lane0_up[payloadport])
	    lane0_up_mask |= 1<<(payloadport-1);
	  if(lane1_up[payloadport])
	    lane1_up_mask |= 1<<(payloadport-1);
	  if(channel_up[payloadport])
	     channel_up_mask |= 1<<(payloadport-1);
	}
    }

  if(displayVme)
    {
      ibegin=2; iend=21;
    }
  else
    {
      ibegin=1; iend=17;
    }

  /* Get the firmware versions */
  for(ifpga=U1; ifpga<NFPGA; ifpga++)
    firmware_version[ifpga] = 
      fpga[ifpga].status2 & CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK;
  
  /* Now printout what we've got */
  printf("STATUS for Crate Trigger Processor (CTP)\n");
  printf("--------------------------------------------------------------------------------\n");

  printf("  FPGA firmware versions:\n");
  for(ifpga=U1;ifpga<NFPGA;ifpga++)
    {
      printf("       %3s: 0x%04x\n",sfpga[ifpga],firmware_version[ifpga]);
    }
  printf("\n");

  if(displayRegs) 
    {
      printf("   Raw Regs:\n");
      for(ifpga=0;ifpga<NFPGA;ifpga++)
	{
	  printf("  %s: status0 0x%04x    status1 0x%04x\n",
		 sfpga[ifpga],fpga[ifpga].status0,fpga[ifpga].status1);
	  printf("  %s: temp    0x%04x    vint    0x%04x\n",
		 sfpga[ifpga],fpga[ifpga].temp,fpga[ifpga].vint);
	  printf("  %s: config0 0x%04x\n",
		 sfpga[ifpga],fpga[ifpga].config0);
	  if(ifpga==U24)
	    {
	      printf("  %s: thr_lsb 0x%04x    thr_msb 0x%04x\n",
		     sfpga[ifpga],threshold_lsb,threshold_msb);
	    }
	  printf("\n");
	}
    }

  if(displayVme)
    printf("  VME Slots lanes up: \n\t");
  else
    printf("  Payload port lanes up: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(lane0_up_mask);
  printf(" 0: ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if((lane0_up_mask) & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n\t");
  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(lane1_up_mask);
  printf(" 1: ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if((lane1_up_mask) & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n");

#ifdef SHOWDOWN
  if(displayVme)
    printf("  VME Slots lanes down: \n\t");
  else
    printf("  Payload port lanes down: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask((~lane0_up_mask)&0xffff);
  printf(" 0: ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if((~lane0_up_mask) & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n\t");
  vmeslotmask = vxsPayloadPortMask2vmeSlotMask((~lane1_up_mask)&0xffff);
  printf(" 1: ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if((~lane1_up_mask) & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }

  printf("\n");
#endif

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(channel_up_mask);
  if(displayVme)
    printf("  VME Slots Channels up:\n\t");
  else
    printf("  Payload port Channels up:\n\t");

  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if((channel_up_mask) & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n");

#ifdef SHOWDOWN
  vmeslotmask = vxsPayloadPortMask2vmeSlotMask((~channel_up_mask)&0xffff);
  if(displayVme)
    printf("  VME Slots Channels down:\n\t");
  else
    printf("  Payload port Channels down:\n\t");

  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if((~channel_up_mask) & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }
  printf("\n");
#endif

  if(displayVme)
    printf("  VME Slots Enabled: \n\t");
  else
    printf("  Payload ports Enabled: \n\t");

  vmeslotmask = vxsPayloadPortMask2vmeSlotMask(fpga[U1].config0);
  printf("    ");
  for(ii=ibegin; ii<iend; ii++)
    {
      if(displayVme)
	{
	  if(vmeslotmask & (1<<ii))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
      else
	{
	  if(fpga[U1].config0 & (1<<(ii-1)))
	    printf("%2d ",ii);
	  else
	    printf("   ");
	}
    }

  printf("\n\n");

  printf("  Threshold lsb = %d (0x%04x)\n",threshold_lsb,threshold_lsb);
  printf("  Threshold msb = %d (0x%04x)\n",threshold_msb,threshold_msb);

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 == CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("\n");
      printf("  BCAL U24 Firmware Parameters\n");
      printf("      Front Panel Input Mask : 0x%x\n",
	     (u24config2 &CTP_FPGA24_CONFIG3_FP_INPUT_MASK)>>12);
      printf("      Window Width           : %d (%d ns)\n",
	     u24config3 & CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK,
	     (u24config3 & CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK)*4);
      printf("      Threshold              : %d\n",
	     u24config4 & CTP_FPGA24_CONFIG4_BCAL_THRESHOLD);

      printf("\n\n");
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 == CTP_FIRMWARE_PROJECT_MICROSCOPE)
    {
      printf("\n");
      printf("  Microscope/Hodoscope U24 Firmware Parameters\n");
      printf("    Mode: %s\n",
	     (u24config3 & CTP_FPGA24_CONFIG3_HODOSCOPE_SELECT)?"Hodoscope":"Microscope");
      printf("    Window width: %d\n",
	     (u24config3 & CTP_FPGA24_CONFIG3_HODOSCOPE_WIDTH_MASK)>>7);

      printf("\n\n");
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 == CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      spect = ((u24config3 & CTP_FPGA24_CONFIG3_SPECTROMETER_COUNTER_MODE_MASK)>>14);
      printf("\n");
      printf("  Spectrometer U24 Firmware Parameters\n");
      printf("    Mode: %s\n", spectMode[spect]);
      printf("    Window width: %d\n",
	     (u24config3 & CTP_FPGA24_CONFIG3_SPECTROMETER_WINDOW_WIDTH_MASK));

      printf("\n\n");
    }
  printf("--------------------------------------------------------------------------------\n");
  printf("\n\n");

  return OK;
}

/*
  ctpSetFinalSumThreshold
  - Set the threshold for the Final Sum
  - Arm the trigger, if specified

*/
int
ctpSetFinalSumThreshold(unsigned int threshold, int arm)
{
  unsigned int threshold_lsb, threshold_msb;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if(arm < 0 || arm > 1)
    {
      printf("%s: Invalid value for arm (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,arm);
      return ERROR;
    }
  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) does not support this routine.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }


  threshold_lsb = threshold&0xffff;
  threshold_msb = threshold>>16;


  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.sum_threshold_lsb, threshold_lsb);
  vmeWrite32(&CTPp->fpga24.sum_threshold_msb, threshold_msb);

  threshold_lsb = vmeRead32(&CTPp->fpga24.sum_threshold_lsb);
  threshold_msb = vmeRead32(&CTPp->fpga24.sum_threshold_msb);

  CTPUNLOCK;

  if(arm)
    {
      ctpArmHistoryBuffer();
    }

  return OK;
}

/*
  ctpGetFinalSumThreshold
  - Return the value set for the Final Sum threshold

*/
int
ctpGetFinalSumThreshold(int pflag)
{
  unsigned int rval;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) does not support this outine.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.sum_threshold_lsb);
  rval |= (vmeRead32(&CTPp->fpga24.sum_threshold_msb)<<16);
  CTPUNLOCK;

  if(pflag)
    {
      printf("%s: Set to %d (0x%x)\n",
	     __FUNCTION__,rval, rval);
    }
  return rval;
}

/*
  ctpSetHistoryBufferTrigger
  - Set the trigger bit mask for the history buffer
  - Arm the trigger, if specified

*/
int
ctpSetHistoryBufferTriggerMask(unsigned int mask, int arm)
{
  unsigned int trig_lsb, trig_msb;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_MICROSCOPE)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) does not support this routine.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  if(arm < 0 || arm > 1)
    {
      printf("%s: Invalid value for arm (%d).  Must be 0 or 1.\n",
	     __FUNCTION__,arm);
      return ERROR;
    }


  trig_lsb = mask&0xffff;
  trig_msb = mask>>16;


  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.sum_threshold_lsb, trig_lsb);
  vmeWrite32(&CTPp->fpga24.sum_threshold_msb, trig_msb);

  trig_lsb = vmeRead32(&CTPp->fpga24.sum_threshold_lsb);
  trig_msb = vmeRead32(&CTPp->fpga24.sum_threshold_msb);

  CTPUNLOCK;

  if(arm)
    {
      ctpArmHistoryBuffer();
    }

  return OK;


}

/*
  ctpGetHistoryBufferTrigger
  - Return the value set for the History Buffer Trigger mask

*/
unsigned int
ctpGetHistoryBufferTriggerMask(int pflag)
{
  unsigned int rval;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_MICROSCOPE)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) does no support this routine.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.sum_threshold_lsb);
  rval |= (vmeRead32(&CTPp->fpga24.sum_threshold_msb)<<16);
  CTPUNLOCK;

  if(pflag)
    {
      printf("%s: Set to %d (0x%x)\n",
	     __FUNCTION__,rval, rval);
    }
  return rval;
}

/*
  ctpSetPayloadEnableMask
  - Set the payload ports from the input MASK to be enabled.
  RETURNS: OK if successful, otherwise ERROR.
  - Mask Convention: 
    bit 0: Port 1
    bit 1: Port 2
    ...
    bit 5: Port 6
    .etc.

*/

int
ctpSetPayloadEnableMask(int enableMask)
{
  unsigned int chipMask=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if( enableMask >= (1<<(17-1)) ) /* 16 = Maximum Payload port number */
    {
      printf("%s: ERROR: Invalid enableMask (0x%x).  Includes payload port > 16.\n",
	     __FUNCTION__,enableMask);
      return ERROR;
    }

  CTPLOCK;
  chipMask = enableMask;
  vmeWrite32(&CTPp->fpga1.config0,chipMask);
  vmeWrite32(&CTPp->fpga3.config0,chipMask);
  vmeWrite32(&CTPp->fpga24.config0,chipMask);
  CTPUNLOCK;

  printf("%s: Set enable mask to 0x%08x\n",__FUNCTION__,chipMask);

  return OK;
}

/*
  ctpSetVmeSlotEnableMask
  - Set the VME slots from the input MASK to be enabled.
  - Mask Convention: 
    bit  0: Vme Slot 0 (non-existant)
    bit  1: Vme Slot 1 (controller slot)
    bit  2: Vme Slot 2 (not used by CTP)
    bit  3: Vme Slot 3 (First slot on the LHS of crate that is used by CTP)
    ..
    bit 20: Vme Slot 20 (Last slot that is used by the CTP)
    bit 21: Vme Slot 21 (Slot for the TID)

  RETURNS: OK if successful, otherwise ERROR.

*/

int
ctpSetVmeSlotEnableMask(unsigned int vmemask)
{
  unsigned int payloadmask=0;

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
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
  payloadmask = vmeSlotMask2vxsPayloadPortMask(vmemask);

  ctpSetPayloadEnableMask(payloadmask);

  return OK;

}


/*
  ctpGetAllChanUp 
  - Returns the status of all configured channels up, from each chip where
    bit0: U1
    bit1: U3
    bit2: U24
*/

int
ctpGetAllChanUp(int pflag)
{
  int chip1, chip3, chip24;

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  chip1  = vmeRead32(&CTPp->fpga1.status1) & (CTP_FGPA_STATUS1_ALLCHANUP);
  chip3  = vmeRead32(&CTPp->fpga3.status1) & (CTP_FGPA_STATUS1_ALLCHANUP);
  chip24 = vmeRead32(&CTPp->fpga24.status1) & (CTP_FGPA_STATUS1_ALLCHANUP);
  CTPUNLOCK;

  if(pflag)
    {
      printf("%s: chip1 = %d, chip3 = %d, chip24 = %d\n",
	     __FUNCTION__,chip1,chip3,chip24);
    }

  return (chip1>>1) | chip3 | (chip24<<1);

}

int
ctpGetErrorLatchFS(int pflag)
{
  int rval=0;

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.status1) & (CTP_FPGA24_STATUS1_ERROR_LATCH_FS);
  CTPUNLOCK;

  if(rval)
    rval=1;

  if(pflag)
    {
      if(rval)
	printf("%s: ERROR: Bad summing sequence!\n",__FUNCTION__);
      else
	printf("%s: Summing sequence is OK.\n",__FUNCTION__);
    }

  return (rval);

}

int
ctpGetAlignmentStatus(int pflag, int ntries)
{
  int itry=0, rval=0;

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  for(itry=0; itry<ntries; itry++)
    {
      rval = vmeRead32(&CTPp->fpga24.status1) & CTP_FPGA24_STATUS1_ALIGNMENT_SUCCESS;
      
      if(rval)
	{
	  rval=1;
	  break;
	}
    }

  CTPUNLOCK;

  if(pflag)
    {
      if(rval==0)
	printf("%s: ERROR: Bad Alignment Status!\n",__FUNCTION__);
      else
	printf("%s: Alignment Status is OK.\n",__FUNCTION__);
    }

  return (rval);

}


int
ctpAlignAtSyncReset(int enable)
{
  unsigned int reg1=0, reg3=0, reg24=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  reg1  = vmeRead32(&CTPp->fpga1.config1) & 0xFFFF;
  reg3  = vmeRead32(&CTPp->fpga3.config1) & 0xFFFF;
  reg24 = vmeRead32(&CTPp->fpga24.config1) & 0xFFFF;

  if(enable)
    {
      vmeWrite32(&CTPp->fpga1.config1, reg1  | CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET);
      vmeWrite32(&CTPp->fpga3.config1, reg3  | CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET);
      vmeWrite32(&CTPp->fpga24.config1,reg24 | CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET);
    }
  else
    {
      vmeWrite32(&CTPp->fpga1.config1, reg1  & ~CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET);
      vmeWrite32(&CTPp->fpga3.config1, reg3  & ~CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET);
      vmeWrite32(&CTPp->fpga24.config1,reg24 & ~CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET);
    }
  CTPUNLOCK;

  return OK;


}

int
ctpArmHistoryBuffer()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config1,CTP_FPGA24_CONFIG1_ARM_HISTORY_BUFFER);
  vmeWrite32(&CTPp->fpga24.config1,0);
  CTPUNLOCK;

  return OK;
}

int
ctpDReady()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.status1) & CTP_FPGA24_STATUS1_HISTORY_BUFFER_READY;
  CTPUNLOCK;
  
  if(rval)
    rval=1;

  return rval;
}

int
ctpReadEvent(volatile unsigned int *data, int nwrds)
{
  int ii=0, dCnt=0;
  if(CTPp==NULL)
    {
      logMsg("\nctpReadEvent: ERROR: CTP not initialized\n",0,0,0,0,0,0);
      return ERROR;
    }
  if(data==NULL) 
    {
      logMsg("\nctpReadEvent: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }
  if(nwrds>512)
    {
      logMsg("\nctpReadEvent: ERROR: Invalid nwrds (%d).  Must be less than 512.\n",
	     nwrds,0,0,0,0,0);
      return ERROR;
    }

  CTPLOCK;
  while(ii<nwrds)
    {
      data[ii] = (vmeRead32(&CTPp->fpga24.history_buffer_lsb) 
		  | (vmeRead32(&CTPp->fpga24.history_buffer_msb)<<16)) & CTP_DATA_MASK;
#ifndef VXWORKS
      data[ii] = LSWAP(data[ii]);
#endif
      ii++;
    }
  ii++;

  /* Use this to clear the data ready bit (dont set back to zero) */
  vmeWrite32(&CTPp->fpga24.config1,CTP_FPGA24_CONFIG1_ARM_HISTORY_BUFFER);
  CTPUNLOCK;

  dCnt += ii;
  return dCnt;

}

void
ctpFiberReset()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config1, CTP_FPGA24_CONFIG1_RESET_FIBER_MGT);
  vmeWrite32(&CTPp->fpga24.config1, 0);
  CTPUNLOCK;

}

void
ctpPayloadReset()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga1.config1, CTP_FGPA_CONFIG1_INIT_ALL_MGT);
  vmeWrite32(&CTPp->fpga1.config1, 0);

  vmeWrite32(&CTPp->fpga3.config1, CTP_FGPA_CONFIG1_INIT_ALL_MGT);
  vmeWrite32(&CTPp->fpga3.config1, 0);

  vmeWrite32(&CTPp->fpga24.config1, CTP_FGPA_CONFIG1_INIT_ALL_MGT);
  vmeWrite32(&CTPp->fpga24.config1, 0);
  CTPUNLOCK;

}

int
ctpResetScalers()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.scaler_ctrl,
	     CTP_SCALER_CTRL_RESET_SYNC |
	     CTP_SCALER_CTRL_RESET_TRIG1 |
	     CTP_SCALER_CTRL_RESET_TRIG2);
  vmeWrite32(&CTPp->fpga24.scaler_ctrl,0);
  CTPUNLOCK;

  return OK;
}

int
ctpResetSyncScaler()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, CTP_SCALER_CTRL_RESET_SYNC);
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, 0);
  CTPUNLOCK;

  return ERROR;
}

int
ctpResetTrig1Scaler()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, CTP_SCALER_CTRL_RESET_TRIG1);
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, 0);
  CTPUNLOCK;

  return ERROR;
}

int
ctpResetTrig2Scaler()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, CTP_SCALER_CTRL_RESET_TRIG2);
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, 0);
  CTPUNLOCK;

  return ERROR;
}

int
ctpGetClockScaler()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.clock_scaler) & CTP_CLOCK_SCALER_COUNT_MASK;
  CTPUNLOCK;

  return rval;
}

int
ctpGetSyncScaler()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.sync_scaler) & CTP_SCALER_COUNT_MASK;
  CTPUNLOCK;

  return rval;

}

int
ctpGetTrig1Scaler()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.trig1_scaler) & CTP_SCALER_COUNT_MASK;
  CTPUNLOCK;

  return rval;

}

int
ctpGetTrig2Scaler()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.trig2_scaler) & CTP_SCALER_COUNT_MASK;
  CTPUNLOCK;

  return rval;

}

int
ctpGetSerialNumber(char **rval)
{
  int iaddr=0, byte=0;
  int sn[8], ret_len;
  char sn_str[20];
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  for(iaddr=0; iaddr<8; iaddr++)
    {
      byte = ctpSROMRead(iaddr,1000);
      if(byte==-1)
	{
	  printf("%s: ERROR Reading SROM (addr = %d)\n",__FUNCTION__,iaddr);
	  return ERROR;
	}
      sn[iaddr] = byte;
    }
  printf("\n");

  sprintf(sn_str,"%c%c%c%c%c-%c%c%c",sn[0],sn[1],sn[2],sn[3],sn[4],sn[5],sn[6],sn[7]);

  if(*rval != NULL)
    {
      strcpy((char *)rval,sn_str);
      ret_len = (int)strlen(sn_str);
    }
  else
    ret_len = 0;
  
  printf("%s: CTP Serial Number: %s\n",__FUNCTION__,sn_str);

  return ret_len;
}

static int
ctpSROMRead(int addr, int ntries)
{
  int itry, rval, dataValid=0;
  int maxAddr=CTP_FPGA3_CONFIG2_SROM_ADDR_MASK;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(addr>maxAddr)
    {
      printf("%s: ERROR: addr (0x%x) > maxAddr (0x%x)\n",
	     __FUNCTION__,addr,maxAddr);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga3.config2, 0);
  vmeWrite32(&CTPp->fpga3.config2, addr | CTP_FPGA3_CONFIG2_SROM_READ);
  vmeWrite32(&CTPp->fpga3.config2, addr);

  for(itry=0; itry<ntries; itry++)
    {
      rval = vmeRead32(&CTPp->fpga3.status3);
      if(rval & CTP_FPGA3_STATUS3_SROM_DATA_VALID)
	{
	  rval &= CTP_FPGA3_STATUS3_SROM_DATA_MASK;
	  dataValid=1;
	  break;
	}
    }

  vmeWrite32(&CTPp->fpga3.config2, 0);
  CTPUNLOCK;

  if(!dataValid)
    {
      printf("%s: Timeout on SROM Read\n",__FUNCTION__);
      rval = ERROR;
    }

  return rval;
}



int
ctpSetCrateID(int crateID)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((crateID<0)||(crateID>0xFFFF))
    {
      printf("%s: ERROR: Invalid crateID (%d)\n",__FUNCTION__,crateID);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config2, crateID);
  CTPUNLOCK;

  return OK;
}

int
ctpGetCrateID()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.config2) & 0xFFFF;
  CTPUNLOCK;

  return rval;
}

int
ctpGetFirmwareVersion(int fpga)
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fpga!=U1) && (fpga!=U3) && (fpga!=U24))
    {
      printf("%s: ERROR: Invalid FPGA number (%d)\n",
	     __FUNCTION__,fpga);
      return ERROR;
    }

  CTPLOCK;
  switch(fpga)
    {
    case U1:
      rval = (int)(vmeRead32(&CTPp->fpga1.status2) & CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK);
      break;
    case U3:
      rval = (int)(vmeRead32(&CTPp->fpga3.status2) & CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK);
      break;
    case U24:
      rval = (int)(vmeRead32(&CTPp->fpga24.status2) & CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK);
      break;
    }
  CTPUNLOCK;

  return rval;
}

unsigned int
ctpGetChannelUpMask(int fType)
{
  struct CTP_FPGA_U1_Struct fpga[NFPGA]; // Array to handle the "common" registers
  int ichan=0, ifpga;
  unsigned int payloadport=0;
  unsigned int channel_up_mask=0;
  int channel_up[22];
  
  memset((char *)channel_up,0,22);

  CTPLOCK;
  fpga[U1].status0 = vmeRead32(&CTPp->fpga1.status0);
  fpga[U1].status1 = vmeRead32(&CTPp->fpga1.status1);
  fpga[U3].status0 = vmeRead32(&CTPp->fpga3.status0);
  fpga[U3].status1 = vmeRead32(&CTPp->fpga3.status1);
  fpga[U24].status0 = vmeRead32(&CTPp->fpga24.status0);
  fpga[U24].status1 = vmeRead32(&CTPp->fpga24.status1);
  CTPUNLOCK;

  /* Loop over FPGAs and Channels to get the detailed status info. */
  for(ichan=0; ichan<6; ichan++)
    {
      for(ifpga=U1; ifpga<NFPGA; ifpga++)
	{
	  payloadport = ctpPayloadPort[ifpga][ichan];
	  if(payloadport==0)
	    continue;
	  
	  /* Get MGT Channel Up Status */
	  switch(payloadport)
	    {
	    case 15:
	    case 16:
	    case 4:
	      channel_up[payloadport] = fpga[ifpga].status1 & CTP_FPGA_STATUS1_CHANUP_EXTRA1;
	      break;
	      
	    case 6:
	      channel_up[payloadport] = fpga[ifpga].status1 & CTP_FPGA_STATUS1_CHANUP_EXTRA2;
	      break;
	      
	    default:
	      channel_up[payloadport] = fpga[ifpga].status0 & CTP_FPGA_STATUS0_CHAN_UP(ichan);
	      
	    }
	  
	  if(channel_up[payloadport])
	    channel_up_mask |= 1<<(payloadport-1);
	}
    }
  
  if(fType) /* return as vmeslot mask */
    return vxsPayloadPortMask2vmeSlotMask(channel_up_mask);

  return channel_up_mask;
}

unsigned int
ctpGetLaneUpMask(int lane, int fType)
{
  struct CTP_FPGA_U1_Struct fpga[NFPGA]; // Array to handle the "common" registers
  int ichan=0, ifpga;
  unsigned int payloadport=0;
  unsigned int lane_up_mask=0;
  int lane_up[22];
  
  memset((char *)lane_up,0,22);

  CTPLOCK;
  fpga[U1].status0 = vmeRead32(&CTPp->fpga1.status0);
  fpga[U3].status0 = vmeRead32(&CTPp->fpga3.status0);
  fpga[U24].status0 = vmeRead32(&CTPp->fpga24.status0);
  CTPUNLOCK;

  /* Loop over FPGAs and Channels to get the detailed status info. */
  for(ichan=0; ichan<6; ichan++)
    {
      for(ifpga=U1; ifpga<NFPGA; ifpga++)
	{
	  payloadport = ctpPayloadPort[ifpga][ichan];
	  if(payloadport==0)
	    continue;
	  
	  if(lane==0)
	    lane_up[payloadport] = fpga[ifpga].status0 & CTP_FPGA_STATUS0_LANE0_UP(ichan);
	  else
	    lane_up[payloadport] = fpga[ifpga].status0 & CTP_FPGA_STATUS0_LANE1_UP(ichan);


	  if(lane_up[payloadport])
	    lane_up_mask |= 1<<(payloadport-1);
	  
	}
    }
  
  if(fType) /* return as vmeslot mask */
    return vxsPayloadPortMask2vmeSlotMask(lane_up_mask);

  return lane_up_mask;
}

float
ctpGetFPGATemperature(int fpga, int pflag)
{
  float rval=0;
  char sfpga[NFPGA][4] = {"U1", "U3", "U24"};

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((fpga!=U1) && (fpga!=U3) && (fpga!=U24))
    {
      printf("%s: ERROR: Invalid FPGA number (%d)\n",
	     __FUNCTION__,fpga);
      return ERROR;
    }

  switch(fpga)
    {
    case U1:
      rval = CTP_FPGA_TEMP_SLOPE*((float)(vmeRead32(&CTPp->fpga1.temp)>>6 & 0x03FF)) 
	+ CTP_FPGA_TEMP_YINT;
      break;
    case U3:
      rval = CTP_FPGA_TEMP_SLOPE*((float)(vmeRead32(&CTPp->fpga3.temp)>>6 & 0x03FF)) 
	+ CTP_FPGA_TEMP_YINT;
      break;
    case U24:
      rval = CTP_FPGA_TEMP_SLOPE*((float)(vmeRead32(&CTPp->fpga24.temp)>>6 & 0x03FF)) 
	+ CTP_FPGA_TEMP_YINT;
      break;
    }

  if(pflag)
    {
      printf("%s: %s FPGA Die Temperature = %.2f degC\n",__FUNCTION__,
	     sfpga[fpga],rval);
    }

  return rval;
}


/*************************************************************
 * Routines for BCAL Cosmics: U24 v06xx
 *************************************************************/
int
ctpSetFPInputMask(int mask)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if((mask<0) || (mask>0xF))
    {
      printf("%s: ERROR: Invalid mask (0x%x)\n",
	     __FUNCTION__,mask);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config3,
	     (vmeRead32(&CTPp->fpga24.config3) &~ CTP_FPGA24_CONFIG3_FP_INPUT_MASK) |
	     mask<<12);
  CTPUNLOCK;

  return OK;
} 

int
ctpGetFPInputMask()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_FP_INPUT_MASK)>>12;
  CTPUNLOCK;

  return rval;
}

int
ctpSetBCALWindowWidth(int width)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if(width>CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK)
    {
      printf("%s: ERROR: Invalid width (%d)\n",
	     __FUNCTION__,width);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config3,
	     (vmeRead32(&CTPp->fpga24.config3) & ~CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK) |
	     width);
  CTPUNLOCK;

  return OK;
}

int
ctpGetBCALWindowWidth()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK;
  CTPUNLOCK;

  return rval;
}

int
ctpSetBCALThreshold(int thres)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if(thres>CTP_FPGA24_CONFIG4_BCAL_THRESHOLD)
    {
      printf("%s: ERROR: Invalid threshold (%d)\n",
	     __FUNCTION__,thres);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config4,thres & CTP_FPGA24_CONFIG4_BCAL_THRESHOLD);
  CTPUNLOCK;

  return OK;
}

int
ctpGetBCALThreshold()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  
  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.config4) & CTP_FPGA24_CONFIG4_BCAL_THRESHOLD;
  CTPUNLOCK;

  return rval;
}

int
ctpSetBCALScalerInhibitWindow(int width)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if(width>0xF)
    {
      printf("%s: ERROR: Invalid width (%d)\n",
	     __FUNCTION__,width);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config3,
	     (vmeRead32(&CTPp->fpga24.config3) & ~CTP_FPGA24_CONFIG3_BCAL_SCALERS_INHIBIT_WINDOW_MASK) |
	     (width<<8));
  CTPUNLOCK;

  return OK;
}

int
ctpGetBCALScalerInhibitWindow(int width)
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_BCAL_SCALERS_INHIBIT_WINDOW_MASK)>>8;
  CTPUNLOCK;

  return rval;
}

int
ctpSetBCALScalerThresholds(int thres1, int thres2, int thres3, int thres4)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if((thres1<0) || (thres1>0xFFFFF))
    {
      printf("%s: ERROR: Invalid Scaler 1 threshold (%d)\n",
	     __FUNCTION__,thres1);
      return ERROR;
    }
  if((thres2<0) || (thres2>0xFFFFF))
    {
      printf("%s: ERROR: Invalid Scaler 2 threshold (%d)\n",
	     __FUNCTION__,thres1);
      return ERROR;
    }
  if((thres3<0) || (thres3>0xFFFFF))
    {
      printf("%s: ERROR: Invalid Scaler 3 threshold (%d)\n",
	     __FUNCTION__,thres1);
      return ERROR;
    }
  if((thres4<0) || (thres4>0xFFFFF))
    {
      printf("%s: ERROR: Invalid Scaler 4 threshold (%d)\n",
	     __FUNCTION__,thres1);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config5, thres1 & 0xFFFF);
  vmeWrite32(&CTPp->fpga24.config6, thres2 & 0xFFFF);
  vmeWrite32(&CTPp->fpga24.config7, thres3 & 0xFFFF);
  vmeWrite32(&CTPp->fpga24.config8, thres4 & 0xFFFF);

  vmeWrite32(&CTPp->fpga24.config9, 
	     (thres4 & 0xF0000)>>4 | 
	     (thres3 & 0xF0000)>>8 |
	     (thres2 & 0xF0000)>>12 | 
	     (thres1 & 0xF0000)>>16 );
  CTPUNLOCK;

  return OK;
}

int
ctpGetBCALScalerThreshold(int scal)
{
  int rval=0;
  int msb=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if((scal<1) || (scal>4))
    {
      printf("%s: ERROR: Invalid Scaler (%d)\n",
	     __FUNCTION__,scal);
      return ERROR;
    }

  CTPLOCK;
  msb = vmeRead32(&CTPp->fpga24.config9) & 0xFFFF;
  switch(scal)
    {
    case 1:
      msb = (msb & CTP_FPGA24_CONFIG9_SCALER1_MSB_MASK)<<16;
      rval = (vmeRead32(&CTPp->fpga24.config5) & 0xFFFF) | msb;
      break;

    case 2:
      msb = (msb & CTP_FPGA24_CONFIG9_SCALER2_MSB_MASK)<<12;
      rval = (vmeRead32(&CTPp->fpga24.config6) & 0xFFFF) | msb;
      break;

    case 3:
      msb = (msb & CTP_FPGA24_CONFIG9_SCALER3_MSB_MASK)<<8;
      rval = (vmeRead32(&CTPp->fpga24.config7) & 0xFFFF) | msb;
      break;

    case 4:
      msb = (msb & CTP_FPGA24_CONFIG9_SCALER4_MSB_MASK)<<4;
      rval = (vmeRead32(&CTPp->fpga24.config8) & 0xFFFF) | msb;
      break;
    }
  CTPUNLOCK;

  return rval;
}

int
ctpReadBCALData(int latch, int clear, volatile unsigned int *data)
{
  int doLatch=0, doClear=0;
  int dCnt=0;
  if(CTPp==NULL)
    {
      logMsg("ctpReadBCALData: ERROR: CTP not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      logMsg("ctpReadBCALData: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     u24FirmwareVersion,2,3,4,5,6);
      return ERROR;
    }
  if(data==NULL) 
    {
      logMsg("\nctpReadEvent: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  if(latch)
    doLatch=CTP_SCALER_CTRL_BCAL_RISING_EDGE_LATCH;

  if(clear)
    doClear=CTP_SCALER_CTRL_BCAL_CLEAR_COUNTERS_AFTER_READ;

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, doLatch | doClear);

  /* Timestamp data */
  data[0] = vmeRead32(&CTPp->fpga24.status5) & 0xFFFF;
  data[1] = (vmeRead32(&CTPp->fpga24.status4) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status3) & 0xFFFF);

  /* Scalers 1 */
  data[2] = (vmeRead32(&CTPp->fpga24.status7) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status6) & 0xFFFF);

  /* Scalers 2 */
  data[3] = (vmeRead32(&CTPp->fpga24.status9) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status8) & 0xFFFF);

  /* Scalers 3 */
  data[4] = (vmeRead32(&CTPp->fpga24.status11) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status10) & 0xFFFF);

  /* Scalers 4 */
  data[5] = (vmeRead32(&CTPp->fpga24.status13) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status12) & 0xFFFF);

  dCnt=6;

  vmeWrite32(&CTPp->fpga24.scaler_ctrl, 0);

  CTPUNLOCK;

  return dCnt;
}

int
ctpPrintBCALData(int latch, int clear)
{
  volatile unsigned int data[6];
  int ndata=0;

  if(CTPp==NULL)
    {
      logMsg("ctpReadBCALData: ERROR: CTP not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_BCAL)
    {
      logMsg("ctpReadBCALData: ERROR: U24 Firmware (0x%x) not supported for BCAL Routines.\n",
	     u24FirmwareVersion,2,3,4,5,6);
      return ERROR;
    }

  ndata = ctpReadBCALData(latch, clear, (unsigned int *)&data);
  if(ndata==6)
    {
      printf("Timestamp         : 0x%04x 0x%08x\n",data[0],data[1]);
      printf("Scaler 1          : %d\n",data[2]);
      printf("Scaler 2          : %d\n",data[3]);
      printf("Scaler 3          : %d\n",data[4]);
      printf("Scaler 4          : %d\n",data[5]);
    }
  else
    {
      printf("%s: ERROR: Unexpected number of data words (%d)\n",
	     __FUNCTION__,ndata);
      return ERROR;
    }

  return OK;
}

/*************************************************************
 * Routines for Hodoscope/Microscope Trigger: U24 v0axx
 *************************************************************/
int
ctpSetScopeTrigger(int type, int windowwidth)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_MICROSCOPE)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Hodoscope/Microscope Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if((type<0) || (type>1))
    {
      printf("%s: ERROR: Invalid type (%d)\n",
	     __FUNCTION__,type);
      return ERROR;
    }
  
  if((windowwidth<0) || (windowwidth>0xF))
    {
      printf("%s: ERROR: Invalid window width (0x%x)\n",
	     __FUNCTION__,windowwidth);
      return ERROR;
    }

  CTPLOCK;
  switch(type)
    {
    case 0: /* Microscope trigger */
      vmeWrite32(&CTPp->fpga24.config3,
		 (windowwidth<<7));
      break;

    case 1: /* Hodoscope trigger */
      vmeWrite32(&CTPp->fpga24.config3,
		 CTP_FPGA24_CONFIG3_HODOSCOPE_SELECT |
		 (windowwidth<<7));
      break;
    }
  CTPUNLOCK;
  
  return OK;
}

int
ctpGetScopeTrigger(int pflag)
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_MICROSCOPE)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Hodoscope/Microscope Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  
  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_HODOSCOPE_SELECT)>>11;
  CTPUNLOCK;

  if(pflag)
    {
      if(rval)
	printf("%s: Hodoscope trigger algorithm selected\n",__FUNCTION__);
      else
	printf("%s: Microscope trigger algorithm selected\n",__FUNCTION__);
    }

  return rval;
}

int
ctpGetScopeTriggerWidth()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_MICROSCOPE)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Hodoscope/Microscope Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  
  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_MICROSCOPE_WIDTH_MASK)>>7;
  CTPUNLOCK;

  return rval;
}

/*************************************************************
 * Routines for Spectrometer Trigger: U24 v0exx
 *************************************************************/

int
ctpSetSpectrometerMode(int mode, int width)
{
  char spectMode[3][10] = 
    {
      "StCT",
      "PSS",
      "PSCT"
    };

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  
  if((mode<0) || (mode>2))
    {
      printf("%s: ERROR: Invalid mode (%d)\n",
	     __FUNCTION__,mode);
      return ERROR;
    }

  if((width<0)||(width>CTP_FPGA24_CONFIG3_SPECTROMETER_WINDOW_WIDTH_MASK))
    {
      printf("%s: ERROR: Invalid width (%d)\n",
	     __FUNCTION__,width);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config3, (mode<<14) | (width));
  CTPUNLOCK;

  printf("%s: Mode set to %s\n",
	 __FUNCTION__,spectMode[mode]);

  return OK;
}

int
ctpGetSpectrometerMode()
{
  int rval=0;
  char spectMode[3][10] = 
    {
      "StCT",
      "PSS",
      "PSCT"
    };

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  
  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config3) & 
	  CTP_FPGA24_CONFIG3_SPECTROMETER_COUNTER_MODE_MASK)>>14;
  CTPUNLOCK;

  printf("%s: Mode set to %s\n",
	 __FUNCTION__,spectMode[rval]);

  return rval;
}

int
ctpGetSpectrometerWidth()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }
  
  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_SPECTROMETER_WINDOW_WIDTH_MASK);
  CTPUNLOCK;

  return rval;
}

int
ctpReadSpectrometerData(int latch, int clear, volatile unsigned int *data)
{
  int doLatch=0, doClear=0;
  int dCnt=0;
  if(CTPp==NULL)
    {
      logMsg("ctpReadSpectrometerData: ERROR: CTP not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      logMsg("ctpReadSpectrometerData: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     u24FirmwareVersion,2,3,4,5,6);
      return ERROR;
    }
  if(data==NULL) 
    {
      logMsg("\nctpReadEvent: ERROR: Invalid Destination address\n",0,0,0,0,0,0);
      return(ERROR);
    }

  if(latch)
    doLatch=CTP_SCALER_CTRL_SPECTROMETER_RISING_EDGE_LATCH;

  if(clear)
    doClear=CTP_SCALER_CTRL_SPECTROMETER_CLEAR_COUNTERS_AFTER_READ;

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.scaler_ctrl, doLatch | doClear);

  /* Timestamp data */
  data[0] = vmeRead32(&CTPp->fpga24.status5) & 0xFFFF;
  data[1] = (vmeRead32(&CTPp->fpga24.status4) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status3) & 0xFFFF);

  /* Left Arm Counter */
  data[2] = (vmeRead32(&CTPp->fpga24.status7) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status6) & 0xFFFF);

  /* Right Arm Counter */
  data[3] = (vmeRead32(&CTPp->fpga24.status9) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status8) & 0xFFFF);

  /* Coincident Hit Counter */
  data[4] = (vmeRead32(&CTPp->fpga24.status11) & 0xFFFF)<<16 |
    (vmeRead32(&CTPp->fpga24.status10) & 0xFFFF);

  /* Hit channel */
  data[5] = (vmeRead32(&CTPp->fpga24.status12) & 0xFFFF);

  dCnt=6;

  vmeWrite32(&CTPp->fpga24.scaler_ctrl, 0);

  CTPUNLOCK;

  return dCnt;
}

int
ctpPrintSpectrometerData(int latch, int clear)
{
  volatile unsigned int data[6];
  int ndata=0;

  if(CTPp==NULL)
    {
      logMsg("ctpReadSpectrometerData: ERROR: CTP not initialized\n",1,2,3,4,5,6);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      logMsg("ctpReadSpectrometerData: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     u24FirmwareVersion,2,3,4,5,6);
      return ERROR;
    }

  ndata = ctpReadSpectrometerData(latch, clear, (unsigned int *)&data);
  if(ndata>0)
    {
      printf("Timestamp         : 0x%04x 0x%08x\n",data[0],data[1]);
      printf("Left Arm Counter  : %d\n",data[2]);
      printf("Right Arm Counter : %d\n",data[3]);
      printf("Coinc Counter     : %d\n",data[4]);
      printf("Hit channel   (R) : %d   (L) : %d",
	     (data[5] & 0xFF00)>>8,(data[5] & 0xFF));
    }

  return OK;
}

int
ctpSetSpectrometerFPOutput(int enable)
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  if(enable)
    vmeWrite32(&CTPp->fpga24.config1, 
	       vmeRead32(&CTPp->fpga24.config1) | CTP_FPGA24_CONFIG1_SPECTROMETER_FP_OUTPUT);
  else
    vmeWrite32(&CTPp->fpga24.config1, 
	       vmeRead32(&CTPp->fpga24.config1) & ~CTP_FPGA24_CONFIG1_SPECTROMETER_FP_OUTPUT);
  CTPUNLOCK;

  return OK;
}

int
ctpGetSpectrometerFPOutput()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_SPECTROMETER)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for Spectrometer Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = (vmeRead32(&CTPp->fpga24.config1) & CTP_FPGA24_CONFIG1_SPECTROMETER_FP_OUTPUT)>>4;
  CTPUNLOCK;

  return rval;
}


/*************************************************************
 * Routines for TOF Trigger: U24 v12xx
 *************************************************************/
int
ctpSetTOFCoincWidth(int windowwidth)
{

  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_TOF)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for TOF Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  if((windowwidth<0) || (windowwidth>0xF))
    {
      printf("%s: ERROR: Invalid window width (0x%x)\n",
	     __FUNCTION__,windowwidth);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga24.config3, windowwidth);
  CTPUNLOCK;

  return OK;
}


int
ctpGetTOFCoincWidth()
{
  int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((u24FirmwareVersion & CTP_FIRMWARE_PROJECT_MASK)>>10 != CTP_FIRMWARE_PROJECT_TOF)
    {
      printf("%s: ERROR: U24 Firmware (0x%x) not supported for TOF Routines.\n",
	     __FUNCTION__,u24FirmwareVersion);
      return ERROR;
    }

  CTPLOCK;
  rval = vmeRead32(&CTPp->fpga24.config3) & CTP_FPGA24_CONFIG3_TOF_WIDTH_MASK;

  CTPUNLOCK;

  return rval;
}




/*************************************************************
 * Firmware Updating Tools
 *************************************************************/
int
ctpSetFWSize(int size)
{
  if(size<=MAX_FIRMWARE_SIZE)
    {
      printf("%s: INFO: Setting override firmware size to %d\n",
	     __FUNCTION__,size);
      my_fw_data_size=size;
    }
  else
    {
      printf("%s: ERROR: Override size (%d) greater than maximum size (%d)\n",
	     __FUNCTION__,size,MAX_FIRMWARE_SIZE);
      return ERROR;
    }
  return OK;
}

int
ctpSetNSectorErase(int nsectors)
{
  if(nsectors<=1024)
    {
      printf("%s: INFO: Setting override erase sectors to %d\n",
	     __FUNCTION__,nsectors);
      my_erase_n_sectors=nsectors;
    }
  else
    {
      printf("%s: ERROR: Override erase # sectors (%d) greater than 1024\n",
	     __FUNCTION__,nsectors);
      return ERROR;
    }
  return OK;
}

int
ctpFirmwareUpload(int ifpga, int reboot)
{
  int stat;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if((ifpga<U1) | (ifpga>U24))
    {
      printf("%s: Invalid FPGA choice (%d)\n",__FUNCTION__,ifpga);
      return ERROR;
    }

  if(my_fw_data_size!=0)
    {
      if(my_fw_data_size>fw_data_size)
	{
	  printf("%s: ERROR: Override firmware size (%d) > original size (%d)\n",
		 __FUNCTION__,
		 fw_data_size, my_fw_data_size);
	  return ERROR;
	}
      printf("%s: INFO: Overriding firmware size (%d) with %d\n",
	     __FUNCTION__,
	     fw_data_size, my_fw_data_size);
      fw_data_size = my_fw_data_size;
    }
 
  /* Erase CROM */
  printf("\n%s: Erasing CROM \n",__FUNCTION__);
  stat = ctpCROMErase(ifpga);
  if(stat==ERROR)
    return ERROR;
  
  /* Data to SRAM */
  printf("\n%s: Loading SRAM with data \n",__FUNCTION__);
  stat = ctpWriteFirmwareToSRAM();
  if(stat==ERROR)
    return ERROR;

#ifdef SKIP
  /* Compare SRAM to Data Array */
  printf("\n%s: Verifying data \n",__FUNCTION__);
  stat = ctpVerifySRAMData();
  if(stat==ERROR)
    return ERROR;
#endif

  /* SRAM TO CROM */
  printf("\n%s: Loading CROM with SRAM data \n",__FUNCTION__);
  stat = ctpProgramCROMfromSRAM(ifpga);
  if(stat==ERROR)
    return ERROR;

  /* CROM TO SRAM (For verification) */
  printf("\n%s: Loading SRAM with CROM data \n",__FUNCTION__);
  stat = ctpWriteCROMToSRAM(ifpga);
  if(stat==ERROR)
    return ERROR;

  /* Compare SRAM to Data Array */
  printf("\n%s: Verifying data \n",__FUNCTION__);
  stat = ctpVerifySRAMData();
  if(stat==ERROR)
    return ERROR;

  if(reboot)
    {
      /* CROM to FPGA (Reboot FPGA) */
      printf("\n%s: Rebooting FPGAs \n",__FUNCTION__);
      stat = ctpRebootAllFPGA();
      if(stat==ERROR)
	return ERROR;
    }

  printf("\n%s: Done programming CTP FPGA %d\n",
	 __FUNCTION__,ifpga);

  return OK;
}

static int
hex2num(char c)
{
  c = toupper(c);

  if(c > 'F')
    return 0;

  if(c >= 'A')
    return 10 + c - 'A';

  if((c > '9') || (c < '0') )
    return 0;

  return c - '0';
}


int
ctpReadFirmwareFile(char *fw_filename)
{
  FILE *fwFile=NULL;
  char ihexLine[200], *pData;
  int len=0, datalen=0, byte_shift;
  unsigned int line=0, nbytes=0, hiChar=0, loChar=0;
  unsigned int readFWfile=0;

  /* Initialize global variables */
  memset((char *)fw_data,0,sizeof(fw_data));
  fw_data_size=0;
  fw_file_loaded=0;

  memset((char *)ihexLine,0,sizeof(ihexLine));

  fwFile = fopen(fw_filename,"r");
  if(fwFile==NULL)
    {
      perror("fopen");
      printf("%s: ERROR opening file (%s) for reading\n",
	     __FUNCTION__,fw_filename);
      return ERROR;
    }

  while(!feof(fwFile))
    {
      /* Get the current line */
      if(!fgets(ihexLine, sizeof(ihexLine), fwFile))
	break;
      
      /* Get the the length of this line */
      len = strlen(ihexLine);

      if(len >= 5)
	{
	  /* Check for the start code */
	  if(ihexLine[0] != ':')
	    {
	      printf("%s: ERROR parsing file at line %d\n",
		     __FUNCTION__,line);
	      return ERROR;
	    }

	  /* Get the byte count */
	  hiChar = hex2num(ihexLine[1]);
	  loChar = hex2num(ihexLine[2]);
	  datalen = (hiChar)<<4 | loChar;

	  if(strncmp("00",&ihexLine[7], 2) == 0) /* Data Record */
	    {
	      pData = &ihexLine[9]; /* point to the beginning of the data */
	      while(datalen--)
		{
		  hiChar = hex2num(*pData++);
		  loChar = hex2num(*pData++);
		  fw_data[readFWfile] = 
		    ((hiChar)<<4) | (loChar);
		  if(readFWfile>=MAX_FIRMWARE_SIZE)
		    {
		      printf("%s: ERROR: TOO BIG!\n",__FUNCTION__);
		      return ERROR;
		    }
		  readFWfile++;
		  nbytes++;
		}
	    }
	  else if(strncmp("01",&ihexLine[7], 2) == 0) /* End of File, contains FPGA ID */
	    {
	      byte_shift=24;
	      fw_fpgaid=0;
	      pData = &ihexLine[9]; /* point to the beginning of the data */
	      while(datalen--)
		{
		  if(byte_shift<0)
		    {
		      printf("%s: ERROR: FPGA ID too large!\n",__FUNCTION__);
		      return ERROR;
		    }
		  hiChar = hex2num(*pData++);
		  loChar = hex2num(*pData++);

		  fw_fpgaid |= ((hiChar<<4) | (loChar))<<byte_shift;
#ifdef DEBUGFILE
		  printf("%2d: fw_fpgaid = 0x%08x\n",datalen,fw_fpgaid);
#endif
		  byte_shift -= 8;
		}
	    }

	}
      line++;
    }

  fw_data_size = readFWfile;
  
#ifdef DEBUGFILE
  printf("fw_data_size = %d (0x%x)\n",fw_data_size, fw_data_size);

  int ichar=0;
  for(ichar=0; ichar<16*10; ichar++)
    {
      if((ichar%16) == 0)
	printf("\n");
      printf("0x%02x ",fw_data[ichar]);
    }
  printf("\n\n");
#endif
  fw_file_loaded = 1;

  fclose(fwFile);
  return OK;


}

int
ctpFirmwareRevFromFpgaID(int pflag)
{
  int rval=0;

  rval = fw_fpgaid & 0xFFFF;

  if(pflag)
    {
      printf("%s: Rev = 0x%x\n",__FUNCTION__,rval);
    }
  return rval;
}

int
ctpFirmwareChipFromFpgaID(int pflag)
{
  int rval=0;
  unsigned int id;

  id = (fw_fpgaid & CTP_FIRMWARE_FPGA_MASK)>>8;

  switch(id)
    {
    case CTP_FIRMWARE_FPGA_U1:
      rval = U1;
      break;
    case CTP_FIRMWARE_FPGA_U3:
      rval = U3;
      break;
    case CTP_FIRMWARE_FPGA_U24:
      rval = U24;
      break;
    default:
      rval = ERROR;
    }

  if(pflag)
    {
      if(rval!=ERROR)
	{
	  printf("%s: ID = 0x%08x FPGA Chip = %d\n",
		 __FUNCTION__,fw_fpgaid,rval);
	}
      else
	{
	  printf("%s: ID (0x%08x) does not match any available FPGAs for this program\n",
		 __FUNCTION__,fw_fpgaid);
	}
    }
  
  return rval;
}


static int
ctpCROMErase(int fpga)
{
  int iblock=0, stat=0;
  int nsectors = 1024; /* NOTE U1: U3: 400, U24: 583 */
  unsigned int eraseCommand=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  switch(fpga)
    {
    case U1: 
      eraseCommand=CTP_FPGA1_CONFIG2_U1_CONFIG | CTP_FPGA1_CONFIG2_ERASE_EPROM_U1;
      break;

    case U3:
      eraseCommand=CTP_FPGA1_CONFIG2_U3_CONFIG | CTP_FPGA1_CONFIG2_ERASE_EPROM_U3;
      break;

    case U24:
      eraseCommand=CTP_FPGA1_CONFIG2_U24_CONFIG | CTP_FPGA1_CONFIG2_ERASE_EPROM_U24;
      break;

    default:
      printf("%s: ERROR: Invalid fpga (%d)\n",__FUNCTION__,fpga);
      return ERROR;
    }

  if(my_erase_n_sectors!=0)
    {
      printf("%s: INFO: Overriding Erase sectors to %d\n",
	     __FUNCTION__,
	     my_erase_n_sectors);
      nsectors = my_erase_n_sectors;
    }

  CTPLOCK;
  for(iblock=0; iblock<nsectors; iblock++)
    {
      /* Write block number to erase */
      vmeWrite32(&CTPp->fpga1.config3, iblock);

      /* Beginning of opCode */
      vmeWrite32(&CTPp->fpga1.config2,eraseCommand);
      vmeWrite32(&CTPp->fpga1.config2,eraseCommand | CTP_FPGA1_CONFIG2_EXEC_OPCODE);
      stat = ctpWaitForCommandDone(1000,0);
      if(stat==ERROR)
	{
	  printf("%s: ERROR Sending Opcode when erasing fpga (%d)\n",__FUNCTION__,fpga);
	  CTPUNLOCK;
	  return ERROR;
	}

      /* End of opCode */
      vmeWrite32(&CTPp->fpga1.config2,eraseCommand);

      /* Display 60 *'s of progress */
      if((iblock%17)==0)
	{
	  printf("*"); fflush(stdout);
	}
      
      /* Wait for at least 220 milliseconds */
      taskDelay(13);
    }
  CTPUNLOCK;

  return OK;
}

static int
ctpWaitForCommandDone(int ntries, int display)
{
  int itries=0, done=0;
  unsigned int rval=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }
  
  for(itries=0; itries<ntries; itries++)
    {
      rval = vmeRead32(&CTPp->fpga1.status2);
/*       printf("%s: rval = 0x%04x\n",__FUNCTION__,rval); */
      if(rval & CTP_FPGA1_STATUS2_READY_FOR_OPCODE)
	{
	  done=1;
	  break;
	}
      taskDelay(1);
      if(display)
	{
	  if((itries%display)==0)
	    {
	      printf("."); fflush(stdout);
	    }
	}
    }

#ifdef DEBUGCOMMAND
  printf("\n");
  printf("%s: done = %d  itries = %d  ntries = %d\n",__FUNCTION__,
	 done, itries, ntries);
#endif

  if(!done)
    rval = ERROR;
  else
    rval = OK;

  return rval;
}


static int
ctpWriteFirmwareToSRAM()
{
  unsigned int stat=0;
  int ibyte=0, iaddr=0;
  unsigned short data=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!fw_file_loaded | (fw_data_size==0))
    {
      printf("%s: ERROR: Firmware file not loaded.\n",
	     __FUNCTION__);
      return ERROR;
    }
  
  CTPLOCK;
  /* Make sure opCode ready */
  stat = ctpWaitForCommandDone(100,0);
  if(stat!=OK)
    {
      printf("%s: ERROR: OPCode wait timeout.\n",__FUNCTION__);
      CTPUNLOCK;
      return ERROR;
    }
  
  /* Enter in the Download opCode */
  vmeWrite32(&CTPp->fpga1.config2, CTP_FPGA1_CONFIG2_SRAM_WRITE);

  for(ibyte = 0; ibyte<fw_data_size; ibyte+=2)
    {
      data = (fw_data[ibyte]<<8) | fw_data[ibyte+1];
      vmeWrite32(&CTPp->fpga1.config4, data);
      vmeWrite32(&CTPp->fpga1.config5, iaddr & CTP_FPGA1_CONFIG5_SRAM_ADDR_MASK);
      vmeWrite32(&CTPp->fpga1.config6, CTP_FPGA1_CONFIG6_SRAM_WRITE | 
		 ((iaddr>>16) & CTP_FPGA1_CONFIG6_SRAM_ADDR_MASK));

      stat = ctpWaitForCommandDone(1000,0);
      if(stat!=OK)
	{
	  printf("%s: ERROR: OPCode wait timeout.\n",__FUNCTION__);
	  CTPUNLOCK;
	  return ERROR;
	}

      vmeWrite32(&CTPp->fpga1.config6, 
		 ((iaddr>>16) & CTP_FPGA1_CONFIG6_SRAM_ADDR_MASK));
      iaddr++;
      
      
      if((ibyte%(fw_data_size/120))==0)
	printf("*"); fflush(stdout);

    }
  CTPUNLOCK;

  return OK;
}

static int
ctpVerifySRAMData()
{
  int ibyte=0, iaddr=0, stat=0;
  unsigned int data=0, rdata=0;
  int errorCount=0, zeroCount=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  if(!fw_file_loaded | (fw_data_size==0))
    {
      printf("%s: ERROR: Firmware file not loaded.\n",
	     __FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  /* Select SRAM to READ */
  vmeWrite32(&CTPp->fpga1.config2, CTP_FPGA1_CONFIG2_SRAM_WRITE);

  for(ibyte=0; ibyte<fw_data_size; ibyte+=2)
    {
      data = (fw_data[ibyte]<<8) | fw_data[ibyte+1];
      vmeWrite32(&CTPp->fpga1.config5, iaddr & CTP_FPGA1_CONFIG5_SRAM_ADDR_MASK);
      vmeWrite32(&CTPp->fpga1.config6, CTP_FPGA1_CONFIG6_SRAM_READ |
		 ((iaddr>>16) & CTP_FPGA1_CONFIG6_SRAM_ADDR_MASK));
		 
      stat = ctpWaitForCommandDone(1000,0);
      if(stat!=OK)
	{
	  printf("%s: ERROR: OPCode wait timeout.\n",__FUNCTION__);
	  CTPUNLOCK;
	  return ERROR;
	}
      
      vmeWrite32(&CTPp->fpga1.config6, 
		 ((iaddr>>16) & CTP_FPGA1_CONFIG6_SRAM_ADDR_MASK));

      rdata = vmeRead32(&CTPp->fpga1.status3) & CTP_FPGA1_STATUS3_SRAM_DATA_MASK;
      if(rdata==0)
	zeroCount++;

      if(rdata != data)
	{
	  errorCount++;
	  if(errorCount<=20)
	    {
	      printf("%s: ERROR: Invalid data read from SRAM (iaddr = 0x%x).\n\tExpected (0x%x) != Readback (0x%x)\n",
		     __FUNCTION__,iaddr,data,rdata);
	      if(errorCount==20)
		printf("%s: ERROR: Further errors are suppressed\n",
		       __FUNCTION__);
	    }
/* 	  CTPUNLOCK; */
/* 	  return ERROR; */
	}
      iaddr++;

      if((ibyte%(fw_data_size/120))==0)
	{
	  printf("."); fflush(stdout);
	}

    }

  CTPUNLOCK;

  if(errorCount>0)
    {
      printf("%s: ERROR: Total data read errors = %d (out of %d)\n",
	     __FUNCTION__,errorCount,fw_data_size/2);
      return ERROR;
    }

/*   printf("%s: INFO: Zero Count = %d\n",__FUNCTION__,zeroCount); */

  return OK;
}


static int
ctpProgramCROMfromSRAM(int ifpga)
{
  unsigned int opCode=0;
  int stat=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  switch(ifpga)
    {
    case U1:
      opCode = CTP_FPGA1_CONFIG2_U1_CONFIG | CTP_FPGA1_CONFIG2_PROG_DATA_U1;
      break;

    case U3:
      opCode = CTP_FPGA1_CONFIG2_U3_CONFIG | CTP_FPGA1_CONFIG2_PROG_DATA_U3;
      break;

    case U24:
      opCode = CTP_FPGA1_CONFIG2_U24_CONFIG | CTP_FPGA1_CONFIG2_PROG_DATA_U24;
      break;

    default:
      printf("%s: Invalid FPGA choice (%d).\n",__FUNCTION__,ifpga);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga1.config2, opCode);
  vmeWrite32(&CTPp->fpga1.config2, opCode | CTP_FPGA1_CONFIG2_EXEC_OPCODE);

  /* Wait for "13 minutes"... 50000 = ~13.9minutes */
  stat = ctpWaitForCommandDone(100000,880);
  if(stat!=OK)
    {
      printf("%s: ERROR: OPCode (0x%x) wait timeout.\n",__FUNCTION__,opCode);
      CTPUNLOCK;
      return ERROR;
    }

  vmeWrite32(&CTPp->fpga1.config2, opCode);

  CTPUNLOCK;

  return OK;
}

static int
ctpWriteCROMToSRAM(int ifpga)
{
  unsigned int opCode=0;
  int stat=0;
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  switch(ifpga)
    {
    case U1:
      opCode = CTP_FPGA1_CONFIG2_U1_CONFIG | CTP_FPGA1_CONFIG2_READ_DATA_U1;
      break;

    case U3:
      opCode = CTP_FPGA1_CONFIG2_U3_CONFIG | CTP_FPGA1_CONFIG2_READ_DATA_U3;
      break;

    case U24:
      opCode = CTP_FPGA1_CONFIG2_U24_CONFIG | CTP_FPGA1_CONFIG2_READ_DATA_U24;
      break;

    default:
      printf("%s: Invalid FPGA choice (%d).\n",__FUNCTION__,ifpga);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga1.config2, opCode);
  vmeWrite32(&CTPp->fpga1.config2, opCode | CTP_FPGA1_CONFIG2_EXEC_OPCODE);

  stat = ctpWaitForCommandDone(100000,145);
  if(stat!=OK)
    {
      printf("%s: ERROR: OPCode (0x%x) wait timeout.\n",__FUNCTION__,opCode);
      CTPUNLOCK;
      return ERROR;
    }

  vmeWrite32(&CTPp->fpga1.config2, opCode);
  CTPUNLOCK;

  return OK;
}

static int
ctpRebootAllFPGA()
{
  if(CTPp==NULL)
    {
      printf("%s: ERROR: CTP not initialized\n",__FUNCTION__);
      return ERROR;
    }

  CTPLOCK;
  vmeWrite32(&CTPp->fpga3.config3,CTP_FPGA3_CONFIG3_REBOOT_ALL_FPGA);
  vmeWrite32(&CTPp->fpga3.config3,CTP_FPGA3_CONFIG3_REBOOT_SAFETY);
  vmeWrite32(&CTPp->fpga3.config3,CTP_FPGA3_CONFIG3_REBOOT_ALL_FPGA);
  CTPUNLOCK;

  return OK;
}
