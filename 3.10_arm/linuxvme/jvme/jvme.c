/*----------------------------------------------------------------------------*
 *  Copyright (c) 2010        Southeastern Universities Research Association, *
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
 *     A front for the stuff that actually does the work.
 *      APIs are switched from the Makefile
 *
 *----------------------------------------------------------------------------*/

#ifdef ARCH_armv7l
#include "jvme.h"
#else
/*! This is required for using mutex robust-ness */
#define _GNU_SOURCE

#ifdef VXWORKS
#include <vxWorks.h>
#include <sysLib.h>
#include <logLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <iv.h>
#include <semLib.h>
#include <vxLib.h>
#else
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#endif
#include <stdio.h>
#include "jvme.h"

#ifdef GEFANUC
#include "jlabgef.h"
#include "jlabgefDMA.h"
#include "jlabTsi148.h"
#include "jlabUniverseII.h"
#endif

#ifdef VXWORKS_UNIV
#include "universeDma.h"
#endif

#ifdef VXWORKS
IMPORT STATUS intDisconnect(int);
/* usrTempeDma.c */
extern unsigned int sysTempeSetAM(unsigned int owid, unsigned int amcode);
extern void usrVmeDmaConfig1(UINT32 busattr);
extern STATUS sysVmeDmaSend (UINT32 locAdrs, UINT32 vmeAdrs, int size, BOOL toVme);
extern void usrVmeDmaConfig1(UINT32 busattr);
extern int sysVmeDmaDone(int pcnt, int pflag);
extern STATUS usrVmeDmaConfig(UINT32 addrType, UINT32 dataType, UINT32 sstMode);
#endif

/* global variables */
unsigned int vmeQuietFlag = 1;
int vmeDebugMode=0;
FILE *fDebugMode=NULL;
pid_t processID;

/* VXS Payload Port to VME Slot map */
#define MAX_VME_SLOTS 21    /* This is either 20 or 21 */
static int maxVmeSlots=MAX_VME_SLOTS;
unsigned short PayloadPort21[MAX_VME_SLOTS+1] =
  {
    0,     /* Filler for mythical VME slot 0 */
    0,     /* VME Controller */
    17, 15, 13, 11, 9, 7, 5, 3, 1,
    0,     /* Switch Slot A - SD */
    0,     /* Switch Slot B - CTP/GTP */
    2, 4, 6, 8, 10, 12, 14, 16,
    18     /* VME Slot Furthest to the Right - TI */
  };

unsigned short PayloadPort20[MAX_VME_SLOTS+1] =
  {
    0,     /* Filler for mythical VME slot 0 */
    17, 15, 13, 11, 9, 7, 5, 3, 1,
    0,     /* Switch Slot A - SD */
    0,     /* Switch Slot B - CTP/GTP */
    2, 4, 6, 8, 10, 12, 14, 16,
    18,     /* VME Slot Furthest to the Right - TI */
    0
  };

static int jvmeBridgeType = 0; /* initialized in jlabgefOpenDefaultWindows */

/*!
  Routine to delay a task from executing

  @see usleep(3)

  @param ticks number of ticks to delay task (units: 1 tick = 16.7 ms)

  @return 0 if successful, -1 on error
*/
#ifndef VXWORKS
STATUS
taskDelay(int ticks)
{
  return usleep(16700*ticks);
}

/*!
  Routine to print a formatted message.  Same usage as printf(...)

  @see printf(3)

  @param format Formatted message
  @param ... additional parameters as needed by message
*/
int
logMsg(const char *format, ...)
{
  va_list args;
  int retval;

  va_start(args,format);
  retval = vprintf(format, args);
  va_end(args);

  return retval;
}

/*!
  Routine to return system scaler

  @returns system clock ticks since reset (or rollover)
*/
unsigned long long int
rdtsc(void)
{
  /*    unsigned long long int x; */
  unsigned a, d;

  __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

  return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}
#endif

/*!
  Routine to enable (default) or disable verbose messages in VME API

  @param pflag
  - 0 to disable
  - 1 to enable
*/
void
vmeSetQuietFlag(unsigned int pflag)
{
  if(pflag <= 1)
    vmeQuietFlag = pflag;
  else
    printf("%s: ERROR: invalid argument pflag=%d\n",
	   __func__,pflag);
}

/*!
  Routine to enable (default) or disable VME Debug Mode

  @param pflag
  - >=1 to enable
  - Otherwise to disable
*/
void
vmeSetVMEDebugMode(int enable)
{
  if(enable>=1)
    {
      vmeDebugMode=1;
      printf("%s: Enabled\n",__func__);
    }
  else
    {
      vmeDebugMode=0;
      printf("%s: Disabled\n",__func__);
    }

  if(fDebugMode==NULL)
    fDebugMode = stdout;
}

int
vmeSetVMEDebugModeOutputFilename(char *fOutput)
{
  fDebugMode = fopen(fOutput,"w");
  if(fDebugMode==NULL)
    {
      perror("vmeSetVMEDebugModeOutput");
      return ERROR;
    }

  printf("%s: Output set to %s\n",__func__,fOutput);

  return OK;
}

int
vmeSetVMEDebugModeOutput(int *fOutput)
{
  fDebugMode = (FILE *)fOutput;

  if(fOutput == (int *)stdout)
    printf("%s: Output set to STDOUT\n",__func__);
  else if(fOutput == (int *)stderr)
    printf("%s: Output set to STDERR\n",__func__);
  else
    printf("%s: Output set to FILE\n",__func__);

  return OK;
}

int
vmeSetA32BltWindowWidth(unsigned int size)
{
  int status = OK;

#ifdef GEFANUC
  status = jlabgefSetA32BltWindowWidth(size);
#endif

  return status;
}

