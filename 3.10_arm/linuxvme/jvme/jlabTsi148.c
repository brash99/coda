/*----------------------------------------------------------------------------*
 *  Copyright (c) 2020        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Author:  Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Routines specific to the tsi148 VME Bridge
 *
 *----------------------------------------------------------------------------*/

#include <pthread.h>
#include "jvme.h"
#include "tsi148.h"
#include "jlabTsi148.h"
#include "dmaPList.h"

extern unsigned long long dma_timer[10];

#define LOCK_TSI {				\
    if(pthread_mutex_lock(&bridge_mutex)<0)	\
      perror("pthread_mutex_lock");		\
  }
/*! Unlock the mutex for access to the Tempe driver
   \hideinitializer
 */
#define UNLOCK_TSI {				\
    if(pthread_mutex_unlock(&bridge_mutex)<0)	\
      perror("pthread_mutex_unlock");		\
  }

/** Mutex for locking/unlocking Tempe driver access */
extern pthread_mutex_t bridge_mutex;

extern unsigned int vmeQuietFlag;


/*! Userspace base address of Tempe registers */
volatile tsi148_t *pTempe = NULL;

/*! Maximum allowed entries in DMA Linked List */
#define TSI148_DMA_MAX_LL 21
/*! Sample descriptor - contains preset attributes */
static volatile tsi148DmaDescriptor_t dmaDescSample;
/*! Pointer to Linked-List descriptors */
static volatile tsi148DmaDescriptor_t *dmaDescList;

/* Map and address from the linked list memory allocation in jlabgefDMA.c */
extern volatile unsigned int *dmaListMap;
extern unsigned long dmaListAdr;


/*! Total number of requested words in the DMA Linked List */
static unsigned int dmaLL_totalwords=0;
static int dmaLLInvalid=0;  /* Set in jlabgefDmaSetupLL */

/** \name Userspace window pointer for a24
    Global pointer to the Userspace a24 window
    \{ */
extern void *a24_window;

static int dmaBerrStatus=0;
/*! Buffer node pointer */
extern DMANODE *the_event;
/*! Data pointer */
extern unsigned int *dma_dabufp;


/*!
  Update A32 Slave window to allow all types of block transfers.

  \see jlabgefOpenA32Slave()

  @param window Slave Window Number
  @param base Address to return A32 Slave Window physical memory base address.
  @return OK, if successful.  Otherwise ERROR.
*/
int
jlabTsi148UpdateA32SlaveWindow(int window, unsigned long *base)
{
  int rval = OK;
  unsigned int itat, itsal, itofl, iteal;
  unsigned int itsau, itofu;
  unsigned long phys_addr;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  if((window < 0) || (window > 4))
    {
      printf("%s: ERROR: Invalid A32 Slave window number (%d)\n",
	     __func__, window);
      return ERROR;
    }

  LOCK_TSI;
  itsau = pTempe->lcsr.inboundTranslation[0].itsau; itsau = LSWAP(itsau);
  itsal = pTempe->lcsr.inboundTranslation[0].itsal; itsal = LSWAP(itsal);
  itofu = pTempe->lcsr.inboundTranslation[0].itofu; itofu = LSWAP(itofu);
  itofl = pTempe->lcsr.inboundTranslation[0].itofl; itofl = LSWAP(itofl);
  iteal = pTempe->lcsr.inboundTranslation[0].iteal; iteal = LSWAP(iteal);

  /* FIXME: Check upper addresses as well for 64bit address */
  phys_addr = itsal+itofl;
  *base = phys_addr;

  itat = pTempe->lcsr.inboundTranslation[0].itat; itat = LSWAP(itat);

#ifdef DEBUGSLAVE
  printf("%s:\n  ITSAL = 0x%08x\n  ITOFL = 0x%08x\n  ITEAL = 0x%08x\n",
	 __func__,itsal,itofl,iteal);

  printf("  phys_addr = 0x%08x\n",phys_addr);
#endif


  if(itat & TSI148_LCSR_ITAT_EN)
    {
      itat |= (TSI148_LCSR_ITAT_2eSSTB | TSI148_LCSR_ITAT_2eSST | TSI148_LCSR_ITAT_2eVME |
	       TSI148_LCSR_ITAT_MBLT | TSI148_LCSR_ITAT_BLT);
      itat |= (TSI148_LCSR_ITAT_SUPR | TSI148_LCSR_ITAT_NPRIV |
	       TSI148_LCSR_ITAT_PGM | TSI148_LCSR_ITAT_DATA);
      pTempe->lcsr.inboundTranslation[0].itat = LSWAP(itat);
    }
  else
    {
      printf("%s: ERROR: Inbound window attributes not modified.  Unexpected window number.\n",
	     __func__);
      rval = ERROR;
    }

  UNLOCK_TSI;

  return rval;
}

