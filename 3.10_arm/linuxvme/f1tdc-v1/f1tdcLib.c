/******************************************************************************
 *
 *  f1tdcLib.c  -  Driver library for JLAB config and readout of JLAB F1 TDC
 *                   using a VxWorks 5.4 or later based Single Board computer.
 *
 *  Author: David Abbott
 *          Jefferson Lab Data Acquisition Group
 *          August 2003
 *
 *  Revision  1.0 - Initial Revision
 *                    - Supports up to 20 F1 boards in a Crate
 *                    - Programmed I/O and Block reads
 *
 *  Revision  1.1 - Support for JLAB Signal Distribution Card
 *                              JLAB Backplane Dist. board
 *                              Mulitblock Readout
 *                              Global Set routines
 *                              Support for new F1 firmware
 *
 *  Revision  1.2 - Bug fixes for use with MV5500 CPUs
 *                  Added Improved Status and new Event Flush routines
 *                  Added more Global routines for Multiblock modes
 *
 *
 *  Revision 1.3 - Requires Firmware upgrade 0xd7 or greater
 *                 More robust readout/config routines
 *                 Error condition Checking
 *                 Bug fixes - Filler word check
 *
 *  Revision 1.4 - Requires Firmware upgrade 0xe2 or greater
 *                 Additional readout/config/test routines
 *
 *
 */

#ifdef VXWORKS
#include <vxWorks.h>
#else
#include "jvme.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#ifdef VXWORKS
#include <logLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <iv.h>
#include <semLib.h>
#include <vxLib.h>
#else
#include <unistd.h>
#endif

/* Include TDC definitions */
#include "f1tdcLib.h"

/* Include DMA Library definitions */
#ifdef VXWORKS
#include "universeDma.h"
#endif

/* Mutex to guard flexio read/writes */
pthread_mutex_t f1Mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t f1sdcMutex = PTHREAD_MUTEX_INITIALIZER;
#define F1LOCK      if(pthread_mutex_lock(&f1Mutex)<0) perror("pthread_mutex_lock");
#define F1UNLOCK    if(pthread_mutex_unlock(&f1Mutex)<0) perror("pthread_mutex_unlock");
#define F1SDCLOCK   if(pthread_mutex_lock(&f1sdcMutex)<0) perror("pthread_mutex_lock");
#define F1SDCUNLOCK if(pthread_mutex_unlock(&f1sdcMutex)<0) perror("pthread_mutex_unlock");

/* Define external Functions */
#ifdef VXWORKS
IMPORT STATUS sysBusToLocalAdrs(int, char *, char **);
IMPORT STATUS intDisconnect(int);
IMPORT STATUS sysIntEnable(int);
IMPORT STATUS sysIntDisable(int);

#define EIEIO    __asm__ volatile ("eieio")
#define SYNC     __asm__ volatile ("sync")
#endif

/* Define Interrupts variables */
BOOL f1tdcIntRunning = FALSE;	/* running flag */
int f1tdcIntID = -1;		/* id number of ADC generating interrupts */
LOCAL VOIDFUNCPTR f1tdcIntRoutine = NULL;	/* user interrupt service routine */
LOCAL int f1tdcIntArg = 0;	/* arg to user routine */
LOCAL UINT32 f1tdcIntLevel = F1_VME_INT_LEVEL;	/* default VME interrupt level */
LOCAL UINT32 f1tdcIntVec = F1_VME_INT_VEC;	/* default interrupt Vector */

/* Define static default config data
   0 Hi Rez      - Synchronous
   1 Hi Rez      - Non-Synchronous (Start reqd)
   2 Normal Rez  - Synchronous
   3 Normal Rez  - Non-Synchonous (Start reqd)
   4 Not Initialized - Read data from a file
*/
LOCAL int f1ConfigData[5][16] = {
  {0x0180, 0x8000, 0x407F, 0x407F, 0x407F, 0x407F,
   0x003F, 0xA400, 0x31D2, 0x31D2, 0x1FBA,
   0x0000, 0x0000, 0x0000, 0x0000, 0x000C,},
  {0x0180, 0x8000, 0x407F, 0x407F, 0x407F, 0x407F,
   0x003F, 0xA400, 0x31D2, 0x31D2, 0x1FBA,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0008,},
  {0x0180, 0x0000, 0x4040, 0x4040, 0x4040, 0x4040,
   0x003F, 0xC880, 0x31D2, 0x31D2, 0x1FBA,
   0x0000, 0x0000, 0x0000, 0x0000, 0x000C,},
  {0x0180, 0x0000, 0x4040, 0x4040, 0x4040, 0x4040,
   0x003F, 0xC880, 0x31D2, 0x31D2, 0x1FBA,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0008,},
  {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
   0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
};


/* Define global variables */
int nf1tdc = 0;			/* Number of TDCs in Crate */
int f1tdcA32Base = 0x08000000;	/* Minimum VME A32 Address for use by TDCs */
int f1tdcA32Offset = 0x08000000;	/* Difference in CPU A32 Base - VME A32 Base */
int f1tdcA24Offset = 0x0;	/* Difference in CPU A24 Base - VME A24 Base */
int f1tdcA16Offset = 0x0;	/* Difference in CPU A16 Base - VME A16 Base */
volatile struct f1tdc_struct *f1p[(F1_MAX_BOARDS + 1)];	/* pointers to TDC memory map */
volatile unsigned int *f1pd[(F1_MAX_BOARDS + 1)];	/* pointers to TDC FIFO memory */
volatile unsigned int *f1pmb;	/* pointer to Multblock window */
volatile struct f1SDC_struct *f1SDCp;	/* pointer to Signal Distribution Card */
int f1ID[F1_MAX_BOARDS];	/* array of slot numbers for TDCs */
int f1Rev[(F1_MAX_BOARDS + 1)];	/* Board Revision Info for each module */
int f1MaxSlot = 0;		/* Highest Slot hold an F1 */
int f1MinSlot = 0;		/* Lowest Slot holding an F1 */
int f1UseSDC = 0;		/* Flag=1 means F1 system controlled by SDC */
int f1UseBDC = 0;		/* Flag=1 means F1 system controlled by BDC */
int f1tdcIntCount = 0;		/* Count of interrupts from TDC */
unsigned int f1LetraMode = 0;	/* Mask of Letra Enabled TDCs. Rising edge only default */

/*******************************************************************************
 *
 * f1Init - Initialize JLAB F1 TDC Library.
 *
 *    if iFlag > 0xf then It is assumed that a JLAB signal distribution
 *                   card is available and that iFlag contains the address
 *               0xXXX0 Address bits
 *               0x000X Resolution flag - see below
 *
 *    iFlag:  Flag to determine intial configuration load
 *            0  - high resolution   - Trigger Matching mode
 *            1  - high resolution   - Start-Stop Synchronous
 *            2  - normal resolution - Trigger matching mode
 *            3  - normal resolution - Start-Stop Synchronous
 *            4-7 - reserved
 *
 *            8-F Same modes as above but Boards are initialized
 *                to use the Backplane Distribution Card (Front Panel disabled).
 *
 *
 * RETURNS: OK, or ERROR if the address is invalid or a board is not present.
 */

STATUS
f1Init(UINT32 addr, UINT32 addr_inc, int ntdc, int iFlag)
{
  int ii, res, rdata, errFlag = 0;
  int boardID = 0;
  int maxSlot = 1;
  int minSlot = 21;
  unsigned int laddr, a32addr, a16addr;
  volatile struct f1tdc_struct *f1;


  /* Check for valid address */
  if (addr == 0)
    {
      printf
	("f1Init: ERROR: Must specify a Bus (VME-based A24) address for TDC 0\n");
      return (ERROR);
    }
  else if (addr > 0x00ffffff)
    {				/* A24 Addressing */
      printf("f1Init: ERROR: A32 Addressing not allowed for the F1 TDC\n");
      return (ERROR);
    }
  else
    {				/* A24 Addressing */
      if ((addr_inc == 0) || (ntdc == 0))
	ntdc = 1;		/* assume only one TDC to initialize */

      /* get the TDC address */
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x39, (char *) addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x39,0x%x,&laddr) \n",
		 addr);
	  return (ERROR);
	}
#else
      res = vmeBusToLocalAdrs(0x39, (char *) addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in vmeBusToLocalAdrs(0x39,0x%x,&laddr) \n",
		 addr);
	  return (ERROR);
	}
#endif
      f1tdcA24Offset = laddr - addr;
    }

  /* Init Some Global variables */
  f1UseBDC = 0;
  f1UseSDC = 0;
  nf1tdc = 0;

  for (ii = 0; ii < ntdc; ii++)
    {
      f1 = (struct f1tdc_struct *) (laddr + ii * addr_inc);
      /* Check if Board exists at that address */
#ifdef VXWORKS
      res = vxMemProbe((char *) &(f1->version), VX_READ, 4, (char *) &rdata);
#else
      res = vmeMemProbe((char *) &(f1->version), 4, (char *) &rdata);
#endif
      if (res < 0)
	{
#ifdef VXWORKS
	  printf("f1Init: ERROR: No addressable board at addr=0x%x\n",
		 (UINT32) f1);
#else
	  printf
	    ("f1Init: ERROR: No addressable board at VME (USER) addr=0x%x (0x%x)\n",
	     (UINT32) addr + ii * addr_inc, (UINT32) f1);
#endif
	  errFlag = 1;
	  break;
	}
      else
	{
	  /* Check that it is an F1 board */
	  if ((rdata & F1_BOARD_MASK) != F1_BOARD_ID)
	    {
	      printf(" ERROR: Invalid Board ID: 0x%x\n", rdata);
	      return (ERROR);
	    }
	  /* Check if this is board has a valid slot number */
	  boardID = ((vmeRead32(&(f1->intr))) & F1_SLOT_ID_MASK) >> 16;
	  if ((boardID <= 0) || (boardID > 21))
	    {
	      printf(" ERROR: Board Slot ID is not in range: %d\n", boardID);
	      return (ERROR);
	    }
	  f1p[boardID] = (struct f1tdc_struct *) (laddr + ii * addr_inc);
	  f1Rev[boardID] = rdata & F1_VERSION_MASK;
	}
      f1ID[nf1tdc] = boardID;
      if (boardID >= maxSlot)
	maxSlot = boardID;
      if (boardID <= minSlot)
	minSlot = boardID;

      nf1tdc++;
#ifdef VXWORKS
      printf("Initialized TDC %d  Slot # %d at address 0x%08x \n",
	     ii, f1ID[ii], (UINT32) f1p[(f1ID[ii])]);
#else
      printf
	("Initialized TDC %d  Slot # %d at VME (USER) address 0x%x (0x%x) \n",
	 ii, f1ID[ii], (UINT32) f1p[(f1ID[ii])] - f1tdcA24Offset,
	 (UINT32) f1p[(f1ID[ii])]);
#endif
    }

  /* Check if we are using a JLAB Signal Distribution Card (SDC)
     or if we are using a Backplane Distribution Card (BDC)
     NOTE the SDC board only supports 5 F1 TDCs - so if there are
     more than 5 TDCs in the crate they can only be controlled by daisychaining
     multiple SDCs together - or by using a Backplane distribution board (BDC) */

  a16addr = iFlag & F1_SDC_MASK;
  if (a16addr)
    {
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x29, (char *) a16addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x29,0x%x,&laddr) \n",
		 a16addr);
	  return (ERROR);
	}

      res = vxMemProbe((char *) laddr, VX_READ, 2, (char *) &rdata);
#else
      res = vmeBusToLocalAdrs(0x29, (char *) a16addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in vmeBusToLocalAdrs(0x29,0x%x,&laddr) \n",
		 a16addr);
	  return (ERROR);
	}

      res = vmeMemProbe((char *) laddr, 2, (char *) &rdata);
