/*----------------------------------------------------------------------------*/
/**
 * @mainpage
 * <pre>
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
 *
 *             Gerard Visser
 *             gvisser@indiana.edu
 *             Indiana University
 *
 * __DATE__:
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Driver library for readout of the 125MSPS ADC using vxWorks 5.5
 *     (or later) or Intel based single board computer
 * </pre>
 *----------------------------------------------------------------------------*/

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#ifdef VXWORKS
#include <vxWorks.h>
#include <logLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <iv.h>
#else
#include <stdlib.h>
#include "jvme.h"
#endif
#include <pthread.h>
#include "fa125Lib.h"

#ifndef LSWAP
#define LSWAP(x)        ((((x) & 0x000000ff) << 24) | \
                         (((x) & 0x0000ff00) <<  8) | \
                         (((x) & 0x00ff0000) >>  8) | \
                         (((x) & 0xff000000) >> 24))
#endif

/* Mutex to guard TD read/writes */
pthread_mutex_t    fa125Mutex = PTHREAD_MUTEX_INITIALIZER;
#define FA125LOCK     if(pthread_mutex_lock(&fa125Mutex)<0) perror("pthread_mutex_lock");
#define FA125UNLOCK   if(pthread_mutex_unlock(&fa125Mutex)<0) perror("pthread_mutex_unlock");

/* Define global variables */
int nfa125=0; /* Number of initialized modules */
volatile struct fa125_a24 *fa125p[(FA125_MAX_BOARDS+1)]; /* pointers to FA125 memory map */
volatile struct fa125_a32 *fa125pd[(FA125_MAX_BOARDS+1)]; /* pointers to FA125 FIFO memory */
volatile unsigned int *FA125pmb;                        /* pointer to Multblock window */
int fa125ID[FA125_MAX_BOARDS]; /* array of slot numbers for FA125s */
unsigned int fa125A32Base   = 0x09000000;                      /* Minimum VME A32 Address for use by FA125s */
unsigned long fa125A32Offset = 0x08000000;                      /* Difference in CPU A32 Base - VME A32 Base */
unsigned long fa125A24Offset=0;                            /* Difference in CPU A24 Base and VME A24 Base */
unsigned int fa125AddrList[FA125_MAX_BOARDS];            /* array of a24 addresses for FA125s */
int fa125MaxSlot=0;                                   /* Highest Slot hold an FA125 */
int fa125MinSlot=0;                                   /* Lowest Slot holding an FA125 */
int fa125TriggerSource=0;
int berr_count=0; /* A count of the number of BERR that have occurred when running fa125Poll() */
/* store the dacOffsets in the library, until the firmware is able to read them back */
static unsigned short fa125dacOffset[FA125_MAX_BOARDS+1][72];
int fa125BlockError=FA125_BLOCKERROR_NO_ERROR;       /* Whether (1) or not (0) Block Transfer had an error */

/**
 * @defgroup Config Initialization/Configuration
 * @defgroup PulserConfig Pulser Initialization/Configuration
 *   @ingroup Config
 * @defgroup Status Status
 * @defgroup Readout Data Readout
 * @defgroup FWUpdate Firmware Updating Utilities
 * @defgroup Deprec Deprecated - To be removed
 */

/**
 *  @ingroup Config
 *  @brief Initialize the fa125 Library
 *
 * @param addr
 *  - A24 VME Address of the fADC125
 * @param addr_inc
 *  - Amount to increment addr to find the next fADC125
 * @param nadc
 *  - Number of times to increment
 *
 * @param iFlag 17 bit integer
 * <pre>
 *       Low 6 bits - Specifies the default Signal distribution (clock,trigger)
 *                    sources for the board (Internal, FrontPanel, VXS, VME(Soft))
 *       bit    0:  defines Sync Reset source
 *                 0  VXS (P0)
 *                 1  VME (software)
 *       bits 2-1:  defines Trigger source
 *           (0) 0 0  VXS (P0)
 *           (1) 0 1  Internal Timer
 *           (2) 1 0  Internal Multiplicity Sum
 *           (3) 1 1  P2 Connector (Backplane)
 *       bit    3:  NOT USED WITH THIS FIRMWARE VERSION
 *       bits 5-4:  defines Clock Source
 *           (0) 0 0  P2 Connector (Backplane)
 *           (1) 0 1  VXS (P0)
 *           (2) 1 0  Internal 125MHz Clock
 *       bit   16:  Exit before board initialization (just map structure pointer)
 *                 0: Initialize fa125(s)
 *                 1: Skip initialization
 *       bit   17:  Use fa125AddrList instead of addr and addr_inc
 *                  for VME addresses.
 *                 0: Initialize with addr and addr_inc
 *                 1: Use fa125AddrList
 *       bit   18:  Skip firmware check.  Useful for firmware updating.
 *                 0: Perform firmware check
 *                 :1 Skip firmware check
 * </pre>
 * @return OK, or ERROR if the address is invalid or a board is not present.
 */
int
fa125Init (UINT32 addr, UINT32 addr_inc, int nadc, int iFlag)
{
  int res=0;
  volatile unsigned int rdata=0;
  unsigned long laddr=0;
  unsigned int a32addr=0;
  volatile struct fa125_a24 *fa125;
  int useList=0, noBoardInit=0, noFirmwareCheck=0;
  int nfind=0, islot=0, FA_SLOT=0, ii=0;
  int trigSrc=0, clkSrc=0, srSrc=0;
  unsigned int boardID=0, fw_version=0;
  int maxSlot = 1;
  int minSlot = 21;

  /* Initialize some global variables */
  nfa125=0;
  memset((char *)fa125ID,0,sizeof(fa125ID));
  memset((char *)fa125dacOffset,0,sizeof(fa125dacOffset));

  /* Check if we're skipping initialization, and just mapping the structure pointer */
  if(iFlag & FA125_INIT_SKIP)
    noBoardInit=1;

  /* Check if we're initializing using a list */
  if(iFlag & FA125_INIT_USE_ADDRLIST)
    {
      useList=1;
      nfind = nadc;
    }

  /* Are we skipping the firmware check? */
  if(iFlag & FA125_INIT_SKIP_FIRMWARE_CHECK)
    {
      noFirmwareCheck=1;
      printf("%s: INFO: Firmware Check Disabled\n",
	     __FUNCTION__);
    }


  /* Check for valid address */
  if((addr==0) && (useList==0))
    { /* Loop through JLab Standard GEOADDR to VME addresses to make a list */
      useList=1;
      nfind=16;

      for(islot=3; islot<11; islot++) /* First 8 */
	fa125AddrList[islot-3] = (islot<<19);

      /* Skip Switch Slots */

      for(islot=13; islot<21; islot++) /* Last 8 */
	fa125AddrList[islot-5] = (islot<<19);

    }
  else if(addr > 0x00ffffff)  /* A32 Addressing */
    {
      printf("\n%s: ERROR: A32 Addressing not allowed for FA125 configuration space\n\n",
	     __FUNCTION__);
      return(ERROR);
    }
  else if((addr!=0) && (useList==0))
    { /* A24 Addressing */
      if(addr_inc==0) /* Just one module */
	{
	  fa125AddrList[0] = addr;
	  nfind=1;
	}
      else /* Up to nadc modules */
	{
	  for(islot=0; islot<nadc; islot++)
	    {
	      fa125AddrList[islot] = addr+addr_inc*islot;
	    }
	  nfind=nadc;
	}
    }

  /* Determine the A24 offset from the first module address */
#ifdef VXWORKS
  res = sysBusToLocalAdrs(0x39,(char *)fa125AddrList[0],(char **)&laddr);
#else
  res = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)fa125AddrList[0],(char **)&laddr);
#endif

  if (res != 0)
    {
#ifdef VXWORKS
      printf("\n%s: ERROR in sysBusToLocalAdrs(0x39,0x%x,&laddr) \n\n",
	     __FUNCTION__,addr);
#else
      printf("\n%s: ERROR in vmeBusToLocalAdrs(0x39,0x%x,&laddr) \n\n",
	     __FUNCTION__,addr);
#endif
      return(ERROR);
    }

  fa125A24Offset = laddr - fa125AddrList[0];

  /* Calculate the A32 Offset for use in Block Transfers */
#ifdef VXWORKS
  res = sysBusToLocalAdrs(0x09,(char *)fa125A32Base,(char **)&laddr);
  if (res != 0)
    {
      printf("\n%s: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n\n",
	     __FUNCTION__,fa125A32Base);
      return(ERROR);
    }
  else
    {
      fa125A32Offset = laddr - fa125A32Base;
    }
#else
  res = vmeBusToLocalAdrs(0x09,(char *)(unsigned long)fa125A32Base,(char **)&laddr);
  if (res != 0)
    {
      printf("\n%s: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n\n",
	     __FUNCTION__,fa125A32Base);
      return(ERROR);
    }
  else
    {
      fa125A32Offset = laddr - fa125A32Base;
    }
#endif

  for(islot=0; islot<nfind; islot++)
    {

      fa125 = (volatile struct fa125_a24 *)(fa125AddrList[islot]+fa125A24Offset);
      /* Check if Board exists at that address */
#ifdef VXWORKS
      res = vxMemProbe((char *) &(fa125->main.id),VX_READ,4,(char *)&rdata);
#else
      res = vmeMemProbe((char *)&(fa125->main.id),4,(char *)&rdata);
#endif
      if(res < 0)
	{
	  printf("%s: WARN: No addressable board at addr=0x%x\n",
		 __FUNCTION__,(UINT32) fa125AddrList[islot]);
	}
      else
	{
	  /* Check that it is an FA125 */
	  if(rdata != FA125_ID)
	    {
	      printf("\n%s: ERROR: For module at 0x%x, Invalid Board ID: 0x%x\n\n",
		     __FUNCTION__,fa125AddrList[islot],rdata);
	      continue;
	    }
	  else
	    {
	      /* Check the Firmware Versions */
	      if(!noFirmwareCheck)
		{
		  int fw_error=0;
		  /* MAIN */
		  fw_version = vmeRead32(&fa125->main.version);
		  if(fw_version==0xffffffff)
		    { /* buggy firmware.. re-read */
		      printf("%s: bum main read\n",__FUNCTION__);
		      fw_version = vmeRead32(&fa125->main.version);
		    }
		  if(fw_version != FA125_MAIN_SUPPORTED_FIRMWARE)
		    {
		      printf("\n%s: ERROR: For module at 0x%x, Unsupported MAIN firmware version 0x%x\n\n",
			     __FUNCTION__,fa125AddrList[islot],fw_version);
		      fw_error=1;
		    }

		  /* PROC */
		  fw_version = vmeRead32(&fa125->proc.version);
		  if(fw_version==0xffffffff)
		    { /* buggy firmware.. re-read */
		      printf("%s: bum proc read\n",__FUNCTION__);
		      fw_version = vmeRead32(&fa125->proc.version);
		    }
		  if(fw_version != FA125_PROC_SUPPORTED_FIRMWARE)
		    {
		      printf("\n%s: ERROR: For module at 0x%x, Unsupported PROC firmware version 0x%x\n\n",
			     __FUNCTION__,fa125AddrList[islot],fw_version);
		      fw_error=1;
		    }

		  /* FE - just check the first one */
		  fw_version = vmeRead32(&fa125->fe[0].version);
		  if(fw_version==0xffffffff)
		    { /* buggy firmware.. re-read */
		      printf("%s: bum FE read\n",__FUNCTION__);
		      fw_version = vmeRead32(&fa125->fe[0].version);
		    }
		  if(fw_version != FA125_FE_SUPPORTED_FIRMWARE)
		    {
		      printf("\n%s: ERROR: For module at 0x%x, Unsupported FE firmware version 0x%x\n\n",
			     __FUNCTION__,fa125AddrList[islot],fw_version);
		      fw_error=1;
		    }

		  if(fw_error)
		    continue;
		}

	      /* Get the Geographic Address */
	      boardID = vmeRead32(&fa125->main.slot_ga);

	      if((boardID<2) || (boardID>21))
		{
		  printf("%s: For module at 0x%x, Invalid Slot Number %d\n",
			 __FUNCTION__,fa125AddrList[islot],boardID);
		  continue;
		}

	      if(boardID >= maxSlot) maxSlot = boardID;
	      if(boardID <= minSlot) minSlot = boardID;

	      fa125p[boardID] = (struct fa125_a24 *) (fa125AddrList[islot]+fa125A24Offset);
	      fa125ID[nfa125] = boardID;

	      printf("Initialized FA125 %2d  Slot # %2d at address 0x%08lx (0x%08x)\n",
		     nfa125,fa125ID[nfa125],
		     (unsigned long)fa125p[fa125ID[nfa125]],
		     (unsigned int)((unsigned long)fa125p[fa125ID[nfa125]] - fa125A24Offset));

	    }
	  nfa125++;
	}
    }

  if(noBoardInit)
    {
      if(nfa125>0)
	{
	  printf("%s: %d FA125(s) successfully mapped (not initialized)\n",
		 __FUNCTION__,nfa125);
	  return OK;
	}
    }

  if(nfa125==0)
    {
      printf("\n%s: ERROR: Unable to initialize any FA125 modules\n\n",
	     __FUNCTION__);
      return ERROR;
    }

  /* Determine the clock, sync, trigger configuration */
  /* Sync Reset (only VXS available, ignore) */
  srSrc = 1;

  /* Trigger */
  trigSrc = (iFlag&0x6)>>1;
  fa125TriggerSource = trigSrc;

  /* Clock Source */
  clkSrc = (iFlag&0x30)>>4;

  /* Perform some initialization here */
  for(islot=0; islot<nfa125; islot++)
    {
      FA_SLOT = fa125ID[islot];

      fa125Reset(FA_SLOT, 1);
      fa125Clear(FA_SLOT);

      /* Set the clock source */
      fa125SetClockSource(FA_SLOT,clkSrc);

      /* Set the trigger source */
      fa125SetTriggerSource(FA_SLOT,fa125TriggerSource);

      /* Set the SyncReset source */
      fa125SetSyncResetSource(FA_SLOT,srSrc);
    }

  for(ii=0;ii<nfa125; ii++)
    {

      /* Program an A32 access address for this FA125's FIFO */
      a32addr = fa125A32Base + ii*FA125_MAX_A32_MEM;
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x09,(char *)a32addr,(char **)&laddr);
      if (res != 0)
	{
	  printf("\n%s: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n\n",
		 __FUNCTION__,a32addr);
	  return(ERROR);
	}
#else
      res = vmeBusToLocalAdrs(0x09,(char *)(unsigned long)a32addr,(char **)&laddr);
      if (res != 0)
	{
	  printf("\n%s: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n\n",
		 __FUNCTION__,a32addr);
	  return(ERROR);
	}
#endif
      fa125pd[fa125ID[ii]] = (volatile struct fa125_a32 *)(laddr);  /* Set a pointer to the FIFO */
      if(!noBoardInit)
	{
	  vmeWrite32(&fa125p[fa125ID[ii]]->main.adr32, (a32addr>>16) | FA125_ADR32_ENABLE);  /* Write the register and enable */
	  vmeWrite32(&fa125p[fa125ID[ii]]->main.ctrl1,
			     vmeRead32(&fa125p[fa125ID[ii]]->main.ctrl1) | FA125_CTRL1_ENABLE_BERR);/* Enable Bus Error termination */
	}

    }

  /* If there are more than 1 FA125 in the crate then setup the Muliblock Address
     window. This must be the same on each board in the crate */
  if(nfa125 > 1)
    {
      a32addr = fa125A32Base + (nfa125+1)*FA125_MAX_A32_MEM; /* set MB base above individual board base */
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x09,(char *)a32addr,(char **)&laddr);
      if (res != 0)
	{
	  printf("\n%s: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n\n",__FUNCTION__,a32addr);
	  return(ERROR);
	}
#else
      res = vmeBusToLocalAdrs(0x09,(char *)(unsigned long)a32addr,(char **)&laddr);
      if (res != 0)
	{
	  printf("\n%s: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n\n",__FUNCTION__,a32addr);
	  return(ERROR);
	}
#endif
      FA125pmb = (unsigned int *)(laddr);  /* Set a pointer to the FIFO */
      if(!noBoardInit)
	{
	  unsigned int ctrl1=0;
	  for (ii=0;ii<nfa125;ii++)
	    {
	      /* Write to the register and enable */
	      vmeWrite32(&fa125p[fa125ID[ii]]->main.adr_mb,
			 (a32addr+FA125_MAX_A32MB_SIZE) | (a32addr>>16) | FA125_ADRMB_ENABLE);
	      ctrl1 = vmeRead32(&fa125p[fa125ID[ii]]->main.ctrl1) &
		~(FA125_CTRL1_FIRST_BOARD | FA125_CTRL1_LAST_BOARD);
	      vmeWrite32(&fa125p[fa125ID[ii]]->main.ctrl1,
			 ctrl1 | FA125_CTRL1_ENABLE_MULTIBLOCK);
	    }
	}
      /* Set First Board and Last Board */
      fa125MaxSlot = maxSlot;
      fa125MinSlot = minSlot;
      if(!noBoardInit)
	{
	  vmeWrite32(&fa125p[minSlot]->main.ctrl1,
		     vmeRead32(&fa125p[minSlot]->main.ctrl1) | FA125_CTRL1_FIRST_BOARD);
	  vmeWrite32(&fa125p[maxSlot]->main.ctrl1,
		     vmeRead32(&fa125p[maxSlot]->main.ctrl1) | FA125_CTRL1_LAST_BOARD);
	}
    }

  if(nfa125 > 0)
    printf("%s: %d FA125(s) successfully initialized\n",__FUNCTION__,nfa125);

  return(OK);

}

void
fa125CheckAddresses(int id)
{
  unsigned int offset=0, expected=0;
  unsigned long base=0;
  int ife=0;
  struct fa125_a24 fadcp;

  printf("%s:\n\t ---------- Checking FA125 address space ---------- \n",__FUNCTION__);

  base = (unsigned long) &fadcp.main;

  for(ife=0; ife<12; ife++)
    {
      offset = ((unsigned long) &fadcp.fe[ife]) - base;
      expected = 0x1000 + ife*0x1000;
      if(offset != expected)
	printf("\n%s: ERROR fa125p[%d]->fe[%d] not at offset = 0x%x (@ 0x%x)\n\n",
	       __FUNCTION__,id,ife,expected,offset);
    }

  offset = ((unsigned long) &fadcp.fe[0].ie) - base;
  expected = 0x10b0;
  if(offset != expected)
    printf("\n%s: ERROR fa125p[%d]->fe[0].ie not at offset = 0x%x (@ 0x%x)\n\n",
	   __FUNCTION__,id,expected,offset);

  offset = ((unsigned long) &fadcp.proc) - base;
  expected = 0xD000;
  if(offset != expected)
    printf("\n%s: ERROR fa125p[%d]->proc not at offset = 0x%x (@ 0x%x)\n\n",
	   __FUNCTION__,id,expected,offset);

  offset = ((unsigned long) &fadcp.proc.trigsrc) - base;
  expected = 0xD008;
  if(offset != expected)
    printf("\n%s: ERROR fa125p[%d]->proc.trigsrc not at offset = 0x%x (@ 0x%x)\n\n",
	   __FUNCTION__,id,expected,offset);

}


/**
 *  @ingroup Config
 *  @brief Convert an index into a slot number, where the index is
 *          the element of an array of fA125s in the order in which they were
 *          initialized.
 *
 * @param i Initialization number
 * @return Slot number if Successfull, otherwise ERROR.
 *
 */
int
fa125Slot(unsigned int i)
{
  if(i>=nfa125)
    {
      printf("\n%s: ERROR: Index (%d) >= FA125s initialized (%d).\n\n",
	     __FUNCTION__,i,nfa125);
      return ERROR;
    }

  return fa125ID[i];
}

