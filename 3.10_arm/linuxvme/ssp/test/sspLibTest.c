/*
 * File:
 *    sspLibTest.c
 *
 * Description:
 *    Test ssp library
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "sspLib.h"

int main()
{
  int stat;
  int iFlag=0;
	
  vmeSetQuietFlag(1);

  stat = vmeOpenDefaultWindows();

  if(stat!=OK)
    goto CLOSE;

  iFlag = SSP_INIT_NO_INIT;

  sspInit(3<<19,1<<19,18,iFlag);

  sspGStatus(0);

 CLOSE:
  vmeOpenDefaultWindows();
	
  return 0;
}
