#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "jvme.h"

#define NBUFF 100

DMA_MEM_ID vmeIN;

int
main(void)
{
  int i, isValid = 0, randy=0;
  int status = 0;
  DMANODE *outEvent[NBUFF];
  unsigned long testMemBase = 0, base = 0, width = 0;


  status = vmeOpenDefaultWindows();
  if(status != OK)
    goto CLOSE;

  vmeIN  = dmaPCreate("vmeIN",1*1024*1024,NBUFF,0);

  dmaPStatsAll();
  dmaPReInitAll();

  srand(time(NULL));
  randy = (int)(((float) rand()/((float) RAND_MAX)) * NBUFF);
  printf("randy = %d\n", randy);

  for (i = 0; i < NBUFF; i++)
    {
      outEvent[i] = dmaPGetItem(vmeIN);

      base = outEvent[i]->physMemBase +
	(long)&(outEvent[i]->data[0]) - outEvent[i]->partBaseAdr;

      width = outEvent[i]->part->size;

      if(i == randy)
	testMemBase = base;

      printf("  base = 0x%0lx   width = 0x%0lx     %s\n",
	     base, width,
	     (i == randy)?"***********":"");

      dmaPFreeItem(outEvent[i]);
    }

  isValid = dmaPMemIsValid(testMemBase + 0x1800);

  printf("  0x%lx is%s valid\n",
	 testMemBase + 0x1800,
	 isValid ? "" : " NOT");

  isValid = dmaPMemIsValid(testMemBase - 0x1800);

  printf("  0x%lx is%s valid\n",
	 testMemBase - 0x1800,
	 isValid ? "" : " NOT");


  dmaPStatsAll();

 CLOSE:
  dmaPFreeAll();
  dmaPStatsAll();
  vmeCloseDefaultWindows();

    printf("randy = %d\n", randy);

  exit(EXIT_SUCCESS);
}

/*
  Local Variables:
  compile-command: "make -k -B tree_search"
  End:
 */