#endif
      if (res < 0)
	{
	  printf("f1Init: ERROR: No addressable SDC board at addr=0x%x\n",
		 (UINT32) laddr);
	}
      else
	{
	  f1tdcA16Offset = laddr - a16addr;
	  f1SDCp = (struct f1SDC_struct *) laddr;
	  vmeWrite16(&(f1SDCp->ctrl), F1_SDC_RESET);	/* Reset the Module */

	  if (nf1tdc > 5)
	    {
	      printf
		("WARN: A Single JLAB F1 Signal Distribution Module only supports 5 TDCs\n");
	      printf
		("WARN: You must use multiple SDCs to support more TDCs - this must be configured in hardware\n");
	    }
#ifdef VXWORKS
	  printf("Using JLAB F1 Signal Distribution Module at address 0x%x\n",
		 (UINT32) f1SDCp);
#else
	  printf
	    ("Using JLAB F1 Signal Distribution Module at VME (USER) address 0x%x (0x%x)\n",
	     (UINT32) f1SDCp - f1tdcA16Offset, (UINT32) f1SDCp);
#endif
	  f1UseSDC = 1;
	}
    }
  if ((iFlag & F1_BDC_MASK) && (f1UseSDC == 0))
    {				/* Assume BDC exists */
      f1UseBDC = 1;
      printf
	("f1Init: JLAB F1 Backplane Distribution Card is Assumed in Use\n");
      printf("f1Init: Front Panel Inputs will be disabled. \n");
    }
  else
    {
      f1UseBDC = 0;
    }

  /* Reset all TDCs */
  for (ii = 0; ii < nf1tdc; ii++)
    {
      vmeWrite32(&(f1p[f1ID[ii]]->csr), F1_CSR_HARD_RESET);
    }
  taskDelay(30);
  /* Initialize Interrupt variables */
  f1tdcIntID = -1;
  f1tdcIntRunning = FALSE;
  f1tdcIntLevel = F1_VME_INT_LEVEL;
  f1tdcIntVec = F1_VME_INT_VEC;
  f1tdcIntRoutine = NULL;
  f1tdcIntArg = 0;

  /* Calculate the A32 Offset for use in Block Transfers */
#ifdef VXWORKS
  res = sysBusToLocalAdrs(0x09, (char *) f1tdcA32Base, (char **) &laddr);
  if (res != 0)
    {
      printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",
	     f1tdcA32Base);
      return (ERROR);
    }
  else
    {
      f1tdcA32Offset = laddr - f1tdcA32Base;
    }
#else
  res = vmeBusToLocalAdrs(0x09, (char *) f1tdcA32Base, (char **) &laddr);
  if (res != 0)
    {
      printf("f1Init: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n",
	     f1tdcA32Base);
      return (ERROR);
    }
  else
    {
      f1tdcA32Offset = laddr - f1tdcA32Base;
    }
#endif

  /* Write configuration registers with default Config */
  for (ii = 0; ii < nf1tdc; ii++)
    {
      f1ConfigWrite(f1ID[ii], (int *) &f1ConfigData[(iFlag & 0x7)],
		    F1_ALL_CHIPS);

      /* Program an A32 access address for this TDC's FIFO */
      a32addr = f1tdcA32Base + ii * F1_MAX_A32_MEM;
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x09, (char *) a32addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",
		 a32addr);
	  return (ERROR);
	}
#else
      res = vmeBusToLocalAdrs(0x09, (char *) a32addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in vmeBusToLocalAdrs(0x09,0x%x,&laddr) \n",
		 a32addr);
	  return (ERROR);
	}
#endif
      f1pd[f1ID[ii]] = (unsigned int *) (laddr);	/* Set a pointer to the FIFO */
      vmeWrite32(&(f1p[f1ID[ii]]->adr32), (a32addr >> 16) + 1);	/* Write the register and enable */

      if ((f1UseSDC <= 0) && (f1UseBDC <= 0))
	f1EnableClk(f1ID[ii], 0);
      f1ClearStatus(f1ID[ii], F1_ALL_CHIPS);
      f1SetBlockLevel(f1ID[ii], 1);	/* default Block Level to 1 */
    }

  /* If there are more than 1 TDC in the crate then setup the Muliblock Address
     window. This must be the same on each board in the crate */
  if (nf1tdc > 1)
    {
      a32addr = f1tdcA32Base + (nf1tdc + 1) * F1_MAX_A32_MEM;	/* set MB base above individual board base */
#ifdef VXWORKS
      res = sysBusToLocalAdrs(0x09, (char *) a32addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",
		 a32addr);
	  return (ERROR);
	}
#else
      res = vmeBusToLocalAdrs(0x09, (char *) a32addr, (char **) &laddr);
      if (res != 0)
	{
	  printf("f1Init: ERROR in sysBusToLocalAdrs(0x09,0x%x,&laddr) \n",
		 a32addr);
	  return (ERROR);
	}
#endif
      f1pmb = (unsigned int *) (laddr);	/* Set a pointer to the FIFO */
      for (ii = 0; ii < nf1tdc; ii++)
	{
	  /* Write the register and enable */
	  vmeWrite32(&(f1p[f1ID[ii]]->adr_mb),
		     (a32addr + F1_MAX_A32MB_SIZE) + (a32addr >> 16) + 1);
	}

      /* Set First Board and Last Board */
      f1MaxSlot = maxSlot;
      f1MinSlot = minSlot;
      vmeWrite32(&(f1p[minSlot]->ctrl),
		 vmeRead32(&(f1p[minSlot]->ctrl)) | F1_FIRST_BOARD);
      vmeWrite32(&(f1p[maxSlot]->ctrl),
		 vmeRead32(&(f1p[maxSlot]->ctrl)) | F1_LAST_BOARD);

    }

  if (f1UseSDC)
    {				/* If we have a Signal Distribution Board. Set up to Use That */
      f1SDC_Config();
    }
  else if (f1UseBDC)
    {				/* or Setup to use BDC */
      f1BDC_Config();
    }

  if (errFlag > 0)
    {
      printf("f1Init: ERROR: Unable to initialize all TDC Modules\n");
      if (nf1tdc > 0)
	printf("f1Init: %d TDC(s) successfully initialized\n", nf1tdc);
      return (ERROR);
    }
  else
    {
      return (OK);
    }


}

int
f1CheckAddresses()
{
  struct f1tdc_struct test;
  unsigned int offset=0, expected=0, base=0;

  base = (unsigned int) &test;

  printf("%s:\n\t ---------- Checking f1TDC address space ---------- \n",__FUNCTION__);

  offset = ((unsigned int) &test.ctrl) - base;
  expected = 0x4;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "ctrl",
	   expected,offset);

  offset = ((unsigned int) &test.version) - base;
  expected = 0x2C;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "version",
	   expected,offset);

  offset = ((unsigned int) &test.config[0][0]) - base;
  expected = 0x800;
  if(offset != expected)
    printf("%s: ERROR %s not at offset = 0x%x (@ 0x%x)\n",
	   __FUNCTION__,
	   "config[0][0]",
	   expected,offset);

  return OK;
}

/**
 *  @ingroup Config
 *  @brief Convert an index into a slot number, where the index is
 *          the element of an array of F1TDCs in the order in which they were
 *          initialized.
 *
 * @param i
 *  - Initialization number
 * @return Slot number if Successfull, otherwise ERROR.
 *
 */
int
f1Slot(unsigned int i)
{
  if(i>=nf1tdc)
    {
      printf("%s: ERROR: Index (%d) >= f1TDCs initialized (%d).\n",
	     __FUNCTION__,i,nf1tdc);
      return ERROR;
    }

  return f1ID[i];
}

int
f1ConfigWrite(int id, int *config_data, int chipMask)
{
  int ii, jj, reg, clk = 0, ext = 0;
  int order[16] = { 15, 10, 0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 13, 14, 7 };
  unsigned int rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1SetConfig: ERROR : TDC in slot %d is not initialized \n", id);
      return (ERROR);
    }

  if (chipMask == 0)
    {				/* Assume all chips programmed the same */
      chipMask = F1_ALL_CHIPS;
    }

  /* Disable FIFOs and Edges */
  f1DisableData(id);

  /* Disable the clock if it is enabled */
  F1LOCK;
  clk = vmeRead32(&(f1p[id]->ctrl)) & F1_REF_CLK_PCB;
  ext = vmeRead32(&(f1p[id]->ctrl)) & F1_REF_CLK_SEL;
  if (clk)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) & ~F1_REF_CLK_PCB);
  if (ext)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) & ~F1_REF_CLK_SEL);

  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if ((chipMask) & (1 << ii))
	{
	  for (jj = 0; jj < 16; jj++)	/* write cfg data */
	    {
	      reg = order[jj];	/* program reisters in the correct order */
	      if ((reg >= 2) && (reg <= 5))
		vmeWrite32(&(f1p[id]->config[ii][reg]),
			   (config_data[reg] & F1_OFFSET_MASK));
	      else
		vmeWrite32(&(f1p[id]->config[ii][reg]), config_data[reg]);
	      /* read it back and check */
	      rval = vmeRead32(&(f1p[id]->config[ii][reg])) & 0xffff;
	    }
	}
    }

  if (clk)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) | F1_REF_CLK_PCB);
  if (ext)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) | F1_REF_CLK_SEL);
  /* clear all latched status bits */
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if (chipMask & (1 << ii))
	vmeWrite16(&(f1p[id]->stat[ii]), F1_CHIP_CLEAR_STATUS);
    }
  F1UNLOCK;

  return (OK);

}

int
f1SetConfig(int id, int iflag, int chipMask)
{

  /* Set from four default configuations
     0 Hi Rez      - Synchronous
     1 Hi Rez      - Non-Synchronous
     2 Normal Rez  - Synchronous
     3 Normal Rez  - Non-Synchonous
     4 User specified (from file)
   */

  int ii, jj, reg, clk = 0, ext = 0;
  int order[16] = { 15, 10, 0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 13, 14, 7 };
  unsigned int rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1SetConfig: ERROR : TDC in slot %d is not initialized \n", id);
      return (ERROR);
    }

  if ((iflag < 0) || (iflag > 4))
    {
      printf
	("f1SetConfig: ERROR: Invalid config number. Choose from 0-3 where\n");
      printf
	("             0 -> Hi Resolution (32 chan)     - Trigger Matching\n");
      printf
	("             1 -> Hi Resolution (32 chan)     - Non-Sync (Start reqd)\n");
      printf
	("             2 -> Normal Resolution (64 chan) - Trigger Matching\n");
      printf
	("             3 -> Normal Resolution (64 chan) - Non-Sync (Start reqd)\n");
      printf
	("             4 -> User specified from a file (call f1ConfigReadFile() first)\n");
      return (ERROR);
    }

  if (iflag == 4)
    {				/* check if there is valid config data there */
      if (f1ConfigData[iflag][0] == 0)
	{
	  printf("f1SetConfig:ERROR: Invalid Config data\n");
	  return (ERROR);
	}
    }

  if (chipMask == 0)
    {				/* Assume all chips programmed the same */
      chipMask = F1_ALL_CHIPS;
    }

  /* Disable FIFOs and Edges */
  f1DisableData(id);

  /* Disable the clock if it is enabled */
  F1LOCK;
  clk = vmeRead32(&(f1p[id]->ctrl)) & F1_REF_CLK_PCB;
  ext = vmeRead32(&(f1p[id]->ctrl)) & F1_REF_CLK_SEL;
  if (clk)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) & ~F1_REF_CLK_PCB);
  if (ext)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) & ~F1_REF_CLK_SEL);

  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if ((chipMask) & (1 << ii))
	{
	  for (jj = 0; jj < 16; jj++)	/* write cfg data */
	    {
	      reg = order[jj];	/* program registers in the correct order */
	      if ((reg >= 2) && (reg <= 5))
		vmeWrite32(&(f1p[id]->config[ii][reg]),
			   ((f1ConfigData[iflag][reg]) & F1_OFFSET_MASK));
	      else
		vmeWrite32(&(f1p[id]->config[ii][reg]),
			   f1ConfigData[iflag][reg]);
	      /* read it back and check */
	      rval = vmeRead32(&(f1p[id]->config[ii][reg])) & 0xffff;
	    }
	}
    }
  if (clk)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) | F1_REF_CLK_PCB);
  if (ext)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) | F1_REF_CLK_SEL);
  F1UNLOCK;


  return (OK);

}


