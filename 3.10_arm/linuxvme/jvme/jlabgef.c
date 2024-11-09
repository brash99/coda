/*----------------------------------------------------------------------------*
 *  Copyright (c) 2009,2010   Southeastern Universities Research Association, *
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
 *     JLab extra routines to compliment the GEFanuc API
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/mman.h>
#include <linux/pci.h>
#include "jvme.h"
#include "jlabgef.h"
#include "tsi148.h"
#include "jlabTsi148.h"
#include "jlabUniverseII.h"
#include "ca91c042.h"
#include "gef/gefcmn_vme.h"
#include "gef/gefcmn_osa.h"
#include "gef/gefcmn_vme_framework.h"

/*! GE-VME Driver handle */
GEF_VME_BUS_HDL vmeHdl;

/**
   Maximum window size for A32 addressing
*/
#define A32_MAX_WINDOW_MAP_SIZE   0x0f000000

/** \name Userspace window pointers
    Global pointers to the Userspace windows
    \{ */
void *a16_window = NULL;
void *a24_window = NULL;
void *a32_window = NULL;
void *a32blt_window = NULL;
void *a32slave_window = NULL;
void *crcsr_window = NULL;
/*  \} */
/** \name Physical Memory Base of the Slave Window
     Physical Memory Base of the Slave Window
     \{ */
unsigned long a32slave_physmembase = 0;
/*  \} */

/** \name VME address handles for GEF library
    \{ */
static GEF_VME_MASTER_HDL a16_hdl, a24_hdl, a32_hdl, a32blt_hdl;
static GEF_VME_MASTER_HDL crcsr_hdl;
static GEF_VME_SLAVE_HDL a32slave_hdl;
static GEF_MAP_HDL a16map_hdl, a24map_hdl, a32map_hdl, a32bltmap_hdl,
  a32slavemap_hdl;
static GEF_MAP_HDL crcsrmap_hdl;
/* \} */

/** \name Default VME window widths
    \{ */
unsigned int a32_window_width = 0x00010000;
unsigned int a32blt_window_width = 0x0a000000;
unsigned int a24_window_width = 0x01000000;
unsigned int a16_window_width = 0x00010000;
unsigned int crcsr_window_width = 0x01000000;
/*  \} */

extern int vmeSetBridgeType(int bridge);
/*! Userspace base address of Tempe registers (declared in jlabTsi148.c)*/
extern volatile tsi148_t *pTempe;

/*! Userspace base address of UnverseII registers (declared in jlabUniverseII.c)*/
extern volatile unsigned int *pUniv;

/** Handler for Bridge Register mmap */
static struct _GEF_VME_MAP_MASTER *mapHdl;

/** Flag for turning off or on driver verbosity
    \see vmeSetQuietFlag()
*/
extern unsigned int vmeQuietFlag;

/** Mutex for locking/unlocking VME Bridge driver access */
pthread_mutex_t bridge_mutex = PTHREAD_MUTEX_INITIALIZER;

/*! \name Mutex Defines
  Definitions for mutex locking/unlocking access to Tempe driver
  \{ */
/*! Lock the mutex for access to the Tempe driver
   \hideinitializer
 */
#define LOCK_BRIDGE {				\
    if(pthread_mutex_lock(&bridge_mutex)<0)	\
      perror("pthread_mutex_lock");		\
  }
/*! Unlock the mutex for access to the Tempe driver
   \hideinitializer
 */
#define UNLOCK_BRIDGE {				\
    if(pthread_mutex_unlock(&bridge_mutex)<0)	\
      perror("pthread_mutex_unlock");		\
  }
/* \} */


/** \name Default VME window settings (GE-VME API structures)
    \{ */
GEF_VME_ADDR addr_A16 = {
  .upper = 0x00000000,
  .lower = 0x00000000,
  .addr_space = GEF_VME_ADDR_SPACE_A16,
  .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
  .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
  .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
  .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
  .broadcast_id = 0,
  .flags = 0
};

GEF_VME_ADDR addr_A24 = {
  .upper = 0x00000000,
  .lower = 0x00000000,
  .addr_space = GEF_VME_ADDR_SPACE_A24,
  .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
  .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
  .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
  .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
  .broadcast_id = 0,
  .flags = 0
};

GEF_VME_ADDR addr_A32 = {
  .upper = 0x00000000,
  .lower = 0x08000000,
  .addr_space = GEF_VME_ADDR_SPACE_A32,
  .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
  .transfer_mode = GEF_VME_TRANSFER_MODE_SCT,
  .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
  .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
  .broadcast_id = 0,
  .flags = 0
};

GEF_VME_ADDR addr_A32blt = {
  .upper = 0x00000000,
  .lower = 0x08000000,
  .addr_space = GEF_VME_ADDR_SPACE_A32,
  .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
  .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
  .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
  .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
  .broadcast_id = 0,
  .flags = 0
};

GEF_VME_ADDR addr_A32slave = {
  .upper = 0x00000000,
  .lower = 0x08000000,
  .addr_space = GEF_VME_ADDR_SPACE_A32,
  .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_SUPER |
    GEF_VME_ADDR_MODE_DATA | GEF_VME_ADDR_MODE_PROGRAM,
  .transfer_mode = GEF_VME_TRANSFER_MODE_2eSST,
  .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
  .vme_2esst_rate = GEF_VME_2ESST_RATE_320,
  .broadcast_id = 0,
  .flags = 0
};

