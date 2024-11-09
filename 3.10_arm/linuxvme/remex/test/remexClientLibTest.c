/*
 * File:
 *    remexSendTest.c
 *
 * Description:
 *    Receiver (executer) side test
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cMsg.h"
#include "jvme.h"
#include "remexLib.h"

int 
main(int argc, char *argv[]) 
{
  int res;
  float vFloat;
  double vDouble;
  int vInt32;
  short vInt16;
  unsigned int vUint32;
  unsigned short vUint16;

  printf("\n REMEX Sender test \n");

  res = remexClientInit(NULL,"dafarm28");
  if(res==-1)
    return 0;

  res = remexClientLoadLibrary("robot","libjvme.so");
  res = remexClientLoadLibrary("robot","libfadc.so");

  res = remexClientExecFunction("robot","faGStatus()");

  remexClientDisconnect();

  return 0;
}