int
f1ConfigRead(int id, unsigned int *config_data, int chipID)
{
  int jj;			//, reg;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1ConfigRead: ERROR : TDC in slot %d is not initialized \n",
	     id);
      return (ERROR);
    }

  if ((chipID < 0) || (chipID >= F1_MAX_TDC_CHIPS))
    {
      printf("f1ConfigRead: ERROR : Invalid Chip ID %d (range 0-7)\n",
	     chipID);
      return (ERROR);
    }

  F1LOCK;
  for (jj = 0; jj < 16; jj++)
    {				/* READ cfg data */
      config_data[jj] = vmeRead32(&(f1p[id]->config[chipID][jj]));
    }
  F1UNLOCK;

  return (OK);

}

int
f1ConfigReadFile(char *filename)
{
  FILE *fd_1;
  unsigned int value, config[16];
  int ii, jj;

  if (filename == NULL)
    {
      printf("f1ConfigReadFile: ERROR: No Config file specified.\n");
      return (ERROR);
    }

  fd_1 = fopen(filename, "r");
  if (fd_1 > 0)
    {
      printf("Reading Data from file: %s\n", filename);
      jj = 4;			/* location for file data */
      for (ii = 0; ii < 16; ii++)
	{
	  fscanf(fd_1, "%x", &value);
	  f1ConfigData[jj][ii] = 0xFFFF & value;
	  config[ii] = f1ConfigData[jj][ii];
	  printf("ALL Chips: Reg %2d  =  %04x\n", ii, config[ii]);
	}

      fclose(fd_1);

      return (OK);
    }
  else
    {
      printf("f1ConfigReadFile: ERROR opening file %s\n", filename);
      return (ERROR);
    }
}

static int
f1ChipsHaveSameConfig(int id)
{
  int ichip, ireg;
  unsigned short chipReg0;

  F1LOCK;
  for(ireg = 0; ireg < 16; ireg++)
    {
      chipReg0 = vmeRead32(&f1p[id]->config[0][ireg]) & 0xFFFF;

      for(ichip = 1; ichip < F1_MAX_TDC_CHIPS; ichip++)
	{
	  if(chipReg0 != (vmeRead32(&f1p[id]->config[ichip][ireg]) & 0xFFFF))
	    {
	      F1UNLOCK;
	      return 0;
	    }
	}
    }
  F1UNLOCK;

  return 1;
}

typedef struct chipinfo_struct
{
  int chip;
  float clk_period;
  int rez;
  float bin_size;
  float full_range;
  float window;
  float latency;
  unsigned int rollover_count;
} chipInfo;

static void
f1ConfigDecode(int id, chipInfo *ci, int pflag)
{
  int ireg, ii;
  unsigned short chipReg[16];
  float factor, tframe;
  int sync;
  unsigned int refcnt, exponent, refclkdiv, hsdiv, trigwin, triglat;

  F1LOCK;
  for (ireg = 0; ireg < 16; ireg++)
    {
      chipReg[ireg] = vmeRead32(&f1p[id]->config[ci->chip][ireg]) & 0xffff;
    }
  F1UNLOCK;

  ci->clk_period = 25.0;

  if (pflag)
    {
      printf("\n ---------------- Chip %d ----------------", ci->chip);

      for (ireg = 0; ireg < 16; ireg++)
	{
	  if ((ireg % 8) == 0)
	    printf("\n");
	  printf("0x%04x  ", chipReg[ireg]);
	}
      printf("\n");
    }

  if (chipReg[1] & 0x8000)
    {
      if (pflag)
	printf("High Resolution mode\n");
      factor = 0.5;
      ci->rez = 1;
    }
  else
    {
      if (pflag)
	printf("Normal Resolution mode\n");
      factor = 1.;
      ci->rez = 0;
    }

  if (chipReg[15] & 0x4)
    {
      sync = 1;
      if (pflag)
	printf("Synchronous mode\n");
    }
  else
    {
      sync = 0;
      if (pflag)
	printf("Non-synchronous mode (Start required)\n");
    }

  refcnt = (chipReg[7] >> 6) & 0x1FF;
  tframe = (float) (ci->clk_period * (refcnt + 2));
  if (pflag)
    printf("refcnt = %d   tframe (ns) = %.1f\n", refcnt, tframe);

  exponent = (chipReg[10] >> 8) & 0x7;
  refclkdiv = 1;
  for (ii = 0; ii < exponent; ii++)
    refclkdiv = 2 * refclkdiv;
  hsdiv = chipReg[10] & 0xFF;
  ci->bin_size =
    factor * (ci->clk_period / 152.) * ((float) refclkdiv) / ((float) hsdiv);
  ci->full_range = 65536 * ci->bin_size;
  if (pflag)
    printf
      ("refclkdiv = %d   hsdiv = %d   bin_size (ns) = %.4f   full_range (ns) = %.1f\n",
       refclkdiv, hsdiv, ci->bin_size, ci->full_range);

  trigwin = chipReg[8] & 0xffff;
  triglat = chipReg[9] & 0xffff;
  ci->window = ((float) trigwin) * ci->bin_size / factor;
  ci->latency = ((float) triglat) * ci->bin_size / factor;
  if (pflag)
    printf
      ("trigwin = %d   triglat = %d   window (ns) = %.1f   latency (ns) = %.1f\n",
       trigwin, triglat, ci->window, ci->latency);

  if (sync)
    {
      ci->rollover_count = (unsigned int) (tframe / ci->bin_size);
      if (pflag)
	printf("Rollover count = %d\n", ci->rollover_count);
    }
  else
    {
      ci->rollover_count = 65536;
      if (pflag)
	printf("Rollover count = N/A (Full Range - 65536)\n");

    }

}

void
f1ConfigShow(int id, int chipMask)
{
  chipInfo ci;
  int ichip;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1ConfigShow: ERROR : TDC in slot %d is not initialized \n",
	     id);
      return;
    }

  if (chipMask == 0)		/* Print out config info for all chips */
    chipMask = F1_ALL_CHIPS;

  for (ichip = 0; ichip < F1_MAX_TDC_CHIPS; ichip++)
    {
      if ((chipMask) & (1 << ichip))
	{
	  ci.chip = ichip;
	  f1ConfigDecode(id, &ci, 1);
	}
    }

}


void
f1Status(int id, int sflag)
{
  int jj;
  unsigned int a32Base, ambMin, ambMax;
  unsigned int csr, ctrl, count, level, intr, addr32, addrMB;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int cmask = 0;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1Status: ERROR : TDC in slot %d is not initialized \n", id);
      return;
    }

  F1LOCK;
  csr = vmeRead32(&(f1p[id]->csr)) & F1_CSR_MASK;
  ctrl = vmeRead32(&(f1p[id]->ctrl)) & F1_CONTROL_MASK;
  count = vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK;
  level = vmeRead32(&(f1p[id]->ev_level)) & F1_EVENT_LEVEL_MASK;
  intr = vmeRead32(&(f1p[id]->intr));
  addr32 = vmeRead32(&(f1p[id]->adr32));
  a32Base = (addr32 & F1_A32_ADDR_MASK) << 16;
  addrMB = vmeRead32(&(f1p[id]->adr_mb));
  ambMin = (addrMB & F1_AMB_MIN_MASK) << 16;
  ambMax = (addrMB & F1_AMB_MAX_MASK);

#ifdef VXWORKS
  printf("\nSTATUS for TDC in slot %d at base address 0x%x \n", id,
	 (UINT32) f1p[id]);
#else
  printf
    ("\nSTATUS for TDC in slot %d at VME (USER) base address 0x%x (0x%x) \n",
     id, (UINT32) f1p[id] - f1tdcA24Offset, (UINT32) f1p[id]);
#endif
  printf("---------------------------------------------------------------------- \n");
  printf(" Firmware Version = 0x%02x\n",
	 vmeRead32(&(f1p[id]->version)) & F1_VERSION_MASK);

  F1UNLOCK;

  if (sflag != 1)
    {
      if (addrMB & F1_AMB_ENABLE)
	{
	  printf(" Alternate VME Addressing: Multiblock Enabled\n");
	  if (addr32 & F1_A32_ENABLE)
	    printf("   A32 Enabled at VME (Local) base 0x%08x (0x%08x)\n",
		   a32Base, (UINT32) f1pd[id]);
	  else
	    printf("   A32 Disabled\n");

	  printf("   Muliblock VME Address Range 0x%08x - 0x%08x\n", ambMin,
		 ambMax);
	}
      else
	{
	  printf(" Alternate VME Addressing: Multiblock Disabled\n");
	  if (addr32 & F1_A32_ENABLE)
	    printf("   A32 Enabled at VME (Local) base 0x%08x (0x%08x)\n",
		   a32Base, (UINT32) f1pd[id]);
	  else
	    printf("   A32 Disabled\n");
	}

      if (ctrl & F1_INT_ENABLE_MASK)
	{
	  printf("\n  Interrupts ENABLED: ");
	  if (ctrl & F1_EVENT_LEVEL_INT)
	    printf("EventLevel(%d)", level);
	  if (ctrl & F1_ERROR_INT)
	    printf("Errors ");
	  printf("\n");
	  printf("  VME INT Vector = 0x%x  Level = %d\n",
		 (intr & F1_INT_VEC_MASK), ((intr & F1_INT_LEVEL_MASK) >> 8));
	}

      printf("\n Configuration: \n");
      if (f1UseSDC)
	printf("  USING Signal Distribution card at 0x%x\n", (UINT32) f1SDCp);
      if (f1UseBDC)
	printf("  USING Backplane Distribution Card\n");
      if (ctrl & F1_FB_SEL)
	printf("   Control Inputs from Backplane\n");
      else
	printf("   Control Inputs from Front Panel\n");

      if (ctrl & F1_REF_CLK_PCB)
	printf("   Internal Clock ON\n");
      else
	printf("   Internal Clock OFF\n");

      if (ctrl & F1_REF_CLK_SEL)
	printf("   Use EXTERNAL Clock\n");
      else
	printf("   Use INTERNAL Clock\n");

      if (ctrl & F1_ENABLE_BERR)
	printf("   Bus Error ENABLED\n");
      else
	printf("   Bus Error DISABLED\n");

      if (ctrl & F1_ENABLE_MULTIBLOCK)
	{
	  if (ctrl & F1_FIRST_BOARD)
	    printf("   MultiBlock transfer ENABLED (First Board)\n");
	  else if (ctrl & F1_LAST_BOARD)
	    printf("   MultiBlock transfer ENABLED (Last Board)\n");
	  else
	    printf("   MultiBlock transfer ENABLED\n");
	}
      else
	{
	  printf("   MultiBlock transfer DISABLED\n");
	}

      if (ctrl & F1_ENABLE_SOFT_TRIG)
	printf("   Software Triggers ENABLED\n");


      printf("\n");
      if (csr & F1_CSR_ERROR)
	printf("  CSR     Register = 0x%08x - TDC Chip(s) Error Condition\n", csr);
      else
	printf("  CSR     Register = 0x%08x \n", csr);
      printf("  Control Register = 0x%08x \n", ctrl);
      if ((csr & F1_MODULE_EMPTY_MASK) == F1_MODULE_EMPTY_MASK)
	printf("  Events in FIFO   = %d  (Block level = %d)\n", count, level);
      else
	printf
	  ("  Events in FIFO   = %d  (Block level = %d) - Data Available\n",
	   count, level);

      /* Print out Chip Status (with error description, if errors present */
      f1ChipStatus(id, (csr & F1_CSR_ERROR) ? 1 : 0);
    }
  else
    {
      /* Print minimal Error status of Chips on the TDC */
      F1LOCK;
      for (jj = 0; jj < F1_MAX_TDC_CHIPS; jj++)
	{
	  chipstat[jj] = vmeRead16(&(f1p[id]->stat[jj]));
	  if (((chipstat[jj] & F1_CHIP_RES_LOCKED) == 0)
	      || (chipstat[jj] & F1_CHIP_ERROR_COND))
	    cmask |= (1 << jj);
	}
      F1UNLOCK;
      if (cmask)
	f1ChipStatus(id, 1);
      else
	printf(" Chip Status: ALL Chips - OK \n");
    }

  if (sflag == 2)
    {				/* Print out Chip configuration */
      f1ConfigShow(id, F1_ALL_CHIPS);
    }

}