GEF_VME_ADDR addr_CRCSR = {
  .upper = 0x00000000,
  .lower = 0x00000000,
  .addr_space = GEF_VME_ADDR_SPACE_CRCSR,
  .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
  .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
  .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
  .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
  .broadcast_id = 0,
  .flags = 0
};

/*  \} */

/* didOpen=0(1) when the default windows have not (have been) opened */
static int didOpen = 0;

/* Prototypes of routines not included in the gefaunc headers */
GEF_STATUS GEF_STD_CALL
gefVmeReadReg(GEF_VME_BUS_HDL bus_hdl,
	      GEF_UINT32 offset, GEF_VME_DWIDTH width, void *buffer);

GEF_STATUS GEF_STD_CALL
gefVmeWriteReg(GEF_VME_BUS_HDL bus_hdl,
	       GEF_UINT32 offset, GEF_VME_DWIDTH width, void *buffer);

/*!
  Open a VME window for the A16 address space, and mmap it into userspace

  \see jlabgefCloseA16()

  @param *a16p Where to return base address of userspace map
  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefOpenA16(void *a16p)
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A16,
				    a16_window_width, &a16_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Now map the VME A16 Window into a local address */
  status =
    gefVmeMapMasterWindow(a16_hdl, 0, a16_window_width, &a16map_hdl, a16p);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeMapMasterWindow",
			   status);
      gefVmeReleaseMasterWindow(a16_hdl);
      a16p = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Open a VME window for the A24 address space, and mmap it into userspace

  \see jlabgefCloseA24()

  @param *a24p Where to return base address of userspace map
  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefOpenA24(void *a24p)
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  status =
    gefVmeCreateMasterWindow(vmeHdl, &addr_A24, a24_window_width, &a24_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Now map the VME A24 Window into a local address */
  status =
    gefVmeMapMasterWindow(a24_hdl, 0, a24_window_width, &a24map_hdl, a24p);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeMapMasterWindow",
			   status);
      gefVmeReleaseMasterWindow(a24_hdl);
      a24p = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Open a VME window for the A32 address space, and mmap it into userspace

  \see jlabgefCloseA32()

  @param base  VME Base address of the VME window
  @param size  Size of the VME window
  @param *a32p Where to return base address of userspace map
  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefOpenA32(unsigned int base, unsigned int size, void *a32p)
{

  GEF_STATUS status;

  addr_A32.lower = base;

  LOCK_BRIDGE;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A32, size, &a32_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  if((size == 0) || (size > A32_MAX_WINDOW_MAP_SIZE))
    {
      printf
	("%s: WARN: Invalid Window map size specified = 0x%x. Not Mapped.\n",
	 __func__, size);
      a32p = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Now map the VME A32 Window into a local address */
  status = gefVmeMapMasterWindow(a32_hdl, 0, size, &a32map_hdl, a32p);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeMapMasterWindow",
			   status);
      gefVmeReleaseMasterWindow(a32_hdl);
      a32p = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  UNLOCK_BRIDGE;

  return OK;
}

int
jlabgefSetA32BltWindowWidth(unsigned int size)
{
  a32blt_window_width = size;
  if(size > A32_MAX_WINDOW_MAP_SIZE)
    {
      printf("%s: WARN: size (0x%x) larger than Max Map size.\n",
	     __func__, size);
    }
  return OK;
}