/**
 *  @ingroup Status
 *  @brief Print Status of fADC125 to standard out
 *  @param id Slot Number
 *  @param pflag Print option flag
 *    - bit   0 (FA125_STATUS_SHOWREGS): Show some register values to standard out
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125Status(int id, int pflag)
{
  struct fa125_a24_main m;
  struct fa125_a24_proc p;
  struct fa125_a24_fe   f[12];
  unsigned int clksrc, trigsrc, srsrc;
  unsigned long faBase;
  unsigned int a32Base, ambMin, ambMax;
  int i=0, showregs=0, sign=1;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(pflag & FA125_STATUS_SHOWREGS)
    showregs=1;

  FA125LOCK;
  m.id = vmeRead32(&fa125p[id]->main.id);
  m.swapctl = vmeRead32(&fa125p[id]->main.swapctl);
  m.version = vmeRead32(&fa125p[id]->main.version);
  m.pwrctl = vmeRead32(&fa125p[id]->main.pwrctl);
  m.slot_ga = vmeRead32(&fa125p[id]->main.slot_ga);
  m.clock = vmeRead32(&fa125p[id]->main.clock);

  for(i=0; i<4; i++)
    m.serial[i] = vmeRead32(&fa125p[id]->main.serial[i]);

  f[0].version = vmeRead32(&fa125p[id]->fe[0].version);
  if(f[0].version==0xffffffff)
    {
      f[0].version = vmeRead32(&fa125p[id]->fe[0].version);
    }

  p.version = vmeRead32(&fa125p[id]->proc.version);
  p.csr     = vmeRead32(&fa125p[id]->proc.csr);
  p.trigsrc = vmeRead32(&fa125p[id]->proc.trigsrc);
  p.ctrl2   = vmeRead32(&fa125p[id]->proc.ctrl2);

  m.adr32        = vmeRead32(&fa125p[id]->main.adr32);
  m.adr_mb       = vmeRead32(&fa125p[id]->main.adr_mb);

  m.ctrl1        = vmeRead32(&fa125p[id]->main.ctrl1);

  m.block_count  = vmeRead32(&fa125p[id]->main.block_count);

  p.trig_count   = vmeRead32(&fa125p[id]->proc.trig_count);
  p.ev_count     = vmeRead32(&fa125p[id]->proc.ev_count);

  m.blockCSR     = vmeRead32(&fa125p[id]->main.blockCSR);

  f[0].config1   = vmeRead32(&fa125p[id]->fe[0].config1);
  f[0].nw        = vmeRead32(&fa125p[id]->fe[0].nw) & FA125_FE_NW_MASK;
  f[0].pl        = vmeRead32(&fa125p[id]->fe[0].pl) & FA125_FE_PL_MASK;
  f[0].ie        = vmeRead32(&fa125p[id]->fe[0].ie);
  f[0].ped_sf    = vmeRead32(&fa125p[id]->fe[0].ped_sf);
  sign           = (f[0].ped_sf&FA125_FE_PED_SF_PBIT_SIGN)?-1:1;

  for(i=0; i<12; i++)
    {
      f[i].test  = vmeRead32(&fa125p[id]->fe[i].test);
    }
  FA125UNLOCK;

  faBase  = (unsigned long) &fa125p[id]->main.id;
  a32Base = (m.adr32 & FA125_ADR32_BASE_MASK)<<16;
  ambMin  = (m.adr_mb & FA125_ADRMB_MIN_MASK)<<16;
  ambMax  = (m.adr_mb & FA125_ADRMB_MAX_MASK);

  #ifdef VXWORKS
  printf("\nSTATUS for FA125 in slot %d at base address 0x%x \n",
	 id, (UINT32) fa125p[id]);
#else
  printf("\nSTATUS for FA125 in slot %d at VME (Local) base address 0x%x (0x%lx)\n",
	 id, (UINT32)((unsigned long)fa125p[id] - fa125A24Offset), (unsigned long) fa125p[id]);
#endif
  printf("--------------------------------------------------------------------------------\n");
  printf(" Main Firmware Revision     = 0x%08x\n",
	 m.version);
  printf(" FrontEnd Firmware Revision = 0x%08x\n",
	 f[0].version);
  printf(" Processing Revision        = 0x%08x\n",
	 p.version);

  printf("      Main SN = 0x%04x%08x\n",m.serial[0], m.serial[1]);
  printf(" Mezzanine SN = 0x%04x%08x\n",m.serial[2], m.serial[3]);

  printf("\n");
  if(showregs)
    {
      printf("Registers:\n");
      printf("  blockCSR       (0x%04lx) = 0x%08x\t",
	     (unsigned long)(&fa125p[id]->main.blockCSR) - faBase, m.blockCSR);
      printf("  ctrl1          (0x%04lx) = 0x%08x\n",
	     (unsigned long)(&fa125p[id]->main.ctrl1) - faBase, m.ctrl1);
      printf("  adr32          (0x%04lx) = 0x%08x\t",
	     (unsigned long)(&fa125p[id]->main.adr32) - faBase, m.adr32);
      printf("  adr_mb         (0x%04lx) = 0x%08x\n",
	     (unsigned long)(&fa125p[id]->main.adr_mb) - faBase, m.adr_mb);
      printf("  trigsrc        (0x%04lx) = 0x%08x\t",
	     (unsigned long)(&fa125p[id]->proc.trigsrc) - faBase, p.trigsrc);

      printf("  clock          (0x%04lx) = 0x%08x\n",
	     (unsigned long)(&fa125p[id]->main.clock) - faBase, m.clock);

      printf("  config1        (0x%04lx) = 0x%08x\n",
	     (unsigned long)(&fa125p[id]->fe[0].config1) - faBase, f[0].config1);

      printf("\n");

      for(i=0; i<12; i=i+2)
	{
	  printf("  test %2d        (0x%04lx) = 0x%08x\t", i,
		 (unsigned long)(&fa125p[id]->fe[i].test) - faBase, f[i].test);
	  printf("  test %2d        (0x%04lx) = 0x%08x\n", i+1,
		 (unsigned long)(&fa125p[id]->fe[i+1].test) - faBase, f[i+1].test);
	}

      printf("\n");
    }

  if(m.ctrl1 & FA125_CTRL1_ENABLE_MULTIBLOCK)
    {
      printf(" Alternate VME Addressing: Multiblock Enabled\n");
      if(m.adr32&FA125_ADR32_ENABLE)
	printf("   A32 Enabled at VME (Local) base 0x%08x (0x%08lx)\n",a32Base,
	       (unsigned long) fa125pd[id]);
      else
	printf("   A32 Disabled\n");

      printf("   Multiblock VME Address Range    0x%08x - 0x%08x\n",ambMin,ambMax);
    }
  else
    {
      printf(" Alternate VME Addressing: Multiblock Disabled\n");
      if(m.adr32&FA125_ADR32_ENABLE)
	printf("   A32 Enabled at VME (Local) base 0x%08x (0x%08lx)\n",a32Base,
	       (unsigned long) fa125pd[id]);
      else
	printf("   A32 Disabled\n");
    }
  printf("\n");

  /* POWER */
  if(m.pwrctl)
    printf(" Power is ON\n");
  else
    printf(" Power is OFF\n");

  /* CLOCK */
  printf(" Clock Source (0x%02x)   :",m.clock);
  clksrc = m.clock & 0xffff;
  if(clksrc == FA125_CLOCK_P2)
    printf(" P2\n");
  else if (clksrc == FA125_CLOCK_P0)
    printf(" P0 (VXS)\n");
  else if (clksrc == FA125_CLOCK_INTERNAL)
    printf(" Internal\n");
  else
    printf(" ????\n");

  /* TRIGGER */
  printf(" Trigger Source (0x%02x) :",p.trigsrc);
  trigsrc = p.trigsrc & FA125_TRIGSRC_TRIGGER_MASK;
  if(trigsrc == FA125_TRIGSRC_TRIGGER_P0)
    printf(" P0 (VXS)\n");
  else if (trigsrc == FA125_TRIGSRC_TRIGGER_SOFTWARE)
    printf(" Software (VME)\n");
  else if (trigsrc == FA125_TRIGSRC_TRIGGER_INTERNAL_SUM)
    printf(" Internal Sum\n");
  else if (trigsrc == FA125_TRIGSRC_TRIGGER_P2)
    printf(" P2\n");

  /* SYNCRESET */
  printf(" SyncReset Source      :");
  srsrc = (f[0].test & FA125_FE_TEST_SYNCRESET_ENABLE)>>2;
  if(srsrc)
    printf(" P0 (VXS)\n");
  else
    printf(" DISABLED\n");

  printf("\n");

  printf(" Bus Error %s\n",
	 (m.ctrl1&FA125_CTRL1_ENABLE_BERR)?"ENABLED":"DISABLED");

  if(m.ctrl1 & FA125_CTRL1_ENABLE_MULTIBLOCK)
    {
      if(m.ctrl1&FA125_CTRL1_FIRST_BOARD)
	printf(" MultiBlock transfer ENABLED (First Board)\n");
      else if(m.ctrl1&FA125_CTRL1_LAST_BOARD)
	printf(" MultiBlock transfer ENABLED (Last Board)\n");
      else
	printf(" MultiBlock transfer ENABLED\n");
    }
  else
    printf(" MultiBlock transfer DISABLED\n");

  printf("\n");

  printf(" Processing Configuration: \n");
  printf("  Mode = %s  (%d)  - %s\n\n",
	 fa125_mode_names[(f[0].config1&FA125_FE_CONFIG1_MODE_MASK) + 1],
	 (f[0].config1&FA125_FE_CONFIG1_MODE_MASK)+1,
	 (f[0].config1 & FA125_FE_CONFIG1_ENABLE)?"ENABLED":"DISABLED");
  printf("  Lookback                         (PL) = %5d %5dns\n",
	 f[0].pl, 8*f[0].pl);
  printf("  Time Window                      (NW) = %5d %5dns\n",
	 f[0].nw, 8*f[0].nw);
  printf("  Integration End                  (IE) = %5d %5dns\n",
	 (f[0].ie & FA125_FE_IE_INTEGRATION_END_MASK),
	 8*(f[0].ie & FA125_FE_IE_INTEGRATION_END_MASK));
  printf("  Pedestal Gap                     (PG) = %5d %5dns\n",
	 ((f[0].ie & FA125_FE_IE_PEDESTAL_GAP_MASK)>>12),
	 8*((f[0].ie & FA125_FE_IE_PEDESTAL_GAP_MASK)>>12));
  printf("  Initial Pedestal exponent        (P1) = %5d\n",
	 (f[0].ped_sf & FA125_FE_PED_SF_NP_MASK));
  printf("  Initial Pedestal window  (NP = 2**P1) = %5d %5dns\n",
	 (1<<(f[0].ped_sf & FA125_FE_PED_SF_NP_MASK)),
	 8*(1<<(f[0].ped_sf & FA125_FE_PED_SF_NP_MASK)));
  printf("  Local Pedestal exponent          (P2) = %5d\n",
	 (f[0].ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8);
  printf("  Local Pedestal window    (NP2= 2**P2) = %5d %5dns\n",
	 (1<<((f[0].ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8)),
	 8*(1<<((f[0].ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8)));
  printf("\n");
  printf("  Scale Factors:\n");
  printf("    Integration (IBIT) = %d   Amplitude (ABIT) = %d   Pedestal (PBIT) = %d\n",
	 ((f[0].ped_sf & FA125_FE_PED_SF_IBIT_MASK)>>16),
	 ((f[0].ped_sf & FA125_FE_PED_SF_ABIT_MASK)>>19),
	 sign*((f[0].ped_sf & FA125_FE_PED_SF_PBIT_MASK)>>22));
  printf("             (2**IBIT) = %-3d        (2**ABIT) = %-3d       (2**PBIT) = ",
	 1<<((f[0].ped_sf & FA125_FE_PED_SF_IBIT_MASK)>>16),
	 1<<((f[0].ped_sf & FA125_FE_PED_SF_ABIT_MASK)>>19));
  if(sign==1)
    printf("%-2d\n\n",1<<((f[0].ped_sf & FA125_FE_PED_SF_PBIT_MASK)>>22));
  else
    printf("1/%-2d\n\n",1<<((f[0].ped_sf & FA125_FE_PED_SF_PBIT_MASK)>>22));

  printf("  Max Peak Count   = %d \n",(f[0].config1 & FA125_FE_CONFIG1_NPULSES_MASK)>>4);
  printf("  Playback Mode    = %s \n",
	 (f[0].config1 & FA125_FE_CONFIG1_PLAYBACK_ENABLE)?"ENABLED":"DISABLED");

  printf("\n");

  printf(" Block Count = %d\n",m.block_count);
  printf(" Trig  Count = %d\n",p.trig_count);
  printf(" Ev    Count = %d\n",p.ev_count);

  printf("\n");
  fa125CheckThresholds(id,1);

  printf("--------------------------------------------------------------------------------\n");
  return OK;

}

/**
 *  @ingroup Status
 *  @brief Print a summary of all initialized fADC1250s
 *  @param pflag Not used
*/
void
fa125GStatus(int pflag)
{
  int ifa, id;
  struct fa125_a24_main m[20];
  struct fa125_a24_proc p[20];
  struct fa125_a24_fe   f[20];
  unsigned int a24addr[20];
  int th_check[20], sign[20];

  FA125LOCK;
  for (ifa=0;ifa<nfa125;ifa++)
    {
      id = fa125Slot(ifa);
      a24addr[id]    = (unsigned int)((unsigned long)fa125p[id] - fa125A24Offset);

      m[id].version     = vmeRead32(&fa125p[id]->main.version);
      m[id].adr32       = vmeRead32(&fa125p[id]->main.adr32);
      m[id].adr_mb      = vmeRead32(&fa125p[id]->main.adr_mb);
      m[id].pwrctl      = vmeRead32(&fa125p[id]->main.pwrctl);
      m[id].clock       = vmeRead32(&fa125p[id]->main.clock);
      m[id].ctrl1       = vmeRead32(&fa125p[id]->main.ctrl1);
      m[id].blockCSR    = vmeRead32(&fa125p[id]->main.blockCSR);
      m[id].block_count = vmeRead32(&fa125p[id]->main.block_count);


      p[id].version     = vmeRead32(&fa125p[id]->proc.version);
      p[id].trigsrc     = vmeRead32(&fa125p[id]->proc.trigsrc);
      p[id].ctrl2       = vmeRead32(&fa125p[id]->proc.ctrl2);
      p[id].blocklevel  = vmeRead32(&fa125p[id]->proc.blocklevel);
      p[id].trig_count  = vmeRead32(&fa125p[id]->proc.trig_count);
      p[id].trig2_count = vmeRead32(&fa125p[id]->proc.trig2_count);
      p[id].sync_count  = vmeRead32(&fa125p[id]->proc.sync_count);



      f[id].version = vmeRead32(&fa125p[id]->fe[0].version);
      f[id].config1 = vmeRead32(&fa125p[id]->fe[0].config1);
      f[id].pl      = vmeRead32(&fa125p[id]->fe[0].pl) & FA125_FE_PL_MASK;
      f[id].nw      = vmeRead32(&fa125p[id]->fe[0].nw) & FA125_FE_NW_MASK;
      f[id].ie      = vmeRead32(&fa125p[id]->fe[0].ie);
      f[id].ped_sf  = vmeRead32(&fa125p[id]->fe[0].ped_sf);
      sign[id]      = (f[id].ped_sf & FA125_FE_PED_SF_PBIT_SIGN)?-1:1;
    }
  FA125UNLOCK;

  for (ifa=0;ifa<nfa125;ifa++)
    {
      id = fa125Slot(ifa);
      th_check[id] = fa125CheckThresholds(id, 0);
    }


  printf("\n");

  printf("                      fADC125 Module Configuration Summary\n\n");
  printf("     ..........Firmware Rev.......... .................Addresses................\n");
  printf("Slot    Main        FE        Proc       A24        A32     A32 Multiblock Range\n");
  printf("--------------------------------------------------------------------------------\n");

  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      printf(" %2d   ",id);

      printf("%08x   %08x   %08x   ",
	     m[id].version, f[id].version, p[id].version);

      printf("%06x    ",
	     a24addr[id]);

      if(m[id].adr32 & FA125_ADR32_ENABLE)
	{
	  printf("%08x    ",
		 (m[id].adr32&FA125_ADR32_BASE_MASK)<<16);
	}
      else
	{
	  printf("  Disabled   ");
	}

      if(m[id].adr_mb & FA125_ADRMB_ENABLE)
	{
	  printf("%08x-%08x",
		 (m[id].adr_mb&FA125_ADRMB_MIN_MASK)<<16,
		 (m[id].adr_mb&FA125_ADRMB_MAX_MASK));
	}
      else
	{
	  printf("Disabled");
	}

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");


  printf("\n");
  printf("              .Signal Sources..                        \n");
  printf("Slot  Power   Clk   Trig   Sync     MBlk  Token  BERR  \n");
  printf("--------------------------------------------------------------------------------\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      printf(" %2d    ",id);

      printf("%s   ",
	     m[id].pwrctl ? " ON" : "OFF");

      printf("%s  ",
	     (m[id].clock & FA125_CLOCK_MASK)==FA125_CLOCK_INTERNAL ? " INT " :
	     (m[id].clock & FA125_CLOCK_MASK)==FA125_CLOCK_INTERNAL_ENABLE ? "*INT*" :
	     (m[id].clock & FA125_CLOCK_MASK)==FA125_CLOCK_P0 ? " VXS " :
	     (m[id].clock & FA125_CLOCK_MASK)==FA125_CLOCK_P2 ? "  P2 " :
	     " ??? ");

      printf("%s  ",
	     (p[id].trigsrc & FA125_TRIGSRC_TRIGGER_MASK)
	     ==FA125_TRIGSRC_TRIGGER_SOFTWARE ? " VME " :
	     (p[id].trigsrc & FA125_TRIGSRC_TRIGGER_MASK)
	     ==FA125_TRIGSRC_TRIGGER_INTERNAL_SUM ? " SUM " :
	     (p[id].trigsrc & FA125_TRIGSRC_TRIGGER_MASK)
	     ==FA125_TRIGSRC_TRIGGER_P0 ? " VXS " :
	     (p[id].trigsrc & FA125_TRIGSRC_TRIGGER_MASK)
	     ==FA125_TRIGSRC_TRIGGER_P2 ? "  P2 " :
	     " ??? ");

      // FIXME: Just check enable bit... disabled or VXS
      printf("%s     ",
	     (f[id].test & FA125_FE_TEST_SYNCRESET_ENABLE)>>2
	     == 1 ? " VXS " : " OFF ");

      printf("%s ",
	     (m[id].ctrl1 & FA125_CTRL1_ENABLE_MULTIBLOCK) ? "YES":" NO");

      printf(" VXS");
      printf("%s   ",
	     m[id].ctrl1 & (FA125_CTRL1_FIRST_BOARD) ? "-F":
	     m[id].ctrl1 & (FA125_CTRL1_LAST_BOARD) ? "-L":
	     "  ");

      printf("%s     ",
	     m[id].ctrl1 & FA125_CTRL1_ENABLE_BERR ? "YES" : " NO");

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        fADC125 Processing Mode Config\n\n");
  printf("      Block\n");
  printf("Slot  Level  Mode         ......PL......   ....NW.....   ....IE....   ...PG...\n");
  printf("--------------------------------------------------------------------------------\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      printf(" %2d    ",id);

      printf("%3d   ",p[id].blocklevel & FA125_PROC_BLOCKLEVEL_MASK);

      printf("%-12s ",fa125_modes[(f[id].config1 & FA125_FE_CONFIG1_MODE_MASK) + 1]);

      printf("%5d %6dns   ", f[id].pl, 8*f[id].pl);

      printf("%4d %4dns   ", f[id].nw, 8*f[id].nw);

      printf("%3d %4dns    ", (f[id].ie & FA125_FE_IE_INTEGRATION_END_MASK),
	     8*(f[id].ie & FA125_FE_IE_INTEGRATION_END_MASK));

      printf("%d  %2dns", ((f[id].ie & FA125_FE_IE_PEDESTAL_GAP_MASK)>>12),
	     8*((f[id].ie & FA125_FE_IE_PEDESTAL_GAP_MASK)>>12));

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        fADC125 Processing Mode Config\n\n");
  printf("             Pedestal Windows               Scale   \n");
  printf("       ---Initial---  ----Local----      ..Factors..            Playback  Thres\n");
  printf("Slot   P1 ...NP1....  P2 ...NP2....      I    A    P      NPK     Mode    Check\n");
  printf("--------------------------------------------------------------------------------\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      printf(" %2d    ",id);

      printf("%d  %3d %4dns  ", (f[id].ped_sf & FA125_FE_PED_SF_NP_MASK),
	     (1<<(f[id].ped_sf & FA125_FE_PED_SF_NP_MASK)),
	     8*(1<<(f[id].ped_sf & FA125_FE_PED_SF_NP_MASK)));

      printf("%d  %3d %4dns      ", (f[id].ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8,
	     8*(1<<((f[id].ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8)),
	     (1<<((f[id].ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8)) );

      printf("%d    ", (f[id].ped_sf & FA125_FE_PED_SF_IBIT_MASK)>>16);

      printf("%d   ", (f[id].ped_sf & FA125_FE_PED_SF_ABIT_MASK)>>19);

      printf("%2d      ", sign[id] * ((f[id].ped_sf & FA125_FE_PED_SF_PBIT_MASK)>>22 ));

      printf("%2d    ", (f[id].config1 & FA125_FE_CONFIG1_NPULSES_MASK)>>4);

      printf("%s   ",
	     (f[id].config1 & FA125_FE_CONFIG1_PLAYBACK_ENABLE) ?" Enabled":"Disabled");

      printf("%s",
	     (th_check[id]==OK)?" OK":"ERR");

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        fADC125 Signal Scalers\n\n");
  printf("Slot       Trig1       Trig2   SyncReset\n");
  printf("--------------------------------------------------------------------------------\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      printf(" %2d   ",id);

      printf("%10d  ", p[id].trig_count & FA125_PROC_TRIGCOUNT_MASK);

      printf("%10d  ", p[id].trig2_count & FA125_PROC_TRIG2COUNT_MASK);

      printf("%10d  ", p[id].sync_count & FA125_PROC_SYNCCOUNT_MASK);

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        fADC125 Data Status\n\n");
  printf("      Trigger   Block                 \n");
  printf("Slot  Source    Ready  Blocks In Fifo \n");
  printf("--------------------------------------------------------------------------------\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      printf(" %2d  ",id);

      printf("%s    ",
	     f[id].config1 & FA125_FE_CONFIG1_ENABLE ? " Enabled" : "Disabled");

      printf("%s       ",
	     m[id].blockCSR & FA125_BLOCKCSR_BLOCK_READY ? "YES" : " NO");

      printf("%10d ",
	     m[id].block_count & FA125_BLOCKCOUNT_MASK);

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("\n");

}

static int
fa125GetModeNumber(char *mode)
{
  int imode=0;

  for(imode=0; imode<FA125_MAXIMUM_NMODES; imode++)
    {
      if(strlen(fa125_modes[imode])==0) continue;

#ifdef VXWORKS
      if(strcmp(mode,fa125_modes[imode])==0)
#else
      if(strcasecmp(mode,fa125_modes[imode])==0)
#endif
	{
	  return imode;
	}
    }

  return ERROR;
}

/**
 *  @ingroup Config
 *  @brief Configure the processing type/mode
 *
 *  @param id Slot number
 *  @param pmode  Processing Mode
 *     -     3 - Pulse Integral and Time (CDC)
 *     -     4 - Pulse Integral and Time (FDC)
 *     -     5 - Peak Amplitude and Time (FDC_amp)
 *     -     6 - Pulse Samples (CDC_long)
 *     -     7 - Pulse Samples (FDC_long)
 *  @param  PL  Window Latency
 *  @param  NW  Window Width
 *  @param  IE  Integration End
 *  @param  PG  Pedestal Gap
 *  @param  NPK Number of pulses processed per window
 *  @param  P1  Parameter for initial pedestal window (NP = 2^P1)
 *  @param  P2  Parameter for local pedestal window (NP2 = 2^P2)
 *
 *     (1) NW > NP + NE
 *     (2) NW > NU
 *     (3) NU > 14
 *     (4) NE >= NU-PG-PED
 *     (5) NE >= 6
 *     (6) NPK > 0
 *     (7) NP >= NP2
 *     (8) NP2 > 0
 *     (9) H > TH > TL
 *    (10) PED > 4
 *    (11) PG > 0
 *    (12) PED+PG < NU
 *
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetProcMode(int id, char *mode, unsigned int PL, unsigned int NW,
		 unsigned int IE, unsigned int PG, unsigned int NPK,
		 unsigned int P1, unsigned int P2)
{
  int imode=0, pmode=0, supported_modes[FA125_SUPPORTED_NMODES] = FA125_SUPPORTED_MODES;
  int cdc_modes[FA125_CDC_NMODES] = FA125_CDC_MODES;
  int mode_supported=0, cdc_mode=0;
  int NE=20;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  pmode = fa125GetModeNumber(mode);

  /* Check if mode is supported */
  for(imode=0; imode<FA125_SUPPORTED_NMODES; imode++)
    {
      if(pmode == supported_modes[imode])
	{
	  mode_supported=1;
	  break;
	}
    }
  if(!mode_supported)
    {
      printf("\n%s: ERROR: Processing Mode (%d) not supported\n\n",
	     __FUNCTION__,pmode);
      return ERROR;
    }

  /* Check if mode is a CDC mode */
  for(imode=0; imode<FA125_CDC_NMODES; imode++)
    {
      if(pmode == cdc_modes[imode])
	{
	  cdc_mode=1;
	  break;
	}
    }

  /* Defaults */
  if((PL==0) || (PL>FA125_MAX_PL))
    {
      printf("%s: WARN: Invalid PL (%d). Setting default (%d)\n",
	     __FUNCTION__,PL,FA125_DEFAULT_PL);
      PL  = FA125_DEFAULT_PL;
    }
  if((NW==0) || (NW>FA125_MAX_NW))
    {
      printf("%s: WARN: Invalid NW (%d). Setting default (%d)\n",
	     __FUNCTION__,NW,FA125_DEFAULT_NW);
      NW = FA125_DEFAULT_NW;
    }
  if((IE==0) || (IE>FA125_MAX_IE))
    {
      printf("%s: WARN: Invalid IE (%d). Setting default (%d)\n",
	     __FUNCTION__,IE,FA125_DEFAULT_IE);
      IE = FA125_DEFAULT_IE;
    }
  if((PG==0) || (PG>FA125_MAX_PG))
    {
      printf("%s: WARN: Invalid PG (%d). Setting default (%d)\n",
	     __FUNCTION__,PG,FA125_DEFAULT_PG);
      PG = FA125_DEFAULT_PG;
    }
  if((NPK==0) || (NPK>FA125_MAX_NPK))
    {
      printf("%s: WARN: Invalid NPK (%d). Setting default (%d)\n",
	     __FUNCTION__,NPK,FA125_DEFAULT_NPK);
      NPK = FA125_DEFAULT_NPK;
    }
  if(cdc_mode && (NPK!=1))
    {
      printf("%s: WARN: Invalid NPK (%d) for CDC mode. Setting to 1\n",
	     __FUNCTION__,NPK);
      NPK=1;
    }


  if(NW <= ((2^P1) + NE))
    {
      printf("\n%s: ERROR: Window must be > Initial Pedestal Window + NE (%d)\n\n",__FUNCTION__
	     ,NE);
      return ERROR;
    }

  if(P1 < P2)
    {
      printf("\n%s: ERROR: Initial Pedestal Window Must be >= Local Pedestal Window\n\n",
	     __FUNCTION__);
      return ERROR;
    }


  FA125LOCK;
  /* Disable ADC processing while writing window info */
  vmeWrite32(&fa125p[id]->fe[0].config1, ((pmode-1) | (NPK<<4)));
  vmeWrite32(&fa125p[id]->fe[0].pl, PL);
  vmeWrite32(&fa125p[id]->fe[0].nw, NW);
  vmeWrite32(&fa125p[id]->fe[0].ie, IE | (PG<<12));
  vmeWrite32(&fa125p[id]->fe[0].ped_sf,
	     (vmeRead32(&fa125p[id]->fe[0].ped_sf) &
	      ~(FA125_FE_PED_SF_NP_MASK | FA125_FE_PED_SF_NP2_MASK)) |
	     (P1 | (P2<<8)) );

  /* Enable ADC processing */
  vmeWrite32(&fa125p[id]->fe[0].config1, ((pmode-1) | (NPK<<4) | FA125_FE_CONFIG1_ENABLE) );

  FA125UNLOCK;


  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the Integration, Amplitude, and Pedestal scale factors for the selected module
 *  @param id   Slot number
 *  @param IBIT Integration Scale Factor
 *  @param ABIT Amplitude Scale Factor
 *  @param PBIT Pedestal Scale Factor
 *  @return OK if successful, otherwise ERROR.
 */

int
fa125SetScaleFactors(int id, unsigned int IBIT, unsigned int ABIT, int PBIT)
{
  int rval=OK, pbit_sign_bit=0, p2=0;
  unsigned int ped_sf=0, check=0, uint_PBIT=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(IBIT>FA125_MAX_IBIT)
    {
      printf("\n%s: ERROR: Invalid IBIT scale factor. Must be <= %d\n\n",
	     __FUNCTION__,FA125_MAX_IBIT);
      return ERROR;
    }

  if(ABIT>FA125_MAX_ABIT)
    {
      printf("\n%s: ERROR: Invalid ABIT scale factor. Must be <= %d\n\n",
	     __FUNCTION__,FA125_MAX_ABIT);
      return ERROR;
    }

  if(abs(PBIT)>FA125_MAX_PBIT)
    {
      printf("\n%s: ERROR: Invalid PBIT scale factor. Must be <= %d\n\n",
	     __FUNCTION__,FA125_MAX_PBIT);
      return ERROR;
    }

  if(PBIT<0)
    pbit_sign_bit = 1;

  FA125LOCK;
  ped_sf = vmeRead32(&fa125p[id]->fe[0].ped_sf);
  p2     = ((ped_sf & FA125_FE_PED_SF_NP2_MASK)>>8);

  if((p2 + PBIT) < 0)
    {
      printf("%s: ERROR: P2 + PBIT < 0  (%d + %d) = %d\n",
	     __FUNCTION__, p2, PBIT, p2 + PBIT);
      printf("\tSetting PBIT to default = %d\n", FA125_DEFAULT_PBIT);
      PBIT = FA125_DEFAULT_PBIT;
      pbit_sign_bit = 0;
      rval = ERROR;
    }

  if((p2 + PBIT) > 7)
    {
      printf("%s: ERROR: P2 + PBIT > 7  (%d + %d) = %d\n",
	     __FUNCTION__, p2, PBIT, p2 + PBIT);
      printf("\tSetting PBIT to default = %d\n", FA125_DEFAULT_PBIT);
      PBIT = FA125_DEFAULT_PBIT;
      pbit_sign_bit = 0;

      rval = ERROR;
    }

  uint_PBIT = pbit_sign_bit ? (unsigned int)((-1) * PBIT) : PBIT;

  vmeWrite32(&fa125p[id]->fe[0].ped_sf,
	     (ped_sf &
	      (FA125_FE_PED_SF_NP_MASK | FA125_FE_PED_SF_NP2_MASK)) |
	     (IBIT<<16) | (ABIT<<19) | (uint_PBIT<<22) | (pbit_sign_bit<<25));
  check = (vmeRead32(&fa125p[id]->fe[0].ped_sf) & FA125_FE_PED_SF_CALC_MASK) >> 26;

  if(check != (p2 + PBIT))
    {
      printf("%s: FIRMWARE ERROR:  P2 + PBIT  fw:  = %d    lib: %d\n",
	     __FUNCTION__,check, p2 + PBIT);
      printf("   register = 0x%08x\n",vmeRead32(&fa125p[id]->fe[0].ped_sf));
      rval = ERROR;
    }
  FA125UNLOCK;

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Return the value stored for the Integration Scale Factor
 *  @param id Slot number
 *  @return Integration Scale Factor if successful, otherwise ERROR.
 */
int
fa125GetIntegrationScaleFactor(int id)
{
  int rval=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  rval = (vmeRead32(&fa125p[id]->fe[0].ped_sf) & FA125_FE_PED_SF_IBIT_MASK)>>16;
  FA125UNLOCK;

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Return the value stored for the Amplitude Scale Factor
 *  @param id Slot number
 *  @return Amplitude Scale Factor if successful, otherwise ERROR.
 */
int
fa125GetAmplitudeScaleFactor(int id)
{
  int rval=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  rval = (vmeRead32(&fa125p[id]->fe[0].ped_sf) & FA125_FE_PED_SF_ABIT_MASK)>>19;
  FA125UNLOCK;

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Return the value stored for the Pedestal Scale Factor
 *  @param id Slot number
 *  @return Pedestal Scale Factor if successful, otherwise ERROR.
 */
int
fa125GetPedestalScaleFactor(int id)
{
  int rval=0, sign=1;
  unsigned int ped_sf=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  ped_sf = vmeRead32(&fa125p[id]->fe[0].ped_sf);
  sign   = (ped_sf & FA125_FE_PED_SF_PBIT_SIGN)?-1:1;
  rval   = sign * ((ped_sf & FA125_FE_PED_SF_PBIT_MASK)>>22);
  FA125UNLOCK;

  return rval;
}


/**
 *  @ingroup Config
 *  @brief Set the Low and High timing threshold for the selected channel in the selected module.
 *  @param id Slot number
 *  @param chan Channel Number
 *  @param lo Low timing threshold
 *  @param hi High timing threshold
 *  @return Pedestal Scale Factor if successful, otherwise ERROR.
 */
int
fa125SetTimingThreshold(int id, unsigned int chan, unsigned int lo, unsigned int hi)
{
  unsigned int wval = 0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(chan>=FA125_MAX_ADC_CHANNELS)
  {
      printf("\n%s: ERROR: Invalid channel (%d). Must be 0-%d\n\n",
	     __FUNCTION__,chan,FA125_MAX_ADC_CHANNELS);
      return ERROR;
    }

  if(lo>FA125_MAX_LOW_TTH)
    {
      printf("\n%s: ERROR: Invalid value for Low Timing Threshold (%d)\n\n",
	     __FUNCTION__,lo);
      return ERROR;
    }

  if(hi>FA125_MAX_HIGH_TTH)
    {
      printf("\n%s: ERROR: Invalid value for High Timing Threshold (%d)\n\n",
	     __FUNCTION__,hi);
      return ERROR;
    }

  FA125LOCK;
  /* Write the lo value */
  if((chan%2)==0)
    {
      wval = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_lo[(chan/2)%3]) & 0xFFFF0000) |
	(lo<<8);
      vmeWrite32(&fa125p[id]->fe[chan/6].timing_thres_lo[(chan/2)%3],
		 wval);
    }
  else
    {
      wval = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_lo[(chan/2)%3]) & 0xFFFF) |
	(lo<<24);
      vmeWrite32(&fa125p[id]->fe[chan/6].timing_thres_lo[(chan/2)%3],
		 wval);
    }

  /* Write the hi value */
  if((chan%3)==0)
    {
      wval = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2]) & 0x07fffe00) |
	(hi);
      vmeWrite32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2],
		 wval);
    }
  else if((chan%3)==1)
    {
      wval = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2]) & 0x07fc01ff) |
	(hi<<9);
      vmeWrite32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2],
		 wval);
    }
  else
    {
      wval = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2]) & 0x0003ffff) |
	(hi<<18);
      vmeWrite32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2],
		 wval);
    }
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the Low and High timing threshold for all channels in the selected module.
 *  @param id Slot number
 *  @param lo Low timing threshold
 *  @param hi High timing threshold
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetCommonTimingThreshold(int id, unsigned int lo, unsigned int hi)
{
  int chan=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(lo>FA125_MAX_LOW_TTH)
    {
      printf("\n%s: ERROR: Invalid value for Low Timing Threshold (%d)\n\n",
	     __FUNCTION__,lo);
      return ERROR;
    }

  if(hi>FA125_MAX_HIGH_TTH)
    {
      printf("\n%s: ERROR: Invalid value for High Timing Threshold (%d)\n\n",
	     __FUNCTION__,hi);
      return ERROR;
    }

  for(chan=0;chan<FA125_MAX_ADC_CHANNELS;chan++)
    {
      fa125SetTimingThreshold(id, chan, lo, hi);
    }

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the Low and High timing threshold for all channels in all initialized modules.
 *  @param lo Low timing threshold
 *  @param hi High timing threshold
 */
void
fa125GSetCommonTimingThreshold(unsigned int lo, unsigned int hi)
{
  int id=0;

  for(id=0; id<nfa125; id++)
    {
      fa125SetCommonTimingThreshold(fa125Slot(id), lo, hi);
    }
}


/**
 *  @ingroup Status
 *  @brief Get the timing threshold values for the selected channel on the selected module.
 *  @param id Slot number
 *  @param chan Channel Number
 *  @return ((High Timing Threshold) | (Low timing threshold<<8)) if successful, otherwise ERROR.
 */
int
fa125GetTimingThreshold(int id, unsigned int chan, int *lo, int *hi)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(chan>=FA125_MAX_ADC_CHANNELS)
  {
      printf("\n%s: ERROR: Invalid channel (%d). Must be 0-%d\n\n",
	     __FUNCTION__,chan,FA125_MAX_ADC_CHANNELS);
      return ERROR;
    }

  FA125LOCK;
  if((chan%2)==0)
    {
      *lo = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_lo[(chan/2)%3]) &
	FA125_FE_TIMING_THRES_LO_MASK(chan))>>8;
    }
  else
    {
      *lo = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_lo[(chan/2)%3]) &
	    FA125_FE_TIMING_THRES_LO_MASK(chan))>>24;
    }

  if((chan%3)==0)
    {
      *hi = vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2]) &
	FA125_FE_TIMING_THRES_HI_MASK(chan);
    }
  else if((chan%3)==1)
    {
      *hi = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2]) &
	    FA125_FE_TIMING_THRES_HI_MASK(chan))>>9;
    }
  else
    {
      *hi = (vmeRead32(&fa125p[id]->fe[chan/6].timing_thres_hi[(chan/3)%2]) &
	    FA125_FE_TIMING_THRES_HI_MASK(chan))>>18;
    }
  FA125UNLOCK;

  return OK;
}