void
f1GStatus(int sFlag)
{
  int itdc, id, ichip, ireg;
  struct f1tdc_struct *st;
  unsigned int a24addr[F1_MAX_BOARDS+1];
  chipInfo ci[F1_MAX_BOARDS+1][8];
  int chipCommonConfig[F1_MAX_BOARDS+1];
  int nchips;
  unsigned int errmask;

  st = (struct f1tdc_struct *) malloc((F1_MAX_BOARDS + 1) *
				      sizeof(struct f1tdc_struct));

  if(!st)
    {
      printf("%s: ERROR: Unable to allocate memory for f1TDC register map\n",
	     __func__);
      return;
    }

  F1LOCK;
  for (itdc=0;itdc<nf1tdc;itdc++)
    {
      id = f1Slot(itdc);
      a24addr[id]    = (unsigned int)f1p[id] - f1tdcA24Offset;

      st[id].csr      = vmeRead32(&f1p[id]->csr);
      st[id].ctrl     = vmeRead32(&f1p[id]->ctrl);
      st[id].ev_count = vmeRead32(&f1p[id]->ev_count);
      st[id].ev_level = vmeRead32(&f1p[id]->ev_level);
      st[id].intr     = vmeRead32(&f1p[id]->intr);
      st[id].adr32    = vmeRead32(&f1p[id]->adr32);
      st[id].adr_mb   = vmeRead32(&f1p[id]->adr_mb);

      for(ichip = 0; ichip < F1_MAX_TDC_CHIPS; ichip++)
	{
	  st[id].stat[ichip] = vmeRead16(&f1p[id]->stat[ichip]);

	  for(ireg = 0; ireg < 16; ireg++)
	    {
	      st[id].config[ichip][ireg] = vmeRead32(&f1p[id]->config[ichip][ireg]);
	    }
	}

      st[id].version = vmeRead32(&f1p[id]->version);
    }
  F1UNLOCK;
  for (itdc=0;itdc<nf1tdc;itdc++)
    {
      id = f1Slot(itdc);
      if(f1ChipsHaveSameConfig(id))
	{
	  chipCommonConfig[id]=1;
	  ci[id][0].chip = 0;
	  f1ConfigDecode(id,&ci[id][0],0);
	}
      else
	{
	  chipCommonConfig[id]=0;
	  for(ichip=0; ichip<F1_MAX_TDC_CHIPS; ichip++)
	    {
	      ci[id][ichip].chip = ichip;
	      f1ConfigDecode(id,&ci[id][ichip],0);
	    }
	}
    }

  printf("\n");
  printf("                       f1TDC Module Configuration Summary\n\n");
  printf("     Firmware  ..................Addresses.................\n");
  printf("Slot   Rev        A24         A32      A32 Multiblock Range\n");
  printf("--------------------------------------------------------------------------------\n");

  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);

      printf(" %2d   ",id);

      printf("0x%02x     ",st[id].version&F1_VERSION_MASK);

      printf("0x%06x   ",
	     a24addr[id]);

      if(st[id].adr32 &F1_A32_ENABLE)
	{
	  printf("0x%08x  ",
		 (st[id].adr32&F1_A32_ADDR_MASK)<<16);
	}
      else
	{
	  printf("  Disabled  ");
	}

      if(st[id].adr_mb & F1_AMB_ENABLE)
	{
	  printf("0x%08x-0x%08x",
		 (st[id].adr_mb&F1_AMB_MIN_MASK)<<16,
		 (st[id].adr_mb&F1_AMB_MAX_MASK));
	}
      else
	{
	  printf("Disabled");
	}

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("      Block  .Signal Sources..                        ..TDC CHIP..\n");
  printf("Slot  Level  Clk   Trig   Sync     MBlk  Token  BERR  Enabled Mask\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);

      printf(" %2d   ",id);

      printf("%5d ",st[id].ev_level & F1_EVENT_LEVEL_MASK);

      printf("%s  ",
	     ((st[id].ctrl & F1_REF_CLK_SEL)==0) ? " INT " :
	     (st[id].ctrl & F1_FB_SEL) ? " BDC " :
	     "  FP ");

      printf("%s  ",
	     (st[id].ctrl & F1_FB_SEL) ? " BDC " :
	     "  FP ");

      printf("%s     ",
	     (st[id].ctrl & F1_FB_SEL) ? " BDC " :
	     "  FP ");

      printf("%s  ",
	     (st[id].ctrl & F1_ENABLE_MULTIBLOCK) ? "YES":" NO");

      printf("%s   ",
	     st[id].ctrl & (F1_FIRST_BOARD) ? "FIRST":
	     st[id].ctrl & (F1_LAST_BOARD) ? " LAST":
	     "     ");

      printf("%s      ",
	     st[id].ctrl & F1_ENABLE_BERR ? "YES" : " NO");

      printf("0x%02X",
	       (st[id].ctrl & F1_ENABLE_DATA_TDCMASK) >> 16);

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("                        f1TDC Chip Configuration\n\n");
  printf("                                       Bin      Full     Rollover\n");
  printf("Slot  Chip    Rez     PL       PTW     Size     Range    Count\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);

      printf(" %2d    ",id);

      if(chipCommonConfig[id]==1)
	nchips=1;
      else
	nchips = F1_MAX_TDC_CHIPS;

      for(ichip=0; ichip < nchips; ichip++)
	{
	  if(nchips==1)
	    printf("ALL    ");
	  else
	    {
	      if(ichip!=0)
		printf("       ");

	      printf("%d      ",ci[id][ichip].chip);
	    }

	  printf("%s   ",(ci[id][ichip].rez==1)? "High":"Norm");

	  printf("%5.1f   ",ci[id][ichip].window);

	  printf("%5.1f   ",ci[id][ichip].latency);

	  printf("%2.4f   ",ci[id][ichip].bin_size);

	  printf("%5.1f   ",ci[id][ichip].full_range);

	  printf("%d",ci[id][ichip].rollover_count);

	  printf("\n");
	}

    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("      Block  Events In     ...........TDC Chip Error Status.........\n");
  printf("Slot  Ready     Fifo       Config   Other   Chip #    Res NOT Locked\n");
  printf("--------------------------------------------------------------------------------\n");
  for(itdc=0; itdc<nf1tdc; itdc++)
    {
      id = f1Slot(itdc);

      printf(" %2d    ",id);

      printf("%s     ",
	     st[id].csr & F1_CSR_EVENT_LEVEL_FLAG ? "YES" : " NO");

      printf("%5d       ",
	     st[id].ev_count&F1_EVENT_COUNT_MASK);

      printf("%s    ",
	     st[id].csr & F1_CSR_CONFIG_ERROR ? "ERROR" : "  OK " );

      errmask=F1_CSR_CONFIG_ERROR;
      printf("%s ",
	     st[id].csr & errmask ? "ERROR" : "  OK " );

      errmask=F1_CSR_ERROR_MASK;
      if(st[id].csr & errmask)
	{
	  printf(" ");
	  for(ichip=0; ichip < F1_MAX_TDC_CHIPS; ichip++)
	    {
	      if(((st[id].csr & errmask)>>8) & (1<<ichip))
		printf("%d",ichip);
	      else
		printf(" ");
	    }
	  printf(" ");
	}
      else
	printf("                ");

      for(ichip=0; ichip < F1_MAX_TDC_CHIPS; ichip++)
	{
	  if((st[id].stat[ichip] & F1_CHIP_RES_LOCKED)==0)
	    printf("%d",ichip);
	  else
	    printf(" ");
	}

      printf("\n");
    }
  printf("--------------------------------------------------------------------------------\n");

  printf("\n");
  printf("\n");

}

void
f1ChipStatus(int id, int pflag)
{

  int ii, reg, lock, latch, stat;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1ChipStatus: ERROR : TDC in slot %d is not initialized \n", id);
      return;
    }

  F1LOCK;
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      chipstat[ii] = vmeRead16(&(f1p[id]->stat[ii]));
    }
  F1UNLOCK;

  printf("\n CHIP Status: (slot %d)\n", id);
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      reg = chipstat[ii] & 0xffff;
      lock = reg & 0x1;
      stat = reg & 0x7e;
      latch = (reg & 0x1f00) >> 8;
      if (!(reg & F1_CHIP_INITIALIZED))
	{
	  printf("   CHIP %d  Reg = 0x%04x -  NOT Initialized  \n", ii, reg);
	}
      else
	{
	  if (lock == 0)
	    {
	      printf("   CHIP %d  Reg = 0x%04x ** Resolution NOT Locked **\n",
		     ii, reg);
	    }
	  else if (latch)
	    {
	      printf("   CHIP %d  Reg = 0x%04x ** Check Latched Status **\n",
		     ii, reg);
	      if (pflag)
		{
		  if (latch & F1_CHIP_RES_LOCKED)
		    printf("        Resolution Lock Failed\n");
		  if (latch & F1_CHIP_HITFIFO_OVERFLOW)
		    printf("        Hit FIFO Overflow occurred\n");
		  if (latch & F1_CHIP_TRIGFIFO_OVERFLOW)
		    printf("        Trigger FIFO Overflow occured\n");
		  if (latch & F1_CHIP_OUTFIFO_OVERFLOW)
		    printf("        Output FIFO Overflow occured\n");
		  if (latch & F1_CHIP_EXTFIFO_FULL)
		    printf("        External FIFO Full occured\n");
		}
	    }
	  else
	    {
	      printf("   CHIP %d  Reg = 0x%04x - OK\n", ii, reg);
	      if (pflag)
		{
		  if (stat & F1_CHIP_HITFIFO_OVERFLOW)
		    printf("        Hit FIFO has Overflowed\n");
		  if (stat & F1_CHIP_TRIGFIFO_OVERFLOW)
		    printf("        Trigger has FIFO Overflowed\n");
		  if (stat & F1_CHIP_OUTFIFO_OVERFLOW)
		    printf("        Output has FIFO Overflowed\n");
		  if (stat & F1_CHIP_EXTFIFO_FULL)
		    printf("        External FIFO is Full\n");
		  if (stat & F1_CHIP_EXTFIFO_ALMOST_FULL)
		    printf
		      ("        External FIFO Almost Full (Busy Asserted)\n");
		  if ((stat & F1_CHIP_EXTFIFO_EMPTY) == 0)
		    printf("        External FIFO NOT Empty\n");
		}
	    }
	}

    }
}