/*!
  Get the status of the SysReset bit

  \see jlabgefSysReset()

  @return If successful, 0 if SysReset is clear, 1 if high.  Otherwise ERROR.
*/
int
jlabTsi148GetSysReset()
{
  int rval = 0;
  unsigned int reg;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  reg = pTempe->lcsr.vctrl; reg = LSWAP(reg);
  UNLOCK_TSI;

#ifdef DEBUG_SYSRESET
  printf("VCTRL = 0x%08x SRESET = 0x%x\n",
	 reg,
	 reg & TSI148_LCSR_VCTRL_SRESET);
#endif /* DEBUG_SYSRESET */

  rval = (reg & TSI148_LCSR_VCTRL_SRESET) ? 1 : 0;

  return rval;
}

/*!
  Clear the status of the SysReset bit

  \see jlabgefSysReset()

  @return OK, if successful.  Otherwise ERROR.
*/
int
jlabTsi148ClearSysReset()
{
  int rval = 0;
  unsigned int reg;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  reg = pTempe->lcsr.vctrl; reg = LSWAP(reg);
  pTempe->lcsr.vctrl = LSWAP(reg & ~TSI148_LCSR_VCTRL_SRESET);
  UNLOCK_TSI;

  return rval;
}


/*!
  Get the status of the IRQ response setting to BERR.

  \see jlabgefBERRIrqStatus()

  @return 1 if enabled, 0 if disabled, -1 if error.
*/
int
jlabTsi148GetBERRIrq()
{
  unsigned int tmpCtl;
  unsigned int inten_enabled=0, inteo_enabled=0;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inten; tmpCtl = LSWAP(tmpCtl);
  UNLOCK_TSI;
  if(tmpCtl == -1)
    {
      printf("%s: ERROR TEMPE_INTEN read failed.", __func__);
      return ERROR;
    }

  /* Check if BERR IRQ is enabled in INTEN */
  if(tmpCtl&TSI148_LCSR_INTEN_VERREN)
    inten_enabled=1;
  else
    inten_enabled=0;

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inteo; tmpCtl = LSWAP(tmpCtl);
  UNLOCK_TSI;
  if(tmpCtl == -1)
    {
      printf("%s: ERROR: TEMPE_INTEO read failed.", __func__);
      return ERROR;
    }

  /* Check if BERR IRQ is enabled in INTEO */
  if(tmpCtl&TSI148_LCSR_INTEO_VERREO)
    inteo_enabled=1;
  else
    inteo_enabled=0;

  if(inten_enabled != inteo_enabled)
    {
      printf("%s: ERROR: TEMPE_INTEN != TEMPE_INTEO (%d != %d)",
	     __func__, inten_enabled, inteo_enabled);
      return ERROR;
    }

  return inten_enabled;
}

/*!
  Set the status of the IRQ response setting to BERR.

  \see jlabgefDisableBERRIrq(int pflag)
  \see jlabgefEnableBERRIrq(int pflag)

  @param enable 1 to enable, otherwise disable

  @return 1 if successful, otherwise ERROR
*/
int
jlabTsi148SetBERRIrq(int enable)
{
  unsigned int tmpCtl;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inten; tmpCtl = LSWAP(tmpCtl);

  if(tmpCtl == -1)
    {
      printf("%s: (TEMPE_INTEN) read failed.", __func__);
      UNLOCK_TSI;
      return -1;
    }

  if(enable)
    tmpCtl  |= TSI148_LCSR_INTEN_VERREN;
  else
    tmpCtl  &= ~TSI148_LCSR_INTEN_VERREN;

  pTempe->lcsr.inten = LSWAP(tmpCtl);
  pTempe->lcsr.inteo = LSWAP(tmpCtl);
  UNLOCK_TSI;

  return OK;
}


/*!
  Routine to clear any VME Exception that is currently flagged on the VME Bridge Chip

  @param pflag
  - 1 to turn on verbosity
  - 0 to disable verbosity.
*/
int
jlabTsi148ClearException(int pflag)
{
  /* check the VME Exception Attribute... clear it, and put out a warning */
  volatile unsigned int vmeExceptionAttribute=0;
  unsigned int veal=0;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  vmeExceptionAttribute = pTempe->lcsr.veat;
  vmeExceptionAttribute = LSWAP(vmeExceptionAttribute);

  if(vmeExceptionAttribute & TSI148_LCSR_VEAT_VES)
    {
      if(pflag==1)
	{
	  veal = pTempe->lcsr.veal; veal = LSWAP(veal);
	  printf("%s: Clearing VME Exception (0x%x) at VME address 0x%x\n",
		 __func__,
		 vmeExceptionAttribute, veal);
	}
      pTempe->lcsr.veat = LSWAP(TSI148_LCSR_VEAT_VESCL);
    }
  UNLOCK_TSI;

  return OK;
}