int
fa125PrintTimingThresholds(int id)
{
  int ichan, rval, i, lo[FA125_MAX_ADC_CHANNELS], hi[FA125_MAX_ADC_CHANNELS];
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  for(ichan=0; ichan<FA125_MAX_ADC_CHANNELS; ichan++)
    {
      rval = fa125GetTimingThreshold(id, ichan, &lo[ichan], &hi[ichan]);
      if(rval != OK)
	{
	  return ERROR;
	}
    }

  printf("%s:\n\n",__FUNCTION__);
  printf("Ch     TL   TH      TL   TH      TL   TH      TL   TH      TL   TH      TL   TH\n");
  printf("--------------------------------------------------------------------------------\n");
  for(ichan=0; ichan<FA125_MAX_ADC_CHANNELS; ichan+=6)
    {
      printf("%2d:   ",ichan);
      for(i=ichan; i<6+ichan; i++)
	{
	  printf("%3d  %3d     ",lo[i],hi[i]);
	}
      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n\n");

  return OK;
}

int
fa125CheckThresholds(int id, int pflag)
{
  int rval=OK, ichan, tval, TL, TH, H;
  int header_printed=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  for(ichan=0; ichan<FA125_MAX_ADC_CHANNELS; ichan++)
    {
      tval = fa125GetTimingThreshold(id, ichan, &TL, &TH);
      if(tval==ERROR)
	return ERROR;
      H   = fa125GetThreshold(id, ichan);

      if( !( (H>TH) && (TH>TL) ) )
	{
	  rval = ERROR;
	  if(pflag)
	    {
	      if(header_printed==0)
		{
		  printf("\n");
		  printf("%s: ERROR: Invalid Threshold Settings for Module in slot %d\n",
			 __FUNCTION__, id);
		  header_printed=1;
		}
	      printf("  chan = %3d  H = %4d  TL = %4d  TH = %4d\n",
		     ichan, H, TL, TH);
	    }
	}
    }

  if(pflag)
    {
      if(rval==ERROR)
	printf("\n");
      else
	printf("%s: Threshold conditions OK!\n",
	       __FUNCTION__);
    }

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Power Off the fADC125
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125PowerOff (int id)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  printf("%s: Power Off for slot %d\n",__FUNCTION__,id);

  FA125LOCK;
  vmeWrite32(&fa125p[id]->main.pwrctl, 0);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Power On the fADC125
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125PowerOn (int id)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  printf("%s: Power On (0x%08x) for slot %d\n",__FUNCTION__,
	 FA125_PWRCTL_KEY_ON,id);

  FA125LOCK;
  vmeWrite32(&fa125p[id]->main.pwrctl, FA125_PWRCTL_KEY_ON);
  FA125UNLOCK;

#ifdef VXWORKS
  taskDelay(18);
#else
  usleep(300000);         // delay 300 ms for stable power;
#endif

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set DAC value of a specific channel
 *  @param id Slot number
 *  @param dacChan
 *  @param dacData
 *  @return OK if successful, otherwise ERROR.
 */
static int
fa125SetLTC2620 (int id, int dacChan, int dacData)
{
  UINT32 sdat[5]={0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff};
  UINT32 bmask,dmask,x;
  int k,j;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if ((dacChan<0)||(dacChan>79)) {
    printf("\n%s: ERROR: Invalid DAC Channel %d\n\n", __FUNCTION__,dacChan);
    return ERROR;
  }

  sdat[(dacChan/8)%5] = 0x00200000;               // set command nibble and have other fields zeroed
  sdat[(dacChan/8)%5] |= (dacData & 0x0000ffff);  // fill in the data
  sdat[(dacChan/8)%5] |= ((dacChan%8) << 16);     // fill in the subchannel

  if (dacChan>=40)
    {
      bmask=FA125_DACCTL_DACCS_MASK|FA125_DACCTL_ADACSI_MASK; // these assserted for the duration
      dmask=FA125_DACCTL_BDACSI_MASK;                      // this is the data bit mask
    }
  else
    {
      bmask=FA125_DACCTL_DACCS_MASK|FA125_DACCTL_BDACSI_MASK; // these assserted for the duration
      dmask=FA125_DACCTL_ADACSI_MASK;                      // this is the data bit mask
    }

  FA125LOCK;
  for(k=4;k>=0;k--)
    for(j=31;j>=0;j--)
      {
	x = bmask | ( ((sdat[k]>>j)&1)!=0 ? dmask : 0 );
	vmeWrite32(&fa125p[id]->main.dacctl, x);
	vmeWrite32(&fa125p[id]->main.dacctl, x | FA125_DACCTL_DACSCLK_MASK);
      }

  vmeWrite32(&fa125p[id]->main.dacctl, 0);  // this deasserts CS, setting the DAC
  FA125UNLOCK;

  return OK;
}


/**
 *  @ingroup Config
 *  @brief Set the DAC offset for a specific fADC125 Channel.
 *  @param id Slot number
 *  @param chan Channel Number
 *  @param dacData DAC value to set
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetOffset (int id, int chan, int dacData)
{
  int rval=0;
  const int DAC_CHAN_OFFSET[72] =
    {
      34, 33, 32, 39, 38, 37, 36, 27, 26, 25, 24, 31,
      74, 73, 72, 79, 78, 77, 76, 67, 66, 65, 64, 71,
      30, 29, 28, 18, 17, 16, 23, 22, 21, 20, 10, 9,
      70, 69, 68, 58, 57, 56, 63, 62, 61, 60, 50, 49,
      8, 15, 14, 13, 12, 2, 1, 0, 7, 6, 5, 4,
      48, 55, 54, 53, 52, 42, 41, 40, 47, 46, 45, 44
    };

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if ((chan<0)||(chan>71))
    {
      printf("\n%s: ERROR: Invalid Channel %d\n\n",__FUNCTION__,chan);
      return ERROR;
    }

  rval = fa125SetLTC2620(id,DAC_CHAN_OFFSET[chan],dacData);
  fa125dacOffset[id][chan] = dacData;

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Set the DAC offset for a specific fADC125 Channel from a specified file
 *  @param id Slot number
 *  @param filename Name of file from which to read DAC offsets
 *     Format must be of the type:
 *    dac0   dac1   dac2 ...  dac71
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetOffsetFromFile(int id, char *filename)
{
  FILE *fd_1;
  int ichan;
  int offset_control=0;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(filename == NULL)
    {
      printf("\n%s: ERROR: No file specified.\n\n",__FUNCTION__);
      return ERROR;
    }

  fd_1 = fopen(filename,"r");
  if(fd_1 > 0)
    {
      printf("%s: Reading Data from file: %s\n",__FUNCTION__,filename);
      for(ichan=0;ichan<72;ichan++)
	{
	  fscanf(fd_1,"%d",&offset_control);
	  fa125SetOffset(id, ichan, offset_control);
	}

	fclose(fd_1);
    }
  else
    {
      printf("\n%s: ERROR opening file: %s\n\n",__FUNCTION__,filename);
      return ERROR;
    }

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Readback the DAC offset set for a specific fADC125 Channel
 *  @param id Slot number
 *  @param chan Channel Number
 *  @return DAC offset if successful, otherwise ERROR.
 */
unsigned short
fa125ReadOffset(int id, int chan)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if((chan<0)||(chan>71))
    {
      printf("\n%s: ERROR: channel (%d) out of range.\n\n",
	     __FUNCTION__,chan);
      return ERROR;
    }

  return fa125dacOffset[id][chan];

}

/**
 *  @ingroup Status
 *  @brief Readback the DAC offset set for a specific fADC125 Channel into a specified file
 *  @param id Slot number
 *  @param filename Filename to store all DAC offset
 *  @return DAC offset if successful, otherwise ERROR.
 */
int
fa125ReadOffsetToFile(int id, char *filename)
{
  FILE *fd_1;
  int ichan;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(filename == NULL)
    {
      printf("\n%s: ERROR: No file specified.\n\n",__FUNCTION__);
      return ERROR;
    }

  fd_1 = fopen(filename,"w");
  if(fd_1 > 0)
    {
      printf("%s: Writing DAC offsets to file: %s\n",__FUNCTION__,filename);
      for(ichan=0;ichan<72;ichan++)
	{
	  fprintf(fd_1,"%5d ",fa125dacOffset[id][ichan]);
	  if(((ichan+1)%12)==0)
	    fprintf(fd_1,"\n");
	}
      fprintf(fd_1,"\n");
      fclose(fd_1);
    }
  else
    {
      printf("\n%s: ERROR opening file: %s\n\n",__FUNCTION__,filename);
      return ERROR;
    }

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the readout threshold for a specific fADC125 Channel.
 *  @param id Slot number
 *  @param tvalue Threshold Value
 *  @param chan Channel Number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetThreshold(int id, unsigned short chan, unsigned short tvalue)
{
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetThreshold: ERROR : FA125 in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return(ERROR);
    }

  if(tvalue>FA125_MAX_HIGH_HTH)
    {
      logMsg("\nfa125SetThreshold: ERROR: Invalid threshold (%d). Must be <= %d \n\n",
	     tvalue,FA125_MAX_HIGH_HTH,3,4,5,6);
      return ERROR;
    }

  if(chan>=FA125_MAX_ADC_CHANNELS)
    {
      logMsg("\nfa125SetThreshold: ERROR: Invalid channel (%d). Must be 0-%d\n\n",
	     chan,FA125_MAX_ADC_CHANNELS,3,4,5,6);
      return ERROR;
    }


  FA125LOCK;
  vmeWrite32(&fa125p[id]->fe[chan/6].threshold[chan%6],tvalue);
  FA125UNLOCK;

  return(OK);
}

/**
 *  @ingroup Config
 *  @brief Set the self trigger threshold for a specific fADC125 Channel.
 *  @param id Slot number
 *  @param chan Channel Number [1,6]
 *  @param tvalue Threshold Value [0,4095]
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetSelfTriggerThreshold(int id, unsigned short chan, unsigned short tvalue)
{
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetSelfTriggerThreshold: ERROR : FA125 in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return(ERROR);
    }

  if(chan>=FA125_MAX_ADC_CHANNELS)
    {
      logMsg("\nfa125SetSelfTriggerThreshold: ERROR: Invalid channel (%d). Must be 0-%d\n\n",
	     chan,FA125_MAX_ADC_CHANNELS,3,4,5,6);
      return ERROR;
    }

  if(tvalue>FA125_FE_SELFTRIG_THRES_MASK)
    {
      logMsg("\nfa125SetSelfTriggerThreshold: ERROR: Invalid threshold (%d). Must be <= %d \n\n",
	     tvalue,FA125_FE_SELFTRIG_THRES_MASK,3,4,5,6);
      return ERROR;
    }


  FA125LOCK;
  vmeWrite32(&fa125p[id]->fe[chan/6].selftrig_thres[chan%6],tvalue);
  FA125UNLOCK;

  return(OK);
}

/**
 *  @ingroup Config
 *  @brief Disable a specific fADC125 Channel.
 *  @param id Slot number
 *  @param chan Channel Number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetChannelDisable(int id, int channel)
{
  int feChip=0, feChan=0;
  unsigned int chipMask=0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetChannelDisable: ERROR : FA125 in slot %d is not initialized \n\n",
	     id,0,0,0,0,0);
      return(ERROR);
    }

  if((channel<0) || (channel>=FA125_MAX_ADC_CHANNELS))
    {
      logMsg("\nfaSetChannelDisable: ERROR: Invalid channel (%d).  Must be 0-%d\n\n",
	     channel, FA125_MAX_ADC_CHANNELS-1,3,4,5,6);
      return ERROR;
    }

  feChip = (int)(channel/6);
  feChan = (int)(channel%6);

  FA125LOCK;
  chipMask = (vmeRead32(&fa125p[id]->fe[feChip].config2) & FA125_FE_CONFIG2_CH_MASK)
    | (1<<feChan);
  vmeWrite32(&fa125p[id]->fe[feChip].config2,chipMask);
  FA125UNLOCK;

  return(OK);
}

/**
 *  @ingroup Config
 *  @brief Disable specific fADC125 Channels using three separate channel masks
 *  @param id Slot number
 *  @param cmask0 Disable channel mask for channels 0-23
 *  @param cmask1 Disable channel mask for channels 24-47
 *  @param cmask2 Disable channel mask for channels 48-71
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetChannelDisableMask(int id, unsigned int cmask0,
			   unsigned int cmask1, unsigned int cmask2)
{
  int ichip=0;
  unsigned int chipMask=0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetChannelDisableMask: ERROR : FA125 in slot %d is not initialized \n\n",
	     id,0,0,0,0,0);
      return(ERROR);
    }

  if((cmask0 > 0xFFFFFF) || (cmask1 > 0xFFFFFF) || (cmask2 > 0xFFFFFF))
    {
      logMsg("\nfa125SetChannelDisableMask: ERROR : Invalid channel mask(s) (0x%08x, 0x%08x, 0x%08x).\n",
	     cmask0,cmask1,cmask2,0,0,0);
      logMsg("                          : Each mask must be less than 24 bits.\n\n",
	     0,0,0,0,0,0);
      return(ERROR);
    }

  FA125LOCK;
  for(ichip=0; ichip<4; ichip++)
    {
      chipMask = (cmask0>>(ichip*6)) & FA125_FE_CONFIG2_CH_MASK;
      vmeWrite32(&fa125p[id]->fe[ichip].config2,chipMask);
    }
  for(ichip=4; ichip<8; ichip++)
    {
      chipMask = (cmask1>>((ichip-4)*6)) & FA125_FE_CONFIG2_CH_MASK;
      vmeWrite32(&fa125p[id]->fe[ichip].config2,chipMask);
    }
  for(ichip=8; ichip<12; ichip++)
    {
      chipMask = (cmask2>>((ichip-8)*6)) & FA125_FE_CONFIG2_CH_MASK;
      vmeWrite32(&fa125p[id]->fe[ichip].config2,chipMask);
    }
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Enable a specific fADC125 Channel.
 *  @param id Slot number
 *  @param chan Channel Number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetChannelEnable(int id, int channel)
{
  int feChip=0, feChan=0;
  unsigned int chipMask=0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfaSetChannelEnable: ERROR : ADC in slot %d is not initialized\n\n",id,0,0,0,0,0);
      return ERROR;
    }

  if((channel<0) || (channel>=FA125_MAX_ADC_CHANNELS))
    {
      logMsg("\nfaSetChannelEnable: ERROR: Invalid channel (%d).  Must be 0-%d\n\n",
	     channel, FA125_MAX_ADC_CHANNELS-1,3,4,5,6);
      return ERROR;
    }


  feChip = (int)(channel/6);
  feChan = (int)(channel%6);

  FA125LOCK;
  chipMask = (vmeRead32(&fa125p[id]->fe[feChip].config2) & FA125_FE_CONFIG2_CH_MASK)
    & ~(1<<feChan);

  vmeWrite32(&fa125p[id]->fe[feChip].config2,chipMask);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Enable specific fADC125 Channels using three separate channel masks
 *  @param id Slot number
 *  @param cmask0 Enable channel mask for channels 0-23
 *  @param cmask1 Enable channel mask for channels 24-47
 *  @param cmask2 Enable channel mask for channels 48-71
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetChannelEnableMask(int id, unsigned int cmask0,
			  unsigned int cmask1, unsigned int cmask2)
{
  int ichip=0;
  unsigned int chipMask=0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetChannelEnableMask: ERROR : FA125 in slot %d is not initialized \n\n",
	     id,0,0,0,0,0);
      return(ERROR);
    }

  if((cmask0 > 0xFFFFFF) || (cmask1 > 0xFFFFFF) || (cmask2 > 0xFFFFFF))
    {
      logMsg("\nfa125SetChannelEnableMask: ERROR : Invalid channel mask(s) (0x%08x, 0x%08x, 0x%08x).\n",
	     cmask0,cmask1,cmask2,0,0,0);
      logMsg("                          : Each mask must be less than 24 bits.\n\n",
	     0,0,0,0,0,0);
      return(ERROR);
    }

  cmask0 = (~cmask0) & 0xFFFFFF;
  cmask1 = (~cmask1) & 0xFFFFFF;
  cmask2 = (~cmask2) & 0xFFFFFF;

  FA125LOCK;
  for(ichip=0; ichip<4; ichip++)
    {
      chipMask = (cmask0>>(ichip*6)) & FA125_FE_CONFIG2_CH_MASK;
      vmeWrite32(&fa125p[id]->fe[ichip].config2,chipMask);
    }
  for(ichip=4; ichip<8; ichip++)
    {
      chipMask = (cmask1>>((ichip-4)*6)) & FA125_FE_CONFIG2_CH_MASK;
      vmeWrite32(&fa125p[id]->fe[ichip].config2,chipMask);
    }
  for(ichip=8; ichip<12; ichip++)
    {
      chipMask = (cmask2>>((ichip-8)*6)) & FA125_FE_CONFIG2_CH_MASK;
      vmeWrite32(&fa125p[id]->fe[ichip].config2,chipMask);
    }
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set a common readout threshold for all fADC125 Channels.
 *  @param id Slot number
 *  @param tvalue Threshold Value
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetCommonThreshold(int id, unsigned short tvalue)
{
  int ii,rval=OK;

  for(ii=0;ii<FA125_MAX_ADC_CHANNELS;ii++)
    {
      rval |= fa125SetThreshold(id, ii, tvalue);
    }

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Set a common readout threshold for all fADC125 Channels for all initialized modules.
 *  @param tvalue Threshold Value
 */
void
fa125GSetCommonThreshold(unsigned short tvalue)
{
  int ii;

  for (ii=0;ii<nfa125;ii++)
    {
      fa125SetCommonThreshold(fa125Slot(ii),tvalue);
    }
}

/**
 *  @ingroup Status
 *  @brief Return the value for the readout threshold for specified channel and module.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125GetThreshold(int id, int chan)
{
  int rval=0;

  FA125LOCK;
  rval = vmeRead32(&fa125p[id]->fe[chan/6].threshold[chan%6]) & FA125_FE_THRESHOLD_MASK;
  FA125UNLOCK;

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Print to standarad out the set readout threshold for all fADC125 Channels in the specified module.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125PrintThreshold(int id)
{
  int ii;
  unsigned short tval[FA125_MAX_ADC_CHANNELS];

  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125PrintThreshold: ERROR : FA125 in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return(ERROR);
    }

  FA125LOCK;
  for(ii=0;ii<FA125_MAX_ADC_CHANNELS;ii++)
    {
      tval[ii] = vmeRead32(&fa125p[id]->fe[ii/6].threshold[ii%6]);
    }
  FA125UNLOCK;


  printf(" Threshold Settings for FA125 in slot %d:",id);
  for(ii=0;ii<FA125_MAX_ADC_CHANNELS;ii++)
    {
      if((ii%4)==0)
	{
	  printf("\n");
	}
      printf("Chan %2d: %5d   ",(ii+1),tval[ii]);
    }
  printf("\n");


  return(OK);
}


/**
 *  @ingroup Config
 *  @brief Set the output Pulser Amplitude for the specified connector
 *  @param id Slot number
 *  @param chan Connector number (0-2)
 *  @param dacData Pulser Amplitude.
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetPulserAmplitude (int id, int chan, int dacData)
{
  int rval=0;
  const int DAC_CHAN_PULSER[3]={35, 19, 11};

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if ((chan<0)||(chan>2))
    {
      printf("\n%s: ERROR: Invalid Channel %d\n\n",__FUNCTION__,chan);
      return ERROR;
    }

  rval = fa125SetLTC2620(id,DAC_CHAN_PULSER[chan],dacData);

  return rval;
}

/**
 *  @ingroup Status
 *  @brief Print the temperature of the main board and mezzanine to standard out.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125PrintTemps(int id)
{
  double temp1=0, temp2=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  temp1 = 0.0625*((int) vmeRead32(&fa125p[id]->main.temperature[0]));
  temp2 = 0.0625*((int) vmeRead32(&fa125p[id]->main.temperature[1]));
  FA125UNLOCK;

  printf("%s: Main board temperature: %5.2lf \tMezzanine board temperature: %5.2lf\n",
	 __FUNCTION__,
	 temp1,
	 temp2);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the clock source for the specified fADC125
 *  @param id Slot number
 *  @param clksrc Defines Clock Source
 *           0  P2 Clock
 *           1  VXS (P0)
 *           2  Internal 125MHz Clock
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetClockSource(int id, int clksrc)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if(clksrc>2)
    {
      printf("\n%s: ERROR: Invalid Clock Source specified (%d)\n\n",
	     __FUNCTION__,clksrc);
      return ERROR;
    }

  switch(clksrc)
    {
    case 0: /* P2 Clock */
      clksrc = FA125_CLOCK_P2;
      break;

    case 1: /* VXS (P0) Clock */
      clksrc = FA125_CLOCK_P0;
      break;

    case 2: /* Internal Clock */
    default:
      clksrc = FA125_CLOCK_INTERNAL;
      break;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->main.clock, clksrc);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the trigger source for the specified fADC125
 *  @param id Slot number
 *  @param trigsrc Defines Trigger Source
 *           0  P0 (VXS)
 *           1  Software (VME)
 *           2  Internal Sum
 *           3  P2
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetTriggerSource(int id, int trigsrc)
{
  unsigned int regset=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if((trigsrc<0) || (trigsrc>3))
    {
      printf("\n%s: ERROR: Invalid Trigger Source specified (%d)\n\n",
	     __FUNCTION__,trigsrc);
      return ERROR;
    }

  switch(trigsrc)
    {
    case 1: /* Software */
      regset = FA125_TRIGSRC_TRIGGER_SOFTWARE;
      break;

    case 2: /* Internal Sum */
      regset = FA125_TRIGSRC_TRIGGER_INTERNAL_SUM;
      break;

    case 3: /* P2 */
      regset = FA125_TRIGSRC_TRIGGER_P2;
      break;

    case 0: /* P0 */
    default:
      regset = FA125_TRIGSRC_TRIGGER_P0;
      break;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.trigsrc, regset);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Returns the trigger source for the specified fADC125
 *  @param id Slot number
 *  @return
 *          - 0  P0 (VXS)
 *          - 1  Software
 *          - 2  Internal Sum
 *          - 3  P2
 *          - ERROR  otherwise
 */
int
fa125GetTriggerSource(int id)
{
  int rval=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  rval = vmeRead32(&fa125p[id]->proc.trigsrc);
  FA125UNLOCK;

  return rval;
}

/**
 *  @ingroup Config
 *  @brief Set the Sync Reset source for the specified fADC125
 *  @param id Slot number
 *  @param srsrc Sync Reset source
 *      - 0: P0 (VXS)
 *      - 1: Software (VME)
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetSyncResetSource(int id, int srsrc)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id);
      return ERROR;
    }

  if((srsrc<0) || (srsrc>1))
    {
      printf("\n%s: ERROR: Invalid SyncReset Source specified (%d)\n\n",
	     __FUNCTION__,srsrc);
      return ERROR;
    }

  if(srsrc)
    printf("\n%s: WARN: VME SyncReset Source no longer supported. Setting to VXS.\n\n",
	   __FUNCTION__);

  FA125LOCK;
  /* Enable */
  vmeWrite32(&fa125p[id]->fe[0].test,
	     (vmeRead32(&fa125p[id]->fe[0].test) & ~FA125_FE_TEST_SYNCRESET_ENABLE) |
	     FA125_FE_TEST_SYNCRESET_ENABLE);
  FA125UNLOCK;

  return OK;
}