/**************************************************************************************
 *
 *  f1ReadEvent - General Event readout routine
 *
 *    id    - Slot number of module to read
 *    data  - local memory address to place data
 *    nwrds - Max number of words to transfer
 *    rflag - Readout Flag
 *              0 - programmed I/O from the specified board
 *              1 - DMA transfer using Universe DMA Engine
 *                    (DMA VME transfer Mode must be setup prior)
 *              2 - Multiblock DMA transfer (Multiblock must be enabled
 *                     and daisychain in place or BDC being used)
 */
int
f1ReadEvent(int id, volatile UINT32 * data, int nwrds, int rflag)
{
  int ii, evnum, trigtime;
  int stat, retVal, xferCount;
  int dCnt, berr = 0;
  int dummy = 0;
  volatile unsigned int *laddr;
  unsigned int status, head, val;
  unsigned int vmeAdr, csr;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1ReadEvent: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return (ERROR);
    }

  if (data == NULL)
    {
      logMsg("f1ReadEvent: ERROR: Invalid Destination address\n", 0, 0, 0, 0,
	     0, 0);
      return (ERROR);
    }

  if (nwrds <= 0)
    nwrds = F1_MAX_TDC_CHANNELS * F1_MAX_HITS_PER_CHANNEL;

  if (rflag >= 1)
    {				/* Block Transfers */

      /*Assume that the DMA programming is already setup. */
      /* Don't Bother checking if there is valid data - that should be done prior
         to calling the read routine */

      /* Check for 8 byte boundary for address - insert dummy word (Slot 30 TDC DATA 0xffff) */
      if ((unsigned long) (data) & 0x7)
	{
#ifdef VXWORKS
	  *data = F1_DUMMY_DATA;
#else
	  *data = LSWAP(F1_DUMMY_DATA);
#endif
	  dummy = 1;
	  laddr = (data + 1);
	}
      else
	{
	  dummy = 0;
	  laddr = data;
	}

      if (rflag == 2)
	{			/* Multiblock Mode */
	  F1LOCK;
	  if ((vmeRead32(&(f1p[id]->ctrl)) & F1_FIRST_BOARD) == 0)
	    {
	      logMsg
		("f1ReadEvent: ERROR: TDC in slot %d is not First Board\n",
		 id, 0, 0, 0, 0, 0);
	      F1UNLOCK;
	      return (ERROR);
	    }
	  F1UNLOCK;
	  vmeAdr = (unsigned int) (f1pmb) - f1tdcA32Offset;
	}
      else
	{
	  vmeAdr = (unsigned int) (f1pd[id]) - f1tdcA32Offset;
	}

      F1LOCK;
#ifdef VXWORKS
      retVal = sysVmeDmaSend((UINT32) laddr, vmeAdr, (nwrds << 2), 0);
#else
      retVal = vmeDmaSend((UINT32) laddr, vmeAdr, (nwrds << 2));
#endif

      if (retVal |= 0)
	{
	  logMsg("f1ReadEvent: ERROR in DMA transfer Initialization 0x%x\n",
		 retVal, 0, 0, 0, 0, 0);
	  F1UNLOCK;
	  return (retVal);
	}
      /* Wait until Done or Error */
#ifdef VXWORKS
      retVal = sysVmeDmaDone(10000, 1);
#else
      retVal = vmeDmaDone();
#endif
      F1UNLOCK;

      if (retVal > 0)
	{
	  /* Check to see that Bus error was generated by TDC */
	  if (rflag == 2)
	    {
	      F1LOCK;
	      csr = vmeRead32(&(f1p[f1MaxSlot]->csr));	/* from Last TDC */
	      F1UNLOCK;
	      stat = (csr) & F1_CSR_BERR_STATUS;	/* from Last TDC */
	    }
	  else
	    {
	      F1LOCK;
	      stat = vmeRead32(&(f1p[id]->csr)) & F1_CSR_BERR_STATUS;	/* from TDC id */
	      F1UNLOCK;
	    }
	  if ((retVal > 0) && (stat))
	    {
#ifdef VXWORKS
	      xferCount = (nwrds - (retVal >> 2) + dummy);	/* Number of Longwords transfered */
#else
	      xferCount = (retVal >> 2) + dummy;	/* Number of Longwords transfered */
#endif
	      return (xferCount);	/* Return number of data words transfered */
	    }
	  else
	    {
#ifdef VXWORKS
	      xferCount = (nwrds - (retVal >> 2) + dummy);	/* Number of Longwords transfered */
#else
	      xferCount = (retVal >> 2) + dummy;	/* Number of Longwords transfered */
#endif
	      logMsg
		("f1ReadEvent: DMA transfer terminated by unknown BUS Error (csr=0x%x nwrds=%d)\n",
		 csr, xferCount, 0, 0, 0, 0);
	      return (xferCount);
/* 	return(ERROR); */
	    }
	}
      else if (retVal == 0)
	{			/* Block Error finished without Bus Error */
#ifdef VXWORKS
	  logMsg
	    ("f1ReadEvent: WARN: DMA transfer terminated by word count 0x%x\n",
	     nwrds, 0, 0, 0, 0, 0);
#else
	  logMsg
	    ("f1ReadEvent: WARN: DMA transfer returned zero word count 0x%x\n",
	     nwrds, 0, 0, 0, 0, 0);
#endif
	  return (nwrds);
	}
      else
	{			/* Error in DMA */
#ifdef VXWORKS
	  logMsg("f1ReadEvent: ERROR: sysVmeDmaDone returned an Error\n", 0,
		 0, 0, 0, 0, 0);
#else
	  logMsg("f1ReadEvent: ERROR: vmeDmaDone returned an Error\n", 0, 0,
		 0, 0, 0, 0);
#endif
	  return (retVal);
	}

    }
  else
    {				/*Programmed IO */

      /* Check if Bus Errors are enabled. If so then disable for Prog I/O reading */
      F1LOCK;
      berr = vmeRead32(&(f1p[id]->ctrl)) & F1_ENABLE_BERR;
      if (berr)
	vmeWrite32(&(f1p[id]->ctrl),
		   vmeRead32(&(f1p[id]->ctrl)) & ~F1_ENABLE_BERR);
      F1UNLOCK;

      dCnt = 0;
      /* Read Header - should be first word */
      head = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      head = LSWAP(head);
#endif
      if ((head & F1_HT_DATA_MASK) != F1_HEAD_DATA)
	{
	  /* We got bad data - Check if there is any data at all */
	  F1LOCK;
	  if ((vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK) == 0)
	    {
	      logMsg("f1ReadEvent: FIFO Empty (0x%08x)\n", head, 0, 0, 0, 0,
		     0);
	      F1UNLOCK;
	      return (0);
	    }
	  else
	    {
	      logMsg("f1ReadEvent: ERROR: Invalid Header Word 0x%08x\n", head,
		     0, 0, 0, 0, 0);
	      F1UNLOCK;
	      return (ERROR);
	    }
	}
      else
	{
	  evnum = (head & F1_HT_EVENT_MASK) >> 16;
	  trigtime = (head & F1_HT_TRIG_MASK) >> 7;
#ifndef VXWORKS
	  head = LSWAP(head);	/* Swap back to little-endian */
#endif
	  data[dCnt] = head;
	  dCnt++;
	}

      ii = 0;
      while (ii < nwrds)
	{
	  F1LOCK;
	  val = (unsigned int) *f1pd[id];
	  F1UNLOCK;
	  data[ii + 1] = val;
#ifndef VXWORKS
	  val = LSWAP(val);
#endif
	  if ((val & F1_HT_DATA_MASK) == F1_TAIL_DATA)
	    break;
	  ii++;
	}
      ii++;
      dCnt += ii;

      /* Check if this is an end of Block and there is a filler word. If
         so then it should be read out and discarded */
      F1LOCK;
      status = vmeRead32(&(f1p[id]->csr));
      F1UNLOCK;
      if ((status & F1_CSR_NEXT_BUF_NO) && (status & F1_CSR_FILLER_FLAG))
	{
	  F1LOCK;
	  val = (unsigned int) *f1pd[id];
	  F1UNLOCK;
#ifndef VXWORKS
	  val = LSWAP(val);
#endif
	  if (val & F1_DATA_SLOT_MASK)
	    logMsg("f1ReadData: ERROR: Invalid filler word 0x%08x\n", val, 0,
		   0, 0, 0, 0);
	}

      if (berr)
	{
	  F1LOCK;
	  vmeWrite32(&(f1p[id]->ctrl),
		     vmeRead32(&(f1p[id]->ctrl)) | F1_ENABLE_BERR);
	  F1UNLOCK;
	}
      return (dCnt);
    }

  return (OK);
}

int
f1PrintEvent(int id, int rflag)
{
  int ii, evnum, trigtime, MAXWORDS = 64 * 8;
  int dCnt, berr = 0;
  unsigned int status, head, tail, val, chip;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      printf("f1PrintEvent: ERROR : TDC in slot %d is not initialized \n",
	     id);
      return (ERROR);
    }

  /* Check if data available */
  F1LOCK;
  if ((vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK) == 0)
    {
      printf("f1PrintEvent: ERROR: FIFO Empty\n");
      F1UNLOCK;
      return (0);
    }
  F1UNLOCK;

  /* Check if Bus Errors are enabled. If so then disable for reading */
  F1LOCK;
  berr = vmeRead32(&(f1p[id]->ctrl)) & F1_ENABLE_BERR;
  if (berr)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) & ~F1_ENABLE_BERR);
  F1UNLOCK;

  dCnt = 0;
  /* Read Header - */
  F1LOCK;
  head = (unsigned int) *f1pd[id];
  F1UNLOCK;
#ifndef VXWORKS
  head = LSWAP(head);
#endif

  /* check if valid data */
  if ((head & F1_DATA_SLOT_MASK) == F1_DATA_INVALID)
    {
      printf("f1PrintEvent: ERROR: Invalid Data from FIFO 0x%08x\n", head);
      return (ERROR);
    }

  if ((head & F1_HT_DATA_MASK) != F1_HEAD_DATA)
    {
      printf("f1PrintEvent: ERROR: Invalid Header Word 0x%08x\n", head);
      return (ERROR);
    }
  else
    {
      printf("TDC DATA for Module in Slot %d\n", id);
      evnum = (head & F1_HT_EVENT_MASK) >> 16;
      trigtime = (head & F1_HT_TRIG_MASK) >> 7;
      chip = (head & F1_HT_CHIP_MASK) >> 3;
      dCnt++;
      printf("  Header  = 0x%08x(chip %d)   Event = %d   Trigger Time = %d ",
	     head, chip, evnum, trigtime);
    }

  ii = 0;
  while (ii < MAXWORDS)
    {
      if ((ii % 5) == 0)
	printf("\n    ");
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if ((val & F1_HT_DATA_MASK) == F1_TAIL_DATA)
	{
	  tail = val;
	  ii++;
	  printf("\n");
	  break;
	}
      else if ((val & F1_HT_DATA_MASK) == F1_HEAD_DATA)
	{
	  chip = (val & F1_HT_CHIP_MASK) >> 3;
	  printf("  0x%08x(H%d T%d)", val, chip,
		 (val & F1_HT_TRIG_MASK) >> 7);
	  ii++;
	}
      else
	{
	  printf("  0x%08x    ", val);
	  ii++;
	}
    }
  dCnt += ii;


  /* Check if this is an end of Block and there is a filler word. If
     so then it should be read out and discarded */
  F1LOCK;
  status = vmeRead32(&(f1p[id]->csr));
  F1UNLOCK;

  if ((status & F1_CSR_NEXT_BUF_NO) && (status & F1_CSR_FILLER_FLAG))
    {
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if (val & F1_DATA_SLOT_MASK)
	printf("f1PrintData: ERROR: Invalid filler word 0x%08x\n", val);

      printf("  Trailer = 0x%08x   Word Count = %d  Filler = 0x%08x\n", tail,
	     dCnt, val);
    }
  else
    {
      printf("  Trailer = 0x%08x   Word Count = %d\n", tail, dCnt);
    }

  if (berr)
    {
      F1LOCK;
      vmeWrite32(&(f1p[id]->ctrl),
		 vmeRead32(&(f1p[id]->ctrl)) | F1_ENABLE_BERR);
      F1UNLOCK;
    }

  return (dCnt);

}