/*!
  Clear any Bus Error exceptions, if they exist
*/
int
jlabTsi148ClearBERR()
{
  volatile unsigned int vmeExceptionAttribute=0;
  unsigned int veal;
  int rval = 0;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  /* check the VME Exception Attribute... clear it, and put out a warning */
  LOCK_TSI;
  vmeExceptionAttribute = pTempe->lcsr.veat;
  vmeExceptionAttribute = LSWAP(vmeExceptionAttribute);

  if( (vmeExceptionAttribute & TSI148_LCSR_VEAT_VES) &&
      ((vmeExceptionAttribute & TSI148_LCSR_VEAT_BERR) ||
       (vmeExceptionAttribute & TSI148_LCSR_VEAT_2EST)) )
    {
      veal = pTempe->lcsr.veal; veal = LSWAP(veal);
      if(!vmeQuietFlag)
	{
	  printf("%s: Clearing VME BERR/2eST (0x%x) at VME address 0x%x\n",
		 __func__,
		 vmeExceptionAttribute,
		 veal);
	}
      pTempe->lcsr.veat = LSWAP(TSI148_LCSR_VEAT_VESCL);
      rval = 1;
    }
  UNLOCK_TSI;

  return rval;
}


/*!
  Routine to change the address modifier of the A24 Outbound VME Window.
  The A24 Window must be opened, prior to calling this routine.

  @param addr_mod Address modifier to be used.  If 0, the default (0x39) will be used.

  @return 0, if successful. -1, otherwise
*/

int
jlabTsi148SetA24AM(int addr_mod)
{
  int iwin, done=0;
  unsigned int amode, otat, AM_OTAT, enabled;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  if(a24_window==NULL)
    {
      printf("%s: ERROR: A24 Window is not open.  Unable to change AM.\n",
	     __func__);
      return -1;
    }

  if( (addr_mod<0x10 || addr_mod>0x1F) && (addr_mod !=0) )
    {
      printf("%s: ERROR: Invalid AM code (0x%x).  Must be 0x10 - 0x1F).",
	     __func__,addr_mod);
    }

  if(addr_mod == 0)
    addr_mod = 0x39;

  /* Translate the AM code to the bits required in the OTAT reg */
  if(addr_mod == 0x39)
    {
      amode = TSI148_LCSR_OTAT_AMODE_A24;
    }
  else
    {
      /* lower 4 bits */
      amode = (1<<3) | ((addr_mod & 0xF)>>2);
      /* bit 4 and 5 */
      amode |= ( (addr_mod&0X3)<<4 );
    }

  /* Loop through the windows and find the one we want (A24) */
  LOCK_TSI;
  for(iwin=0; iwin<8; iwin++)
    {
      otat = pTempe->lcsr.outboundTranslation[iwin].otat;
      otat = LSWAP(otat);

      enabled = otat & TSI148_LCSR_OTAT_EN;
      AM_OTAT = otat & TSI148_LCSR_OTAT_AMODE_M;

      if(enabled)
	{
	  if( (AM_OTAT == TSI148_LCSR_OTAT_AMODE_A24) ||
	      ((AM_OTAT & (2<<2)) == (2<<2) ) )
	    {
	      pTempe->lcsr.outboundTranslation[iwin].otat =
		LSWAP( (otat &
			~(TSI148_LCSR_OTAT_SUP | TSI148_LCSR_OTAT_PGM | TSI148_LCSR_OTAT_AMODE_M) )
		       | amode);
	      done=1;
	    }
	}

    }
  UNLOCK_TSI;

  if(done==0)
    {
      printf("%s: ERROR: Unable to find an open A24 window.\n",
	     __func__);
      return -1;
    }

  if(!vmeQuietFlag)
    printf("%s: INFO: A24 Window AM changed to 0x%x\n",
	   __func__, addr_mod);

  return 0;
}

