/*
 * File:
 *    testTreeSearch.c
 *
 * Description:
 *    Test the tree search algorithm in dmaPlist
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "jvme.h"

extern DMANODE *the_event;
extern unsigned int *dma_dabufp;

int
main(int argc, char *argv[])
{
  DMA_MEM_ID vmeIN,vmeOUT;
  DMANODE *outEvent;
  int isValid, offset = 0;
  unsigned long checkaddr = 0;

  vmeOpenDefaultWindows();


  vmeIN = dmaPCreate("vmeIN",4190000,3,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);

  dmaPStatsAll();

  dmaPReInitAll();

  GETEVENT(vmeIN,0);

  if(the_event != 0)
    {
      offset = ((unsigned long)dma_dabufp - the_event->partBaseAdr);
      printf("offset = 0x%x\n", offset);
      offset += 0x3ff774;
      printf("offset = 0x%x\n", offset);
      checkaddr = the_event->physMemBase + offset;
    }

  isValid = dmaPMemIsValid(checkaddr);

  if(isValid == 0)
    {
      printf("%s: ERROR: Invalid DMA Destination Address (0x%lx).\n",
	     __func__, (the_event->physMemBase + offset));
      printf("    Does not fall within allocated range.\n");
    }
  else if (isValid == -1)
    {
      printf("WARN: Tree Search init\n");
    }

  if(the_event)
    {
      PUTEVENT(vmeOUT);
      outEvent = dmaPGetItem(vmeOUT);

      dmaPFreeItem(outEvent);
    }


  dmaPFreeAll();
  vmeCloseDefaultWindows();

  exit(0);
}

/*
  Local Variables:
  compile-command: "make -B testTreeSearch"
  End:
 */