/* Routine to flush a partial event from the FIFO. Read until a valid trailer is found */
int
f1FlushEvent(int id)
{
/*   int ii, evnum, trigtime, MAXWORDS=64*8; */
  int ii, MAXWORDS = 64 * 8;
  int berr = 0;
  unsigned int status, tail, val;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1FlushEvent: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return (ERROR);
    }

  /* Check if data available - If not then just issue a Clear */
  F1LOCK;
  if ((vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK) == 0)
    {
      F1UNLOCK;
      f1Clear(id);
      return (0);
    }
  F1UNLOCK;

  /* Check if Bus Errors are enabled. If so then disable for reading */
  F1LOCK;
  berr = vmeRead32(&(f1p[id]->ctrl)) & F1_ENABLE_BERR;
  if (berr)
    vmeWrite32(&(f1p[id]->ctrl),
	       vmeRead32(&(f1p[id]->ctrl)) & ~F1_ENABLE_BERR);
  F1UNLOCK;


  ii = 0;
  while (ii < MAXWORDS)
    {
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      if (((val & F1_HT_DATA_MASK) == F1_TAIL_DATA)
	  && ((val & F1_DATA_SLOT_MASK) != F1_DATA_INVALID))
	{
	  tail = val;
	  ii++;
	  break;
	}
      else
	{
	  ii++;
	}
    }

  /* Check if this is an end of Block and there is a filler word. If
     so then it should be read out and discarded */
  F1LOCK;
  status = vmeRead32(&(f1p[id]->csr));
  F1UNLOCK;
  if ((status & F1_CSR_NEXT_BUF_NO) && (status & F1_CSR_FILLER_FLAG))
    {
      F1LOCK;
      val = (unsigned int) *f1pd[id];
      F1UNLOCK;
#ifndef VXWORKS
      val = LSWAP(val);
#endif
      ii++;
      if (val & F1_DATA_SLOT_MASK)
	logMsg("f1FlushData: ERROR: Invalid filler word 0x%08x\n", val, 0, 0,
	       0, 0, 0);

    }

  if (berr)
    {
      F1LOCK;
      vmeWrite32(&(f1p[id]->ctrl),
		 vmeRead32(&(f1p[id]->ctrl)) | F1_ENABLE_BERR);
      F1UNLOCK;
    }
  return (ii);

}

int
f1GPrintEvent(int rflag)
{
  int ii, id, count, total = 0;
  int mask = 0, scan = 0;

  /*check for data in all TDCs */
  for (ii = 0; ii < nf1tdc; ii++)
    {
      mask |= (1 << (f1ID[ii]));
    }
  scan = f1DataScan(0);

  if ((scan != 0) && (scan == mask))
    {
      for (ii = 0; ii < nf1tdc; ii++)
	{
	  id = f1ID[ii];
	  count = f1PrintEvent(id, rflag);
	  total += count;
	  printf("\n");
	}

      printf("f1GPrintEvent: TOTALS:  TDCs = %d  Word Count = %d\n", nf1tdc,
	     total);
      return (total);

    }
  else
    {
      printf
	("f1GPrintEvent: ERROR: Not all modules have data  scan = 0x%x mask = 0x%x\n",
	 scan, mask);
      return (0);
    }

}


void
f1Clear(int id)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1Clear: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return;
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->csr), F1_CSR_SOFT_RESET);
  F1UNLOCK;

}

void
f1GClear()
{
  int ii;

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->csr), F1_CSR_SOFT_RESET);
  F1UNLOCK;

}


void
f1ClearStatus(int id, unsigned int chipMask)
{

  int ii;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1ClearStatus: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return;
    }

  /* Default clear all chips latched status bits */
  if (chipMask <= 0)
    chipMask = F1_ALL_CHIPS;

  F1LOCK;
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if (chipMask & (1 << ii))
	{
	  vmeWrite16(&(f1p[id]->stat[ii]), F1_CHIP_CLEAR_STATUS);
	}
    }
  F1UNLOCK;

}

void
f1GClearStatus(unsigned int chipMask)
{
  int ii, id;

  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      f1ClearStatus(id, chipMask);
    }
}

/* Return the Error status for all the chips on a board */
unsigned int
f1ErrorStatus(int id, int sflag)
{
  int jj;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int err = 0;
  unsigned int cmask = 0;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1ErrorStatus: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (0);
    }

  if (sflag == 1)
    {				/* Do full latch and persistance check of all chips */
      F1LOCK;
      for (jj = 0; jj < F1_MAX_TDC_CHIPS; jj++)
	{
	  chipstat[jj] = vmeRead16(&(f1p[id]->stat[jj]));
	  if (((chipstat[jj] & F1_CHIP_RES_LOCKED) == 0)
	      || (chipstat[jj] & F1_CHIP_ERROR_COND))
	    cmask |= (1 << jj);
	}
      F1UNLOCK;
      return (cmask);

    }
  else
    {				/* Just Read CSR to get error info */
      F1LOCK;
      err = vmeRead32(&(f1p[id]->csr));
      F1UNLOCK;

      if (err & F1_CSR_ERROR)	/* an Error condition exists */
	return ((err & F1_CSR_ERROR_MASK) >> 8);
      else
	return (0);
    }

}

unsigned int
f1GErrorStatus(int sflag)
{
  int ii, jj, id, count;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int emask = 0;

  if (sflag == 1)
    {				/* Do full latch and persistance check of all chips */
      F1LOCK;
      for (ii = 0; ii < nf1tdc; ii++)
	{
	  id = f1ID[ii];
	  for (jj = 0; jj < F1_MAX_TDC_CHIPS; jj++)
	    {
	      chipstat[jj] = vmeRead16(&(f1p[id]->stat[jj]));
	      if (((chipstat[jj] & F1_CHIP_RES_LOCKED) == 0)
		  || (chipstat[jj] & F1_CHIP_ERROR_COND))
		emask |= (1 << id);
	    }
	}
      F1UNLOCK;

      return (emask);

    }
  else
    {				/* Do quick check of CSR registers */
      F1LOCK;
      for (ii = 0; ii < nf1tdc; ii++)
	{
	  id = f1ID[ii];
	  count = (vmeRead32(&(f1p[id]->csr)) & F1_CSR_ERROR);
	  if (count)
	    emask |= (1 << id);
	}
      F1UNLOCK;

      return (emask);
    }

}

/* Get Resolution lock status for all chips on the board
   if return 0 then all chips are locked
   else a bitmask of unlocked chips is returned */
int
f1CheckLock(int id)
{
  int jj;
  unsigned short chipstat[F1_MAX_TDC_CHIPS];
  unsigned int cmask = 0;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1CheckLock: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return (ERROR);
    }

  F1LOCK;
#ifdef VXWORKS
  /* Get Chip status Register data in longword chunks - faster than in 16bit mode */
  bcopyLongs((char *) &f1p[id]->stat[0], (char *) &chipstat[0], 4);
#else
  for (jj = 0; jj < 8; jj++)
    {
      chipstat[jj] = vmeRead16(&(f1p[id]->stat[jj]));
    }
#endif
  F1UNLOCK;

  for (jj = 0; jj < F1_MAX_TDC_CHIPS; jj++)
    {
      if ((chipstat[jj] & F1_CHIP_RES_LOCKED) == 0)
	cmask |= (1 << jj);
    }

  return (cmask);
}

int
f1GCheckLock(int pflag)
{
  int ii, id, mmask = 0;
  unsigned int emask = 0, stat;

  if (nf1tdc <= 0)
    {
      logMsg("f1GCheckLock: ERROR: No TDCs Initialized \n", 0, 0, 0, 0, 0, 0);
      return (ERROR);
    }

  /* Do quick check of CSR registers */
  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      stat = (vmeRead32(&(f1p[id]->csr)) & F1_CSR_ERROR);
      /* If there is an error check if it is Resolution NOT locked */
      if (stat)
	{
	  emask = f1CheckLock(id);
	  if (emask)
	    {
	      mmask |= (1 << id);
	      if (pflag)
		printf("TDC Slot %d  Unlocked chip mask 0x%02x\n", id, emask);
	    }
	}
    }
  F1UNLOCK;

  return (mmask);
}





void
f1Reset(int id, int iFlag)
{
  unsigned int a32addr, addrMB;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1Reset: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return;
    }

  if ((iFlag < 0) || (iFlag > 3))
    iFlag = 2;			/* Default to Normal Syncronous */

  F1LOCK;
  a32addr = vmeRead32(&(f1p[id]->adr32));
  addrMB = vmeRead32(&(f1p[id]->adr_mb));

  vmeWrite32(&(f1p[id]->csr), F1_CSR_HARD_RESET);
  taskDelay(30);
  F1UNLOCK;

  f1ConfigWrite(id, (int *) &f1ConfigData[iFlag], F1_ALL_CHIPS);

  F1LOCK;
  vmeWrite32(&(f1p[id]->adr32), a32addr);
  vmeWrite32(&(f1p[id]->adr_mb), addrMB);
  F1UNLOCK;

  if ((f1UseSDC <= 0) && (f1UseBDC <= 0))
    f1EnableClk(id, 0);

  f1ClearStatus(id, F1_ALL_CHIPS);

}

int
f1HardReset(int id)
{
  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1HardReset: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return ERROR;
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->csr), F1_CSR_HARD_RESET);
  taskDelay(30);
  F1UNLOCK;

  return OK;
}

void
f1Trig(int id)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1Trig: ERROR : TDC in slot %d is not initialized \n", id, 0, 0,
	     0, 0, 0);
      return;
    }

  F1LOCK;
  if (vmeRead32(&(f1p[id]->ctrl)) & (F1_ENABLE_SOFT_TRIG))
    vmeWrite32(&(f1p[id]->csr), F1_CSR_TRIGGER);
  else
    logMsg("f1Trig: ERROR: Software Triggers not enabled", 0, 0, 0, 0, 0, 0);
  F1UNLOCK;

}

void
f1GTrig()
{
  int ii, id;

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      if (vmeRead32(&(f1p[id]->ctrl)) & (F1_ENABLE_SOFT_TRIG))
	vmeWrite32(&(f1p[id]->csr), F1_CSR_TRIGGER);
      else
	logMsg
	  ("f1Trig: ERROR: Software Triggers not enabled for TDC in slot %d",
	   id, 0, 0, 0, 0, 0);
    }
  F1UNLOCK;

}



/* Return Event count for TDC in slot id */
int
f1Dready(int id)
{
  int rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1Dready: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return (ERROR);
    }

  F1LOCK;
  rval = vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK;
  F1UNLOCK;

  return (rval);
}

/* Return True if there is an event Block ready for readout */
int
f1Bready(int id)
{
  int stat;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1Bready: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return (ERROR);
    }

  F1LOCK;
  stat = vmeRead32(&(f1p[id]->csr)) & F1_CSR_EVENT_LEVEL_FLAG;
  F1UNLOCK;

  if (stat)
    return (1);
  else
    return (0);
}