/**
 *  @ingroup Readout
 *  @brief Poll the specified fADC125's busy status.
 *  @param id Slot number
 *  @returns Busy status
 *    - 0: Not Busy
 *    - 1: Busy
 *    - ERROR: otherwise
 */

int
fa125Poll(int id)
{
  int res;
  int rval=0;
  static int nzero=0;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",
	     __FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
#ifdef VXWORKS
  res = vxMemProbe((char *) &(fa125p[id]->proc.csr),VX_READ,4,(char *)&rval);
#else
  res = vmeMemProbe((char *) &(fa125p[id]->proc.csr),4,(char *)&rval);
#ifdef DOBYTESWAP
  rval = LSWAP(rval);
#endif //DOBYTESWAP
#endif
  FA125UNLOCK;

  /* Sometimes get 0xffffffff.  This is accompanied with a bus error. */
  if(res==ERROR)
    {
#define SHOWBERR
#ifndef VXWORKS
#ifdef SHOWBERR
      vmeClearException(1);
#else
      vmeClearException(0);
#endif
#endif
      berr_count++;
      rval=0;
/*       logMsg("%s: BERR      nzero = %6d\n",__FUNCTION__,nzero,3,4,5,6); */
      return 0;
    }

  if(rval&FA125_PROC_CSR_BUSY)
    {
/*       logMsg("%s: poll = 1, nzero = %6d\n",__FUNCTION__,nzero,3,4,5,6); */
      nzero=0;
      return 1;
    }
  else
    {
      nzero++;
      return 0;
    }

}

/**
 *  @ingroup Readout
 *  @brief Return the Bus Error count that occurred while checking the busy status
 *  @return Bus Error count
 *  @sa fa125Poll
 */
unsigned int
fa125GetBerrCount()
{
  return berr_count;
}

/**
 *  @ingroup Deprec
 *  @brief Clear used with serial acquisition buffers.  Deprecated as this is no longer the way to readout data from the module.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125Clear(int id)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.csr, FA125_PROC_CSR_CLEAR);
  vmeWrite32(&fa125p[id]->proc.csr, 0);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Enable buffers and FIFOs for data acquisition.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125Enable(int id)
{
  int ife=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  for(ife=0; ife<12; ife++)
    {
      vmeWrite32(&fa125p[id]->fe[ife].test,
		 (vmeRead32(&fa125p[id]->fe[ife].test) & ~FA125_FE_TEST_COLLECT_ON) |
		  FA125_FE_TEST_COLLECT_ON);
    }
  FA125UNLOCK;

  printf("%s(%2d): ENABLED\n",__FUNCTION__,id);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Disable buffers and FIFOs for data acquisition.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125Disable(int id)
{
  int ife=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  for(ife=0; ife<12; ife++)
    {
      vmeWrite32(&fa125p[id]->fe[ife].test,
		 (vmeRead32(&fa125p[id]->fe[ife].test) & ~FA125_FE_TEST_COLLECT_ON));
    }
  FA125UNLOCK;

  printf("%s(%2d): DISABLED\n",__FUNCTION__,id);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Perform the selected Reset to a specific fADC125
 *  @param id Slot number
 *  @param reset Type of reset to perform
 *     - 0: Soft Reset - Reset of all state machines and FIFOs.  Register values will remain.
 *     - 1: Hard Reset - Soft reset + reset of all register values.
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125Reset(int id, int reset)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  switch(reset)
    {
    case 0:
      vmeWrite32(&fa125p[id]->main.blockCSR, FA125_BLOCKCSR_PULSE_SOFT_RESET);
      vmeWrite32(&fa125p[id]->main.blockCSR, FA125_BLOCKCSR_PULSE_SOFT_RESET);
      vmeWrite32(&fa125p[id]->main.blockCSR, FA125_BLOCKCSR_PULSE_SOFT_RESET);
      break;

    case 1:
      vmeWrite32(&fa125p[id]->main.blockCSR, FA125_BLOCKCSR_PULSE_HARD_RESET);
      break;

    default:
      vmeWrite32(&fa125p[id]->main.blockCSR, FA125_BLOCKCSR_PULSE_SOFT_RESET);
    }
  vmeWrite32(&fa125p[id]->main.blockCSR, 0);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Reset all counters (trigger count, clock count, sync reset count, trig2 count)
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125ResetCounters(int id)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.trig_count,FA125_PROC_TRIGCOUNT_RESET);
  vmeWrite32(&fa125p[id]->proc.clock125_count,FA125_PROC_CLOCK125COUNT_RESET);
  vmeWrite32(&fa125p[id]->proc.sync_count,FA125_PROC_SYNCCOUNT_RESET);
  vmeWrite32(&fa125p[id]->proc.trig2_count,FA125_PROC_TRIG2COUNT_RESET);
  FA125UNLOCK;
  return OK;
}

/**
 *  @ingroup Config
 *  @brief Returns the token to the first module
 *     This routine only has an effect on the first module of the Multiblock setup.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125ResetToken(int id)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->main.blockCSR, FA125_BLOCKCSR_TAKE_TOKEN);
  vmeWrite32(&fa125p[id]->main.blockCSR, 0);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Return the slot mask of modules with the token.
 *  @return Token Slot Mask if successful, otherwise ERROR.
 */