int
vmeSetBridgeType(int bridge)
{
  int status = OK;
  switch(bridge)
    {
    case JVME_UNIVII:
    case JVME_TSI148:
      jvmeBridgeType = bridge;
      break;

    default:
      printf("%s: ERROR: Undefined VME Bridge type (%d)\n",
	     __func__, bridge);
      status = ERROR;
    }
  return status;
}

/*!
  Routine to initialize the VME API
  - opens default VME Windows and maps them into Userspace
  - maps VME Bridge Registers into Userspace
  - disables interrupts on VME Bus Errors
  - creates a shared mutex for interrupt/trigger locking
  - calls vmeBusCreateLockShm()

  @return 0, if successful.  An API dependent error code, otherwise.
*/
int
vmeOpenDefaultWindows()
{
  int status = OK;

#ifdef GEFANUC
  if(jlabgefOpenDefaultWindows() != ERROR)
    {
      jlabgefAllocDMATrash(0x200000); /* 2MB default */
      status = OK;
    }
  else
    {
      status = ERROR;
    }
#endif

  return status;
}

/*!
  Routine to cleanup what was initialized by vmeOpenDefaultWindows()

  @return 0, if successful.  An API dependent error code, otherwise.
*/
int
vmeCloseDefaultWindows()
{
  int status = OK;

#ifdef GEFANUC
  status = jlabgefFreeDMATrash();

  status = (int)jlabgefCloseDefaultWindows();
#endif

  if(fDebugMode!=NULL)
    fclose(fDebugMode);

  return status;
}

/*!
  Routine to open a Slave window to the VME A32 space and map it into Userspace

  @param base The base address of the A32 window
  @param size The size of the A32 window

  @return 0 if successful, otherwise.. an API dependent error code

*/
int
vmeOpenSlaveA32(unsigned int base, unsigned int size)
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefOpenSlaveA32(base, size);
#endif

  return status;
}

/*!
  Routine to close and unmap the slave window opened with vmeOpenSlaveA32()

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
vmeCloseA32Slave()
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefCloseA32Slave();
#endif

  return status;
}

/*!
  Routine to read from a register from the VME Bridge Chip

  @param offset Register offset (from VME Bridge base register) from which to read

  @return 32bit word read from register, if successful.  -1, otherwise.
*/
unsigned int
vmeReadRegister(unsigned int offset)
{
  unsigned int rval = 0;

#ifdef GEFANUC
  rval = (unsigned int)jlabgefReadRegister(offset);
#endif

  return rval;
}

/*!
  Routine to write to a register from the VME Bridge Chip

  @param offset Register offset (from VME Bridge base register) from which to read
  @param buffer 32bit word to write to requested register offset

  @return 0, if successful.  -1, otherwise.
*/
int
vmeWriteRegister(unsigned int offset, unsigned int buffer)
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefWriteRegister(offset,buffer);
#endif

  return status;
}

/*!
  Routine to assert SYSRESET on the VME Bus

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
vmeSysReset()
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefSysReset();
#endif

  return status;
}

/*!
  Routine to query the status of interrupts on VME Bus Error

  @return 1 if enabled, 0 if disabled, -1 if error.
*/
int
vmeBERRIrqStatus()
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivGetBERRIrq();
      break;

    case JVME_TSI148:
      status = jlabTsi148GetBERRIrq();
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

  return status;
}

/*!
  Routine to disable interrupts on VME Bus Error

  @return 0 if successful, otherwise ERROR
*/
int
vmeDisableBERRIrq(int pflag)
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivSetBERRIrq(0);
      break;

    case JVME_TSI148:
      status = jlabTsi148SetBERRIrq(0);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }

  if(!vmeQuietFlag)
    if((pflag==1) && (status == OK))
      printf("%s: VME Bus Error IRQ Disabled\n", __func__);

  return status;
#endif

  return status;
}

/*!
  Routine to enable interrupts on VME Bus Error

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
vmeEnableBERRIrq(int pflag)
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivSetBERRIrq(1);
      break;

    case JVME_TSI148:
      status = jlabTsi148SetBERRIrq(1);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }

  if(!vmeQuietFlag)
    if((pflag==1) && (status == OK))
      printf("%s: VME Bus Error IRQ Enabled\n", __func__);

  return status;
#endif

  return status;
}

/*!
  Routine to probe a VME Address for a VME Bus Error

  @param *addr address to be probed
  @param size  size (1, 2, or 4) of address to read (in bytes)
  @param *rval where to return value

  @return 0, if successful. -1, otherwise.
*/
int
vmeMemProbe(char *addr, int size, char *rval)
{
  int status = OK;

#ifdef GEFANUC
  unsigned int lval; unsigned short sval; char cval;
  int irqOnBerr=0;
  int exStat = 0;

  /* If IRQ on BERR is enabled, disable it... enable it again at the end */
  irqOnBerr = vmeBERRIrqStatus();

  if(irqOnBerr==1)
    {
      vmeDisableBERRIrq(0);
    }

  /* Clear the Exception register, before trying to read */
  vmeClearException(0);

  /* Perform a test read */
  switch(size)
    {
    case 4:
      memcpy(&lval,(void *)addr,sizeof(unsigned int));
      lval = LSWAP((unsigned int)lval);
      memcpy(rval,&lval,sizeof(unsigned int));
      break;
    case 2:
      memcpy(&sval,(void *)addr,sizeof(unsigned short));
      sval = SSWAP((unsigned short)sval);
      memcpy(rval,&sval,sizeof(unsigned short));
      break;
    case 1:
      memcpy(&cval,(void *)addr,sizeof(char));
      memcpy(rval,&cval,sizeof(char));
      break;
    default:
      printf("%s: ERROR: Invalid size %d",__func__,size);
      return ERROR;
    }

  /* Check and clear the Exception register for a VME Bus Error */
  exStat = vmeClearException(0);
  if(exStat != 0)
    status = ERROR;

  if(irqOnBerr==1)
    {
      vmeEnableBERRIrq(0);
    }

  return status;

#endif

#ifdef VXWORKS
  status = vxMemProbe(addr,0,size,rval);
#endif

  return status;
}