unsigned int
f1GBready()
{
  int ii, id, stat;
  unsigned int dmask = 0;

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      stat = vmeRead32(&(f1p[id]->csr)) & F1_CSR_EVENT_LEVEL_FLAG;
      if (stat)
	dmask |= (1 << id);
    }
  F1UNLOCK;

  return (dmask);
}


/* Return Slot mask for modules with data avaialable */
int
f1DataScan(int pflag)
{
  int ii, count, id, dmask = 0;

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      count = vmeRead32(&(f1p[id]->ev_count)) & F1_EVENT_COUNT_MASK;
      if (count)
	dmask |= (1 << id);

      if (pflag)
	logMsg(" F1TDC %2d Slot %2d  Count=%d\n", ii, id, count, 0, 0, 0);
    }
  F1UNLOCK;

  return (dmask);
}

/* return Scan mask for all Initialized TDCs */
unsigned int
f1ScanMask()
{
  int ii, id, dmask = 0;

  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      dmask |= (1 << id);
    }

  return (dmask);
}


int
f1GetRez(int id)
{
  int ii, rez = 0;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1GetRez: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return (ERROR);
    }

  F1LOCK;
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if (vmeRead32(&(f1p[id]->config[ii][1])) & F1_HIREZ_MODE)
	rez |= (1 << ii);
    }
  F1UNLOCK;

  return (rez);

}

int
f1SetWindow(int id, int window, int latency, int chipMask)
{
  int ii, jj, enMask;
  int exponent, refclkdiv, hsdiv;
  int tframe, winMax, latMax;
  unsigned int rval, config[16];

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1SetWindow: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return (ERROR);
    }

  if (chipMask <= 0)
    chipMask = F1_ALL_CHIPS;
  enMask = f1Enabled(id);

  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if (chipMask & (1 << ii))
	{
	  f1ConfigRead(id, config, ii);

	  /* Check if window and latency are OK */
	  tframe = 25 * (((config[7] >> 6) & 0x1ff) + 2);
	  winMax = (tframe * (0.4));
	  latMax = (tframe * (0.9));

	  if ((window > winMax) || (window <= 0))
	    {
	      logMsg
		("f1SetWindow: Trig Window for chip %d Out of range. Set to %d ns\n",
		 ii, winMax, 0, 0, 0, 0);
	      window = winMax;
	    }
	  if (latency < window)
	    {
	      logMsg
		("f1SetWindow: Trig Latency for chip %d is too small. Set to %d ns\n",
		 ii, window, 0, 0, 0, 0);
	      latency = window;
	    }
	  else if (latency > latMax)
	    {
	      logMsg
		("f1SetWindow: Trig Latency for chip %d Out of range. Set to %d ns\n",
		 ii, latMax, 0, 0, 0, 0);
	      latency = latMax;
	    }

	  exponent = ((config[10]) >> 8) & 0x7;
	  refclkdiv = 1;
	  for (jj = 0; jj < exponent; jj++)
	    refclkdiv = 2 * refclkdiv;
	  hsdiv = (config[10]) & 0xFF;
	  config[8] =
	    (int) ((float) (152 * hsdiv * window) / (float) (25 * refclkdiv));
	  config[9] =
	    (int) ((float) (152 * hsdiv * latency) /
		   (float) (25 * refclkdiv));

	  /* Rewrite Window and Latency registers */
	  F1LOCK;
	  for (jj = 8; jj <= 9; jj++)
	    {
	      vmeWrite32(&(f1p[id]->config[ii][jj]), config[jj]);
	      rval = vmeRead32(&(f1p[id]->config[ii][jj])) & 0xffff;
	      if (rval != config[jj])
		{
		  logMsg
		    ("f1SetWindow: Error writing Config (%x != %x) slot=%d\n",
		     rval, config[jj], id, 0, 0, 0);
		}
	    }
	  F1UNLOCK;

	  /*f1ConfigWrite(id,config,(1<<ii)); */
	}
    }

  f1ClearStatus(id, F1_ALL_CHIPS);	/* Clear any latched status bits */

  if (enMask)
    f1EnableData(id, enMask);	/* renable any chips that were enabled */

  return (OK);

}

void
f1GSetWindow(int window, int latency, int chipMask)
{
  int id, ii;

  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      f1SetWindow(id, window, latency, chipMask);
    }

}

unsigned int
f1ReadCSR(int id)
{
  unsigned int rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1ReadCSR: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return (0xffffffff);
    }

  F1LOCK;
  rval = vmeRead32(&(f1p[id]->csr));
  F1UNLOCK;

  return (rval);
}



int
f1WriteControl(int id, unsigned int val)
{
  int rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1WriteControl: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (ERROR);
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl), val);
  rval = vmeRead32(&(f1p[id]->ctrl)) & F1_CONTROL_MASK;
  F1UNLOCK;

  return (rval);
}

void
f1GWriteControl(unsigned int val)
{
  int id, ii;

  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      f1WriteControl(id, val);
    }
}



int
f1Enabled(int id)
{
  int res = 0;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1Enabled: ERROR : TDC in slot %d is not initialized \n", id, 0,
	     0, 0, 0, 0);
      return (ERROR);
    }

  F1LOCK;
  res = (vmeRead32(&(f1p[id]->ctrl)) & 0xff0000) >> 16;
  F1UNLOCK;

  return (res);			/* Return the output FIFO enable mask */
}


int
f1EnableData(int id, int chipMask)
{
  int ii, jj, mask = 0;
  unsigned int reg, rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1EnableData: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return (ERROR);
    }

  if (chipMask <= 0)
    chipMask = F1_ALL_CHIPS;	/* Enable all Chips */

  /* Enable FIFOs for each chip */
  mask = (chipMask & 0xff) << 16;
  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) | mask);

  /* Enable Edges for each chip */
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      if ((chipMask) & (1 << ii))
	{
	  for (jj = 2; jj < 6; jj++)
	    {
	      reg = vmeRead32(&(f1p[id]->config[ii][jj])) & 0xffff;
	      reg = reg | F1_ENABLE_EDGES;
	      vmeWrite32(&(f1p[id]->config[ii][jj]), reg);
	      /* read it back and check */
	      rval = vmeRead32(&(f1p[id]->config[ii][jj])) & 0xffff;
	      if (rval != reg)
		{
		  logMsg("f1EnableData: Error writing Config (%x != %x) \n",
			 rval, reg, 0, 0, 0, 0);
		}
	    }
	}
    }
  F1UNLOCK;

  return (OK);
}

void
f1GEnableData(int chipMask)
{
  int ii, id;

  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      f1EnableData(id, chipMask);
    }

}

int
f1DisableData(int id)
{
  int ii, jj, mask = 0;
  unsigned int reg, rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1DisableData: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (ERROR);
    }

  /* Disable FIFOs for All chips */
  mask = (0xff) << 16;
  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) & ~mask);

  /* Disable Edges for All chips */
  for (ii = 0; ii < F1_MAX_TDC_CHIPS; ii++)
    {
      for (jj = 2; jj < 6; jj++)
	{
	  reg = vmeRead32(&(f1p[id]->config[ii][jj])) & 0xffff;
	  reg = reg & ~(F1_ENABLE_EDGES);
	  vmeWrite32(&(f1p[id]->config[ii][jj]), reg);
	  /* read it back and check */
	  rval = vmeRead32(&(f1p[id]->config[ii][jj])) & 0xffff;
	  if (rval != reg)
	    {
	      logMsg("f1EnableData: Error writing Config (%x != %x) \n", rval,
		     reg, 0, 0, 0, 0);
	    }
	}
    }
  F1UNLOCK;

  return (OK);
}

/* Disable Individual Inputs between 1 and 64 or 32 (High Resolution)*/
int
f1DisableChannel(int id, int input)
{
  int rez;
  unsigned int reg, rval, chip, chan, odd;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1DisableChannel: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (ERROR);
    }

  /* Determine Chip number and Channel for the given input */
  if ((input < 1) || (input > 64))
    {
      return (ERROR);
    }
  else
    {
      /* Check Chip 0 resolution */
      F1LOCK;
      rez = vmeRead32(&(f1p[id]->config[0][1])) & F1_HIREZ_MODE;
      F1UNLOCK;
      if ((rez) && (input > 32))
	return (ERROR);
    }

  if (rez)
    input = input * 2 - 2;
  else
    input -= 1;			/* Input 0 - 63 */
  chip = (int) (input / 8);	/* Chip 0 - 7 */
  chan = (input % 8) + 1;	/* Chan 1 - 8 */
  rval = (int) (((chan - 1) / 2) + 2);	/* Reg  2 - 6 */
  odd = chan % 2;		/* Odd (1) or Even (0) Channel */


  /* Disable Edges for specified Channel */
  F1LOCK;
  reg = vmeRead32(&(f1p[id]->config[chip][rval])) & 0xffff;

  if (rez)
    {
      reg = reg & F1_DISABLE_EDGES;
    }
  else
    {
      if (odd)
	reg = reg & F1_DISABLE_EDGES_ODD;
      else
	reg = reg & F1_DISABLE_EDGES_EVEN;
    }

  vmeWrite32(&(f1p[id]->config[chip][rval]), reg);
  /* read it back and check */
  rval = vmeRead32(&(f1p[id]->config[chip][rval])) & 0xffff;
  F1UNLOCK;

  if (rval != reg)
    {
      logMsg("f1DisableChannel: Error writing Config (%x != %x) \n", rval,
	     reg, 0, 0, 0, 0);
    }

  return (OK);
}




void
f1EnableClk(int id, int cflag)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1EnableClk: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  if (cflag)
    vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) | F1_REF_CLK_SEL);	/* Enable from Backplane */
  else
    vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) | F1_REF_CLK_PCB);	/* Enable internal */
  F1UNLOCK;

}

void
f1DisableClk(int id, int cflag)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1DisableClk: ERROR : TDC in slot %d is not initialized \n", id,
	     0, 0, 0, 0, 0);
      return;
    }

  /* Disable clock depending on flag */
  F1LOCK;
  if (cflag)
    {				/* Disable internal and from Backplane */
      vmeWrite32(&(f1p[id]->ctrl),
		 vmeRead32(&(f1p[id]->ctrl)) & ~(F1_REF_CLK_PCB |
						 F1_REF_CLK_SEL));
    }
  else
    {				/* Just internal */
      vmeWrite32(&(f1p[id]->ctrl),
		 vmeRead32(&(f1p[id]->ctrl)) & ~F1_REF_CLK_PCB);
    }
  F1UNLOCK;

}

unsigned int
f1EnableLetra(int id, int chipMask)
{
/*   int ii; */

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1EnableLetra: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (0);
    }

  if (chipMask <= 0)
    chipMask = F1_ALL_CHIPS;	/* Enable all Chips */

  f1LetraMode |= (1 << id);



  return (f1LetraMode);
}

unsigned int
f1DisableLetra(int id, int chipMask)
{
/*   int ii; */

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1DisableLetra: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (0);
    }

  if (chipMask <= 0)
    chipMask = F1_ALL_CHIPS;	/* Disable all Chips */


  f1LetraMode &= ~(1 << id);

  return (f1LetraMode);
}


void
f1EnableSoftTrig(int id)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1EnableSoftTrig: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl),
	     vmeRead32(&(f1p[id]->ctrl)) | F1_ENABLE_SOFT_TRIG);
  F1UNLOCK;

}

void
f1GEnableSoftTrig()
{
  int ii;

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
	       vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_ENABLE_SOFT_TRIG);
  F1UNLOCK;

}


