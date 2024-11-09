/*----------------------------------------------------------------------------*
 *  Copyright (c) 2009        Southeastern Universities Research Association, *
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
 *     Header for JLab extra routines to compliment the GEFanuc API
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#ifndef __JLABGEF__
#define __JLABGEF__
#include "jvme.h"
#include "gef/gefcmn_errno.h"

int jlabgefOpenA16(void *a16p);
int jlabgefOpenA24(void *a24p);
int jlabgefOpenA32(unsigned int base, unsigned int size, void *a32p);
int jlabgefSetA32BltWindowWidth(unsigned int size);
int jlabgefOpenA32Blt(unsigned int base, unsigned int size, void *a32p);
int jlabgefOpenCRCSR(void *crcsrp);
int jlabgefOpenSlaveA32(unsigned int base, unsigned int size);

int jlabgefCloseA16();
int jlabgefCloseA24();
int jlabgefCloseA32();
int jlabgefCloseA32Blt();
int jlabgefCloseCRCSR();
int jlabgefCloseA32Slave();

int jlabgefOpenDefaultWindows();
int jlabgefCloseDefaultWindows();
int jlabgefGetBridgeType();

GEF_UINT32 jlabgefReadRegister(GEF_UINT32 offset);
GEF_STATUS jlabgefWriteRegister(GEF_UINT32 offset, GEF_UINT32 buffer);
int jlabgefSysReset();

int jlabgefCheckAddress(char *addr);
int jlabgefIntConnect(unsigned int vector, unsigned int level, VOIDFUNCPTR routine, unsigned int arg);
int jlabgefIntDisconnect(unsigned int level);
int jlabgefVmeBusToLocalAdrs(int vmeAdrsSpace, char *vmeBusAdrs, char **pPciAdrs);
int jlabgefLocalToVmeAdrs(unsigned long localAdrs, unsigned int *vmeAdrs, unsigned short *amCode);
int jlabgefSetDebugFlags(int flags);

unsigned int jlabgefVmeRead32(int amcode, unsigned int addr);
unsigned short jlabgefVmeRead16(int amcode, unsigned int addr);
unsigned char jlabgefVmeRead8(int amcode, unsigned int addr);
void jlabgefVmeWrite32(int amcode, unsigned int addr, unsigned int wval);
void jlabgefVmeWrite16(int amcode, unsigned int addr, unsigned short wval);
void jlabgefVmeWrite8(int amcode, unsigned int addr, unsigned char wval);
void jlabgefPrintGefError(char *calling_func, char *gef_func, GEF_STATUS status);

#endif /* __JLABGEF__ */