/*!
  Routine to initialize the Tempe DMA Interface

  @param addrType
  Address Type of the data source
  - 0: A16
  - 1: A24
  - 2: A32
  @param dataType
  Data type of the data source
  - 0: D16
  - 1: D32
  - 2: BLT
  - 3: MBLT
  - 4: 2eVME
  - 5: 2eSST
  @param sstMode
  2eSST transfer rate.  If 2eSST is set for dataType (otherwise, ignored):
  - 0: 160 MB/s
  - 1: 267 MB/s
  - 2: 320 MB/s

  @return 0, if successful. -1, otherwise.
*/
int
jlabTsi148DmaConfig(unsigned int addrType, unsigned int dataType, unsigned int sstMode)
{

  /* Some default attributes */
  dmaDescSample.dsat  = TSI148_LCSR_DDAT_SUP; /* Supervisory Mode */
  dmaDescSample.dsat |= TSI148_LCSR_DDAT_TYP_VME; /* VME Source */

  switch(addrType)
    {
    case 0: /* A16 */
      dmaDescSample.dsat |= TSI148_LCSR_DSAT_AMODE_A16;
      break;
    case 1: /* A24 */
      dmaDescSample.dsat |= TSI148_LCSR_DSAT_AMODE_A24;
      break;
    case 2: /* A32 */
      dmaDescSample.dsat |= TSI148_LCSR_DSAT_AMODE_A32;
      break;
    default:
      printf("%s: ERROR: Address mode addrType=%d is not supported\n",
	     __func__, addrType);
      return ERROR;
    }

  switch(dataType)
    {
    case 0: /* D16 - SCT */
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_DBW_16;
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_TM_SCT;
      break;
    case 1: /* D32 - SCT */
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_DBW_32;
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_TM_SCT;
      break;
    case 2: /* BLK32 */
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_DBW_32;
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_TM_BLT;
      break;
    case 3: /* MBLK */
      /* 64bit Data Width by default, set 32bit for misaligned SCT transfers */
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_DBW_32;
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_TM_MBLT;
      break;
    case 4: /* 2eVME */
      /* 64bit Data Width by default, set 32bit for misaligned SCT transfers */
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_DBW_32;
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_TM_2eVME;
      break;
    case 5: /* 2eSST */
      /* 64bit Data Width by default, set 32bit for misaligned SCT transfers */
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_DBW_32;
      dmaDescSample.dsat |= TSI148_LCSR_DDAT_TM_2eSST;
      switch(sstMode)
	{
	case 0: /* SST160 */
	  dmaDescSample.dsat |= TSI148_LCSR_DSAT_2eSSTM_160;
	  break;
	case 1: /* SST267 */
	  dmaDescSample.dsat |= TSI148_LCSR_DSAT_2eSSTM_267;
	  break;
	case 2: /* SST320 */
	  dmaDescSample.dsat |= TSI148_LCSR_DSAT_2eSSTM_320;
	  break;
	default: /* SST160 */
	  dmaDescSample.dsat |= TSI148_LCSR_DSAT_2eSSTM_160;
	}
      break;
    default:
      printf("%s: ERROR: Data type dataType=%d is not supported\n",
	     __func__, dataType);
      return ERROR;
    }

  return OK;

}