void
f1DisableSoftTrig(int id)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg
	("f1DisableSoftTrig: ERROR : TDC in slot %d is not initialized \n",
	 id, 0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl),
	     vmeRead32(&(f1p[id]->ctrl)) & ~F1_ENABLE_SOFT_TRIG);
  F1UNLOCK;

}

void
f1EnableBusError(int id)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1EnableBusError: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) | F1_ENABLE_BERR);
  F1UNLOCK;

}

void
f1GEnableBusError()
{
  int ii;

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
	       vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_ENABLE_BERR);
  F1UNLOCK;

}


void
f1DisableBusError(int id)
{

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg
	("f1DisableBusError: ERROR : TDC in slot %d is not initialized \n",
	 id, 0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) & ~F1_ENABLE_BERR);
  F1UNLOCK;

}


int
f1SetBlockLevel(int id, int level)
{
  int rval;

  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1SetBlockLevel: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return (ERROR);
    }

  if (level <= 0)
    level = 1;
  F1LOCK;
  vmeWrite32(&(f1p[id]->ev_level), level);
  rval = vmeRead32(&(f1p[id]->ev_level)) & F1_EVENT_LEVEL_MASK;
  F1UNLOCK;

  return (rval);

}

void
f1GSetBlockLevel(int level)
{
  int ii;

  if (level <= 0)
    level = 1;
  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ev_level), level);
  F1UNLOCK;

}

void
f1SetInputPort(int id, int fb)
{
  if (id == 0)
    id = f1ID[0];

  if ((id <= 0) || (id > 21) || (f1p[id] == NULL))
    {
      logMsg("f1SetInputPort: ERROR : TDC in slot %d is not initialized \n",
	     id, 0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  if (fb)
    vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) | F1_FB_SEL);	/* Set to Backplane */
  else
    vmeWrite32(&(f1p[id]->ctrl), vmeRead32(&(f1p[id]->ctrl)) & ~F1_FB_SEL);	/* Set to Front Panel - Default */
  F1UNLOCK;
}


void
f1EnableMultiBlock()
{
  int ii, id;

  if ((nf1tdc <= 1) || (f1p[f1ID[0]] == NULL))
    {
      logMsg("f1EnableMultiBlock: ERROR : Cannot Enable MultiBlock mode \n",
	     0, 0, 0, 0, 0, 0);
      return;
    }

  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      F1LOCK;
      vmeWrite32(&(f1p[id]->ctrl),
		 vmeRead32(&(f1p[id]->ctrl)) | F1_ENABLE_MULTIBLOCK);
      F1UNLOCK;
      f1DisableBusError(id);
      if (id == f1MinSlot)
	{
	  F1LOCK;
	  vmeWrite32(&(f1p[id]->ctrl),
		     vmeRead32(&(f1p[id]->ctrl)) | F1_FIRST_BOARD);
	  F1UNLOCK;
	}
      if (id == f1MaxSlot)
	{
	  F1LOCK;
	  vmeWrite32(&(f1p[id]->ctrl),
		     vmeRead32(&(f1p[id]->ctrl)) | F1_LAST_BOARD);
	  F1UNLOCK;
	  f1EnableBusError(id);	/* Enable Bus Error only on Last Board */
	}
    }

}

void
f1DisableMultiBlock()
{
  int ii;

  if ((nf1tdc <= 1) || (f1p[f1ID[0]] == NULL))
    {
      logMsg("f1DisableMultiBlock: ERROR : Cannot Disable MultiBlock Mode\n",
	     0, 0, 0, 0, 0, 0);
      return;
    }

  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
	       vmeRead32(&(f1p[f1ID[ii]]->ctrl)) & ~F1_ENABLE_MULTIBLOCK);
  F1UNLOCK;

}


/***************************************************************************************
   JLAB F1 Signal Distribution Card (SDC) Routines

*/

STATUS
f1SDC_Config()
{
  int ii, id;
  int iwait = 0;

  if ((f1UseSDC <= 0) || (f1SDCp == NULL))
    {
      logMsg("f1SDC_Config: ERROR : Cannot Configure F1 Signal Board \n", 0,
	     0, 0, 0, 0, 0);
      return (ERROR);
    }

  /* Set up for distributing Clock to all modules
     Front Panel
     Internal Clock off
     Use External Clock          */
  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    {
      vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
		 vmeRead32(&(f1p[f1ID[ii]]->ctrl)) & ~F1_FB_SEL);
      vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
		 vmeRead32(&(f1p[f1ID[ii]]->ctrl)) & ~F1_REF_CLK_PCB);
      vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
		 vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_REF_CLK_SEL);
    }
  F1UNLOCK;

  /* Enable SDC Clock and Sync Reset */
  F1SDCLOCK;
  vmeWrite16(&(f1SDCp->csr), F1_SDC_INTERNAL_CLK | F1_SDC_INTERNAL_SYNC);
  F1SDCUNLOCK;

  // Wait for the Chips on each module to lock to SDC Clock.
  for (ii = 0; ii < nf1tdc; ii++)
    {
      id = f1ID[ii];
      iwait = 0;
      while (iwait < 100000)
	{
	  if (f1CheckLock(id) == 0)
	    break;
	  iwait++;
	}
    }
  if (iwait == 100000)
    printf
      ("WARNING: f1SDC_Config(): Timeout waiting for Chip Resolution Lock.\n");

  return (OK);

}

void
f1SDC_Sync()
{

  if (f1SDCp == NULL)
    {
      logMsg("f1SDC_Sync: ERROR : No F1 SDC available \n", 0, 0, 0, 0, 0, 0);
      return;
    }

  F1SDCLOCK;
  vmeWrite16(&(f1SDCp->ctrl), F1_SDC_SYNC_RESET);
  F1SDCUNLOCK;

}

/***************************************************************************************
   JLAB F1 Backplane Distribution Card (BDC) Routines

*/

STATUS
f1BDC_Config()
{
  int ii;

  if (f1UseBDC <= 0)
    {
      logMsg("f1BDC_Config: ERROR : Cannot Configure F1 Backplane Board \n",
	     0, 0, 0, 0, 0, 0);
      return (ERROR);
    }

  /* Set up for distributing Clock to all modules and triggers from Backplane
     Backpanel Triggers
     Internal Clock off
     Use External Clock          */
  F1LOCK;
  for (ii = 0; ii < nf1tdc; ii++)
    {
      vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
		 vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_FB_SEL);
      vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
		 vmeRead32(&(f1p[f1ID[ii]]->ctrl)) & ~F1_REF_CLK_PCB);
      vmeWrite32(&(f1p[f1ID[ii]]->ctrl),
		 vmeRead32(&(f1p[f1ID[ii]]->ctrl)) | F1_REF_CLK_SEL);
      taskDelay(5);		/* wait a bit so we don't overload the 12 Volts */
    }

  /* Enable Spare Output on 1st TDC */
  vmeWrite32(&(f1p[f1ID[0]]->ctrl),
	     vmeRead32(&(f1p[f1ID[0]]->ctrl)) | F1_ENABLE_SPARE_OUT);
  F1UNLOCK;

  return (OK);

}

void
f1BDC_Sync()
{

  if (f1UseBDC <= 0)
    {
      logMsg("f1BDC_Sync: ERROR : No F1 BDC available \n", 0, 0, 0, 0, 0, 0);
      return;
    }

  /* Strobe Spare Output on 1st TDC */
  F1LOCK;
  vmeWrite32(&(f1p[f1ID[0]]->csr), F1_CSR_PULSE_SPARE_OUT);
  F1UNLOCK;

}



/* Test routines */
/* Max words = 64 channels * 8 hits/chan + 8 headers + 1 trailer */
#define TEST_MAX_WORDS   (64*8 + 9)

int f1TestEventCount, f1TestClearCount, f1TestErrorCount;
unsigned int f1TestData[TEST_MAX_WORDS];

void
f1TestRead()
{
  int ii, slot, retval, scanval;
  int maxwords = TEST_MAX_WORDS;
  int mask = 0;
  unsigned int errmask;
  unsigned int *data;

  maxwords = TEST_MAX_WORDS * nf1tdc;
  data = (unsigned int *) malloc((maxwords << 2));
  bzero((char *) data, (maxwords << 2));
  mask = f1ScanMask();
  if (mask <= 0)
    {
      printf("No TDCs available for Readout\n");
      return;
    }
  else
    {
      printf("F1 TDC Mask = 0x%x\n", mask);
      printf("F1 DATA Buffer (%d bytes)  at addr = 0x%x\n", (maxwords << 2),
	     (int) data);
    }

  f1TestEventCount = 0;
  f1TestClearCount = 0;
  f1TestErrorCount = 0;

  f1GEnableSoftTrig();

  while (1)
    {

      f1GTrig();

      scanval = f1DataScan(0);

      if (scanval == mask)
	{
	  f1TestEventCount++;
	  for (ii = 0; ii < nf1tdc; ii++)
	    {
	      slot = f1ID[ii];
	      retval = f1ReadEvent(slot, data, maxwords, 0);
	      if (retval <= 0)
		{
		  printf("Error in reading data from slot %d, retval=%d\n address = 0x%x",
			 slot, retval, (int) data);
		  return;
		}

	      if (retval == maxwords)
		{
		  f1TestClearCount++;
		  printf("f1TestRead: ERROR: Too much data in slot %d: Clearing TDCs\n", slot);
		  f1Clear(slot);
		}
	    }

	  /* check Error Status */
	  errmask = f1GErrorStatus(1);
	  if (errmask)
	    {
	      f1TestErrorCount++;
	      for (ii = 0; ii < nf1tdc; ii++)
		{
		  slot = f1ID[ii];
		  if ((1 << slot) & errmask)
		    {
		      f1ChipStatus(slot, 1);
		      f1ClearStatus(slot, 0);
		    }
		}
	    }
	}
      else
	{
	  printf("Bad Scan mask 0x%x (should be 0x%x)\n", scanval, mask);
	  taskDelay(1);
	}

      taskDelay(1);
    }

}

void
f1ISR(int arg)
{
  int ii, slot, retval, scanval;
  int maxwords = TEST_MAX_WORDS;
  unsigned int mask, errmask, stat;


  f1TestEventCount++;
  mask = f1ScanMask();
  scanval = f1DataScan(0);

  if (scanval == mask)
    {
      retval = f1ReadEvent(f1MinSlot, &f1TestData[0], maxwords, 2);
      if (retval <= 0)
	{
	  logMsg("Error in reading data retval=%d\n address = 0x%x", retval,
		 (int) f1TestData, 0, 0, 0, 0);
	  return;
	}

      if (retval >= maxwords)
	{
	  /* Check who has the Token */
	  for (ii = 0; ii < nf1tdc; ii++)
	    {
	      stat = f1ReadCSR(f1ID[ii]);
	      if (stat & F1_CSR_TOKEN_STATUS)
		{
		  logMsg("Token stuck in TDC slot %d on event %d\n", f1ID[ii],
			 f1TestEventCount, 0, 0, 0, 0);
		  break;
		}
	    }
	  f1GClear();
	}

      /* check Error Status */
      errmask = f1GErrorStatus(1);
      if (errmask)
	{
	  f1TestErrorCount++;
	  for (ii = 0; ii < nf1tdc; ii++)
	    {
	      slot = f1ID[ii];
	      if ((1 << slot) & errmask)
		{
		  f1ClearStatus(slot, 0);
		}
	    }
	  /*check again */
	  errmask = f1GErrorStatus(1);
	  if (errmask)
	    logMsg("Persistant Error present 0x%x\n", errmask, 0, 0, 0, 0, 0);
	}

    }
  else
    {
      logMsg("f1ISR: WARN: scanmask not correct 0x%x should be 0x%x\n",
	     scanval, mask, 0, 0, 0, 0);
      f1GClear();
    }

}