/*!
  Open a VME window for the A32 address space, and mmap it into userspace
  This window is meant to be used to setup block transfers.

  \see jlabgefCloseA32Blt()

  @param base  VME Base address of the VME window
  @param size  Size of the VME window
  @param *a32p Where to return base address of userspace map
  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefOpenA32Blt(unsigned int base, unsigned int size, void *a32p)
{
  GEF_STATUS status;

  addr_A32blt.lower = base;

  LOCK_BRIDGE;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A32blt, size, &a32blt_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  if((size == 0) || (size > A32_MAX_WINDOW_MAP_SIZE))
    {
      printf
	("%s: WARN: Invalid Window map size specified = 0x%x. Set to Max (0x%x)\n",
	 __func__, size, A32_MAX_WINDOW_MAP_SIZE);
      size = A32_MAX_WINDOW_MAP_SIZE;
    }

  /* Now map the VME A32 Window into a local address */
  status = gefVmeMapMasterWindow(a32blt_hdl, 0, size, &a32bltmap_hdl, a32p);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeMapMasterWindow",
			   status);
      gefVmeReleaseMasterWindow(a32blt_hdl);
      a32p = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Open a VME window for the CRCSR address space, and mmap it into userspace

  \see jlabgefCloseCRCSR()

  @param *acrcsrp Where to return base address of userspace map
  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefOpenCRCSR(void *crcsrp)
{

  GEF_STATUS status;

  LOCK_BRIDGE;

  status =
    gefVmeCreateMasterWindow(vmeHdl, &addr_CRCSR, crcsr_window_width,
			     &crcsr_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Now map the VME CRCSR Window into a local address */
  status =
    gefVmeMapMasterWindow(crcsr_hdl, 0, crcsr_window_width, &crcsrmap_hdl,
			  crcsrp);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeMapMasterWindow",
			   status);
      gefVmeReleaseMasterWindow(crcsr_hdl);
      crcsrp = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Open a Slave VME window for the A32 address space.

  \see jlabgefCloseA32Slave()

  @param base  VME Base address of the VME window
  @param size  Size of the VME window
  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefOpenSlaveA32(unsigned int base, unsigned int size)
{
  GEF_STATUS status;
  int rval = OK;

  addr_A32slave.lower = base;

  LOCK_BRIDGE;

  status =
    gefVmeCreateSlaveWindow(vmeHdl, &addr_A32slave, size, &a32slave_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateSlaveWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Now map the VME A32 Slave Window into a local address */
  status = gefVmeMapSlaveWindow(a32slave_hdl, 0, size, &a32slavemap_hdl,
				(GEF_MAP_PTR *) & a32slave_window);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeMapSlaveWindow", status);
      gefVmeRemoveSlaveWindow(a32slave_hdl);
      a32slave_window = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  UNLOCK_BRIDGE;

#ifdef DEBUGSLAVE
  struct _GEF_VME_MAP_SLAVE *tryHdl =
    (struct _GEF_VME_MAP_SLAVE *) &a32slavemap_hdl;

  printf("%s: phys_addr = 0x%08x\n", __func__, (tryHdl->phys_addr));
#endif

  /* Update window attribytes and get the window's physical memory address */
  if(pTempe)
    rval = jlabTsi148UpdateA32SlaveWindow(0, &a32slave_physmembase);
  else if(pUniv)
    rval = jlabUnivUpdateA32SlaveWindow(0, &a32slave_physmembase);

  if(rval == ERROR)
    return ERROR;

  return OK;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA16()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseA16()
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  /* Unmap A16 address space */
  status = gefVmeUnmapMasterWindow(a16map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeUnmapMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }


  /* Release the A16 Master window */
  status = gefVmeReleaseMasterWindow(a16_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeReleaseMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  a16_window = NULL;

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA24()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseA24()
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  /* Unmap A24 address space */
  status = gefVmeUnmapMasterWindow(a24map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeUnmapMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Release the A24 Master window */
  status = gefVmeReleaseMasterWindow(a24_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeReleaseMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  a24_window = NULL;

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA32()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseA32()
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  /* Unmap A32 address space */
  status = gefVmeUnmapMasterWindow(a32map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeUnmapMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  if(a32_window != NULL)
    {
      /* Release the A32 Master window */
      status = gefVmeReleaseMasterWindow(a32_hdl);
      if(status != GEF_STATUS_SUCCESS)
	{
	  jlabgefPrintGefError((char *) __func__, "gefVmeReleaseMasterWindow",
			       status);
	  UNLOCK_BRIDGE;
	  return ERROR;
	}

      a32_window = NULL;
    }

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA32Blt()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseA32Blt()
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  /* Unmap A32 address space */
  status = gefVmeUnmapMasterWindow(a32bltmap_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeUnmapMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  if(a32blt_window != NULL)
    {
      /* Release the A32 Master window */
      status = gefVmeReleaseMasterWindow(a32blt_hdl);
      if(status != GEF_STATUS_SUCCESS)
	{
	  jlabgefPrintGefError((char *) __func__, "gefVmeReleaseMasterWindow",
			       status);
	  UNLOCK_BRIDGE;
	  return ERROR;
	}

      a32blt_window = NULL;
    }

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenCRCSR()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseCRCSR()
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  /* Unmap CRCSR address space */
  status = gefVmeUnmapMasterWindow(crcsrmap_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeUnmapMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }


  /* Release the A24 Master window */
  status = gefVmeReleaseMasterWindow(crcsr_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeReleaseMasterWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  crcsr_window = NULL;

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Routine to close the slate VME window opened with jlabgefOpenSlaveA32()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseA32Slave()
{
  GEF_STATUS status;

  LOCK_BRIDGE;

  /* Unmap A32 address space */
  status = gefVmeUnmapSlaveWindow(a32slavemap_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeUnmapSlaveWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }


  /* Release the A32 Master window */
  status = gefVmeRemoveSlaveWindow(a32slave_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeRemoveSlaveWindow",
			   status);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  a32slave_window = NULL;

  UNLOCK_BRIDGE;

  return OK;
}

/*!
  Mmap the VME Bridge register space to userspace.

  \see jlabgefBridgeUnmap()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
int
jlabgefBridgeMap()
{
  GEF_STATUS status;
  int rval = OK;
  GEF_UINT32 offset = 0x0;
  unsigned long pci_base = 0x0;
  unsigned int driver_deviveni = 0x0;
  unsigned int map_deviveni = 0x0;
  unsigned int *tmpMap;
  int bridgeType = 0;

  char *virtual_addr;
  mapHdl =
    (struct _GEF_VME_MAP_MASTER *) malloc(sizeof(struct _GEF_VME_MAP_MASTER));

  if(NULL == mapHdl)
    {
      perror("malloc");
      printf("%s: ERROR allocating memory\n", __func__);
      return ERROR;
    }

  memset(mapHdl, 0, sizeof(struct _GEF_VME_MAP_MASTER));

  /* Obtain the base from the PCI-VME Bridge itself */
  pci_base =
    (unsigned long) jlabgefReadRegister(PCI_BASE_ADDRESS_0) & 0xfffff000;
  mapHdl->phys_addr = pci_base;

#ifdef DEBUGMAP
  printf("%s: pci_base = 0x%lx\n", __func__, pci_base);
#endif

  mapHdl->virt_addr = 0;
  GEF_UINT32 size = 0x1000;

  status = gefOsaMap(vmeHdl->osa_intf_hdl,
		     &(mapHdl->osa_map_hdl),
		     &virtual_addr,
		     ((GEF_UINT32) (mapHdl->phys_addr) + offset), size);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefOsaMap", status);
      pTempe = NULL;
      return ERROR;
    }

  tmpMap = (unsigned int *) virtual_addr;

  /* Quick check of the map, compared to a read through the kernel driver */
  LOCK_BRIDGE;

  /*
     jlabgefReadRegister is a 32bit read.
     So this will return: PCI_VENDOR_ID | (PCI_DEVICE_ID << 16)
   */
  driver_deviveni = jlabgefReadRegister(PCI_VENDOR_ID);

  map_deviveni = tmpMap[PCI_VENDOR_ID];

  UNLOCK_BRIDGE;

#ifdef DEBUGMAP
  printf("\tdriver_deviveni\t= 0x%x\n\tmap_deviveni\t= 0x%x\n",
	 driver_deviveni, map_deviveni);
#endif

  if(driver_deviveni != map_deviveni)
    {
      printf("%s: ERROR: kernel map inconsistent with userspace map\n",
	     __func__);
      printf("\tdriver_deviveni\t= 0x%x\n\tmap_deviveni\t= 0x%x\n",
	     driver_deviveni, map_deviveni);
      pTempe = NULL;
      pUniv = NULL;
      rval = ERROR;
    }
  else
    {
      char *bName;
      if(((map_deviveni >> 16) & 0xFFFF) == PCI_DEVICE_ID_TUNDRA_TSI148)
	{
	  pTempe = (tsi148_t *) virtual_addr;
	  bName = "tsi148";
	  bridgeType = JVME_TSI148;
	}
      else if(((map_deviveni >> 16) & 0xFFFF) ==
	      PCI_DEVICE_ID_TUNDRA_CA91C142)
	{
	  pUniv = (unsigned int *) virtual_addr;
	  bName = "universeII";
	  bridgeType = JVME_UNIVII;
	}
      else
	{
	  printf("%s: ERROR: Unknown vendor/device ID (0x%08x)\n",
		 __func__, map_deviveni);
	  pTempe = NULL;
	  pUniv = NULL;
	  return ERROR;
	}

      if(!vmeQuietFlag)
	{
	  printf("%s: %s Userspace map successful\n", __func__, bName);
	}

      if(vmeSetBridgeType(bridgeType) != OK)
	{
	  printf("%s: ERROR setting VME bridge type\n",
		 __func__);
	  rval = ERROR;
	}
    }

  return rval;
}

/*!
  Free up the mmap of the Tempe register space to userspace.

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
int
jlabgefBridgeUnmap()
{
  GEF_STATUS status;
  int rval = OK;

  LOCK_BRIDGE;
  status = gefOsaUnMap(mapHdl->osa_map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefOsaUnMap", status);
      rval = ERROR;
    }

  free(mapHdl);
  UNLOCK_BRIDGE;

  return rval;
}

/*!
  Routine to initialize the GE-VME API
  - opens default VME Windows and maps them into Userspace
  - maps VME Bridge Registers into Userspace
  - disables interrupts on VME Bus Errors
  - creates a shared mutex for interrupt/trigger locking
  - calls vmeBusCreateLockShm()

  \see jlabgefCloseDefaultWindows();

  @return OK, if successful.  Otherwse ERROR.
*/
int
jlabgefOpenDefaultWindows()
{
  int rval = OK, rstatus = OK;
  GEF_STATUS status;

  if(didOpen)
    return OK;

  LOCK_BRIDGE;
  vmeHdl = NULL;

  status = gefVmeOpen(&vmeHdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeOpen", status);
      vmeHdl = NULL;
      UNLOCK_BRIDGE;
      return ERROR;
    }

  /* Turn off the Debug Flags, by default */
  gefVmeSetDebugFlags(vmeHdl, 0x0);

  UNLOCK_BRIDGE;

  /* Open CRCSR window */
  if(crcsr_window == NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening CRCSR Window ");
      rstatus = jlabgefOpenCRCSR(&crcsr_window);
    }
  if(rstatus == OK)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_CRCSR.lower,
	       (unsigned long) crcsr_window, crcsr_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
      rval = ERROR;
    }

  /* Open A32 windows */
  if(a32_window == NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A32 Window   ");
      rstatus = jlabgefOpenA32(0xfb000000, a32_window_width, &a32_window);
    }
  if(rstatus == OK)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A32.lower, (unsigned long) a32_window, a32_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
      rval = ERROR;
    }

  if(a32blt_window == NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A32Blt Window");
      rstatus =
	jlabgefOpenA32Blt(0x08000000, a32blt_window_width, &a32blt_window);
    }
  if(rstatus == OK)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A32blt.lower,
	       (unsigned long) a32blt_window, a32blt_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
      rval = ERROR;
    }

  /* Open A24 window */
  if(a24_window == NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A24 Window   ");
      rstatus = jlabgefOpenA24(&a24_window);
    }
  if(rstatus == OK)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A24.lower, (unsigned long) a24_window, a24_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
      rval = ERROR;
    }

  /* Open A16 window */
  if(a16_window == NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A16 Window   ");
      rstatus = jlabgefOpenA16(&a16_window);
    }
  if(rstatus == OK)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A16.lower, (unsigned long) a16_window, a16_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
      rval = ERROR;
    }

  if(jlabgefBridgeMap() != OK)
    rval = ERROR;

  /* By JLAB default... disable IRQ on BERR */
  rstatus = vmeDisableBERRIrq(1);
  if(status != OK)
    {
      printf("%s: ERROR Disabling IRQ on BERR", __func__);
      rval = ERROR;
    }

  /* Create Shared Mutex */
  vmeBusCreateLockShm();

  didOpen = 1;

  return rval;
}

/*!
  Routine to cleanup what was initialized by jlabgefOpenDefaultWindows()

  @return OK, if successful.  ERROR, otherwise.
*/
int
jlabgefCloseDefaultWindows()
{
  int rval = OK;
  GEF_STATUS status;
  if(didOpen == 0)
    return OK;

  /* Kill Shared Mutex - Here we just unmap it (don't remove the shm file) */
  vmeBusKillLockShm(0);


  if(jlabgefBridgeUnmap() != OK)
    rval = ERROR;

  /* Close A16 window */
  if(!vmeQuietFlag)
    printf("Closing A16 Window\n");
  if(jlabgefCloseA16() == ERROR)
    rval = ERROR;

  /* Close A24 window */
  if(!vmeQuietFlag)
    printf("Closing A24 Window\n");
  if(jlabgefCloseA24() == ERROR)
    rval = ERROR;

  /* Close A32 windows */
  if(!vmeQuietFlag)
    printf("Closing A32Blt Window\n");
  if(jlabgefCloseA32Blt() == ERROR)
    rval = ERROR;

  if(!vmeQuietFlag)
    printf("Closing A32 Window\n");
  if(jlabgefCloseA32() == ERROR)
    rval = ERROR;

  /* Close CRCSR window */
  if(!vmeQuietFlag)
    printf("Closing CRCSR Window\n");
  if(jlabgefCloseCRCSR() == ERROR)
    rval = ERROR;

  /* Close the VME Device file */
  if(!vmeQuietFlag)
    printf("Calling gefVmeClose\n");

  LOCK_BRIDGE;
  status = gefVmeClose(vmeHdl);
  if(status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeClose", status);
      rval = ERROR;
    }
  UNLOCK_BRIDGE;

  didOpen = 0;

  return rval;
}

/*!
  Return an integer indicating the VME Bridge that has been initialized
  by this library.

  @return 1 for Universe II, 2 for Tsi148 (Tempe), otherwise -1;

*/
int
jlabgefGetBridgeType()
{
  int rval = -1;

  if(pTempe != NULL)
    rval = JVME_TSI148;

  if(pUniv != NULL)
    rval = JVME_UNIVII;

  return rval;
}

/*!
  Read a 32bit word from a Tempe Chip register.

  \see gefvme_vme_tempe.h

  @param offset Register offset from Tempe Base Address.  A list of
  defines for offsets from register names is found in the header file:
  gefvme_vme_tempe.h

  @return 32bit value at the requested register, if successful.  -1,
  otherwise.

*/
GEF_UINT32
jlabgefReadRegister(GEF_UINT32 offset)
{
  GEF_STATUS status;
  GEF_UINT32 temp;

  status = gefVmeReadReg(vmeHdl, offset, GEF_VME_DWIDTH_D32, &temp);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeReadReg", status);
      return -1;
    }
  else
    {
      return temp;
    }

}

/*!
  Write a 32bit word to a Tempe Chip register.

  \see gefvme_vme_tempe.h

  @param offset Register offset from Tempe Base Address.  A list of
  defines for offsets from register names is found in the header file:
  gefvme_vme_tempe.h
  @param buffer 32bit word to be written to offset

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefWriteRegister(GEF_UINT32 offset, GEF_UINT32 buffer)
{
  GEF_STATUS status;

  status = gefVmeWriteReg(vmeHdl, offset, GEF_VME_DWIDTH_D32, &buffer);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeWriteReg", status);
    }

  return status;

}

/*!
  Assert SYSRESET on the VMEbus.

  @return OK, if successful.  ERROR, otherwise.
 */
int
jlabgefSysReset()
{
  GEF_STATUS status;
  int stat = 0;

  LOCK_BRIDGE;
  status = gefVmeSysReset(vmeHdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeSysReset", status);
      UNLOCK_BRIDGE;
      return ERROR;
    }
  UNLOCK_BRIDGE;

  /* Wait 200us... SYSRESET should clear by then */
  usleep(200);

  if(pTempe != NULL)
    stat = jlabTsi148GetSysReset();
  else if(pUniv != NULL)
    stat = jlabUnivGetSysReset();

  if(stat)
    {
      jlabTsi148ClearSysReset();
    }

  return OK;
}

/*! Maximum allowed GE-VME callback handles */
#define MAX_CB_HDL 25
/*! Array of GE-VME callback handles linked to their associated interrupt level */
static GEF_CALLBACK_HDL gefCB_hdl[MAX_CB_HDL] = {
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0
};

/*!
  Routine to connect a routine to a VME Bus Interrupt

  @param vector  interrupt vector to attach to
  @param level   VME Bus interrupt level
  @param routine routine to be called
  @param arg     argument to be passed to the routine

  @return OK, if successful. ERROR, otherwise.
*/
int
jlabgefIntConnect(unsigned int vector,
		  unsigned int level, VOIDFUNCPTR routine, unsigned int arg)
{
  GEF_STATUS status;

  if(level >= MAX_CB_HDL)
    {
      printf("%s: ERROR: Interrupt level %d not supported\n",
	     __func__, level);
      return ERROR;
    }

  if(gefCB_hdl[level] != 0)
    {
      printf("%s: ERROR: Interrupt already connected for level %d\n",
	     __func__, level);
      return ERROR;
    }

  status = gefVmeAttachCallback(vmeHdl, level, vector,
				routine, (void *) &arg, &gefCB_hdl[level]);

  if(status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeAttachCallback", status);
      return ERROR;
    }

  return OK;
}

/*!
  Routine to release the routine attached with vmeIntConnect()

  @param level  VME Bus Interrupt level

  @return OK, if successful. ERROR, otherwise.
*/
int
jlabgefIntDisconnect(unsigned int level)
{
  GEF_STATUS status;

  if(level >= MAX_CB_HDL)
    {
      printf("%s: ERROR: Interrupt level %d not supported\n",
	     __func__, level);
      return ERROR;
    }

  if(gefCB_hdl[level] == 0)
    {
      printf("%s: WARN: Interrupt not connected for level %d\n",
	     __func__, level);
      return OK;
    }

  status = gefVmeReleaseCallback(gefCB_hdl[level]);
  if(status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeReleaseCallback",
			   status);
      return ERROR;
    }

  gefCB_hdl[level] = 0;

  return OK;
}

