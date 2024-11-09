/*
 * File:
 *    berrStudy.c
 *
 * Description:
 *    Checkout what happens with the univII for Bus Errors in read and write SCT
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "jvme.h"
#include "../ca91c042.h"

#ifndef PCI_CSR
#define PCI_CSR 0x0004
#endif

extern void univWrite32(unsigned int offset, unsigned int wval);
extern unsigned int univRead32(unsigned int offset);

int
printRegs()
{
  uint32_t pci_csr, dgcs, v_amerr, vaerr;

  pci_csr = univRead32(PCI_CSR);
  dgcs = univRead32(DGCS);
  v_amerr = univRead32(V_AMERR);
  vaerr = univRead32(VAERR);

  printf("pci_csr = 0x%08x\n", pci_csr);
  printf("  dgcs  = 0x%08x\n", dgcs);
  printf("v_amerr = 0x%08x\n", v_amerr);
  printf("vaerr   = 0x%08x\n", vaerr);

  return OK;

}

int
resetCsr()
{
  printf(" ***** Reset CSR *****\n");
  univWrite32(PCI_CSR, (1 << 27) | 0x7);

  printRegs();
  return OK;
}

int
main(int argc, char *argv[])
{
  uint32_t vmeAddr = 0xa00000;
  uint32_t badAddr = 0xa80000;
  unsigned long laddr;
  volatile unsigned int *ptr;
  int stat = OK;

  vmeSetQuietFlag(0);
  vmeOpenDefaultWindows();
  vmeSetDebugFlags(0xffffffff);

  printf("\n\n");
  printf("------------------------------------------------------------\n");

  printf(" Initial Regs\n");
  printRegs();
  printf("\n");

/* #define READSTUDY */

  resetCsr();

  stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)vmeAddr,(char **)&laddr);
  ptr = (uint32_t *) laddr;
#ifdef READSTUDY
  printf(" Read Good\n");
  printf(" 0x%0lx: 0x%08x\n", laddr, vmeRead32(ptr));
#else
  printf(" Write Good\n");
  vmeWrite32(ptr, 0x55);
#endif
  printRegs();
  printf("\n");

  stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)badAddr,(char **)&laddr);
  ptr = (uint32_t *) laddr;
#ifdef READSTUDY
  printf(" Read Bad\n");
  printf(" 0x%0lx: 0x%08x\n", laddr, vmeRead32(ptr));
#else
  printf(" Write Bad\n");
  vmeWrite32(ptr, 0x55);
#endif
  printRegs();
  resetCsr();
  printf("\n");

  stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)vmeAddr,(char **)&laddr);
  ptr = (uint32_t *) laddr;
#ifdef READSTUDY
  printf(" Read Good\n");
  printf(" 0x%0lx: 0x%08x\n", laddr, vmeRead32(ptr));
#else
  printf(" Write Good\n");
  vmeWrite32(ptr, 0x55);
#endif
  printRegs();
  printf("\n");

  stat = vmeBusToLocalAdrs(0x39,(char *)(unsigned long)badAddr,(char **)&laddr);
  ptr = (uint32_t *) laddr;
#ifdef READSTUDY
  printf(" Read Bad\n");
  printf(" 0x%0lx: 0x%08x\n", laddr, vmeRead32(ptr));
#else
  printf(" Write Bad\n");
  vmeWrite32(ptr, 0x55);
#endif
  printRegs();
  resetCsr();

  printf("------------------------------------------------------------\n");
  printf("\n\n");

  vmeCloseDefaultWindows();

  exit(0);
}