int
fa125GetTokenMask()
{
  unsigned int rmask=0;
  int ifa=0, id=0, rval=0;

  for(ifa=0; ifa<nfa125; ifa++)
    {
      id=fa125Slot(ifa);
      rval = (vmeRead32(&fa125p[id]->main.blockCSR) & FA125_BLOCKCSR_HAS_TOKEN)>>4;
      rmask |= (rval<<id);
    }

  return rmask;
}

/**
 * @ingroup Status
 *  @brief Return slot mask of modules with token
 *  @param pflag Option to print status to standard out.
 *  @return Mask of slots with the token, if successful. Otherwise ERROR.
 */

unsigned int
fa125GetTokenStatus(int pflag)
{
  unsigned int rval = 0;
  int ifa = 0;

  if(pflag)
    logMsg("fa125GetTokenStatus: Token in slot(s) ",1,2,3,4,5,6);

  rval = fa125GetTokenMask();

  if(pflag)
    {
      for(ifa = 0; ifa < nfa125; ifa++)
	{
	  if(rval & (1<<fa125ID[ifa]))
	    logMsg("%2d ", fa125ID[ifa], 2, 3, 4, 5, 6);
	}
    }

  if(pflag)
    logMsg("\n", 1, 2, 3, 4, 5, 6);

  return rval;
}


/**
 *  @ingroup Config
 *  @brief Set the number of events in the block
 *  @param id Slot number
 *  @param blocklevel Number of events per block
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetBlocklevel(int id, int blocklevel)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.blocklevel, blocklevel);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the number of un-processed triggers in the trigger buffer before
 *     the module goes BUSY
 *  @param id Slot number
 *  @param ntrig Number of Triggers
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetNTrigBusy(int id, int ntrig)
{
  unsigned int rval = 0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",
	     __FUNCTION__,id);
      return ERROR;
    }

  if((ntrig<0) || (ntrig>0xff))
    {
      printf("\n%s: ERROR: Invalid ntrig (%d).\n\n",
	     __FUNCTION__,ntrig);
      return ERROR;
    }

  FA125LOCK;
  rval = vmeRead32(&fa125p[id]->proc.ntrig_busy) & ~FA125_NTRIG_BUSY_MASK;
  vmeWrite32(&fa125p[id]->proc.ntrig_busy, ntrig | rval);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the number of un-processed triggers in the trigger buffer before
 *     the module goes BUSY, for all initialized modules.
 *  @param ntrig Number of Triggers
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125GSetNTrigBusy(int ntrig)
{
  int id=0;
  unsigned int rval = 0;
  if((ntrig<0) || (ntrig>0xff))
    {
      printf("\n%s: ERROR: Invalid ntrig (%d).\n\n",
	     __FUNCTION__,ntrig);
      return ERROR;
    }

  FA125LOCK;
  for(id=0; id<nfa125; id++)
    {
      rval = vmeRead32(&fa125p[id]->proc.ntrig_busy) & ~FA125_NTRIG_BUSY_MASK;
      vmeWrite32(&fa125p[id]->proc.ntrig_busy, ntrig | rval);
    }
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Get the number of un-processed triggers in the trigger buffer before
 *     the module goes BUSY
 *  @param id Slot number
 *  @return Number of Triggers if successful, otherwise ERROR.
 */
int
fa125GetNTrigBusy(int id)
{
  int rval=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",
	     __FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  rval = vmeRead32(&fa125p[id]->proc.ntrig_busy) & FA125_NTRIG_BUSY_MASK;
  FA125UNLOCK;

  return rval;
}


/**
 *  @ingroup Config
 *  @brief Set the limit of un-processed triggers in the trigger buffer.
 *  @param id Slot number
 *  @param ntrig Number of Triggers
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetNTrigStop(int id, int ntrig)
{
  unsigned int rval = 0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",
	     __FUNCTION__,id);
      return ERROR;
    }

  if((ntrig<0) || (ntrig>0xff))
    {
      printf("\n%s: ERROR: Invalid ntrig (%d).\n\n",
	     __FUNCTION__,ntrig);
      return ERROR;
    }

  FA125LOCK;
  rval = vmeRead32(&fa125p[id]->proc.ntrig_busy) & ~FA125_NTRIG_STOP_MASK;
  vmeWrite32(&fa125p[id]->proc.ntrig_busy, (ntrig << 8) | rval);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Set the limit of un-processed triggers in the trigger buffer.
 *  @param ntrig Number of Triggers
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125GSetNTrigStop(int ntrig)
{
  int id=0;
  unsigned int rval = 0;
  if((ntrig<0) || (ntrig>0xff))
    {
      printf("\n%s: ERROR: Invalid ntrig (%d).\n\n",
	     __FUNCTION__,ntrig);
      return ERROR;
    }

  FA125LOCK;
  for(id=0; id<nfa125; id++)
    {
      rval = vmeRead32(&fa125p[id]->proc.ntrig_busy) & ~FA125_NTRIG_STOP_MASK;
      vmeWrite32(&fa125p[id]->proc.ntrig_busy, (ntrig << 8) | rval);
    }
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Status
 *  @brief Get the limit of un-processed triggers in the trigger buffer.
 *  @param id Slot number
 *  @return Number of Triggers if successful, otherwise ERROR.
 */
int
fa125GetNTrigStop(int id)
{
  int rval=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",
	     __FUNCTION__,id);
      return ERROR;
    }

  FA125LOCK;
  rval = (vmeRead32(&fa125p[id]->proc.ntrig_busy) & FA125_NTRIG_STOP_MASK) >> 8;
  FA125UNLOCK;

  return rval;
}


/**
 *  @ingroup Readout
 *  @brief Initiate a software trigger for a specific fADC125 module.
 *  @param id Slot number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SoftTrigger(int id)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized \n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.softtrig, 1);
  vmeWrite32(&fa125p[id]->proc.softtrig, 0);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup PulserConfig
 *  @brief Set the delay between the output pulse and f1TDC trigger
 *  @param id Slot number
 *  @param delay The number of samples of delay between the output pulse and the trigger
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetPulserTriggerDelay(int id, int delay)
{
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetPulserTriggerDelay: ERROR : ADC in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return ERROR;
    }

  if(delay>FA125_PROC_PULSER_TRIG_DELAY_MASK)
    {
      logMsg("\nfa125SetPulserTriggerDelay: ERROR: delay (%d) out of range.  Must be <= %d\n\n",
	     delay,FA125_PROC_PULSER_TRIG_DELAY_MASK,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.pulser_trig_delay,
	     (vmeRead32(&fa125p[id]->proc.pulser_trig_delay) &~ FA125_PROC_PULSER_TRIG_DELAY_MASK)
	      | delay);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup PulserConfig
 *  @brief Set the width of the output pulse
 *  @param id Slot number
 *  @param width The number of samples that make up the width of the output pulse
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetPulserWidth(int id, int width)
{
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetPulserWidth: ERROR : ADC in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return ERROR;
    }

  if(width>(FA125_PROC_PULSER_WIDTH_MASK>>12))
    {
      logMsg("\nfa125SetPulserWidth: ERROR: width (%d) out of range.  Must be <= %d\n\n",
	     width,(FA125_PROC_PULSER_WIDTH_MASK>>12),3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.pulser_trig_delay,
	     (vmeRead32(&fa125p[id]->proc.pulser_trig_delay) &~ FA125_PROC_PULSER_WIDTH_MASK)
	     | (width<<12));
  FA125UNLOCK;

  return OK;
}

/**
 * @ingroup PulserConfig
 * @brief Trigger the pulser
 *
 *  @param id
 *   - Slot Number
 * @param output
 *   - 0: Pulse out only
 *   - 1: fa125 Trigger only
 *   - 2: Both pulse and trigger
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SoftPulser(int id, int output)
{
  unsigned int selection=0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SoftPulser: ERROR : ADC in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return ERROR;
    }

  switch(output)
    {
    case 0: /* Just the pulse out */
      selection = FA125_PROC_PULSER_CONTROL_PULSE;
      break;

    case 1: /* Just the trigger out */
      selection = FA125_PROC_PULSER_CONTROL_DELAYED_TRIGGER;
      break;

    case 2: /* Pulse and trigger out */
      selection = FA125_PROC_PULSER_CONTROL_PULSE
	| FA125_PROC_PULSER_CONTROL_DELAYED_TRIGGER;
      break;

    default:
      logMsg("\nfa125SoftPulser: ERROR: Invalid output option (%d)\n\n",
	     output,2,3,4,5,6);
      return ERROR;
    }


  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.pulser_control, selection);
  FA125UNLOCK;

  return OK;
}


/**
 *  @ingroup Config
 *  @brief Setup fADC125 Progammable Pulse Generator
 *
 *  @param id Slot number
 *  @param fe_chip Front End FPGA to write to
 *  @param sdata  Array of sample data to be programmed
 *  @param nsamples Number of samples contained in sdata
 *
 *  @sa fa125PPGEnable fa125PPGDisable
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125SetPPG(int id, int fe_chip, unsigned short *sdata, int nsamples)
{
  int ii;
  unsigned short rval;

  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125SetPPG: ERROR : ADC in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return(ERROR);
    }

  if(sdata == NULL)
    {
      logMsg("\nfa125SetPPG: ERROR: Invalid Pointer to sample data\n\n",1,2,3,4,5,6);
      return(ERROR);
    }

  if((nsamples <= 0)||(nsamples>FA125_PPG_MAX_SAMPLES))
    {
      logMsg("fa125SetPPG: WARN: Invalid nsamples (%d).  Setting to %d\n",
	     nsamples, FA125_PPG_MAX_SAMPLES,3,4,5,6);
      nsamples = FA125_PPG_MAX_SAMPLES;
    }

  FA125LOCK;
  for(ii=0;ii<(nsamples-2);ii++)
    {
      vmeWrite32(&fa125p[id]->fe[fe_chip].test_waveform,
		 (sdata[ii]|FA125_FE_TEST_WAVEFORM_WRITE_PPG_DATA));
      rval = vmeRead32(&fa125p[id]->fe[fe_chip].test_waveform)&FA125_FE_TEST_WAVEFORM_PPG_DATA_MASK;
      if( (rval) != sdata[ii])
	logMsg("\nfaSetPPG(%d): ERROR: Write error (%d) %x != %x (ii=%d)\n\n",
	       fe_chip,ii,rval, sdata[ii],ii,6);

    }

  /* Write the last two samples without the write flag */
  vmeWrite32(&fa125p[id]->fe[fe_chip].test_waveform,
	     (sdata[(nsamples-2)]&FA125_FE_TEST_WAVEFORM_PPG_DATA_MASK));
  rval = vmeRead32(&fa125p[id]->fe[fe_chip].test_waveform)&FA125_FE_TEST_WAVEFORM_PPG_DATA_MASK;
  if(rval != sdata[(nsamples-2)])
    logMsg("\nfaSetPPG(%d): ERROR: Write error (%d) %x != %x\n\n",fe_chip,nsamples-2,
	   rval, sdata[nsamples-2],5,6);

  vmeWrite32(&fa125p[id]->fe[fe_chip].test_waveform,
	     (sdata[(nsamples-1)]&FA125_FE_TEST_WAVEFORM_PPG_DATA_MASK));
  rval = vmeRead32(&fa125p[id]->fe[fe_chip].test_waveform)&FA125_FE_TEST_WAVEFORM_PPG_DATA_MASK;
  if(rval != sdata[(nsamples-1)])
    logMsg("\nfaSetPPG(%d): ERROR: Write error (%d) %x != %x\n\n",fe_chip,nsamples-1,
	   rval, sdata[nsamples-1],5,6);

  FA125UNLOCK;

  return(OK);
}

/**
 *  @ingroup Config
 *  @brief Enable the programmable pulse generator
 *  @param id Slot number
 *  @sa fa125SetPPG fa125PPGDisable
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125PPGEnable(int id)
{
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125PPGEnable: ERROR : ADC in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->fe[0].config1,
	     vmeRead32(&fa125p[id]->fe[0].config1) | FA125_FE_CONFIG1_PLAYBACK_ENABLE);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Disable the programmable pulse generator
 *  @param id Slot number
 *  @sa fa125SetPPG fa125PPGEnable
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125PPGDisable(int id)
{
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125PPGDisable: ERROR : ADC in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return ERROR;
    }

  FA125LOCK;
  vmeWrite32(&fa125p[id]->fe[0].config1,
	     vmeRead32(&fa125p[id]->fe[0].config1) & ~FA125_FE_CONFIG1_PLAYBACK_ENABLE);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Readout
 *  @brief Return a Block Ready status
 *  @param id Slot number
 *  @return 1 if block is ready for readout, 0 if not, otherwise ERROR.
*/
int
fa125Bready(int id)
{
  int rval=0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125Bready: ERROR : FA125 in slot %d is not initialized \n\n",id,0,0,0,0,0);
      return(ERROR);
    }

  FA125LOCK;
  rval = (vmeRead32(&fa125p[id]->main.blockCSR) & FA125_BLOCKCSR_BLOCK_READY)>>2;
  FA125UNLOCK;

  return rval;
}

/**
 *  @ingroup Readout
 *  @brief Return a Block Ready status mask for all initialized fADC125s
 *  @return block ready mask, otherwise ERROR.
*/
unsigned int
fa125GBready()
{
  int ii, id, stat=0;
  unsigned int dmask=0;

  FA125LOCK;
  for(ii=0;ii<nfa125;ii++)
    {
      id = fa125ID[ii];

      stat = (vmeRead32(&fa125p[id]->main.blockCSR) & FA125_BLOCKCSR_BLOCK_READY)>>2;
/*       printf("%s(%2d): main.blockCSR = 0x%08x\n", */
/* 	     __FUNCTION__,id, fa125p[id]->main.blockCSR); */
      if(stat)
	dmask |= (1<<id);
    }
  FA125UNLOCK;

  return(dmask);
}

/**
 *  @ingroup Readout
 *  @brief Return a Block Ready status mask for fa125s indicated in supplied slotmask
 *  @param slotmask Slotmask Slotmask of fa125s to check for block ready
 *  @param nloop Number of times to iterate through slotmask.
 *  @return block ready mask, otherwise ERROR.
*/
unsigned int
fa125GBlockReady(unsigned int slotmask, int nloop)
{
  int iloop, id, stat=0;
  unsigned int scanmask = 0, dmask=0;

  scanmask = fa125ScanMask();

  FA125LOCK;
  for(iloop = 0; iloop < nloop; iloop++)
    { /* Loop for user specified number of times */

      for(id = 2; id < 21; id++)  /* Scan over physical slots */
	{
	  if( (scanmask & (1<<id))      /* Module initialized */
	      && (slotmask & (1<<id))   /* slot used */
	      && (!(dmask & (1<<id))) ) /* No block ready yet. */
	    {
	      stat = (vmeRead32(&fa125p[id]->main.blockCSR)
		      & FA125_BLOCKCSR_BLOCK_READY)>>2;

	      if(stat)
		dmask |= (1<<id);

	      if(dmask == slotmask)
		{ /* Blockready mask matches user slotmask */
		  FA125UNLOCK;
		  return(dmask);
		}
	    }
	}
    }
  FA125UNLOCK;

  return(dmask);
}



/**
 *  @ingroup Status
 *  @brief Return the vme slot mask of all initialized fADC125s
 *  @return VME Slot mask, otherwise ERROR.
*/
unsigned int
fa125ScanMask()
{
  int ifa125, id, dmask=0;

  for(ifa125=0; ifa125<nfa125; ifa125++)
    {
      id = fa125ID[ifa125];
      dmask |= (1<<id);
    }

  return(dmask);
}

const char *fa125_blockerror_names[FA125_BLOCKERROR_NTYPES] =
  {
    "No Error",
    "Termination on word count",
    "Unknown Bus Error",
    "Zero Word Count",
    "DmaDone(..) Error"
  };

/**
 *  @ingroup Readout
 *  @brief Return the block error flag and optionally print out the description to standard out
 *  @param pflag If >0 will print the error flag to standard out.
 *  @return Block Error flag.
 *  @sa FA125_BLOCKERROR_FLAGS
 */
int
fa125ReadBlockStatus(int pflag)
{
  if(pflag)
    {
      if(fa125BlockError!=FA125_BLOCKERROR_NO_ERROR)
	{
	  printf("\n%s: ERROR: %s\n",
		 __FUNCTION__,fa125_blockerror_names[fa125BlockError]);
	}
    }

  return fa125BlockError;
}


/**
 *  @ingroup Readout
 *  @brief General Data readout routine
 *
 *  @param  id     Slot number of module to read
 *  @param  data   local memory address to place data
 *  @param  nwrds  Max number of words to transfer
 *  @param  rflag  Readout Flag
 * <pre>
 *              0 - programmed I/O from the specified board
 *              1 - DMA transfer using Universe/Tempe DMA Engine
 *                    (DMA VME transfer Mode must be setup prior)
 *              2 - Multiblock DMA transfer (Multiblock must be enabled
 *                     and daisychain in place or SD being used)
 * </pre>
 *  @return Number of words inserted into data if successful.  Otherwise ERROR.
 */
int
fa125ReadBlock(int id, volatile UINT32 *data, int nwrds, int rflag)
{
  int ii;
  int stat, retVal, xferCount, rmode, async;
  int dCnt, berr=0;
  int dummy=0;
  volatile unsigned int *laddr;
  unsigned int bhead, ehead, val;
  unsigned int vmeAdr, csr;

  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\nfa125ReadBlock: ERROR : FA125 in slot %d is not initialized\n\n",id,0,0,0,0,0);
      return(ERROR);
    }

  if(data==NULL)
    {
      logMsg("\nfa125ReadBlock: ERROR: Invalid Destination address\n\n",0,0,0,0,0,0);
      return(ERROR);
    }

  fa125BlockError=FA125_BLOCKERROR_NO_ERROR;
  if(nwrds <= 0) nwrds= (FA125_MAX_ADC_CHANNELS*FA125_MAX_DATA_PER_CHANNEL) + 8;
  rmode = rflag&0x0f;
  async = rflag&0x80;

  if(rmode >= 1)
    { /* Block Transfers */

      /*Assume that the DMA programming is already setup. */
      /* Don't Bother checking if there is valid data - that should be done prior
	 to calling the read routine */

      /* Check for 8 byte boundary for address - insert dummy word (Slot 0 FA125 Dummy DATA)*/
      if((unsigned long) (data)&0x7)
	{
#ifdef VXWORKS
	  *data = FA125_DUMMY_DATA;
#else
	  *data = LSWAP(FA125_DUMMY_DATA);
#endif
	  dummy = 1;
	  laddr = (data + 1);
	}
      else
	{
	  dummy = 0;
	  laddr = data;
	}

      FA125LOCK;
      if(rmode == 2)
	{ /* Multiblock Mode */
	  if((vmeRead32(&fa125p[id]->main.ctrl1)&FA125_CTRL1_FIRST_BOARD)==0)
	    {
	      logMsg("\nfa125ReadBlock: ERROR: FA125 in slot %d is not First Board\n\n",id,0,0,0,0,0);
	      FA125UNLOCK;
	      return(ERROR);
	    }
	  vmeAdr = (unsigned int)((unsigned long)(FA125pmb) - fa125A32Offset);
	}
      else
	{
	  vmeAdr = (unsigned int)((unsigned long)fa125pd[id] - fa125A32Offset);
	}
#ifdef VXWORKS
      retVal = sysVmeDmaSend((UINT32)laddr, vmeAdr, (nwrds<<2), 0);
#else
      retVal = vmeDmaSend((unsigned long)laddr, vmeAdr, (nwrds<<2));
#endif
      if(retVal != 0)
	{
	  logMsg("\nfa125ReadBlock: ERROR in DMA transfer Initialization 0x%x\n\n",retVal,0,0,0,0,0);
	  FA125UNLOCK;
	  return(retVal);
	}

      if(async)
	{ /* Asynchonous mode - return immediately - don't wait for done!! */
	  FA125UNLOCK;
	  return(OK);
	}
      else
	{
	  /* Wait until Done or Error */
#ifdef VXWORKS
	  retVal = sysVmeDmaDone(10000,1);
#else
	  retVal = vmeDmaDone();
#endif
	}

      if(retVal > 0)
	{
	  /* Check to see that Bus error was generated by FA125 */
	  if(rmode == 2)
	    {
	      csr = vmeRead32(&fa125p[fa125MaxSlot]->main.blockCSR);  /* from Last FA125 */
	      stat = (csr)&FA125_BLOCKCSR_BERR_ASSERTED;  /* from Last FA125 */
	    }
	  else
	    {
	      csr = vmeRead32(&fa125p[id]->main.blockCSR);  /* from Last FA125 */
	      stat = (csr)&FA125_BLOCKCSR_BERR_ASSERTED;  /* from Last FA125 */
	    }
	  if((retVal>0) && (stat))
	    {
#ifdef VXWORKS
	      xferCount = (nwrds - (retVal>>2) + dummy);  /* Number of Longwords transfered */
#else
	      xferCount = ((retVal>>2) + dummy);  /* Number of Longwords transfered */
#endif
	      FA125UNLOCK;
	      return(xferCount); /* Return number of data words transfered */
	    }
	  else
	    {
#ifdef VXWORKS
	      xferCount = (nwrds - (retVal>>2) + dummy);  /* Number of Longwords transfered */
#else
	      xferCount = ((retVal>>2) + dummy);  /* Number of Longwords transfered */
#endif
	      logMsg("fa125ReadBlock: DMA transfer terminated by unknown BUS Error (csr=0x%x xferCount=%d id=%d)\n",
		     csr,xferCount,id,0,0,0);
	      FA125UNLOCK;
	      fa125BlockError=FA125_BLOCKERROR_UNKNOWN_BUS_ERROR;
	      if(rmode == 2)
		fa125GetTokenStatus(1);

	      return(xferCount);
	    }
	}
      else if (retVal == 0)
	{ /* Block Error finished without Bus Error */
#ifdef VXWORKS
	  logMsg("fa125ReadBlock: WARN: DMA transfer terminated by word count 0x%x\n",nwrds,0,0,0,0,0);
	  fa125BlockError=FA125_BLOCKERROR_TERM_ON_WORDCOUNT;
#else
	  logMsg("fa125ReadBlock: WARN: DMA transfer returned zero word count 0x%x\n",nwrds,0,0,0,0,0);
	  fa125BlockError=FA125_BLOCKERROR_ZERO_WORD_COUNT;
#endif
	  FA125UNLOCK;
	  if(rmode == 2)
	    fa125GetTokenStatus(1);

	  return(nwrds);
	}
      else
	{  /* Error in DMA */
#ifdef VXWORKS
	  logMsg("\nfa125ReadBlock: ERROR: sysVmeDmaDone returned an Error\n\n",0,0,0,0,0,0);
#else
	  logMsg("\nfa125ReadBlock: ERROR: vmeDmaDone returned an Error\n\n",0,0,0,0,0,0);
#endif
	  FA125UNLOCK;
	  fa125BlockError=FA125_BLOCKERROR_DMADONE_ERROR;
	  if(rmode == 2)
	    fa125GetTokenStatus(1);

	  return(retVal>>2);
	}

    }
  else
    {  /*Programmed IO */

      /* Check if Bus Errors are enabled. If so then disable for Prog I/O reading */
      FA125LOCK;
      berr = vmeRead32(&fa125p[id]->main.ctrl1)&FA125_CTRL1_ENABLE_BERR;
      if(berr)
	vmeWrite32(&fa125p[id]->main.ctrl1,
		   vmeRead32(&fa125p[id]->main.ctrl1) & ~FA125_CTRL1_ENABLE_BERR);

      dCnt = 0;
      /* Read Block Header - should be first word */
      bhead = fa125pd[id]->data;
#ifndef VXWORKS
      bhead = LSWAP(bhead);
#endif
      if((bhead&FA125_DATA_TYPE_DEFINE)&&((bhead&FA125_DATA_TYPE_MASK) == FA125_DATA_BLOCK_HEADER))
	{
	  ehead = fa125pd[id]->data;
#ifndef VXWORKS
	  ehead = LSWAP(ehead);
#endif
#ifdef VXWORKS
	  data[dCnt] = bhead;
#else
	  data[dCnt] = LSWAP(bhead); /* Swap back to little-endian */
#endif
	  dCnt++;
#ifdef VXWORKS
	  data[dCnt] = ehead;
#else
	  data[dCnt] = LSWAP(ehead); /* Swap back to little-endian */
#endif
	  dCnt++;
	}
      else
	{
	  /* We got bad data - Check if there is any data at all */
	  if( (vmeRead32(&fa125p[id]->proc.ev_count) & FA125_PROC_EVCOUNT_MASK) == 0)
	    {
	      logMsg("fa125ReadBlock: FIFO Empty (0x%08x)\n",bhead,0,0,0,0,0);
	      FA125UNLOCK;
	      return(0);
	    }
	  else
	    {
	      logMsg("\nfa125ReadBlock: ERROR: Invalid Header Word 0x%08x\n\n",bhead,0,0,0,0,0);
	      FA125UNLOCK;
	      return(ERROR);
	    }
	}

      ii=0;
      while(ii<nwrds)
	{
	  val = fa125pd[id]->data;
	  data[ii+2] = val;
#ifndef VXWORKS
	  val = LSWAP(val);
#endif
	  if( (val&FA125_DATA_TYPE_DEFINE)
	      && ((val&FA125_DATA_TYPE_MASK) == FA125_DATA_BLOCK_TRAILER) )
	    break;
	  ii++;
	}
      ii++;
      dCnt += ii;


      if(berr)
	vmeWrite32(&fa125p[id]->main.ctrl1,
		   vmeRead32(&fa125p[id]->main.ctrl1) | FA125_CTRL1_ENABLE_BERR);

      FA125UNLOCK;
      return(dCnt);
    }

  FA125UNLOCK;
  return(OK);
}