/*!
  Routine to convert a a VME Bus address to a Userspace Address

  @param vmeAdrsSpace Bus address space in whihc vmeBusAdrs resides
  @param *vmeBusAdrs  Bus address to convert
  @param **pPciAdrs   Where to return Userspace address

  @return OK, if successful. ERROR, otherwise.
*/
int
jlabgefVmeBusToLocalAdrs(int vmeAdrsSpace, char *vmeBusAdrs, char **pPciAdrs)
{
  unsigned int vmeSpaceMask;
  unsigned int vmeAdrToConvert;
  unsigned int base;
  unsigned int limit;
  unsigned int trans;
  unsigned int busAdrs;
  int adrConverted = 0;
  char *pciBusAdrs = 0;

  switch (vmeAdrsSpace)
    {
    case GEF_VME_ADDR_MOD_A32SP:
    case GEF_VME_ADDR_MOD_A32SD:
    case GEF_VME_ADDR_MOD_A32UP:
    case GEF_VME_ADDR_MOD_A32UD:

      /* See if the window is A32 enabled */

      if(a32_window != NULL || a32blt_window != NULL)
	{
	  vmeSpaceMask = 0xffffffff;
	  vmeAdrToConvert = (unsigned long) vmeBusAdrs;
	  base = addr_A32.lower;
	  limit = a32_window_width + addr_A32.lower;
	  trans = 0;

	  if(((base + trans) <= vmeAdrToConvert) &&
	     ((limit + trans) >= vmeAdrToConvert))
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs =
		(char *) ((unsigned long) busAdrs +
			  (unsigned long) a32_window);
	      adrConverted = 1;
	      break;
	    }
	  else
	    {
	      base = addr_A32blt.lower;
	      limit = a32blt_window_width + addr_A32blt.lower;
	      trans = 0;
	      if(((base + trans) <= vmeAdrToConvert) &&
		 ((limit + trans) >= vmeAdrToConvert))
		{
		  busAdrs = vmeAdrToConvert - base;
		  pciBusAdrs =
		    (char *) ((unsigned long) busAdrs +
			      (unsigned long) a32blt_window);
		  adrConverted = 1;
		  break;
		}

	    }

	  break;
	}

    case GEF_VME_ADDR_MOD_A24SP:
    case GEF_VME_ADDR_MOD_A24SD:
    case GEF_VME_ADDR_MOD_A24UP:
    case GEF_VME_ADDR_MOD_A24UD:

      /* See if the window is A24 enabled */

      if(a24_window != NULL)
	{
	  vmeSpaceMask = 0x00ffffff;
	  vmeAdrToConvert = (unsigned long) vmeBusAdrs & vmeSpaceMask;
	  base = addr_A24.lower;
	  limit = a24_window_width + addr_A24.lower;
	  trans = 0;
	  if(((base + trans) <= vmeAdrToConvert) &&
	     ((limit + trans) >= vmeAdrToConvert))
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs =
		(char *) ((unsigned long) busAdrs +
			  (unsigned long) a24_window);
	      adrConverted = 1;
	      break;
	    }

	  break;
	}

    case GEF_VME_ADDR_MOD_A16S:
    case GEF_VME_ADDR_MOD_A16U:

      /* See if the window is A24 enabled */

      if(a16_window != NULL)
	{
	  vmeSpaceMask = 0x0000ffff;
	  vmeAdrToConvert = (unsigned long) vmeBusAdrs & vmeSpaceMask;
	  base = addr_A16.lower;
	  limit = a16_window_width + addr_A16.lower;
	  trans = 0;
	  if(((base + trans) <= vmeAdrToConvert) &&
	     ((limit + trans) >= vmeAdrToConvert))
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs =
		(char *) ((unsigned long) busAdrs +
			  (unsigned long) a16_window);
	      adrConverted = 1;
	      break;
	    }

	  break;
	}

    case GEF_VME_ADDR_MOD_CR_CSR:

      /* See if the window is CRCSR enabled */

      if(crcsr_window != NULL)
	{
	  vmeSpaceMask = 0x00ffffff;
	  vmeAdrToConvert = (unsigned long) vmeBusAdrs & vmeSpaceMask;
	  base = addr_CRCSR.lower;
	  limit = crcsr_window_width + addr_CRCSR.lower;
	  trans = 0;
	  if(((base + trans) <= vmeAdrToConvert) &&
	     ((limit + trans) >= vmeAdrToConvert))
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs =
		(char *) ((unsigned long) busAdrs +
			  (unsigned long) crcsr_window);
	      adrConverted = 1;
	      break;
	    }

	  break;
	}

    default:
      return (ERROR);		/* invalid address space */
    }

  if(adrConverted == 1)
    {
      *pPciAdrs = pciBusAdrs;
      return (OK);
    }
  return (ERROR);

}

