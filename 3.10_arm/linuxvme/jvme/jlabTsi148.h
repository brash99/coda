#ifndef __JLABTSI148_H__
#define __JLABTSI148_H__
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
 *     Header for Routines specific to the tsi148 VME Bridge
 *
 *----------------------------------------------------------------------------*/

int  jlabTsi148UpdateA32SlaveWindow(int window, unsigned long *base);
int  jlabTsi148GetSysReset();
int  jlabTsi148ClearSysReset();
int  jlabTsi148GetBERRIrq();
int  jlabTsi148SetBERRIrq(int enable);
int  jlabTsi148ClearException(int pflag);
int  jlabTsi148ClearBERR();
int  jlabTsi148SetA24AM(int addr_mod);
int  jlabTsi148DmaConfig(unsigned int addrType, unsigned int dataType, unsigned int sstMode);
int  jlabTsi148DmaSend(unsigned long locAdrs, unsigned int vmeAdrs, int size);
int  jlabTsi148DmaSendPhys(unsigned long physAdrs, unsigned int vmeAdrs, int size);
int  jlabTsi148DmaDone();
int  jlabTsi148GetBerrStatus();
int  jlabTsi148DmaSetupLL(unsigned long locAddrBase, unsigned int *vmeAddr,
			  unsigned int *dmaSize, unsigned int numt);
int  jlabTsi148DmaSendLL();
void jlabTsi148ReadDMARegs();

#endif /* __JLABTSI148_H__ */