/**
 *  @ingroup Config
 *  @brief Enable/Disable suppression of one or both of the trigger time words
 *    in the data stream.
 *  @param id Slot number
 *  @param suppress Suppression Flag
 *      -  0: Trigger time words are enabled in datastream
 *      -  1: Suppress BOTH trigger time words
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125DataSuppressTriggerTime(int id, int suppress)
{
  int val = 0;
  if(id==0) id=fa125ID[0];

  if((id<=0) || (id>21) || (fa125p[id] == NULL))
    {
      printf("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",
	     __FUNCTION__, id);
      return(ERROR);
    }

  if((suppress < 0) || (suppress > 1))
    {
      printf("\n%s: ERROR: Invalid suppress value (%d)\n\n",
	     __FUNCTION__, suppress);
      return ERROR;
    }

  if(suppress)
    val = 0;
  else
    val = FA125_PROC_CTRL2_TRIGTIME_ENABLE;

  FA125LOCK;
  vmeWrite32(&fa125p[id]->proc.ctrl2, val);
  FA125UNLOCK;

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Enable/Disable suppression of one or both of the trigger time words
 *    in the data stream for all initialized modules.
 *  @param suppress Suppression Flag
 *      -  0: Trigger time words are enabled in datastream
 *      -  1: Suppress BOTH trigger time words
 */
void
fa125GDataSuppressTriggerTime(int suppress)
{
  int ifa;

  for(ifa = 0; ifa < nfa125; ifa++)
    fa125DataSuppressTriggerTime(fa125Slot(ifa), suppress);

}

/**
 * @ingroup Status
 *  @brief Return the base address of the A32 for specified module
 *  @param id
 *   - Slot Number
 *  @return A32 address base, if successful. Otherwise ERROR.
 */

unsigned int
fa125GetA32(int id)
{
  unsigned int rval = 0;
  if(fa125pd[id])
    {
      rval = (unsigned int)((unsigned long)fa125pd[id] - fa125A32Offset);
    }
  else
    {
      logMsg("fa125GetA32(%d): A32 pointer not initialized\n",
	     id, 2, 3, 4, 5, 6);
      rval = ERROR;
    }

  return rval;
}

/**
 * @ingroup Status
 *  @brief Return the base address of the A32 Multiblock
 *  @return A32 multiblock address base, if successful. Otherwise ERROR.
 */

unsigned int
fa125GetA32M()
{
  unsigned int rval = 0;
  if(FA125pmb)
    {
      rval = (unsigned int)((unsigned long)FA125pmb - fa125A32Offset);
    }
  else
    {
      logMsg("fa125GetA32M: A32M pointer not initialized\n",
	     1, 2, 3, 4, 5, 6);
      rval = ERROR;
    }

  return rval;
}




struct data_struct
{
  unsigned int new_type;
  unsigned int type;
  unsigned int slot_id_hd;
  unsigned int mod_id_hd;
  unsigned int slot_id_tr;
  unsigned int n_evts;
  unsigned int blk_num;
  unsigned int n_words;
  unsigned int evt_num_1;
  unsigned int time_now;
  unsigned int time_1;
  unsigned int time_2;
  unsigned int chan;
  unsigned int width;
  unsigned int npk;
  unsigned int le_time;
  unsigned int time_quality;
  unsigned int overflow_cnt;
  unsigned int pedestal;
  unsigned int fm_amplitude;
  unsigned int peak_amplitude;
  unsigned int peak_time;
  unsigned int valid_1;
  unsigned int adc_1;
  unsigned int valid_2;
  unsigned int adc_2;
  unsigned int over;
  unsigned int adc_sum;
  unsigned int pulse_num;
  unsigned int thres_bin;
  unsigned int quality;
  unsigned int integral;
  unsigned int time;
  unsigned int chan_a;
  unsigned int source_a;
  unsigned int chan_b;
  unsigned int source_b;
  unsigned int group;
  unsigned int time_coarse;
  unsigned int time_fine;
  unsigned int vmin;
  unsigned int vpeak;
  unsigned int scaler[18];/* data stream scalers */
};

volatile struct data_struct fadc_data;

/**
 *  @ingroup Status
 *  @brief Decode a data word from an fADC125 and print to standard out.
 *  @param data 32bit fADC125 data word
*/
void
fa125DecodeData(unsigned int data)
{
  /* for new data format - 10/23/13 - EJ */

  static unsigned int type_last = 15;/* initialize to type FILLER WORD */
  static unsigned int time_last = 0;
  static unsigned int scaler_index = 0;
  static unsigned int num_scalers = 1;

  static unsigned int slot_id_ev_hd = 0;
  static unsigned int slot_id_dnv = 0;
  static unsigned int slot_id_fill = 0;

  static int nsamples=0;
  static int ipk=0;
/*   static int goto_raw=0; */

  int i_print =1;

  if( scaler_index )/* scaler data word */
    {
      fadc_data.type = 16;/* scaler data words as type 16 */
      fadc_data.new_type = 0;
      if( scaler_index < num_scalers )
	{
	  fadc_data.scaler[scaler_index - 1] = data;
	  if( i_print )
	    printf("%8X - SCALER(%d) = %d\n", data, (scaler_index - 1), data);
	  scaler_index++;
	}
      else/* last scaler word */
	{
	  fadc_data.scaler[scaler_index - 1] = data;
	  if( i_print )
	    printf("%8X - SCALER(%d) = %d\n", data, (scaler_index - 1), data);
	  scaler_index = 0;
	  num_scalers = 1;
	}
    }
  else/* non-scaler word */
    {
      if( data & 0x80000000 )/* data type defining word */
	{
	  fadc_data.new_type = 1;
	  fadc_data.type = (data & 0x78000000) >> 27;
	}
      else/* data type continuation word */
	{
	  fadc_data.new_type = 0;
	  fadc_data.type = type_last;
	}

      switch( fadc_data.type )
	{
	case 0:/* BLOCK HEADER */
	  fadc_data.slot_id_hd = (data & 0x7C00000) >> 22;
	  fadc_data.mod_id_hd =  (data &  0x3C0000) >> 18;
	  fadc_data.n_evts =  (data & 0x000FF);
	  fadc_data.blk_num = (data & 0x7F00) >> 8;
	  if( i_print )
	    printf("%8X - BLOCK HEADER - slot = %d  id = %d  n_evts = %d  n_blk = %d\n",
		   data, fadc_data.slot_id_hd, fadc_data.mod_id_hd, fadc_data.n_evts, fadc_data.blk_num);
	  break;

	case 1:/* BLOCK TRAILER */
	  fadc_data.slot_id_tr = (data & 0x7C00000) >> 22;
	  fadc_data.n_words = (data & 0x3FFFFF);
	  if( i_print )
	    printf("%8X - BLOCK TRAILER - slot = %d   n_words = %d\n",
		   data, fadc_data.slot_id_tr, fadc_data.n_words);
	  break;

	case 2:/* EVENT HEADER */
	  if( fadc_data.new_type )
	    {
	      slot_id_ev_hd        = (data & 0x7C00000) >> 22;
	      fadc_data.evt_num_1 =  (data & 0x03FFFFF);
	      if( i_print )
		printf("%8X - EVENT HEADER - slot = %d  evt_num = %d\n",
		       data, slot_id_ev_hd, fadc_data.evt_num_1);
	    }
	  break;

	case 3:/* TRIGGER TIME */
	  if( fadc_data.new_type )
	    {
	      fadc_data.time_1 = (data & 0xFFFFFF);
	      if( i_print )
		printf("%8X - TRIGGER TIME 1 - time = 0x%08x\n", data, fadc_data.time_1);
	      fadc_data.time_now = 1;
	      time_last = 1;
	    }
	  else
	    {
	      if( time_last == 1 )
		{
		  fadc_data.time_2 = (data & 0xFFFFFF);
		  if( i_print )
		    printf("%8X - TRIGGER TIME 2 - time = 0x%08x\n", data, fadc_data.time_2);
		  fadc_data.time_now = 2;
		}
	      else
		if( i_print )
		  printf("%8X - TRIGGER TIME - (ERROR)\n", data);

	      time_last = fadc_data.time_now;
	    }
	  break;

	case 4:/* WINDOW RAW DATA */
          if( fadc_data.new_type )
            {
              fadc_data.chan = (data & 0x7F00000) >> 20;
	      fadc_data.width = (data & 0xFFF);
              if( i_print )
		printf("%8X - WINDOW RAW DATA - chan = %2d   width = %d\n",
		       data, fadc_data.chan, fadc_data.width);
              nsamples=0;
            }
          else
            {
	      fadc_data.valid_1 = 1;
	      fadc_data.valid_2 = 1;
	      fadc_data.adc_1 = (data & 0x1FFF0000) >> 16;
	      if( data & 0x20000000 )
		fadc_data.valid_1 = 0;
	      fadc_data.adc_2 = (data & 0x1FFF);
	      if( data & 0x2000 )
		fadc_data.valid_2 = 0;
              if( i_print )
		printf("%8X - RAW SAMPLES (%3d) - valid = %d  adc = %4d (%03X)  valid = %d  adc = %4d (%03X)\n",
		       data, nsamples,fadc_data.valid_1, fadc_data.adc_1, fadc_data.adc_1,
		       fadc_data.valid_2, fadc_data.adc_2, fadc_data.adc_2);
              nsamples += 2;
            }
          break;

	case 5:/* PULSE DATA, CDC */
	  if( fadc_data.new_type )
	    {
	      fadc_data.chan = (data & 0x7F00000) >> 20;
	      fadc_data.npk  = (data & 0xF8000)>>15;
	      fadc_data.le_time = (data & 0x7FF0)>>4;
	      fadc_data.time_quality = (data & (1<<3))>>3;
	      fadc_data.overflow_cnt = (data & 0x7);
	      if( i_print )
		printf("%8X - PULSE DATA (CDC IT) - chan = %2d  LE time = %d  Q = %d  OVF = %d\n",
		       data, fadc_data.chan, fadc_data.le_time,
		       fadc_data.time_quality, fadc_data.overflow_cnt);
	    }
	  else
	    {
	      fadc_data.pedestal = (data & 0x7F800000)>>23;
	      fadc_data.integral = (data & 0x007FFE00)>>9;
	      fadc_data.fm_amplitude = (data & 0x000001FF);
	      if( i_print )
		printf("%8X - PULSE DATA (CDC IT) - ped = %d  integral = %d  firstmax ampl = %d\n",
		       data, fadc_data.pedestal, fadc_data.integral, fadc_data.fm_amplitude);
	    }
	  break;

	case 6:/* PULSE DATA, FDC - Integral and Time */
	  if( fadc_data.new_type )
	    {
	      fadc_data.chan = (data & 0x7F00000) >> 20;
	      fadc_data.npk  = (data & 0xF8000)>>15;
	      fadc_data.le_time = (data & 0x7FF0)>>4;
	      fadc_data.time_quality = (data & (1<<3))>>3;
	      fadc_data.overflow_cnt = (data & 0x7);
	      ipk = 0;
	      if( i_print )
		printf("%8X - PULSE DATA (FDC IT) - chan = %2d  NPK = %d  LE time = %d  Q = %d  OVF = %d\n",
		       data, fadc_data.chan, fadc_data.npk, fadc_data.le_time,
		       fadc_data.time_quality, fadc_data.overflow_cnt);
	    }
	  else
	    {
	      ipk++;
	      fadc_data.pedestal = (data & 0x7F800000)>>23;
	      fadc_data.integral = (data & 0x007FFE00)>>9;
	      fadc_data.fm_amplitude = (data & 0x000001FF);
	      if( i_print )
		printf("%8X - PULSE DATA (FDC IT) %d - ped = %d  integral = %d  firstmax ampl = %d\n",
		       data, ipk, fadc_data.pedestal, fadc_data.integral, fadc_data.fm_amplitude);
	    }
	  break;

	case 9:/* PULSE DATA, FDC - Peak Ampl and Time */
	  if( fadc_data.new_type )
	    {
	      fadc_data.chan = (data & 0x7F00000) >> 20;
	      fadc_data.le_time = (data & 0x7FF0)>>4;
	      fadc_data.time_quality = (data & (1<<3))>>3;
	      fadc_data.overflow_cnt = (data & 0x7);
	      ipk = 0;
	      if( i_print )
		printf("%8X - PULSE DATA (FDC AT) - chan = %2d  NPK = %d  LE time = %d  Q = %d  OVF = %d\n",
		       data, fadc_data.chan, fadc_data.npk, fadc_data.le_time,
		       fadc_data.time_quality, fadc_data.overflow_cnt);
	    }
	  else
	    {
	      ipk++;
	      fadc_data.peak_amplitude = (data & 0x7ff80000)>>19;
	      fadc_data.peak_time = (data & 0x0007f800)>>11;
	      fadc_data.pedestal = (data & 0x000007ff);
	      if( i_print )
		printf("%8X - PULSE DATA (FDC AT) %d - Ampl = %d  Time = %d  Pedestal = %d\n",
		       data, ipk, fadc_data.peak_amplitude, fadc_data.peak_time, fadc_data.pedestal);
	    }
	  break;

	case 7:
	case 8:
	case 10:
	case 11:
	case 12:/* UNDEFINED TYPE */
	  if( i_print )
	    printf("%8X - UNDEFINED TYPE = %d\n", data, fadc_data.type);
	  break;

	case 13:/* END OF EVENT */
	  if( i_print )
	    printf("%8X - END OF EVENT = %d\n", data, fadc_data.type);
	  break;

	case 14:/* DATA NOT VALID (no data available) */
	  slot_id_dnv = (data & 0x7C00000) >> 22;
	  if( i_print )
	    printf("%8X - DATA NOT VALID = %d  slot = %d\n", data, fadc_data.type, slot_id_dnv);
	  break;

	case 15:/* FILLER WORD */
	  slot_id_fill = (data & 0x7C00000) >> 22;
	  if( i_print )
	    printf("%8X - FILLER WORD = %d  slot = %d\n", data, fadc_data.type, slot_id_fill);
	  break;
	}

      type_last = fadc_data.type;    /* save type of current data word */

    }

}

/************************************************************
 *  fa125 Firmware Updating Routines
 ************************************************************/
#define        MCS_MAX_SIZE    (FA125_FIRMWARE_MAX_PAGES*FA125_FIRMWARE_MAX_BYTE_PER_PAGE)
static unsigned int   MCS_dataSize = 0;          /* Size of the array holding the firmware */
static unsigned int   MCS_pageSize = 0;          /* Number of pages read from MCS file */
static unsigned char  MCS_DATA[FA125_FIRMWARE_MAX_PAGES][FA125_FIRMWARE_MAX_BYTE_PER_PAGE];    /* The array holding the firmware */
static int            MCS_loaded = 0;             /* 1(0) if firmware loaded (not loaded) */
static unsigned char  tmp_pageData[FA125_FIRMWARE_MAX_BYTE_PER_PAGE];
static int            fa125FirmwareDebug=0;
static int            fa125FirmwareErrorFlags[(FA125_MAX_BOARDS+1)]; /* Firmware Updating Error Flags for each slot */
enum   ifpgatype      {MAIN, FE, PROC, NFPGATYPE};
struct fpga_fw_info
{
  char name[5];
  unsigned int size;
  unsigned int location;
  unsigned int page_location;
  unsigned int page_byte_location;
};

struct firmware_stats
{
  struct timespec erase_time;
  unsigned int nblocks_erased;
  struct timespec buffer_write_time;
  unsigned int nbuffers_written;
  struct timespec buffer_push_time;
  unsigned int nbuffers_pushed;
  struct timespec main_page_read_time;
  unsigned int npages_read;
};

static struct timespec
tsSubtract(struct  timespec  time1, struct  timespec  time2)
{    /* Local variables. */
  struct  timespec  result;

  /* Subtract the second time from the first. */
  if ((time1.tv_sec < time2.tv_sec) ||
      ((time1.tv_sec == time2.tv_sec) &&
       (time1.tv_nsec <= time2.tv_nsec)))
    {		/* TIME1 <= TIME2? */
      result.tv_sec = result.tv_nsec = 0;
    }
  else
    {						/* TIME1 > TIME2 */
      result.tv_sec = time1.tv_sec - time2.tv_sec;
      if (time1.tv_nsec < time2.tv_nsec)
	{
	  result.tv_nsec = time1.tv_nsec + 1000000000L - time2.tv_nsec;
	  result.tv_sec--;				/* Borrow a second. */
	}
      else
	{
	  result.tv_nsec = time1.tv_nsec - time2.tv_nsec;
	}
    }

  return (result);
}

static struct timespec
tsAdd(struct  timespec  time1, struct  timespec  time2)
{
  /* Local variables. */
  struct  timespec  result ;

  /* Add the two times together. */
  result.tv_sec = time1.tv_sec + time2.tv_sec ;
  result.tv_nsec = time1.tv_nsec + time2.tv_nsec ;
  if (result.tv_nsec >= 1000000000L)
    {		/* Carry? */
      result.tv_sec++ ;  result.tv_nsec = result.tv_nsec - 1000000000L ;
    }

  return (result) ;
}

struct firmware_stats fa125FWstats;

struct fpga_fw_info sfpga[NFPGATYPE] =
  {
    {"MAIN", 0x45480,  0x0,      0, 0},
    {"FE"  , 0xC435A,  0x754E0,  0, 0},
    {"PROC", 0x1659FA, 0x1E1B90, 0, 0}
  };