/*!
  Routine to initiate a DMA

  @param locAdrs Destination Userspace address
  @param vmeAdrs VME Bus source address
  @param size    Maximum size of the DMA in bytes

  @return 0, if successful. -1, otherwise.
*/
int
jlabTsi148DmaSend(unsigned long locAdrs, unsigned int vmeAdrs, int size)
{
  long offset;
  int tmp_dctl=0;
  int channel=0; /* hard-coded for now */
  int nbytes=0;
  int isValid = 0;

  dma_timer[0] = rdtsc();

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  if(!the_event)
    {
      printf("%s: ERROR: the_event pointer is invalid!\n",__func__);
      return ERROR;
    }

  if(!the_event->physMemBase)
    {
      printf("%s: ERROR: DMA Physical Memory has an invalid base address (0x%08x)",
	     __func__,
	     (unsigned int)the_event->physMemBase);
      return ERROR;
    }

  /* Clear any previous exception */
  jlabTsi148ClearException(1);

  /* Local addresses (in Userspace) need to be translated to the physical memory */
  /* Here's the offset between current buffer position and the event head */
  offset = locAdrs - the_event->partBaseAdr;
  dmaDescSample.ddal = (the_event->physMemBase + offset) & 0xFFFFFFFF;

#ifdef ARCH_x86_64
  dmaDescSample.ddau = (the_event->physMemBase + offset)>>32;
#else
  dmaDescSample.ddau = 0;
#endif

  /* Make sure the destination address falls within the range of
     allocated Physical Memory */
  isValid = dmaPMemIsValid(the_event->physMemBase + offset);

  if(isValid == 0)
    {
      printf("%s: ERROR: Invalid DMA Destination Address (0x%lx).\n",
	     __func__, (the_event->physMemBase + offset));
      printf("    Does not fall within allocated range.\n");
      return ERROR;
    }

#ifdef DEBUG
  printf("locAdrs     = 0x%lx   partBaseAdr = 0x%lx\n",
	 locAdrs, the_event->partBaseAdr);
  printf("physMemBase = 0x%lx   offset      = 0x%lx\n\n",
	 the_event->physMemBase,offset);
#endif

  /* Source (VME) address */
  dmaDescSample.dsal = vmeAdrs;

  /* Calculate the bytes left in this buffer, leaving space for 2KB (0x800) flush */
  nbytes = the_event->part->size - sizeof(DMANODE);
  nbytes = (nbytes - ((long)dma_dabufp - (long)&(the_event->length))) - 0x800;

  /* Make sure nbytes is realistic */
  if(nbytes<0)
    {
      printf("%s: ERROR: Space left in buffer is less than zero (%d). Quitting\n",
	     __func__,nbytes);
      return ERROR;
    }

  /* Check the specified "size" vs. size left in buffer */
  /* ... if size==0, just use the space left in the buffer */
  if(size>nbytes)
    {
      printf("%s: WARN: Specified number of DMA bytes (%d) is greater than \n",
	     __func__,
	     size);
      printf("\tthe space left in the buffer (%d).  Using %d\n",nbytes,nbytes);
    }
  else if( (size !=0) && (size<=nbytes) )
    {
      nbytes = size;
    }

  dmaDescSample.dcnt = nbytes;

  /* Some defaults hardcoded for the DMA Control Register */
  tmp_dctl  = 0x00830000; /* Set Direct mode, VFAR and PFAR by default */
  tmp_dctl |= (TSI148_LCSR_DCTL_VBKS_2048 |
			 TSI148_LCSR_DCTL_PBKS_2048); /* 2048 VME/PCI Block Size */
  tmp_dctl |= (TSI148_LCSR_DCTL_VBOT_0 |
			 TSI148_LCSR_DCTL_PBOT_0); /* 0us VME/PCI Back-off */

  dma_timer[1] = rdtsc();

  LOCK_TSI;
  /* Source Address - Won't support 64bit addressing */
  pTempe->lcsr.dma[channel].dsal = LSWAP(dmaDescSample.dsal);
  /*   pTempe->lcsr.dma[channel].dsau = 0x0; */

  /* Destination Address */
  pTempe->lcsr.dma[channel].ddal = LSWAP(dmaDescSample.ddal);
  if(dmaDescSample.ddal>0)
    pTempe->lcsr.dma[channel].ddau = LSWAP(dmaDescSample.ddau);

  /* Source attributes */
  pTempe->lcsr.dma[channel].dsat = LSWAP(dmaDescSample.dsat);

  /*   pTempe->lcsr.dma[channel].ddat = 0x0; */

  /* Data count */
  pTempe->lcsr.dma[channel].dcnt = LSWAP(dmaDescSample.dcnt);

  /*   pTempe->lcsr.dma[channel].ddbs = 0x0; */

  pTempe->lcsr.dma[channel].dctl = LSWAP(tmp_dctl);

  /* GO! */
  tmp_dctl |=  TSI148_LCSR_DCTL_DGO;
  pTempe->lcsr.dma[channel].dctl = LSWAP(tmp_dctl);

  UNLOCK_TSI;

#ifdef DEBUG
  jlabTsi148ReadDMARegs();
#endif

  dma_timer[2] = rdtsc();

  return OK;
}