/*!
  Routine to clear any VME Exception that is currently flagged on the VME Bridge Chip

  @param pflag
  - 1 to turn on verbosity
  - 0 to disable verbosity.
*/
int
vmeClearException(int pflag)
{
  int status = OK;
#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivClearException(pflag | !vmeQuietFlag);
      break;

    case JVME_TSI148:
      status = jlabTsi148ClearException(pflag | !vmeQuietFlag);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif
  return status;
}

/*!
  Routine to connect a routine to a VME Bus Interrupt

  @param vector  interrupt vector to attach to
  @param level   VME Bus interrupt level
  @param routine routine to be called
  @param arg     argument to be passed to the routine

  @return 0, if successful. -1, otherwise.
*/
int
vmeIntConnect(unsigned int vector, unsigned int level, VOIDFUNCPTR routine, unsigned int arg)
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefIntConnect(vector, level, routine, arg);
#endif

#ifdef VXWORKS
  status = intConnect((INUM_TO_IVEC(vector)),routine,arg);
#endif

  return status;
}

/*!
  Routine to release the routine attached with vmeIntConnect()

  @param level
    - VME Bus Interrupt level (Linux)
    - VME Bus Interrupt vector (vxWorks)

  @return 0, if successful. -1, otherwise.
*/
int
vmeIntDisconnect(unsigned int level)
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefIntDisconnect(level);
#endif

#ifdef VXWORKS
  status = intDisconnect(level);
#endif

  return status;

}

/*!
  Routine to convert a a VME Bus address to a Userspace Address

  @param vmeAdrsSpace Bus address space in whihc vmeBusAdrs resides
  @param *vmeBusAdrs  Bus address to convert
  @param **pLocalAdrs   Where to return Userspace address

  @return 0, if successful. -1, otherwise.
*/
int
vmeBusToLocalAdrs(int vmeAdrsSpace, char *vmeBusAdrs, char **pLocalAdrs)
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefVmeBusToLocalAdrs(vmeAdrsSpace, vmeBusAdrs, pLocalAdrs);
#endif

#ifdef VXWORKS
  status = (int)sysBusToLocalAdrs(vmeAdrsSpace, vmeBusAdrs, pLocalAdrs);
#endif

  return status;

}

/*!
  Routine to convert a Userspace Address to a VME Bus address

  @param localAdrs  Local (userspace) address to convert
  @param *vmeAdrs   Where to return VME address
  @param *amCode    Where to return address modifier

  @return 0, if successful. -1, otherwise.
*/
int
vmeLocalToVmeAdrs(unsigned long localAdrs, unsigned int *vmeAdrs, unsigned short *amCode)
{
  int status = OK;

#ifdef GEFANUC
  status = (int)jlabgefLocalToVmeAdrs(localAdrs,vmeAdrs,amCode);
#endif

#ifdef VXWORKS
  printf("%s: ERROR: This routine not supported in VXWORKS\n",__func__);
  status=ERROR;
#endif

  return status;
}
/*!
  Routine to enable/disable debug flags set in the VME Bridge Kernel Driver

  @param flags API dependent flags to toggle specific debug levels and messages

  @return 0, if successful. -1, otherwise.
*/
int
vmeSetDebugFlags(int flags)
{
  int status=OK;

#ifdef GEFANUC
  status = jlabgefSetDebugFlags(flags);
#endif

  return status;
}



/*!
  Routine to change the address modifier of the A24 Outbound VME Window.
  The A24 Window must be opened, prior to calling this routine.

  @param addr_mod Address modifier to be used.  If 0, the default (0x39) will be used.

  @return 0, if successful. -1, otherwise
*/

int
vmeSetA24AM(int addr_mod)
{
  int status = OK;

#ifdef VXWORKS
 #ifdef VXWORKS_UNIV
  if(addr_mod>0)
    {
      sysUnivSetUserAM(addr_mod,0);
      status = sysUnivSetLSI(2,6);
    }
  else
    {
      status = sysUnivSetLSI(2,1);
    }
 #else
  status = sysTempeSetAM(2, (unsigned int)addr_mod);
 #endif

#else /* GEFANUC */
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivSetA24AM(addr_mod);
      break;

    case JVME_TSI148:
      status = jlabTsi148SetA24AM(addr_mod);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

  return status;
}

/* DMA SUBROUTINES */

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
vmeDmaConfig(unsigned int addrType, unsigned int dataType, unsigned int sstMode)
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivDmaConfig(addrType,dataType);
      break;

    case JVME_TSI148:
      status = jlabTsi148DmaConfig(addrType,dataType,sstMode);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

#ifdef VXWORKS
#ifdef VXWORKS_UNIV
  status = sysUnivDmaConfig(addrType, dataType);
#else /* tsi148 */
  status = usrVmeDmaConfig(addrType,dataType,sstMode);
#endif
#endif

  return status;
}

/*!
  Routine to initiate a DMA

  @param locAdrs Destination Userspace address
  @param vmeAdrs VME Bus source address
  @param size    Maximum size of the DMA in bytes

  @return 0, if successful. -1, otherwise.
*/
int
vmeDmaSend(unsigned long locAdrs, unsigned int vmeAdrs, int size)
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivDmaSend(locAdrs,vmeAdrs,size);
      break;

    case JVME_TSI148:
      status = jlabTsi148DmaSend(locAdrs,vmeAdrs,size);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

#ifdef VXWORKS
  status = sysVmeDmaSend(locAdrs,vmeAdrs,size,0);
#endif

  return status;

}

