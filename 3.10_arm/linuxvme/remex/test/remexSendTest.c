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

void *domainId;

int 
main(int argc, char *argv[]) 
{
  printf("\n REMEX receiver test \n");

  char *myName = "remexSender";
  char *myDescription = "Sender of Commands to a Remote Executer";
  char *UDL     = "cMsg://localhost/cMsg/remex";
  char *currentUDL;
  int   err, debug=1, msgSize=0, counter=0;

  /* Connect to cMsg server */
  err = cMsgConnect(UDL, myName, myDescription, &domainId);
  if (err != CMSG_OK) {
    if (debug) {
      printf("cMsgConnect: %s\n",cMsgPerror(err));
    }
    exit(1);
  }
  
/*   err = cMsgGetDescription(domainId, (char **)&currentUDL); */
/*   if(err != CMSG_OK) */
/*     { */
/*       printf("not ok\n"); */
/*       exit(1); */
/*     } */
/*   printf("currentUDL = %s\n",currentUDL); */

  /* start receiving messages */
  cMsgReceiveStart(domainId);

  void *msg, *replyMsg;
  struct timespec timeout;
  timeout.tv_sec  = 3;
  timeout.tv_nsec = 0;
  

  msg = cMsgCreateMessage();
  cMsgSetSubject(msg, "ExecuteFunction");
  cMsgSetSubject(msg, "GetVar");
  cMsgSetType(msg, "torobot");
/*   cMsgSetText(msg,"printf(\"captain caveman\n\")"); */

  cMsgAddString(msg, "variable", "plinko");
  cMsgAddString(msg, "return_type", "int");

  err = cMsgSendAndGet(domainId, msg, &timeout, &replyMsg);
  if (err == CMSG_TIMEOUT) 
    {
      printf("TIMEOUT in GET\n");
    }
  
  else if (err != CMSG_OK) 
    {
      printf("cMsgSendAndGet: %s\n",cMsgPerror(err));
      goto end;
    }
  
  else 
    {
      char *subject, *type;
      float floater;
      unsigned int inter;
      cMsgGetSubject(replyMsg, &subject);
      cMsgGetType(replyMsg, &type);
/*       cMsgGetFloat(replyMsg, "return_value", &floater); */
/*       printf(" GOT A MESSAGE: subject = %s, type = %s floater = %lf\n",  */
/* 	     subject, type, floater); */
      cMsgGetUint32(replyMsg, "return_value", &inter);
      printf(" GOT A MESSAGE: subject = %s, type = %s inter = %d\n", 
	     subject, type, inter);
      cMsgFreeMessage(&replyMsg);
    }
  
  cMsgFreeMessage(&msg);

 end:
  
  err = cMsgDisconnect(&domainId);
  if (err != CMSG_OK) {
      if (debug) {
          printf("err = %d, %s\n",err, cMsgPerror(err));
      }
  }

  return 0;
}