/*!
  Routine to initiate a DMA using physical memory address (instead of userspace address)

  @param physAdrs Destination Physical Memory Address
  @param vmeAdrs VME Bus source address
  @param size    Maximum size of the DMA in bytes

  @return 0, if successful. -1, otherwise.
*/
int
jlabTsi148DmaSendPhys(unsigned long physAdrs, unsigned int vmeAdrs, int size)
{
  int tmp_dctl=0;
  int channel=0; /* hard-coded for now */
  int nbytes=0;

  dma_timer[0] = rdtsc();

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  if(!physAdrs)
    {
      printf("%s: ERROR: DMA Physical Memory has an invalid base address (0x%lx)",
	     __func__,
	     (unsigned long)physAdrs);
      return ERROR;
    }

  /* Clear any previous exception */
  jlabTsi148ClearException(1);

  dmaDescSample.ddal = (physAdrs) & 0xFFFFFFFF;
#ifdef ARCH_x86_64
  dmaDescSample.ddau = (physAdrs)>>32;
#else
  dmaDescSample.ddau = 0;
#endif

  /* Source (VME) address */
  dmaDescSample.dsal = vmeAdrs;

  nbytes = size;

  /* Make sure nbytes is realistic */
  if(nbytes<0)
    {
      printf("%s: ERROR: Number of bytes (size) is less than zero (%d). Quitting\n",
	     __func__,nbytes);
      return ERROR;
    }

  dmaDescSample.dcnt = nbytes;

  /* Some defaults hardcoded for the DMA Control Register */
  tmp_dctl  = 0x00830000; /* Set Direct mode, VFAR and PFAR by default */
  tmp_dctl |= (TSI148_LCSR_DCTL_VBKS_2048 |
			 TSI148_LCSR_DCTL_PBKS_2048); /* 2048 VME/PCI Block Size */
  tmp_dctl |= (TSI148_LCSR_DCTL_VBOT_0 |
			 TSI148_LCSR_DCTL_PBOT_0); /* 0us VME/PCI Back-off */

  dma_timer[1] = rdtsc();

  LOCK_TSI;
  pTempe->lcsr.dma[channel].dsal = LSWAP(dmaDescSample.dsal);
  /*   pTempe->lcsr.dma[channel].dsau = 0x0; */
  pTempe->lcsr.dma[channel].ddal = LSWAP(dmaDescSample.ddal);
  if(dmaDescSample.ddal>0)
    pTempe->lcsr.dma[channel].ddau = LSWAP(dmaDescSample.ddau);
  pTempe->lcsr.dma[channel].dsat = LSWAP(dmaDescSample.dsat);
  /*   pTempe->lcsr.dma[channel].ddat = 0x0; */
  pTempe->lcsr.dma[channel].dcnt = LSWAP(dmaDescSample.dcnt);
  /*   pTempe->lcsr.dma[channel].ddbs = 0x0; */

  pTempe->lcsr.dma[channel].dctl = LSWAP(tmp_dctl);

  /* GO! */
  tmp_dctl |=  TSI148_LCSR_DCTL_DGO;
  pTempe->lcsr.dma[channel].dctl = LSWAP(tmp_dctl);

  UNLOCK_TSI;

#ifdef DEBUG
  jlabTsi148ReadDMARegs();
#endif

  dma_timer[2] = rdtsc();

  return OK;
}

/*!
  Routine to poll for a DMA Completion or timeout.

  @return Number of bytes transferred, if successful, -1, otherwise.
*/
int
jlabTsi148DmaDone()
{
  unsigned int val=0,ii=0;
  int channel=0;
  unsigned int timeout=10000000;
  unsigned int vmeExceptionAttribute=0;
  unsigned int veal;
  unsigned int dcnt;
  int status=OK;

  dmaBerrStatus=0;

  dma_timer[3] = rdtsc();

  if(dmaLLInvalid) /* Set in jlabgefDmaSetupLL */
    {
      return ERROR;
    }

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;

  val = pTempe->lcsr.dma[channel].dsta;
  val = LSWAP(val);
  while( ((val&0x1e000000)==0) && (ii<timeout) )
    {
      val = pTempe->lcsr.dma[channel].dsta;
      val = LSWAP(val);
      ii++;
    }

  if(ii>=timeout)
    {
      printf("%s: DMA timed-out. DMA Status Register = 0x%08x\n",
	     __func__,val);
      UNLOCK_TSI;
      jlabTsi148ReadDMARegs();
      LOCK_TSI;
      status = ERROR;
    }

  dma_timer[4] = rdtsc();

  /* check the VME Exception Attribute...
     clear it if the DMA ended on BERR or 2eST (2e Slave Termination) */
  vmeExceptionAttribute = pTempe->lcsr.veat;
  vmeExceptionAttribute = LSWAP(vmeExceptionAttribute);
  if( (vmeExceptionAttribute & TSI148_LCSR_VEAT_VES) &&
      ( (vmeExceptionAttribute & TSI148_LCSR_VEAT_BERR) ||
        (vmeExceptionAttribute & TSI148_LCSR_VEAT_2EST) )
      )
    {
      dmaBerrStatus=1;
      pTempe->lcsr.veat = LSWAP(TSI148_LCSR_VEAT_VESCL);

      if(status != ERROR)
	{
	  /* Read where the BERR occurred */
	  veal = pTempe->lcsr.veal;
	  veal = LSWAP(veal);
	  /* Return value is the difference between VEAL and the Starting Address */
	  status = veal - dmaDescSample.dsal;
	  if(status<0)
	    {
	      printf("%s: ERROR: VME Exception Address < DMA Source Address (0x%08x < 0x%08x)\n",
		     __func__,
		     veal,dmaDescSample.dsal);
	      status = ERROR;
	    }
	}
    }
  else
    {
      dmaBerrStatus=0;
      /* DMA ended on DMA Count (No BERR), return original byte count */
      if(status != ERROR)
	{
	  status = dmaDescSample.dcnt;
	  /* check and make sure 0 bytes are left in the DMA Count register */
	  dcnt = pTempe->lcsr.dma[channel].dcnt;
	  dcnt = LSWAP(dcnt);
	  if(dcnt != 0)
	    {
	      printf("%s: ERROR: DMA terminated on master byte count,",__func__);
	      printf("    however DCNT (%d) != 0 \n", dcnt);
	    }
	}
    }

  /* If we started a linked-list transaction, dmaLL_totalwords should be non-zero */
  if(dmaLL_totalwords>0)
    {
      status = dmaLL_totalwords<<2;
      dmaLL_totalwords=0;
    }

  UNLOCK_TSI;

  dma_timer[5] = rdtsc();

  return status;

}