/*!
  Routine to initiate a DMA using physical memory address

  @param physAdrs Destination Physical Memory address
  @param vmeAdrs VME Bus source address
  @param size    Maximum size of the DMA in bytes

  @return 0, if successful. -1, otherwise.
*/
int
vmeDmaSendPhys(unsigned long physAdrs, unsigned int vmeAdrs, int size)
{
  int status = ERROR;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivDmaSendPhys(physAdrs,vmeAdrs,size);
      break;

    case JVME_TSI148:
      status = jlabTsi148DmaSendPhys(physAdrs,vmeAdrs,size);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

#ifdef VXWORKS
  logMsg("vmeDmaSendPhys: ERROR: This routine not compatible with vxWorks\n",
	 1,2,3,4,5,6);
  status = ERROR;
#endif

  return status;

}

/*!
  Routine to return the Bus Error status from most recent DMA

  @return 1 if Bus Error ended DMA or 0 if not, if successful. Otherwise ERROR.
*/
int
vmeDmaBerrStatus()
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivGetBerrStatus();
      break;

    case JVME_TSI148:
      status = jlabTsi148GetBerrStatus();
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

#ifdef VXWORKS
  logMsg("vmeDmaBerrStatus: ERROR: Routine not supported in vxWorks\n",
	 1, 2, 3, 4, 5, 6);
  status = ERROR;
#endif

  return status;
}

/*!
  Routine to poll for a DMA Completion or timeout.

  @return Number of bytes transferred, if successful, -1, otherwise.
*/
int
vmeDmaDone()
{
  int status = OK;

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivDmaDone(1000);
      break;

    case JVME_TSI148:
      status = jlabTsi148DmaDone();
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif

#ifdef VXWORKS
  status = sysVmeDmaDone(10000,1);
#endif

  return status;
}

/*!
  Routine to DMA rest of transaction to trash can
  @param vmeAddr VME address to continue DMA
  @returns number of bytes transferred.
*/
int
vmeDmaFlush(unsigned int vmeaddr)
{
  int status = OK;

#ifdef GEFANUC
  status = jlabgefDmaFlush(vmeaddr);
#else
  logMsg("vmeDmaFlush: ERROR: Routine not supported.\n",
	 1, 2, 3, 4, 5, 6);
  status = ERROR;
#endif

  return status;
}

/*!
  Routine to allocate memory for a linked list buffer.

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
vmeDmaAllocLLBuffer()
{
  int status = OK;

#ifdef GEFANUC
  status = jlabgefDmaAllocLLBuffer();
#endif

  return status;
}

/*!
  Routine to free the memory allocated with vmeDmaAllocLLBuffer()

  @return 0 if successful, otherwise.. an API dependent error code
*/
int
vmeDmaFreeLLBuffer()
{
  int status = OK;

#ifdef GEFANUC
  status = jlabgefDmaFreeLLBuffer();
#endif

  return status;
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
vmeDmaSetupLL(unsigned long locAddrBase,unsigned int *vmeAddr,
	      unsigned int *dmaSize,unsigned int numt)
{
  int status = OK;
#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivDmaSetupLL(locAddrBase,vmeAddr,dmaSize,numt);
      break;

    case JVME_TSI148:
      status = jlabTsi148DmaSetupLL(locAddrBase,vmeAddr,dmaSize,numt);
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif
  return status;
}

/*!
  Routine to initiate a linked list DMA that was setup with vmeDmaSetupLL()
*/
int
vmeDmaSendLL()
{
  int status = OK;
#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      status = jlabUnivDmaSendLL();
      break;

    case JVME_TSI148:
      status = jlabTsi148DmaSendLL();
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
      status = ERROR;
    }
#endif
  return status;
}

/*!
  Routine to convert the current data buffer pointer position in userspace
  to Physical Memory space

  @param locAdrs
  Pointer to current position in data buffer in userspace.

  @return Physical Memory address of the current position in the data buffer.
*/
unsigned long
vmeDmaLocalToPhysAdrs(unsigned long locAdrs)
{
  unsigned long status=OK;

#ifdef GEFANUC
  status = jlabgefDmaLocalToPhysAdrs(locAdrs);
#endif
  return status;
}

/*!
  Routine to convert the current data buffer pointer position in userspace
  to a VME Address.

  The VME Slave window must be initialized, prior to a call to this routine.

  @param locAdrs
  Pointer to current position in data buffer in userspace.

  @return VME Slave address of the current position in the data buffer.
*/
unsigned int
vmeDmaLocalToVmeAdrs(unsigned long locAdrs)
{
  unsigned int status=OK;

#ifdef GEFANUC
  status =  jlabgefDmaLocalToVmeAdrs(locAdrs);
#endif

  return status;
}

/*!
  Routine to print the Tempe DMA Registers
*/
void
vmeReadDMARegs()
{

#ifdef GEFANUC
  switch(jvmeBridgeType)
    {
    case JVME_UNIVII:
      jlabUnivReadDMARegs();
      break;

    case JVME_TSI148:
      jlabTsi148ReadDMARegs();
      break;

    default:
      printf("%s: ERROR: VME Bridge not Initialized\n",
	     __func__);
    }
#endif

}

/* API independent code here */

#ifndef VXWORKS
/* Shared Robust Mutex for vme bus access */
char* shm_name_vmeBus = "/vmeBus";
struct dma_config
{
  unsigned int dsal;
  unsigned int dsau;
  unsigned int ddal;
  unsigned int ddau;
  unsigned int dsat;
  unsigned int ddat;
  unsigned int dcnt;
  unsigned int ddbs;
  unsigned int dctl; /* without "Go" TSI148_LCSR_DCTL_DGO */
};
/* Keep this as a structure, in case we want to add to it in the future */
struct shared_memory_mutex
{
  pthread_mutex_t mutex;
  pthread_mutexattr_t m_attr;
  pid_t lockPID;
  struct dma_config dma[2];
  int last_dma_channel;
  int current_dma_channel;
};
struct shared_memory_mutex *p_sync=NULL;
/* mmap'd address of shared memory mutex */
void *addr_shm = NULL;