/*!
  Routine to convert a Userspace Address to a VME Bus address

  @param localAdrs  Local (userspace) address to convert
  @param *vmeAdrs   Where to return VME address
  @param *amCode    Where to return address modifier

  @return OK, if successful. ERROR, otherwise.
*/
int
jlabgefLocalToVmeAdrs(unsigned long localAdrs, unsigned int *vmeAdrs,
		      unsigned short *amCode)
{
  /* Go through each window to see where the localAdrs falls */
  if((localAdrs >= (unsigned long) a32_window) &&
     (localAdrs < ((unsigned long) (a32_window + a32_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long) a32_window + addr_A32.lower;
      *amCode = 0x09;
      return OK;
    }

  if((localAdrs >= (unsigned long) a32blt_window) &&
     (localAdrs < ((unsigned long) (a32blt_window + a32blt_window_width))))
    {
      *vmeAdrs =
	localAdrs - (unsigned long) a32blt_window + addr_A32blt.lower;
      *amCode = 0x09;
      return OK;
    }

  if((localAdrs >= (unsigned long) a24_window) &&
     (localAdrs < ((unsigned long) (a24_window + a24_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long) a24_window + addr_A24.lower;
      *amCode = 0x39;
      return OK;
    }

  if((localAdrs >= (unsigned long) a16_window) &&
     (localAdrs < ((unsigned long) (a16_window + a16_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long) a16_window + addr_A16.lower;
      *amCode = 0x29;
      return OK;
    }

  if((localAdrs >= (unsigned long) crcsr_window) &&
     (localAdrs < ((unsigned long) (crcsr_window + crcsr_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long) crcsr_window + addr_CRCSR.lower;
      *amCode = 0x2F;
      return OK;
    }

  printf("%s: ERROR: VME address not found from 0x%lx\n",
	 __func__, localAdrs);

  *amCode = 0xFFFF;
  *vmeAdrs = 0xFFFFFFFF;
  return ERROR;
}

/*!
  Routine to enable/disable debug flags set in the VME Bridge Kernel Driver

  @param flags GE-VME API dependent flags to toggle specific debug levels and messages

  @return OK, if successful. ERROR, otherwise.
*/
int
jlabgefSetDebugFlags(int flags)
{
  GEF_STATUS status;

  status = gefVmeSetDebugFlags(vmeHdl, flags);

  if(status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeSetDebugFlags", status);
      return ERROR;
    }

  return OK;
}

static GEF_VME_MASTER_HDL tempHdl;

static int
jlabgefOpenTmpVmeHdl(int amcode, unsigned int addr)
{
  int rval = OK;
  GEF_STATUS status;
  GEF_VME_ADDR addr_struct = {
    .upper = 0x00000000,
    .lower = addr,
    .addr_space = GEF_VME_ADDR_SPACE_A32,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_SCT,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = 0,
    .broadcast_id = 0,
    .flags = 0
  };

  switch (amcode)
    {
    case GEF_VME_ADDR_MOD_A32SP:
    case GEF_VME_ADDR_MOD_A32SD:
    case GEF_VME_ADDR_MOD_A32UP:
    case GEF_VME_ADDR_MOD_A32UD:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_A32;
	break;
      }
    case GEF_VME_ADDR_MOD_A24SP:
    case GEF_VME_ADDR_MOD_A24SD:
    case GEF_VME_ADDR_MOD_A24UP:
    case GEF_VME_ADDR_MOD_A24UD:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_A24;
	break;
      }

    case GEF_VME_ADDR_MOD_A16S:
    case GEF_VME_ADDR_MOD_A16U:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_A16;
	break;
      }

    case GEF_VME_ADDR_MOD_CR_CSR:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_CRCSR;
	break;
      }

    default:
      {
	rval = ERROR;
      }
    }

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_struct, 0x10000, &tempHdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeCreateMasterWindow",
			   status);
      rval = ERROR;
    }

  return rval;
}

static int
jlabgefCloseTmpVmeHdl()
{
  GEF_STATUS status;

  status = gefVmeReleaseMasterWindow(tempHdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeReleaseMasterWindow",
			   status);
      return ERROR;
    }

  return OK;
}

unsigned int
jlabgefVmeRead32(int amcode, unsigned int addr)
{
  unsigned int rval = 0, base = 0, offset = 0;
  GEF_STATUS stat = 0;

  base = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_BRIDGE;

  if(jlabgefOpenTmpVmeHdl(amcode, base) != OK)
    {
      printf("%s: ERROR: Unable to read from register\n", __func__);
      UNLOCK_BRIDGE;
      return ERROR;
    }


  stat = gefVmeRead32(tempHdl, offset, 1, &rval);
  if(stat != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeRead32", stat);
      jlabgefCloseTmpVmeHdl();
      UNLOCK_BRIDGE;
      return ERROR;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_BRIDGE;

  return LSWAP(rval);
}

unsigned short
jlabgefVmeRead16(int amcode, unsigned int addr)
{
  unsigned short rval = 0;
  unsigned int base = 0, offset = 0;
  GEF_STATUS stat = 0;

  base = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_BRIDGE;

  if(jlabgefOpenTmpVmeHdl(amcode, base) != OK)
    {
      printf("%s: ERROR: Unable to read from register\n", __func__);
      UNLOCK_BRIDGE;
      return ERROR;
    }


  stat = gefVmeRead16(tempHdl, offset, 1, &rval);
  if(stat != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeRead16", stat);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_BRIDGE;

  return SSWAP(rval);
}

unsigned char
jlabgefVmeRead8(int amcode, unsigned int addr)
{
  unsigned char rval = 0;
  unsigned int base = 0, offset = 0;
  GEF_STATUS stat = 0;

  base = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_BRIDGE;

  if(jlabgefOpenTmpVmeHdl(amcode, base) != OK)
    {
      printf("%s: ERROR: Unable to read from register\n", __func__);
      UNLOCK_BRIDGE;
      return ERROR;
    }


  stat = gefVmeRead8(tempHdl, offset, 1, &rval);
  if(stat != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeRead8", stat);
      UNLOCK_BRIDGE;
      return ERROR;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_BRIDGE;

  return rval;
}

void
jlabgefVmeWrite32(int amcode, unsigned int addr, unsigned int wval)
{
  unsigned int base = 0, offset = 0;
  GEF_STATUS stat = 0;

  base = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_BRIDGE;

  if(jlabgefOpenTmpVmeHdl(amcode, base) != OK)
    {
      printf("%s: ERROR: Unable to write to register\n", __func__);
      UNLOCK_BRIDGE;
      return;
    }

  wval = LSWAP(wval);

  stat = gefVmeWrite32(tempHdl, offset, 1, &wval);
  if(stat != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeWrite32", stat);
      UNLOCK_BRIDGE;
      return;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_BRIDGE;
}

void
jlabgefVmeWrite16(int amcode, unsigned int addr, unsigned short wval)
{
  unsigned int base = 0, offset = 0;
  GEF_STATUS stat = 0;

  base = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_BRIDGE;

  if(jlabgefOpenTmpVmeHdl(amcode, base) != OK)
    {
      printf("%s: ERROR: Unable to write to register\n", __func__);
      UNLOCK_BRIDGE;
      return;
    }

  wval = SSWAP(wval);

  stat = gefVmeWrite16(tempHdl, offset, 1, &wval);
  if(stat != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeWrite16", stat);
      UNLOCK_BRIDGE;
      return;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_BRIDGE;
}

void
jlabgefVmeWrite8(int amcode, unsigned int addr, unsigned char wval)
{
  unsigned int base = 0, offset = 0;
  GEF_STATUS stat = 0;

  base = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_BRIDGE;

  if(jlabgefOpenTmpVmeHdl(amcode, base) != OK)
    {
      printf("%s: ERROR: Unable to write to register\n", __func__);
      UNLOCK_BRIDGE;
      return;
    }


  stat = gefVmeWrite8(tempHdl, offset, 1, &wval);
  if(stat != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *) __func__, "gefVmeWrite8", stat);
      UNLOCK_BRIDGE;
      return;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_BRIDGE;
}

void
jlabgefPrintGefError(char *calling_func, char *gef_func, GEF_STATUS status)
{
  GEF_UINT32 errno;
  char message[256][GEF_STATUS_LAST] = {
    "Success",
    "Not Supported",
    "Out Of Memory",
    "Invalid Address",
    "Read Error",
    "Write Error",
    "Device Not Initialized",
    "No Such Device",
    "Driver Error",
    "Interrupted",
    "Timed Out",
    "Event In Use",
    "Thread Creation Failed",
    "Callback Not Attached",
    "Device In Use",
    "Operation Not Allowed",
    "Bad Parameter 1",
    "Bad Parameter 2",
    "Bad Parameter 3",
    "Bad Parameter 4",
    "Bad Parameter 5",
    "Bad Parameter 6",
    "Bad Parameter 7",
    "Bad Parameter 8",
    "Bad Parameter 9",
    "Bad Parameter 10",
    "Bad Parameter 11",
    "Invalid Error Code",
    "No Event Pending",
    "Event Release",
    "CPCI Offset Not Aligned",
  };

  errno = GEF_GET_ERROR(status);

  printf("\n%s: ERROR\n\t %s returned: %s (%d)\n\n",
	 calling_func, gef_func, message[errno], errno);

}