int
jlabTsi148GetBerrStatus()
{
  return dmaBerrStatus;
}


/*!
  Routine to setup a DMA Linked List

  @param locAddrBase Userspace Destination address
  @param *vmeAddr    Array of VME Source addresses
  @param *dmaSize    Array of sizes of each DMA, in bytes
  @param numt        Nuumber of DMA (also size of the arrays)

  @return 0, if successful.  -1, otherwise.
*/
int
jlabTsi148DmaSetupLL(unsigned long locAddrBase, unsigned int *vmeAddr,
		  unsigned int *dmaSize, unsigned int numt)
{
  unsigned long localAddr[TSI148_DMA_MAX_LL];
  unsigned long offset=0;
  unsigned int total_words=0;
  unsigned int size_incr=0; /* "current" size of the entire DMA */
  int isValid = 0, idma;

  /* check to see if the linked list structure array has been init'd */
  if (dmaDescList == NULL)
    {
      printf("%s: ERROR: dmaDescList not initialized\n",__func__);
      return ERROR;
    }

  dmaLL_totalwords=0;
  dmaLLInvalid = 0;

  /* Calculate localAddr array values based on buffer partition base, current location
     of dma_dabufp and the dma size for each linked list member */
  offset = locAddrBase - the_event->partBaseAdr;
  localAddr[0] = the_event->physMemBase + offset;

  total_words = dmaSize[0]>>2;

  for (idma=0;idma<numt;idma++)
    {
      if(idma!=0)
	{
	  localAddr[idma] = localAddr[idma-1] + dmaSize[idma-1];
	  total_words += (dmaSize[idma]>>2);
	}

      /* Set the vme (source) address */
      dmaDescList[idma].dsau = 0x0;
      dmaDescList[idma].dsal = LSWAP(vmeAddr[idma]);

      /* Make sure the destination address falls within the range of
	 allocated Physical Memory */
      isValid = dmaPMemIsValid(localAddr[idma]);

      if(isValid == 0)
	{
	  printf("%s: ERROR: Invalid DMA Destination Address (0x%lx).\n",
		 __func__, localAddr[idma]);
	  printf("    Does not fall within allocated range.\n");
	  dmaLLInvalid = 1;
	  return ERROR;
	}

      /* Set the local (destination) address */
#ifdef ARCH_x86_64
      dmaDescList[idma].ddau = LSWAP(localAddr[idma] >> 32);
#else
      dmaDescList[idma].ddau = 0x0;
#endif
      dmaDescList[idma].ddal = LSWAP(localAddr[idma] & 0xFFFFFFFF);

      /* Set the byte count */
      dmaDescList[idma].dcnt = LSWAP(dmaSize[idma]);
      size_incr += dmaSize[idma];

      /* Set the address of the next descriptor */
      dmaDescList[idma].dnlau = 0x0;
      if( idma < (numt-1) )
	{
	  dmaDescList[idma].dnlal =
	    LSWAP(dmaListAdr + (idma+1)*sizeof(tsi148DmaDescriptor_t));
	}
      else
	{
	  dmaDescList[idma].dnlal = LSWAP(TSI148_LCSR_DNLAL_LLA); /* Last Link Address (LLA) */
	}

      /* Still need to set the Source & Destination Attributes, 2eSST Broadcast select */
      /* These we'll assume to be 0x0 for now */
      dmaDescList[idma].ddat = 0x0;
      dmaDescList[idma].ddbs = 0x0;

      /* Get the DSAT from how was made from jlabgefDmaConfig */
      dmaDescList[idma].dsat = LSWAP(dmaDescSample.dsat);

/* #define DEBUGLL */
#ifdef DEBUGLL
      printf("dmaDescList[%d]:\n",idma);
      printf("locAddrBase = 0x%08x\n",(unsigned int)locAddrBase);
#ifdef ARCH_x86_64
      printf("  DSAL = 0x%08x  DDAU = 0x%08x  DDAL = 0x%08x  DCNT = 0x%08x\n",
	     LSWAP(dmaDescList[idma].dsal),
	     LSWAP(dmaDescList[idma].ddau),
	     LSWAP(dmaDescList[idma].ddal),
	     LSWAP(dmaDescList[idma].dcnt));
#else
      printf("  DSAL = 0x%08x  DDAL = 0x%08x  DCNT = 0x%08x\n",
	     LSWAP(dmaDescList[idma].dsal),
	     LSWAP(dmaDescList[idma].ddal),
	     LSWAP(dmaDescList[idma].dcnt));
#endif
     printf(" DNLAL = 0x%08x  DSAT = 0x%08x\n",
	    LSWAP(dmaDescList[idma].dnlal),
	    LSWAP(dmaDescList[idma].dsat));
#endif

    }
  dmaLL_totalwords=total_words;

  return OK;
}