static int
vmeBusMutexInit()
{
  if(!vmeQuietFlag)
    printf("%s: Initializing vmeBus mutex\n",__func__);

  p_sync->lockPID = 0;
  if(pthread_mutexattr_init(&p_sync->m_attr)<0)
    {
      perror("pthread_mutexattr_init");
      printf("%s: ERROR:  Unable to initialized mutex attribute\n",__func__);
      return ERROR;
    }
  if(pthread_mutexattr_setpshared(&p_sync->m_attr, PTHREAD_PROCESS_SHARED)<0)
    {
      perror("pthread_mutexattr_setpshared");
      printf("%s: ERROR:  Unable to set shared attribute\n",__func__);
      return ERROR;
    }
  if(pthread_mutexattr_setrobust_np(&p_sync->m_attr, PTHREAD_MUTEX_ROBUST_NP)<0)
    {
      perror("pthread_mutexattr_setrobust_np");
      printf("%s: ERROR:  Unable to set robust attribute\n",__func__);
      return ERROR;
    }
  if(pthread_mutex_init(&(p_sync->mutex), &p_sync->m_attr)<0)
    {
      perror("pthread_mutex_init");
      printf("%s: ERROR:  Unable to initialize shared mutex\n",__func__);
      return ERROR;
    }

  return OK;
}