/* Static Firmware Updating routine prototypes */
static int fa125FirmwareWaitForReady(int id, int nwait, int *rwait);
static int fa125FirmwareBlockErase(int id, int iblock, int stayon, int waitForDone);
static int fa125FirmwareWriteToBuffer(int id, int ipage);
static int fa125FirmwarePushBufferToMain(int id, int ipage, int waitForDone);
static int fa125FirmwareWaitForPushBufferToMain(int id, int ipage);
static int fa125FirmwareReadMainPage(int id, int ipage, int stayon);
static int fa125FirmwareReadBuffer(int id);
static int fa125FirmwareVerifyFull(int id);
static int fa125FirmwareVerifyPage(int ipage);
static int fa125FirmwareVerifyErasedPage(int ipage);
static int hex2num(char c);


/**
 *  @ingroup FWUpdate
 *  @brief Set the debugging flags for diagnosing firmware updating problems.
 *  @param debug Debug bit flags
 *  @sa FA125_FIRMWARE_DEBUG_FLAGS
 */
void
fa125FirmwareSetDebug(unsigned int debug)
{
  fa125FirmwareDebug=debug;
}

static int
fa125FirmwareWaitForReady(int id, int nwait, int *rwait)
{
  int iwait=0, rval=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  for(iwait=0; iwait<nwait; iwait++)
    {
      rval = vmeRead32(&fa125p[id]->main.configCSR) & FA125_CONFIGCSR_BUSY;
      if(rval==FA125_CONFIGCSR_BUSY)
	break;
    }

  *rwait = iwait;

  if(iwait==nwait)
    {
      printf("\n%s: ERROR: Operation still in progress after %d tries.\n\n",
	     __FUNCTION__,iwait);
      return ERROR;
    }

  return OK;
}

static int
fa125FirmwareBlockErase(int id, int iblock, int stayon, int waitForDone)
{
#ifdef DOSTAYON
  int rwait=0;
#endif

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;

  /* Configuration csr for block erase */
  vmeWrite32(&fa125p[id]->main.configCSR,
	     FA125_CONFIGCSR_PROG_ENABLE | (FA125_OPCODE_ERASE<<24));

  /* Erase blocks using top 10 bits of page address [30... 21] 0-1023 */
  vmeWrite32(&fa125p[id]->main.configAdrData,
	     (iblock<<21));
  vmeWrite32(&fa125p[id]->main.configAdrData,
	     FA125_CONFIGADRDATA_EXEC | (iblock<<21));
  vmeWrite32(&fa125p[id]->main.configAdrData,
	     (iblock<<21));

  if(waitForDone==0)
    {
      FA125UNLOCK;
      return OK;
    }

  taskDelay(6);

#ifdef DOSTAYON
  /* Pull Execute low before asserting new configuration type */
  if(stayon==0)
    {
      taskDelay(1);
      vmeWrite32(&fa125p[id]->main.configAdrData, 0);

      if(fa125FirmwareWaitForReady(id,100,&rwait)!=OK)
	{
	  printf("\n%s: ERROR: Pull down execute timeout (rwait = %d).\n\n",
		 __FUNCTION__,
		 rwait);
	  FA125UNLOCK;
	  return ERROR;
	}
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_WAIT_FOR_READY)
	{
	  printf("%s: Pull Execute low. wait ticks = %d\n",
		 __FUNCTION__,rwait);
	}
    }
#endif

  FA125UNLOCK;
  return OK;
}

static int
fa125FirmwareWriteToBuffer(int id, int ipage)
{
  int ibadr=0;
  unsigned char data=0;
  int rwait=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  if(ipage>(FA125_FIRMWARE_MAX_PAGES-1))
    {
      printf("\n%s: ERROR: ipage > Maximum page count (%d > %d)\n\n",
	     __FUNCTION__, ipage, (FA125_FIRMWARE_MAX_PAGES-1));
    }

  if(MCS_loaded==0)
    {
      printf("\n%s: ERROR: MCS file not loaded into memory\n\n",
	     __FUNCTION__);
      return ERROR;
    }

  FA125LOCK;
  /* Configuration csr for buffer write */
  vmeWrite32(&fa125p[id]->main.configCSR,
	     FA125_CONFIGCSR_PROG_ENABLE | (FA125_OPCODE_BUFFER_WRITE<<24));

  /* Write configuration data byte using byte addresses 0-527 */
  for(ibadr=0; ibadr<528; ibadr++)
    {
      data = MCS_DATA[ipage][ibadr];

      vmeWrite32(&fa125p[id]->main.configAdrData,
		 (ibadr<<8) | data);
      vmeWrite32(&fa125p[id]->main.configAdrData,
		 FA125_CONFIGADRDATA_EXEC | (ibadr<<8) | data);
      vmeWrite32(&fa125p[id]->main.configAdrData,
		 (ibadr<<8) | data);

      if(fa125FirmwareWaitForReady(id,1000000,&rwait)!=OK)
	{
	  printf("\n%s: ERROR: Buffer Write timeout (byte address = %d, page = %d) (rwait = %d).\n\n",
		 __FUNCTION__,
		 ibadr,ipage,
		 rwait);
	  vmeWrite32(&fa125p[id]->main.configAdrData, 0);
	  FA125UNLOCK;
	  return ERROR;
	}
    }

  FA125UNLOCK;
  return OK;
}

static int
fa125FirmwarePushBufferToMain(int id, int ipage, int waitForDone)
{
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  if(ipage>(FA125_FIRMWARE_MAX_PAGES-1))
    {
      printf("\n%s: ERROR: ipage > Maximum page count (%d > %d)\n\n",
	     __FUNCTION__, ipage, (FA125_FIRMWARE_MAX_PAGES-1));
      return ERROR;
    }

  FA125LOCK;
  /* Configuration csr for buffer to main memory */
  vmeWrite32(&fa125p[id]->main.configCSR,
	     FA125_CONFIGCSR_PROG_ENABLE | (FA125_OPCODE_BUFFER_PUSH<<24));

  /* Push buffer contents using page address */
  vmeWrite32(&fa125p[id]->main.configAdrData,
	     (ipage<<18));
  vmeWrite32(&fa125p[id]->main.configAdrData,
	     FA125_CONFIGADRDATA_EXEC | (ipage<<18));
  vmeWrite32(&fa125p[id]->main.configAdrData,
	     (ipage<<18));
  FA125UNLOCK;

  if(waitForDone==0)
    {
      return OK;
    }

  if(fa125FirmwareWaitForPushBufferToMain(id, ipage)!=OK)
    return ERROR;

  return OK;
}

static int
fa125FirmwareWaitForPushBufferToMain(int id, int ipage)
{
  int rwait=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  FA125LOCK;
  if(fa125FirmwareWaitForReady(id,100000,&rwait)!=OK)
    {
      printf("\n%s: ERROR: Push to main memory timeout (page = %d) (rwait = %d).\n\n",
	     __FUNCTION__,
	     ipage,rwait);
      vmeWrite32(&fa125p[id]->main.configAdrData, 0);
      FA125UNLOCK;
      return ERROR;
    }
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_WAIT_FOR_READY)
    {
      printf("%s: Push buffer contents using page address.  rwait = %d\n",
	     __FUNCTION__,rwait);
    }

  /* Pull Execute low before asserting new configuration type */
  vmeWrite32(&fa125p[id]->main.configAdrData, 0);
  if(fa125FirmwareWaitForReady(id,100,&rwait)!=OK)
    {
      printf("\n%s: ERROR: Pull down execute timeout (rwait = %d).\n\n",
	     __FUNCTION__,
	     rwait);
      FA125UNLOCK;
      return ERROR;
    }

  FA125UNLOCK;
  return OK;
}

static int
fa125FirmwareReadMainPage(int id, int ipage, int stayon)
{
  int ibadr=0;
  int rwait=0;
  unsigned int csraddr = 0;
  unsigned int data=0;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  if(ipage>(FA125_FIRMWARE_MAX_PAGES-1))
    {
      printf("\n%s: ERROR: ipage > Maximum page count (%d > %d)\n\n",
	     __FUNCTION__, ipage, FA125_FIRMWARE_MAX_PAGES-1);
    }

  memset((char *)tmp_pageData, 0, sizeof(tmp_pageData));
/*   taskDelay(1); */

  FA125LOCK;
  /* Configuration csr for main memory read */
  vmeWrite32(&fa125p[id]->main.configCSR,
	     FA125_CONFIGCSR_PROG_ENABLE | (FA125_OPCODE_MAIN_READ<<24));

  for(ibadr=0; ibadr<FA125_FIRMWARE_MAX_BYTE_PER_PAGE; ibadr++)
    {
      csraddr = (ipage<<18) | (ibadr<<8);
      vmeWrite32(&fa125p[id]->main.configAdrData, csraddr);
      vmeWrite32(&fa125p[id]->main.configAdrData, FA125_CONFIGADRDATA_EXEC | csraddr);
      vmeWrite32(&fa125p[id]->main.configAdrData, csraddr);
      if(fa125FirmwareWaitForReady(id,10000,&rwait)!=OK)
	{
	  printf("\n%s: ERROR: Main memory read timeout (byte address = %d, page = %d) (rwait = %d).\n\n",
		 __FUNCTION__,
		 ibadr,ipage,rwait);
	  FA125UNLOCK;
	  return ERROR;
	}

      data = vmeRead32(&fa125p[id]->main.configCSR);
      tmp_pageData[ibadr] = data & FA125_CONFIGCSR_DATAREAD_MASK;

    }

  /* Pull Execute low before asserting new configuration type */
#ifdef DOSTAYON
  if(stayon==0)
    {
      vmeWrite32(&fa125p[id]->main.configAdrData, 0);
      if(fa125FirmwareWaitForReady(id,100,&rwait)!=OK)
	{
	  printf("\n%s: ERROR: Pull down execute timeout (rwait = %d).\n\n",
		 __FUNCTION__,
		 rwait);
	  FA125UNLOCK;
	  return ERROR;
	}
      taskDelay(1);
    }
#endif

  FA125UNLOCK;
  return OK;
}

static int
fa125FirmwareReadBuffer(int id)
{
  int ibadr=0;
  int rwait=0;
  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  memset((char *)tmp_pageData, 0, sizeof(tmp_pageData));

  FA125LOCK;
  /* Configuration csr for buffer memory read */
  vmeWrite32(&fa125p[id]->main.configCSR,
	     FA125_CONFIGCSR_PROG_ENABLE | (FA125_OPCODE_BUFFER_READ<<24));

/*   taskDelay(1); */

  /* Read main memory using full address (page and byte) */
  for(ibadr=0; ibadr<FA125_FIRMWARE_MAX_BYTE_PER_PAGE; ibadr++)
    {
      vmeWrite32(&fa125p[id]->main.configAdrData,
		 (ibadr<<8));
      vmeWrite32(&fa125p[id]->main.configAdrData,
		 FA125_CONFIGADRDATA_EXEC | (ibadr<<8));
      vmeWrite32(&fa125p[id]->main.configAdrData,
		 (ibadr<<8));
      if(fa125FirmwareWaitForReady(id,100,&rwait)!=OK)
	{
	  printf("\n%s: ERROR: Main memory read timeout (byte address = %d) (rwait = %d).\n\n",
		 __FUNCTION__,
		 ibadr,rwait);
	  FA125UNLOCK;
	  return ERROR;
	}

      tmp_pageData[ibadr] = vmeRead32(&fa125p[id]->main.configCSR) & FA125_CONFIGCSR_DATAREAD_MASK;
    }

  /* Pull Execute low before asserting new configuration type */
  vmeWrite32(&fa125p[id]->main.configAdrData, 0);

  FA125UNLOCK;
  return OK;
}

static int
fa125FirmwareVerifyFull(int id)
{
  int ipage=0;
  int stayon=1;
  struct timespec time_start, time_end, res;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

  if(MCS_loaded==0)
    {
      printf("\n%s: ERROR: MCS file not loaded into memory\n\n",
	     __FUNCTION__);
      return ERROR;
    }


#ifndef VXWORKSPPC
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
    {
      fa125FWstats.npages_read                 = 0;
      fa125FWstats.main_page_read_time.tv_sec  = 0;
      fa125FWstats.main_page_read_time.tv_nsec = 0;
    }
#endif

  FA125LOCK;
  vmeWrite32(&fa125p[id]->main.configCSR, 0);
  FA125UNLOCK;

  printf("%3d: ",id);
  fflush(stdout);

  for(ipage=0; ipage<=MCS_pageSize; ipage++)
    {
      if((ipage%(8*0x10))==0)
	{
	  printf(".");
	  fflush(stdout);
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  clock_gettime(CLOCK_MONOTONIC, &time_start);
	}
#endif
      if(ipage==(MCS_pageSize-1)) stayon=0;

      /* Read a page from main memory */
      if(fa125FirmwareReadMainPage(id, ipage, stayon)!=OK)
	{
	  vmeWrite32(&fa125p[id]->main.configAdrData, 0);
	  printf("\n%s: Error reading from main memory (page = %d)\n\n",
		 __FUNCTION__,ipage);
	  return ERROR;
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  fa125FWstats.npages_read++;
	  clock_gettime(CLOCK_MONOTONIC, &time_end);
	  res = tsSubtract(time_end, time_start);
	  fa125FWstats.main_page_read_time = tsAdd(fa125FWstats.main_page_read_time, res);
	}
#endif

      /* Verify the page with that read from the file */
      if(fa125FirmwareVerifyPage(ipage)!=OK)
	{
	  printf("\n%s: ERROR in verifying page %d\n\n",
		 __FUNCTION__,ipage);
	  return ERROR;
	}

    }
  printf("\n");


  return OK;

}

/**
 *  @ingroup FWUpdate
 *  @brief Verify that the firmware has been written correctly to all initialized fADC125s
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125FirmwareGVerifyFull()
{
  int ifa=0, id=0;

  if(MCS_loaded==0)
    {
      printf("\n%s: ERROR: MCS file not loaded into memory\n\n",
	     __FUNCTION__);
      return ERROR;
    }

  printf("** Verifying Main Memory **\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);

      if(fa125FirmwareErrorFlags[id]!=0)
	continue;

      if(fa125FirmwareVerifyFull(id)!=OK)
	{
	  printf("\n%s: Slot %d: Error in verifying full firmware\n\n",
		 __FUNCTION__,id);
	  fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_VERIFY_WRITE;
/* 	  return ERROR; */
	}
    }

  return OK;
}

static int
fa125FirmwareVerifyPage(int ipage)
{
  int ibyte=0;
  int nerror=0;
  if(MCS_loaded==0)
    {
      printf("\n%s: ERROR: MCS file not loaded into memory\n\n",
	     __FUNCTION__);
      return ERROR;
    }

  for(ibyte=0; ibyte<FA125_FIRMWARE_MAX_BYTE_PER_PAGE; ibyte++)
    {
      if(tmp_pageData[ibyte] != MCS_DATA[ipage][ibyte])
	{
	  nerror++;
	  if(nerror<20)
	    {
	      printf("%s: %4d: Buffer (0x%02x) != MCS file (0x%02x)\n",
		     __FUNCTION__,ibyte,tmp_pageData[ibyte],MCS_DATA[ipage][ibyte]);
	    }
	}
    }

  if(nerror>0)
    {
      printf("\n%s: ERROR: Total number of errors = %d\n\n",
	     __FUNCTION__,nerror);
      return ERROR;
    }

  return OK;
}

static int
fa125FirmwareVerifyErasedPage(int ipage)
{
  int ibyte=0;
  int nerror=0;

  for(ibyte=0; ibyte<FA125_FIRMWARE_MAX_BYTE_PER_PAGE; ibyte++)
    {
      if(tmp_pageData[ibyte] != 0xff)
	{
	  nerror++;
	  if(nerror<20)
	    {
	      printf("%s: %4d: Buffer (0x%02x) != Erased (0x%02x)\n",
		     __FUNCTION__,ibyte,tmp_pageData[ibyte],0xff);
	    }
	}
    }

  if(nerror>0)
    {
      printf("\n%s: ERROR: Total number of errors = %d\n\n",
	     __FUNCTION__,nerror);
      return ERROR;
    }

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

/**
 *  @ingroup FWUpdate
 *  @brief Read in the firmware from selected MCS file.
 *  @param filename Name of file that contains the firmware in MCS format.
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125FirmwareReadMcsFile(char *filename)
{
  FILE *mcsFile=NULL;
  char ihexLine[200], *pData;
  int len=0, datalen=0;
  unsigned int nbytes=0, line=0, hiChar=0, loChar=0;
  int ibyte=0, ipage=0;
  unsigned int readMCS=0;
  int ichar;
  int ifpga=MAIN, fpga_bytes=0, getFirmwareLocation=0;
  unsigned int mcs_addr=0xFFF0, addr0=0, addr1=0, addr2=0, addr3=0;
  unsigned int prev_elar_data=-1, elar_data=0, data0=0, data1=0, data2=0, data3=0;
  unsigned int mcs_line_number=0;

  /* Initialize the local storage array */
  memset((char *)MCS_DATA,0xff,sizeof(MCS_DATA));

  mcsFile = fopen(filename,"r");
  if(mcsFile==NULL)
    {
      perror("fopen");
      printf("\n%s: ERROR opening file (%s) for reading\n\n",
	     __FUNCTION__,filename);
      return ERROR;
    }

  ifpga=MAIN; /* First firmware is for the MAIN FPGA */

  while(!feof(mcsFile))
    {
      mcs_line_number++;
      /* Get the current line */
      if(!fgets(ihexLine, sizeof(ihexLine), mcsFile))
	break;

      /* Get the the length of this line */
      len = strlen(ihexLine);

      if(len >= 5)
	{
	  /* Check for the start code */
	  if(ihexLine[0] != ':')
	    {
	      printf("\n%s: ERROR parsing file at line %d\n\n",
		     __FUNCTION__,line);
	      return ERROR;
	    }

	  /* Get the byte count */
	  hiChar = hex2num(ihexLine[1]);
	  loChar = hex2num(ihexLine[2]);
	  datalen = (hiChar)<<4 | loChar;

	  if(strncmp("00",&ihexLine[7], 2) == 0) /* Data Record */
	    {
	      /* Get the address */
	      addr3 = hex2num(ihexLine[3]);
	      addr2 = hex2num(ihexLine[4]);
	      addr1 = hex2num(ihexLine[5]);
	      addr0 = hex2num(ihexLine[6]);
	      mcs_addr = (elar_data<<16) | (addr3<<12) | (addr2<<8) | (addr1<<4) | addr0;

	      /* Determine the initial page and byte number from this address */
	      ipage = (int)(mcs_addr / FA125_FIRMWARE_MAX_BYTE_PER_PAGE);
	      ibyte = (int)(mcs_addr % FA125_FIRMWARE_MAX_BYTE_PER_PAGE);

	      if(getFirmwareLocation==1)
		{
		  /* ipage = sfpga[ifpga].page_location; */
		  /* ibyte = sfpga[ifpga].page_byte_location; */
		  sfpga[ifpga].page_location = ipage;
		  sfpga[ifpga].page_byte_location = ibyte;
		  getFirmwareLocation=0;
		}

	      pData = &ihexLine[9]; /* point to the beginning of the data */
	      while(datalen--)
		{
		  hiChar = hex2num(*pData++);
		  loChar = hex2num(*pData++);
		  MCS_DATA[ipage][ibyte] =
		    ((hiChar)<<4) | (loChar);
		  fpga_bytes++;
		  if(readMCS>=MCS_MAX_SIZE)
		    {
		      printf("\n%s: ERROR: TOO BIG!\n\n",__FUNCTION__);
		      return ERROR;
		    }
/* 		  if(ipage<2) */
/* 		    printf("%4d %3d: 0x%02x\n",ipage,ibyte,MCS_DATA[ipage][ibyte]); */
		  if((ibyte+1)==FA125_FIRMWARE_MAX_BYTE_PER_PAGE)
		    { /* If at the end of the page, start up a new one */
		      ibyte=0;
		      ipage++;
		    }
		  else
		    {
		      ibyte++;
		    }
		  readMCS++;
		  nbytes++;
		}
	    }
	  else if(strncmp("04",&ihexLine[7], 2) == 0) /* ELAR */
	    {
	      /* Get the elar data */
	      data3 = hex2num(ihexLine[9]);
	      data2 = hex2num(ihexLine[10]);
	      data1 = hex2num(ihexLine[11]);
	      data0 = hex2num(ihexLine[12]);
	      elar_data = (data3<<12) | (data2<<8) | (data1<<4) | data0;

/* 	      if(prev_addr!=0xFFF0) */
	      if(elar_data != (prev_elar_data + 1))
		{
		  if(ifpga!=NFPGATYPE)
		    {
		      sfpga[ifpga].size = fpga_bytes;
		      ifpga++;
		      getFirmwareLocation=1;
		      /* ipage = sfpga[ifpga].page_location; */
		      /* ibyte = sfpga[ifpga].page_byte_location; */
		      fpga_bytes=0;
		    }
/* 		  printf("%8d: fpga_bytes = %8d ipage = 0x%06x (bytes = 0x%x)\n", */
/* 			 mcs_line_number,fpga_bytes,ipage, ipage*FA125_FIRMWARE_MAX_BYTE_PER_PAGE); */
		}
	      prev_elar_data = elar_data;
	    }
	  else if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MCS_SKIPPED_LINES)
	    {
	      printf("%s: Skipped line (%d): \t>%s<\n",
		     __FUNCTION__,line,ihexLine);
	    }

	}
      line++;
    }

  MCS_pageSize = ipage+1;
  MCS_dataSize = readMCS;

  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MCS_FILE)
    {
      printf("MCS_dataSize = %d   MCS_pageSize = %d\n",MCS_dataSize,MCS_pageSize);

      for(ichar=0; ichar<16*10; ichar++)
	{
	  if((ichar%16) == 0)
	    printf("\n");
	  printf("0x%02x ",MCS_DATA[0][ichar]);
	}
      printf("\n\n");
    }

  MCS_loaded = 1;

  fa125FirmwarePrintFPGAStats();

  fclose(mcsFile);
  return OK;
}