/*!
  Routine to initiate a linked list DMA that was setup with vmeDmaSetupLL()
  @return 0, if successful.  -1, otherwise.
*/
int
jlabTsi148DmaSendLL()
{
  int channel=0;
  unsigned int dctl = 0x0;

  if(dmaLLInvalid)  /* Set in jlabgefDmaSetupLL */
    {
      return ERROR;
    }

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  /* Write the address of the first entry in the linked list to the TEMPE regs */
  pTempe->lcsr.dma[channel].dnlal = LSWAP(dmaListAdr);
  pTempe->lcsr.dma[channel].dnlau = 0x0;

  /* Set the CTL reg */
  dctl  = 0x00030000; /* Set Linked List mode, VFAR and PFAR by default */
  dctl |= (TSI148_LCSR_DCTL_VBKS_2048 |
			 TSI148_LCSR_DCTL_PBKS_2048); /* 2048 VME/PCI Block Size */
  dctl |= (TSI148_LCSR_DCTL_VBOT_0 |
			 TSI148_LCSR_DCTL_PBOT_0); /* 0us VME/PCI Back-off */

  pTempe->lcsr.dma[channel].dctl = LSWAP(dctl);


  /* Set the "DGO" Bit */
  dctl |= 0x02000000;

  pTempe->lcsr.dma[channel].dctl = LSWAP(dctl);
  UNLOCK_TSI;

  return OK;
}

/*!
  Routine to print the Tempe DMA Registers
*/
void
jlabTsi148ReadDMARegs()
{
  int channel=0;
  unsigned int dsal, dsau, ddal, ddau;
  unsigned int dsat, ddat, dcnt, ddbs;
  unsigned int dnlal, dnlau;
  unsigned int dctl;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return;
    }

  printf("\n%s:\n",__func__);

  LOCK_TSI;
  dsal = pTempe->lcsr.dma[channel].dsal;
  dsau = pTempe->lcsr.dma[channel].dsau;
  ddal = pTempe->lcsr.dma[channel].ddal;
  ddau = pTempe->lcsr.dma[channel].ddau;
  dsat = pTempe->lcsr.dma[channel].dsat;
  ddat = pTempe->lcsr.dma[channel].ddat;
  dcnt = pTempe->lcsr.dma[channel].dcnt;
  ddbs = pTempe->lcsr.dma[channel].ddbs;

  dnlal = pTempe->lcsr.dma[channel].dnlal;
  dnlau = pTempe->lcsr.dma[channel].dnlau;

  dctl = pTempe->lcsr.dma[channel].dctl;
  UNLOCK_TSI;

  printf(" dsal = 0x%08x\n",LSWAP(dsal));
  printf(" dsau = 0x%08x\n",LSWAP(dsau));
  printf(" ddal = 0x%08x\n",LSWAP(ddal));
  printf(" ddau = 0x%08x\n",LSWAP(ddau));
  printf(" dsat = 0x%08x\n",LSWAP(dsat));
  printf(" ddat = 0x%08x\n",LSWAP(ddat));
  printf(" dcnt = 0x%08x\n",LSWAP(dcnt));
  printf(" ddbs = 0x%08x\n",LSWAP(ddbs));

  printf("dnlal = 0x%08x\n",LSWAP(dnlal));
  printf("dnlau = 0x%08x\n",LSWAP(dnlau));


  printf(" dctl = 0x%08x\n",LSWAP(dctl));

  printf("\n");
}