/*!
  Routine to create (if needed) a shared mutex for VME Bus locking

  @return 0, if successful. -1, otherwise.
*/
int
vmeBusCreateLockShm()
{
  int fd_shm;
  int needMutexInit=0, stat=0;
  mode_t prev_mode;

  /* Save the process ID of the current process */
  processID = getpid();

  /* First check to see if the file already exists */
  fd_shm = shm_open(shm_name_vmeBus, O_RDWR,
		    S_IRUSR | S_IWUSR |
		    S_IRGRP | S_IWGRP |
		    S_IROTH | S_IWOTH );
  if(fd_shm<0)
    {
      /* Bad file handler.. */
      if(errno == ENOENT)
	{
	  needMutexInit=1;
	}
      else
	{
	  perror("shm_open");
	  printf(" %s: ERROR: Unable to open shared memory\n",__func__);
	  return ERROR;
	}
    }

  if(needMutexInit)
    {
      if(!vmeQuietFlag)
	printf("%s: Creating vmeBus shared memory file\n",__func__);

      prev_mode = umask(0); /* need to override the current umask, if necessary */

      /* Create and map 'mutex' shared memory */
      fd_shm = shm_open(shm_name_vmeBus, O_CREAT|O_RDWR,
			S_IRUSR | S_IWUSR |
			S_IRGRP | S_IWGRP |
			S_IROTH | S_IWOTH );
      umask(prev_mode);
      if(fd_shm<0)
	{
	  perror("shm_open");
	  printf(" %s: ERROR: Unable to open shared memory\n",__func__);
	  return ERROR;
	}
      ftruncate(fd_shm, sizeof(struct shared_memory_mutex));
    }

  addr_shm = mmap(0, sizeof(struct shared_memory_mutex), PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(addr_shm<0)
    {
      perror("mmap");
      printf("%s: ERROR: Unable to mmap shared memory\n",__func__);
      return ERROR;
    }
  p_sync = addr_shm;

  if(needMutexInit)
    {
      stat = vmeBusMutexInit();
      if(stat==ERROR)
	{
	  printf("%s: ERROR Initializing vmeBus Mutex\n",
		 __func__);
	  return ERROR;
	}
    }

  if(!vmeQuietFlag)
    printf("%s: vmeBus shared memory mutex initialized\n",__func__);
  return OK;
}

/*!
  Routine to destroy the shared mutex created by vmeBusCreateLockShm()

  @return 0, if successful. -1, otherwise.
*/
int
vmeBusKillLockShm(int kflag)
{
  int rval = OK;

  if(munmap(addr_shm, sizeof(struct shared_memory_mutex))<0)
    perror("munmap");

  if(kflag==1)
    {
      if(pthread_mutexattr_destroy(&p_sync->m_attr)<0)
	perror("pthread_mutexattr_destroy");

      if(pthread_mutex_destroy(&p_sync->mutex)<0)
	perror("pthread_mutex_destroy");

      if(shm_unlink(shm_name_vmeBus)<0)
	perror("shm_unlink");

      if(!vmeQuietFlag)
	printf("%s: vmeBus shared memory mutex destroyed\n",__func__);
    }
  return rval;
}

/*!
  Routine to lock the shared mutex created by vmeBusCreateLockShm()

  @return 0, if successful. -1 or other error code otherwise.
*/
int
vmeBusLock()
{
  int rval;

  if(p_sync!=NULL)
    {
      rval = pthread_mutex_lock(&(p_sync->mutex));
      if(rval<0)
	{
	  perror("pthread_mutex_lock");
	  printf("%s: ERROR locking vmeBus\n",__func__);
	}
      else if (rval>0)
	{
	  printf("%s: ERROR: %s\n",__func__,
		 (rval==EINVAL)?"EINVAL":
		 (rval==EBUSY)?"EBUSY":
		 (rval==EAGAIN)?"EAGAIN":
		 (rval==EPERM)?"EPERM":
		 (rval==EOWNERDEAD)?"EOWNERDEAD":
		 (rval==ENOTRECOVERABLE)?"ENOTRECOVERABLE":
		 "Undefined");
	  if(rval==EOWNERDEAD)
	    {
	      printf("%s: WARN: Previous owner of vmeBus (mutex) died unexpectedly\n",
		     __func__);
	      printf("  Attempting to recover..\n");
	      if(pthread_mutex_consistent_np(&(p_sync->mutex))<0)
		{
		  perror("pthread_mutex_consistent_np");
		}
	      else
		{
		  printf("  Successful!\n");
		  rval=OK;
		}
	    }
	  if(rval==ENOTRECOVERABLE)
	    {
	      printf("%s: ERROR: vmeBus mutex in an unrecoverable state!\n",
		     __func__);
	    }
	}
      else
	{
	  p_sync->lockPID = processID;
	}
    }
  else
    {
      printf("%s: ERROR: vmeBusLock not initialized.\n",__func__);
      return ERROR;
    }
  return rval;
}

/*!
  Routine to try to lock the shared mutex created by vmeBusCreateLockShm()

  @return 0, if successful. -1 or other error code otherwise.
*/
int
vmeBusTryLock()
{
  int rval=ERROR;

  if(p_sync!=NULL)
    {
      rval = pthread_mutex_trylock(&(p_sync->mutex));
      if(rval<0)
	{
	  perror("pthread_mutex_trylock");
	}
      else if(rval>0)
	{
	  printf("%s: ERROR: %s\n",__func__,
		 (rval==EINVAL)?"EINVAL":
		 (rval==EBUSY)?"EBUSY":
		 (rval==EAGAIN)?"EAGAIN":
		 (rval==EPERM)?"EPERM":
		 (rval==EOWNERDEAD)?"EOWNERDEAD":
		 (rval==ENOTRECOVERABLE)?"ENOTRECOVERABLE":
		 "Undefined");
	  if(rval==EBUSY)
	    {
	      printf("%s: Locked vmeBus (mutex) owned by PID = %d\n",
		     __func__, p_sync->lockPID);
	    }
	  if(rval==EOWNERDEAD)
	    {
	      printf("%s: WARN: Previous owner of vmeBus (mutex) died unexpectedly\n",
		     __func__);
	      printf("  Attempting to recover..\n");
	      if(pthread_mutex_consistent_np(&(p_sync->mutex))<0)
		{
		  perror("pthread_mutex_consistent_np");
		}
	      else
		{
		  printf("  Successful!\n");
		  rval=OK;
		}
	    }
	  if(rval==ENOTRECOVERABLE)
	    {
	      printf("%s: ERROR: vmeBus mutex in an unrecoverable state!\n",
		     __func__);
	    }
	}
      else
	{
	  p_sync->lockPID = processID;
	}
    }
  else
    {
      printf("%s: ERROR: vmeBus mutex not initialized\n",__func__);
      return ERROR;
    }

  return rval;

}

/*!
  Routine to lock the shared mutex created by vmeBusCreateLockShm()

  @return 0, if successful. -1 or other error code otherwise.
*/

int
vmeBusTimedLock(int time_seconds)
{
  int rval=ERROR;
  struct timespec timeout;

  if(p_sync!=NULL)
    {
      clock_gettime(CLOCK_REALTIME, &timeout);
      timeout.tv_nsec = 0;
      timeout.tv_sec += time_seconds;

      rval = pthread_mutex_timedlock(&p_sync->mutex,&timeout);
      if(rval<0)
	{
	  perror("pthread_mutex_timedlock");
	}
      else if(rval>0)
	{
	  printf("%s: ERROR: %s\n",__func__,
		 (rval==EINVAL)?"EINVAL":
		 (rval==EBUSY)?"EBUSY":
		 (rval==EAGAIN)?"EAGAIN":
		 (rval==ETIMEDOUT)?"ETIMEDOUT":
		 (rval==EPERM)?"EPERM":
		 (rval==EOWNERDEAD)?"EOWNERDEAD":
		 (rval==ENOTRECOVERABLE)?"ENOTRECOVERABLE":
		 "Undefined");
	  if(rval==ETIMEDOUT)
	    {
	      printf("%s: Timeout: Locked vmeBus (mutex) owned by PID = %d\n",
		     __func__, p_sync->lockPID);
	    }
	  if(rval==EOWNERDEAD)
	    {
	      printf("%s: WARN: Previous owner of vmeBus (mutex) died unexpectedly\n",
		     __func__);
	      printf("  Attempting to recover..\n");
	      if(pthread_mutex_consistent_np(&(p_sync->mutex))<0)
		{
		  perror("pthread_mutex_consistent_np");
		}
	      else
		{
		  printf("  Successful!\n");
		  rval=OK;
		}
	    }
	}
      else
	{
	  p_sync->lockPID = processID;
	}
    }
  else
    {
      printf("%s: ERROR: vmeBus mutex not initialized\n",__func__);
      return ERROR;
    }

  return rval;

}

/*!
  Routine to unlock the shared mutex created by vmeBusCreateLockShm()

  @return 0, if successful. -1 or other error code otherwise.
*/
int
vmeBusUnlock()
{
  int rval=0;
  if(p_sync!=NULL)
    {
      p_sync->lockPID = 0;
      rval = pthread_mutex_unlock(&p_sync->mutex);
      if(rval<0)
	{
	  perror("pthread_mutex_unlock");
	}
      else if(rval>0)
	{
	  printf("%s: ERROR: %s \n",__func__,
		   (rval==EINVAL)?"EINVAL":
		   (rval==EBUSY)?"EBUSY":
		   (rval==EAGAIN)?"EAGAIN":
		   (rval==EPERM)?"EPERM":
		   "Undefined");
	}
    }
  else
    {
      printf("%s: ERROR: vmeBus mutex not initialized.\n",__func__);
      return ERROR;
    }
  return rval;
}

/*!
  Routine to check the "health" of the mutex created with vmeBusCreateLockShm()

  If the mutex is found to be stale (Owner of the lock has died), it will
  be recovered.

  @param time_seconds     How many seconds to wait for mutex to unlock when testing

  @return 0, if successful. -1, otherwise.
*/
int
vmeCheckMutexHealth(int time_seconds)
{
  int rval=0, busy_rval=0;

  if(p_sync!=NULL)
    {
      if(!vmeQuietFlag)
	printf("%s: Checking health of vmeBus shared mutex...\n",
	       __func__);

      /* Try the Mutex to see if it's state (locked/unlocked) */
      printf(" * ");
      rval = vmeBusTryLock();
      switch (rval)
	{
	case -1: /* Error */
	  printf("%s: rval = %d: Not sure what to do here\n",
		 __func__,rval);
	  break;
	case 0:  /* Success - Got the lock */
	  if(!vmeQuietFlag)
	    printf(" * ");

	  rval = vmeBusUnlock();
	  break;

	case EAGAIN: /* Bad mutex attribute initialization */
	case EINVAL: /* Bad mutex attribute initialization */
	  /* Re-Init here */
	  if(!vmeQuietFlag)
	    printf(" * ");

	  rval = vmeBusMutexInit();
	  break;

	case EBUSY: /* It's Locked */
	  {
	    /* Check to see if we can unlock it */
	    if(!vmeQuietFlag)
	      printf(" * ");

	    busy_rval = vmeBusUnlock();
	    switch(busy_rval)
	      {
	      case OK:     /* Got the unlock */
		rval=busy_rval;
		break;

	      case EAGAIN: /* Bad mutex attribute initialization */
	      case EINVAL: /* Bad mutex attribute initialization */
		/* Re-Init here */
		if(!vmeQuietFlag)
		  printf(" * ");

		rval = vmeBusMutexInit();
		break;

	      case EPERM: /* Mutex owned by another thread */
		{
		  /* Check to see if we can get the lock within 5 seconds */
		  if(!vmeQuietFlag)
		    printf(" * ");

		  busy_rval = vmeBusTimedLock(time_seconds);
		  switch(busy_rval)
		    {
		    case -1: /* Error */
		      printf("%s: rval = %d: Not sure what to do here\n",
			     __func__,busy_rval);
		      break;

		    case 0:  /* Success - Got the lock */
		      printf(" * ");
		      rval = vmeBusUnlock();
		      break;

		    case EAGAIN: /* Bad mutex attribute initialization */
		    case EINVAL: /* Bad mutex attribute initialization */
		      /* Re-Init here */
		      if(!vmeQuietFlag)
			printf(" * ");

		      rval = vmeBusMutexInit();
		      break;

		    case ETIMEDOUT: /* Timeout getting the lock */
		      /* Re-Init here */
		      if(!vmeQuietFlag)
			printf(" * ");

		      rval = vmeBusMutexInit();
		      break;

		    default:
		      printf("%s: Undefined return from pthread_mutex_timedlock (%d)\n",
			     __func__,busy_rval);
		      rval=busy_rval;

		    }

		}
		break;

	      default:
		printf("%s: Undefined return from vmeBusUnlock (%d)\n",
		       __func__,busy_rval);
		      rval=busy_rval;

	      }

	  }
	  break;

	default:
	  printf("%s: Undefined return from vmeBusTryLock (%d)\n",
		 __func__,rval);

	}

      if(rval==OK)
	{
	  if(!vmeQuietFlag)
	    printf("%s: Mutex Clean and Unlocked\n",__func__);
	}
      else
	{
	  printf("%s: Mutex is NOT usable\n",__func__);
	}

    }
  else
    {
      printf("%s: INFO: vmeBus Mutex not initialized\n",
	     __func__);
      return ERROR;
    }

  return rval;
}
#endif

int
vmeSetMaximumVMESlots(int slots)
{
  if((slots<1)||(slots>MAX_VME_SLOTS))
    {
      printf("%s: ERROR: Invalid slots (%d)\n",
	     __func__,slots);
      return ERROR;
    }
  maxVmeSlots = slots;

  return OK;
}

/*!
 Routine to return the VME slot, provided the VXS payload port.

 @return VME Slot number.
*/
int
vxsPayloadPort2vmeSlot(int payloadport)
{
  int rval=0;
  int islot;
  unsigned short *PayloadPort;

  if(payloadport<1 || payloadport>18)
    {
      printf("%s: ERROR: Invalid payloadport %d\n",
	     __func__,payloadport);
      return ERROR;
    }

  if(maxVmeSlots==20)
    PayloadPort = PayloadPort20;
  else if(maxVmeSlots==21)
    PayloadPort = PayloadPort21;
  else
    {
      printf("%s: ERROR: No lookup table for maxVmeSlots = %d\n",
	     __func__,maxVmeSlots);
      return ERROR;
    }

  for(islot=1;islot<MAX_VME_SLOTS+1;islot++)
    {
      if(payloadport == PayloadPort[islot])
	{
	  rval = islot;
	  break;
	}
    }

  if(rval==0)
    {
      printf("%s: ERROR: Unable to find VME Slot from Payload Port %d\n",
	     __func__,payloadport);
      rval=ERROR;
    }

  return rval;
}

/*!
 Routine to return the VME slot mask, provided the VXS payload port mask.

 @return VME Slot mask.
*/
unsigned int
vxsPayloadPortMask2vmeSlotMask(unsigned int ppmask)
{
  int ipp=0;
  unsigned int vmemask=0;

  for(ipp=0; ipp<18; ipp++)
    {
      if(ppmask & (1<<ipp))
	vmemask |= (1<<vxsPayloadPort2vmeSlot(ipp+1));
    }

  return vmemask;
}

/*!
  Routine to return the VXS Payload Port provided the VME slot

  @return VXS Payload Port number.
*/
int
vmeSlot2vxsPayloadPort(int vmeslot)
{
  int rval=0;
  unsigned short *PayloadPort;

  if(vmeslot<1 || vmeslot>maxVmeSlots)
    {
      printf("%s: ERROR: Invalid VME slot %d\n",
	     __func__,vmeslot);
      return ERROR;
    }

  if(maxVmeSlots==20)
    PayloadPort = PayloadPort20;
  else if(maxVmeSlots==21)
    PayloadPort = PayloadPort21;
  else
    {
      printf("%s: ERROR: No lookup table for maxVmeSlots = %d\n",
	     __func__,maxVmeSlots);
      return ERROR;
    }

  rval = (int)PayloadPort[vmeslot];

  if(rval==0)
    {
      printf("%s: ERROR: Unable to find Payload Port from VME Slot %d\n",
	     __func__,vmeslot);
      rval=ERROR;
    }

  return rval;
}

/*!
  Routine to return the VXS Payload Port mask provided the VME slot mask

  @return VXS Payload Port mask.
*/
unsigned int
vmeSlotMask2vxsPayloadPortMask(unsigned int vmemask)
{
  int islot=0;
  unsigned int ppmask=0;

  for(islot=0; islot<22; islot++)
    {
      if(vmemask & (1<<islot))
	ppmask |= (1<<(vmeSlot2vxsPayloadPort(islot)-1));
    }

  return ppmask;
}


/* Local Memory Register Read/Write routines */
unsigned char
vmeRead8(volatile unsigned char *addr)
{
  unsigned char rval;
  unsigned int vmeAdrs=0;
  unsigned short amcode=0;
  unsigned long local=(unsigned long)addr;

  rval = *addr;

  if(vmeDebugMode)
    {
      vmeLocalToVmeAdrs(local,&vmeAdrs,&amcode);
      fprintf(fDebugMode,"VDM:  0x%02x  D8  READ: 0x%08X    0x%02X\n",
	     amcode,vmeAdrs,rval);
    }

  return rval;
}

unsigned short
vmeRead16(volatile unsigned short *addr)
{
  unsigned short rval;
  unsigned int vmeAdrs=0;
  unsigned short amcode=0;
  unsigned long local=(unsigned long)addr;

  rval = *addr;
#ifndef VXWORKS
  rval = SSWAP(rval);
#endif

  if(vmeDebugMode)
    {
      vmeLocalToVmeAdrs(local,&vmeAdrs,&amcode);
      fprintf(fDebugMode,"VDM:  0x%02x D16  READ: 0x%08X    0x%04X\n",
	     amcode,vmeAdrs,rval);
    }

  return rval;
}

unsigned int
vmeRead32(volatile unsigned int *addr)
{
  unsigned int rval;
  unsigned int vmeAdrs=0;
  unsigned short amcode=0;
  unsigned long local=(unsigned long)addr;

  rval = *addr;
#ifndef VXWORKS
  rval = LSWAP(rval);
#endif

  if(vmeDebugMode)
    {
      vmeLocalToVmeAdrs(local,&vmeAdrs,&amcode);
      fprintf(fDebugMode,"VDM:  0x%02x D32  READ: 0x%08X    0x%08X\n",
	     amcode,vmeAdrs,rval);
    }

  return rval;
}

void
vmeWrite8(volatile unsigned char *addr, unsigned char val)
{
  unsigned int vmeAdrs=0;
  unsigned short amcode=0;
  unsigned long local=(unsigned long)addr;

  *addr = val;
  if(vmeDebugMode)
    {
      vmeLocalToVmeAdrs(local,&vmeAdrs,&amcode);
      fprintf(fDebugMode,"VDM:  0x%02x  D8 WRITE: 0x%08X    0x%02X\n",
	     amcode,vmeAdrs,val);
    }

  return;
}

void
vmeWrite16(volatile unsigned short *addr, unsigned short val)
{
  unsigned int vmeAdrs=0;
  unsigned short amcode=0;
  unsigned long local=(unsigned long)addr;

#ifndef VXWORKS
  val = SSWAP(val);
#endif
  *addr = val;

  if(vmeDebugMode)
    {
      vmeLocalToVmeAdrs(local,&vmeAdrs,&amcode);
      fprintf(fDebugMode,"VDM:  0x%02x D16 WRITE: 0x%08X    0x%04X\n",
	     amcode,vmeAdrs,val);
    }

  return;
}

void
vmeWrite32(volatile unsigned int *addr, unsigned int val)
{
  unsigned int vmeAdrs=0;
  unsigned short amcode=0;
  unsigned long local=(unsigned long)addr;

#ifndef VXWORKS
  val = LSWAP(val);
#endif
  *addr = val;

  if(vmeDebugMode)
    {
      vmeLocalToVmeAdrs(local,&vmeAdrs,&amcode);
      fprintf(fDebugMode,"VDM:  0x%02x D32 WRITE: 0x%08X    0x%08X\n",
	     amcode,vmeAdrs,val);
    }

  return;
}

#ifndef VXWORKS
/* VME Bus Register Read/Write routines */
unsigned char
vmeBusRead8(int amcode, unsigned int vmeaddr)
{
  unsigned char rval=0;

  rval = jlabgefVmeRead8(amcode, vmeaddr);

  return rval;
}

unsigned short
vmeBusRead16(int amcode, unsigned int vmeaddr)
{
  unsigned short rval=0;

  rval = jlabgefVmeRead16(amcode, vmeaddr);

  return rval;
}

unsigned int
vmeBusRead32(int amcode, unsigned int vmeaddr)
{
  unsigned int rval=0;

  rval = jlabgefVmeRead32(amcode, vmeaddr);

  return rval;
}

void
vmeBusWrite8(int amcode, unsigned int vmeaddr, unsigned char val)
{
  jlabgefVmeWrite8(amcode, vmeaddr, val);
}

void
vmeBusWrite16(int amcode, unsigned int vmeaddr, unsigned short val)
{
  jlabgefVmeWrite16(amcode, vmeaddr, val);
}

void
vmeBusWrite32(int amcode, unsigned int vmeaddr, unsigned int val)
{
  jlabgefVmeWrite32(amcode, vmeaddr, val);
}
#endif

#endif /* ARCH_armv7l */
