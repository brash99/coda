#ifndef __JLABUNIVERSEII_H__
#define __JLABUNIVERSEII_H__
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
 *     Header for Routines specific to the Unverse II VME Bridge
 *
 *----------------------------------------------------------------------------*/

int  jlabUnivUpdateA32SlaveWindow(int window, unsigned long *base);
int  jlabUnivGetSysReset();
int  jlabUnivClearSysReset();
int  jlabUnivGetBERRIrq();
int  jlabUnivSetBERRIrq(int enable);
int  jlabUnivClearException(int pflag);
int  jlabUnivDmaReset(int pflag);

int  jlabUnivSetA24AM(int addr_mod);
int  jlabUnivDmaConfig(unsigned int addrType, unsigned int dataType);
int  jlabUnivDmaSend(unsigned long locAdrs, unsigned int vmeAdrs, int size);
int  jlabUnivDmaSendPhys(unsigned long physAdrs, unsigned int vmeAdrs, int size);
int  jlabUnivDmaDone(int pcnt);
int  jlabUnivGetBerrStatus();
int  jlabUnivDmaSetupLL(unsigned long locAddrBase, unsigned int *vmeAddr,
			  unsigned int *dmaSize, unsigned int numt);
int  jlabUnivDmaSendLL();
void jlabUnivReadDMARegs();

#endif /* __JLABUNIVERSEII_H__ */