/**
 *  @ingroup FWUpdate
 *  @brief Prints to standard out, the memory size and location of each FPGA firmware
 */
void
fa125FirmwarePrintFPGAStats()
{
  int ifpga=MAIN;

  for(ifpga=MAIN; ifpga<NFPGATYPE; ifpga++)
    {
      printf("%s: size = %d   location = 0x%08x\n",
	     sfpga[ifpga].name, sfpga[ifpga].size, sfpga[ifpga].location);
    }


}

/**
 *  @ingroup FWUpdate
 *  @brief Prints the selected firmware page to standard out
 *  @param page Selected page
 */
void
fa125FirmwarePrintPage(int page)
{
  int ichar=0;

  if(MCS_loaded==0)
    {
      printf("\n%s: ERROR: MCS file not loaded into memory\n\n",
	     __FUNCTION__);
      return;
    }

  for(ichar=0; ichar<FA125_FIRMWARE_MAX_BYTE_PER_PAGE; ichar++)
    {
      if((ichar%16) == 0)
	printf("\n");
      printf("0x%02x ",MCS_DATA[page][ichar]);
    }
  printf("\n\n");



}

/**
 *  @ingroup FWUpdate
 *  @brief Erase the entire contents of the configuration ROM of the selected fADC125
 *  @param id Slot Number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125FirmwareEraseFull(int id)
{
  int ipage=0;
  int iblock=0, nblocks=1024;
  int stayon=1;
  struct timespec time_start, time_end, res;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

#ifndef VXWORKSPPC
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
    {
      fa125FWstats.nblocks_erased=0;
      fa125FWstats.erase_time.tv_sec  = 0;
      fa125FWstats.erase_time.tv_nsec = 0;
    }
#endif

  printf("** Erasing Main Memory **\n");
  for(iblock=0; iblock<nblocks; iblock++)
    {
      if((iblock%0x10)==0)
	{
	  printf(".");
	  fflush(stdout);
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  clock_gettime(CLOCK_MONOTONIC, &time_start);
	}
#endif

      /* Perform a block erase */
      if(fa125FirmwareBlockErase(id,iblock,stayon,1)!=OK)
	{
	  printf("\n%s: ERROR: Block erase failed\n\n",__FUNCTION__);
	  return ERROR;
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  fa125FWstats.nblocks_erased++;
	  clock_gettime(CLOCK_MONOTONIC, &time_end);
	  res = tsSubtract(time_end, time_start);
	  fa125FWstats.erase_time = tsAdd(res, fa125FWstats.erase_time);
	}
#endif
    }
  printf("\n");
  fflush(stdout);

  taskDelay(3);

  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_VERIFY_ERASE)
    {
      stayon=1;
      printf("** Verify erase **\n");
      for(iblock=0; iblock<nblocks; iblock++)
	{
	  if((iblock%0x10)==0)
	    {
	      printf(".");
	      fflush(stdout);
	    }

	  for(ipage=iblock*8; ipage<8*(iblock+1); ipage++)
	    {

	      /* Read a page from main memory */
	      if(fa125FirmwareReadMainPage(id, ipage, stayon)!=OK)
		{
		  vmeWrite32(&fa125p[id]->main.configAdrData, 0);
		  printf("\n%s: Error reading from main memory (page = %d)\n\n",
			 __FUNCTION__,ipage);
		  return ERROR;
		}

	      /* Verify the page was erased */
	      if(fa125FirmwareVerifyErasedPage(ipage)!=OK)
		{
		  printf("\n%s: ERROR: Block erase failed to erase block %d (page %d)\n\n",
			 __FUNCTION__,iblock,ipage);
		  return ERROR;
		}
	    }
	}
      printf("\n");
      fflush(stdout);
    }

  return OK;

}

/**
 *  @ingroup FWUpdate
 *  @brief Erase the entire contents of the configuration ROM for all initialized fADC125s
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125FirmwareGEraseFull()
{
  int ipage=0;
  int iblock=0, nblocks=1024;
  int stayon=1;
  struct timespec time_start, time_end, res;
  int id=0, ifa=0;
  int nerrors=0;

#ifndef VXWORKSPPC
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
    {
      fa125FWstats.nblocks_erased=0;
      fa125FWstats.erase_time.tv_sec  = 0;
      fa125FWstats.erase_time.tv_nsec = 0;
    }
#endif

  memset((char *)fa125FirmwareErrorFlags, 0, sizeof(fa125FirmwareErrorFlags));

  printf("** Erasing Main Memory **\n");
  printf("All: ");
  fflush(stdout);

  for(iblock=0; iblock<nblocks; iblock++)
    {
      for(ifa=0; ifa<nfa125; ifa++)
	{
	  id = fa125Slot(ifa);

	  if(((iblock%0x10)==0) & (ifa==0))
	    {
	      printf(".");
	      fflush(stdout);
	    }

	  if((iblock!=0) && (ifa==0))
	    {
	      /* Wait for the previous block on first module to complete */
	      taskDelay(7);
	    }

	  if(fa125FirmwareErrorFlags[id]!=0)
	    continue;

	  if(iblock!=0)
	    {
#ifndef VXWORKSPPC
	      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
		{
		  fa125FWstats.nblocks_erased++;
		  clock_gettime(CLOCK_MONOTONIC, &time_end);
		  res = tsSubtract(time_end, time_start);
		  fa125FWstats.erase_time = tsAdd(res, fa125FWstats.erase_time);
		}
#endif
	    }

#ifndef VXWORKSPPC
	  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	    {
	      clock_gettime(CLOCK_MONOTONIC, &time_start);
	    }
#endif

	  /* Perform a block erase */
	  if(fa125FirmwareBlockErase(id,iblock,stayon,0)!=OK)
	    {
	      printf("\n%s: ERROR: Slot %d: Block erase failed to begin\n\n",
		     __FUNCTION__,id);
	      fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_ERASE;
/* 	      return ERROR; */
	    }

	} /* nfa125 */
    } /* nblocks */


  printf("\n");
  fflush(stdout);

  /* Wait for last block erase to complete */
  taskDelay(7);

#ifndef VXWORKSPPC
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
    {
      fa125FWstats.nblocks_erased++;
      clock_gettime(CLOCK_MONOTONIC, &time_end);
      res = tsSubtract(time_end, time_start);
      fa125FWstats.erase_time = tsAdd(res, fa125FWstats.erase_time);
    }
#endif
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_VERIFY_ERASE)
      {
	stayon=1;
	printf("** Verify erase **\n");
	for(ifa=0; ifa<nfa125; ifa++)
	  {
	    id = fa125Slot(ifa);

	    if(fa125FirmwareErrorFlags[id]!=0)
	      continue;

	    printf("%3d: ",id);
	    fflush(stdout);

	    for(iblock=0; iblock<nblocks; iblock++)
	      {
		if((iblock%0x10)==0)
		  {
		    printf(".");
		    fflush(stdout);
		  }

		if(fa125FirmwareErrorFlags[id]!=0)
		  break;

		for(ipage=iblock*8; ipage<8*(iblock+1); ipage++)
		  {

		    if(fa125FirmwareErrorFlags[id]!=0)
		      break;

		    /* Read a page from main memory */
		    if(fa125FirmwareReadMainPage(id, ipage, stayon)!=OK)
		      {
			vmeWrite32(&fa125p[id]->main.configAdrData, 0);
			printf("\n%s: Slot %d: Error reading from main memory (page = %d)\n\n",
			       __FUNCTION__,id,ipage);
			fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_VERIFY_ERASE;
/* 			return ERROR; */
		      }

		    /* Verify the page with that read from the file */
		    if(fa125FirmwareVerifyErasedPage(ipage)!=OK)
		      {
			printf("\n%s: Slot %d: Block erase failed to erase block %d (page %d)\n\n",
			       __FUNCTION__,id, iblock,ipage);
			fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_VERIFY_ERASE;
/* 			return ERROR; */
		      }
		  }
	      } /* nblocks */
	    printf("\n");
	  } /* nfa125 */

	fflush(stdout);
      }

  /* Count how many modules had errors */
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);
      if(fa125FirmwareErrorFlags[id]!=0)
	nerrors++;
    }

  /* Return ERROR if all modules had errors, otherwise we can continue */
  if(nerrors==nfa125)
    return ERROR;

  return OK;

}

/**
 *  @ingroup FWUpdate
 *  @brief Write the contents of the read in MCS file to the selected fADC125 Configuration ROM
 *  @param id Slot Number
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125FirmwareWriteFull(int id)
{
  int ipage=0;
  struct timespec time_start, time_end, res;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

#ifndef VXWORKSPPC
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
    {
      fa125FWstats.nbuffers_written          = 0;
      fa125FWstats.buffer_write_time.tv_sec  = 0;
      fa125FWstats.buffer_write_time.tv_nsec = 0;

      fa125FWstats.nbuffers_pushed          = 0;
      fa125FWstats.buffer_push_time.tv_sec  = 0;
      fa125FWstats.buffer_push_time.tv_nsec = 0;
    }
#endif

  printf("** Writing file to memory **\n");
  for(ipage=0; ipage<=MCS_pageSize; ipage++)
    {
      if((ipage%(8*0x10))==0)
	{
	  printf(".");
	  fflush(stdout);
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  clock_gettime(CLOCK_MONOTONIC, &time_start);
	}
#endif
      if(fa125FirmwareWriteToBuffer(id, ipage)!=OK)
	{
	  printf("\n%s: Error writing to buffer\n\n",__FUNCTION__);
	  return ERROR;
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  fa125FWstats.nbuffers_written++;
	  clock_gettime(CLOCK_MONOTONIC, &time_end);
	  res = tsSubtract(time_end, time_start);
	  fa125FWstats.buffer_write_time = tsAdd(fa125FWstats.buffer_write_time, res);
	}
#endif

      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_WRITE_BUFFER)
	{
	  if(fa125FirmwareReadBuffer(id)!=OK)
	    {
	      printf("\n%s: Error reading from buffer\n\n",__FUNCTION__);
	      return ERROR;
	    }

	  if(fa125FirmwareVerifyPage(ipage)!=OK)
	    {
	      printf("\n\n%s: ERROR in verifying page %d\n\n",
		     __FUNCTION__,ipage);
	      return ERROR;
	    }
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  clock_gettime(CLOCK_MONOTONIC, &time_start);
	}
#endif
      if(fa125FirmwarePushBufferToMain(id, ipage, 1)!=OK)
	{
	  printf("\n%s: Error in pushing buffer to main memory (page = %d)\n\n",
		 __FUNCTION__,ipage);
	  return ERROR;
	}
#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  fa125FWstats.nbuffers_pushed++;
	  clock_gettime(CLOCK_MONOTONIC, &time_end);
	  res = tsSubtract(time_end, time_start);
	  fa125FWstats.buffer_push_time = tsAdd(fa125FWstats.buffer_push_time, res);
	}
#endif
    }

  printf("\n");
  fflush(stdout);

  printf("** Verifying Main Memory **\n");
  if(fa125FirmwareVerifyFull(id)!=OK)
    {
      printf("\n%s: Error in verifying full firmware\n\n",
	     __FUNCTION__);
      return ERROR;
    }

  return OK;
}

/**
 *  @ingroup FWUpdate
 *  @brief Write the contents of the read in MCS file to the Configuration ROM of all initialized fADC125s
 *  @return OK if successful, otherwise ERROR.
 */
int
fa125FirmwareGWriteFull()
{
  int id=0, ifa=0;
  int ipage=0;
  struct timespec time_start, time_end, res;

  if(id==0) id=fa125ID[0];

  if((id<0) || (id>21) || (fa125p[id] == NULL))
    {
      logMsg("\n%s: ERROR : FA125 in slot %d is not initialized\n\n",__FUNCTION__,id,3,4,5,6);
      return ERROR;
    }

#ifndef VXWORKSPPC
  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
    {
      fa125FWstats.nbuffers_written          = 0;
      fa125FWstats.buffer_write_time.tv_sec  = 0;
      fa125FWstats.buffer_write_time.tv_nsec = 0;

      fa125FWstats.nbuffers_pushed          = 0;
      fa125FWstats.buffer_push_time.tv_sec  = 0;
      fa125FWstats.buffer_push_time.tv_nsec = 0;
    }
#endif

  printf("** Writing file to memory **\n");
  printf("All: ");
  fflush(stdout);

  for(ipage=0; ipage<=MCS_pageSize; ipage++)
    {
      for(ifa=0; ifa<nfa125; ifa++)
	{
	  id = fa125Slot(ifa);

	  if(((ipage%(8*0x10))==0) && (ifa==0))
	    {
	      printf(".");
	      fflush(stdout);
	    }

	  if(fa125FirmwareErrorFlags[id]!=0)
	    continue;

	  if(ipage!=0)
	    {
	      if(fa125FirmwareWaitForPushBufferToMain(id, ipage-1)!=OK)
		{
		  printf("\n%s: Slot %d: Failed to push buffer to main (page %d)\n",
			 __FUNCTION__,id,ipage-1);
		  fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_PUSH_WAIT;
/* 		  return ERROR; */
		}

	      /* Wait for the previous page push to complete */
#ifndef VXWORKSPPC
	      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
		{
		  fa125FWstats.nbuffers_pushed++;
		  clock_gettime(CLOCK_MONOTONIC, &time_end);
		  res = tsSubtract(time_end, time_start);
		  fa125FWstats.buffer_push_time = tsAdd(fa125FWstats.buffer_push_time, res);
		}
#endif
	    }

#ifndef VXWORKSPPC
	  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	    {
	      clock_gettime(CLOCK_MONOTONIC, &time_start);
	    }
#endif

	  /* Write page to buffer */
	  if(fa125FirmwareWriteToBuffer(id, ipage)!=OK)
	    {
	      printf("\n%s: Slot %d: Error writing to buffer\n",__FUNCTION__,id);
	      fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_WRITE;
/* 	      return ERROR; */
	    }

#ifndef VXWORKSPPC
	  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	    {
	      fa125FWstats.nbuffers_written++;
	      clock_gettime(CLOCK_MONOTONIC, &time_end);
	      res = tsSubtract(time_end, time_start);
	      fa125FWstats.buffer_write_time = tsAdd(fa125FWstats.buffer_write_time, res);
	    }

	  if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	    {
	      clock_gettime(CLOCK_MONOTONIC, &time_start);
	    }
#endif

	  /* Push buffer to main */
	  if(fa125FirmwarePushBufferToMain(id, ipage, 0)!=OK)
	    {
	      printf("\n%s: Error in pushing buffer to main memory (page = %d)\n",
		     __FUNCTION__,ipage);
	      fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_PUSH;
/* 	      return ERROR; */
	    }

	} /* nfa125 */
    } /* MCS_pageSize */

  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);

      if(fa125FirmwareErrorFlags[id]!=0)
	continue;

      /* Wait for last page push to complete */
      if(fa125FirmwareWaitForPushBufferToMain(id, MCS_pageSize-1)!=OK)
	{
	  printf("\n%s: Slot %d: Failed to push buffer to main (page %d)\n",
		 __FUNCTION__,id,ipage-1);
	  fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_PUSH_WAIT;
/* 	  return ERROR; */
	}

#ifndef VXWORKSPPC
      if(fa125FirmwareDebug&FA125_FIRMWARE_DEBUG_MEASURE_TIMES)
	{
	  fa125FWstats.nbuffers_pushed++;
	  clock_gettime(CLOCK_MONOTONIC, &time_end);
	  res = tsSubtract(time_end, time_start);
	  fa125FWstats.buffer_push_time = tsAdd(fa125FWstats.buffer_push_time, res);
	}
#endif

    } /* nfa125 */

  printf("\n");
  fflush(stdout);

  printf("** Verifying Main Memory **\n");
  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);

      if(fa125FirmwareErrorFlags[id]!=0)
	continue;

      if(fa125FirmwareVerifyFull(id)!=OK)
	{
	  printf("\n%s: Slot %d: Error in verifying full firmware\n",
		 __FUNCTION__,id);
	  fa125FirmwareErrorFlags[id] |= FA125_FIRMWARE_ERROR_VERIFY_WRITE;
/* 	  return ERROR; */
	}
    }

  return OK;
}

/**
 *  @ingroup FWUpdate
 *  @brief Print the time it took for each firmware update step to standard out.
 *     debug = FA125_FIRMWARE_DEBUG_MEASURE_TIMES
 *     Must be selected prior to update.
 *  @sa fa125FirmwareSetDebug
 */
void
fa125FirmwarePrintTimes()
{
  double erase, write, push, read;

  erase = fa125FWstats.erase_time.tv_sec
    + (double)fa125FWstats.erase_time.tv_nsec*1e-9;
  write = fa125FWstats.buffer_write_time.tv_sec
    + (double)fa125FWstats.buffer_write_time.tv_nsec*1e-9;
  push  = fa125FWstats.buffer_push_time.tv_sec
    + (double)fa125FWstats.buffer_push_time.tv_nsec*1e-9;
  read  = fa125FWstats.main_page_read_time.tv_sec
    + (double)fa125FWstats.main_page_read_time.tv_nsec*1e-9;

  printf("\n");

  printf(" Blocks Erased  = %d\n",
	 fa125FWstats.nblocks_erased);
  printf(" Erase time   %5ld (sec)  %10ld (ns)  = %lf (sec)\n",
	 fa125FWstats.erase_time.tv_sec,
	 fa125FWstats.erase_time.tv_nsec,
	 erase);
  printf("\n");

  printf(" Pages written  = %d\n",
	 fa125FWstats.nbuffers_written);
  printf(" Write time   %5ld (sec)  %10ld (ns)  = %lf (sec)\n",
	 fa125FWstats.buffer_write_time.tv_sec,
	 fa125FWstats.buffer_write_time.tv_nsec,
	 write);
  printf("\n");

  printf(" Pages pushed   = %d\n",
	 fa125FWstats.nbuffers_pushed);
  printf(" Push time    %5ld (sec)  %10ld (ns)  = %lf (sec)\n",
	 fa125FWstats.buffer_push_time.tv_sec,
	 fa125FWstats.buffer_push_time.tv_nsec,
	 push);
  printf("\n");


  printf(" Pages verified = %d  (per module)\n",
	 fa125FWstats.npages_read);
  printf(" Read time    %5ld (sec)  %10ld (ns)  = %lf (sec)\n",
	 fa125FWstats.main_page_read_time.tv_sec,
	 fa125FWstats.main_page_read_time.tv_nsec,
	 read);
  printf("\n");
}

/**
 *  @ingroup FWUpdate
 *  @brief Print to standard out the results of the firmware update for each initialized fADC125
 */
int
fa125FirmwareGCheckErrors()
{
  int ifa=0, id=0;
  int rval=OK;

  for(ifa=0; ifa<nfa125; ifa++)
    {
      id = fa125Slot(ifa);

      printf("%3d: ",id);
      fflush(stdout);

      if(fa125FirmwareErrorFlags[id] == 0)
	{
	  printf(" OK!\n");
	  continue;
	}

      if(fa125FirmwareErrorFlags[id] & FA125_FIRMWARE_ERROR_ERASE)
	{
	  printf(" ERROR on Erasing Main Memory\n");
	  rval=ERROR;
	}

      if(fa125FirmwareErrorFlags[id] & FA125_FIRMWARE_ERROR_VERIFY_ERASE)
	{
	  printf(" ERROR on Verifying Erased Main Memory\n");
	  rval=ERROR;
	}

      if(fa125FirmwareErrorFlags[id] & FA125_FIRMWARE_ERROR_WRITE)
	{
	  printf(" ERROR on Writing to Buffer\n");
	  rval=ERROR;
	}

      if(fa125FirmwareErrorFlags[id] & FA125_FIRMWARE_ERROR_PUSH)
	{
	  printf(" ERROR on Pushing Buffer to Main Memory\n");
	  rval=ERROR;
	}

      if(fa125FirmwareErrorFlags[id] & FA125_FIRMWARE_ERROR_PUSH_WAIT)
	{
	  printf(" ERROR on Waiting to Push Buffer to Main Memory\n");
	  rval=ERROR;
	}

      if(fa125FirmwareErrorFlags[id] & FA125_FIRMWARE_ERROR_VERIFY_WRITE)
	{
	  printf(" ERROR on Verifying Firmware in Main Memory\n");
	  rval=ERROR;
	}

    }

  return rval;
}

/* Processing Mode Names (long version)
   Indices are in library convention (HW+1)
*/
const char *fa125_mode_names[FA125_MAXIMUM_NMODES] =
  {
    "", // 0
    "", // 1
    "", // 2
    "Integral and Time (CDC_short)",             // 3
    "Integral and Time (FDC_short)",             // 4
    "Peak Amplitude and Time (FDC_amp_short)",   // 5
    "Pulse Data and Samples (CDC_long)",         // 6
    "Pulse Data and Samples (FDC_sum_long)",     // 7
    "Peak Amplitude and Samples (FDC_amp_long)", // 8
  };

/* Processing Mode Names (short version)
   Indices are in library convention (HW+1)
*/
const char *fa125_modes[FA125_MAXIMUM_NMODES] =
  {
    "", // 0
    "", // 1
    "", // 2
    "CDC_short",      // 3
    "FDC_short",      // 4
    "FDC_amp_short",  // 5
    "CDC_long",       // 6
    "FDC_sum_long",   // 7
    "FDC_amp_long",   // 8
  };
