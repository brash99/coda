/*----------------------------------------------------------------------------*
 *  Copyright (c) 2013        Southeastern Universities Research Association, *
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
 *     Client side routines for the remex client.
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <unistd.h>
#include "remexLib.h"
#include "cMsg.h"

static int remexCliInitd=0;
static void *domainId;
static char cmsgUDL[300] = "";
static char cmsgPassword[300] = "";
static int useMutexLock=0;

/*
 * remexSetCmsgPassword
 *   - Set the cMsg Password based on the supplied password
 *
 */

int
remexClientSetCmsgPassword(char *password)
{
  if(password==NULL)
    {
      printf("%s: ERROR: password is NULL\n",__FUNCTION__);
      return -1;
    }

  if(remexCliInitd==1)
    {
      printf("%s: ERROR: This routine must be called prior to remexClientInit\n",
	     __FUNCTION__);
      return -1;
    }

  strncat(cmsgPassword,password,253);

  return 0;
}

/*
 * remexClientInit
 *  - Initialize the client side..
 *    clientName = cMsg subscriber name
 *        if NULL, set to hostname_pid
 *    cMsgServer = hostname of cMsg server
 *        if NULL, set to multicast
 */

int
remexClientInit(char *clientName, char *cMsgServer)
{
  char myName[253], tmpHost[253];
  char *UDL     = "cMsg://multicast/cMsg/remex?multicastTO=5"; /* default to multicast */
  char *myDescription = "Sender of Commands to a Remote Executer";
  int err, errHN, debug=1;
  pid_t currentPID;
  char sPID[10];

  if(clientName==NULL) /* Default clientName = hostname_pid */
    {
      /* Get the hostname */
      errHN = gethostname(tmpHost,253);
      if(errHN)
	{
	  perror("gethostname");
	  printf("%s: ERROR: gethostname failed.  Must specify hostname\n",
		 __FUNCTION__);
	  return -1;
	}

      strncpy(myName,tmpHost,253);

      /* Append the current PID */
      currentPID = getpid();
      sprintf(sPID,"_%d",currentPID);
      strcat(myName,sPID);

    }
  else /* User specified clientName */
    {
      strncpy(myName,clientName,253);
    }

  if(cMsgServer==NULL)
    {
      strncpy(cmsgUDL,UDL,300);
    }
  else
    {
      strcpy(cmsgUDL,"cMsg://");
      strncat(cmsgUDL,cMsgServer,253);
      strcat(cmsgUDL,"/cMsg/remex?multicastTO=5");
    }

  if(strlen(cmsgPassword)!=0)
    {
      strcat(cmsgUDL,"&cmsgpassword=");
      strncat(cmsgUDL,cmsgPassword,253);
    }

#ifdef DEBUG
  printf("%s: myName  = %s\n",__FUNCTION__,myName);
  printf("%s: cmsgUDL = %s\n",__FUNCTION__,cmsgUDL);
#endif

  /* Connect to cMsg server */
  err = cMsgConnect(cmsgUDL, myName, myDescription, &domainId);
  if (err != CMSG_OK) {
    if (debug) {
      printf("cMsgConnect:\n %s\n",cMsgPerror(err));
    }
    return -1;
  }

  cMsgReceiveStart(domainId);

  remexCliInitd=1;

  return 0;
}


static int
remexClientGet(enum remexGetType gType,
	       char *remexHost, char *variable,
	       enum varReturnType rType, struct varReturnVal *rVal)
{
  int err, retVal=0;
  void *msg, *replyMsg=NULL;
  char msgType[253];
  struct timespec timeout;

  timeout.tv_sec  = 3;
  timeout.tv_nsec = 0;

  msg = cMsgCreateMessage();

  switch(gType)
    {
    case rGetVariable:
      cMsgSetSubject(msg, "GetVar");
      break;

    case rLoadLibrary:
      cMsgSetSubject(msg, "LoadLibrary");
      break;

    case rExecuteFunction:
    default:
      cMsgSetSubject(msg, "ExecuteFunction");
      break;
    }

  strcpy(msgType,"to");
  strncat(msgType,remexHost,253);

  cMsgSetType(msg, msgType);

  cMsgAddString(msg, "variable", variable);

  cMsgAddInt32(msg, "return_type",rType);

  cMsgAddInt32(msg, "useMutexLock", useMutexLock);

  err = cMsgSendAndGet(domainId, msg, &timeout, &replyMsg);


  if (err == CMSG_TIMEOUT)
    {
      printf("cMsgSendAndGet:\n %s\n",cMsgPerror(err));
      retVal = -1;
    }

  else if (err != CMSG_OK)
    {
      printf("cMsgSendAndGet:\n %s\n",cMsgPerror(err));
      retVal = -1;
    }

  else if(replyMsg==NULL)
    {
      printf("%s: %s not subscribed at cMsg server.\n",__FUNCTION__,
	     remexHost);
      retVal = -1;
    }

  else
    {
      char *subject, *type;
      enum remexErrCode errCode;

      cMsgGetSubject(replyMsg, (const char**)&subject);
      cMsgGetType(replyMsg, (const char**)&type);

      switch(rType)
	{
	case rFLOAT:
	  cMsgGetFloat(replyMsg, "return_value", &(rVal->rFloat));
	  break;

	case rDOUBLE:
	  cMsgGetDouble(replyMsg, "return_value", &(rVal->rDouble));
	  break;

	case rINT32:
	  cMsgGetInt32(replyMsg, "return_value", &(rVal->rInt32));
	  break;

	case rINT16:
	  cMsgGetInt16(replyMsg, "return_value", &(rVal->rInt16));
	  break;

	case rUINT32:
	  cMsgGetUint32(replyMsg, "return_value", &(rVal->rUint32));
	  break;

	case rUINT16:
	  cMsgGetUint16(replyMsg, "return_value", &(rVal->rUint16));
	  break;

	case rVOID:
	default:
	  break;
	}

      /* Redirection output, for ExecuteFunction */
      if(gType == rExecuteFunction)
	{
	  const char *msg_buf[REDIRECT_MAX_SIZE];
	  int stat=0;
	  stat = cMsgGetString(replyMsg,"redirect_output",(const char **)&msg_buf);
	  if(stat == CMSG_OK)
	    {
	      printf("-- Redirected Output --\n");
	      printf("%s",*msg_buf);
	      printf("-- Redirected Output --\n");
	    }
	  else
	    {
	      printf("-- No Redirected Output --\n");
	    }
	}

      cMsgGetInt32(replyMsg, "error_code", (int *)&errCode);
#ifdef DEBUG
      printf(" GOT A MESSAGE: subject = %s, type = %s\n\terrCode = %d\n",
	     subject, type, errCode);
#endif
      if(errCode != REMEX_ERRCODE_SUCCESS)
	{
	  retVal = -1;
	  remexPrintErrCode(errCode);
	}
      else
	retVal = 0;
      cMsgFreeMessage(&replyMsg);
    }

  cMsgFreeMessage(&msg);

  return retVal;
}

int
remexClientExecFunction(char *remexHost, char *command)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rVOID, &rval);

  return res;
}

int
remexClientGetFloatFunction(char *remexHost, char *command, float *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rFLOAT, &rval);
  if(res==-1)
    *value = 0.;
  else
    *value = rval.rFloat;

  return res;
}

int
remexClientGetDoubleFunction(char *remexHost, char *command, double *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rDOUBLE, &rval);
  if(res==-1)
    *value = 0.;
  else
    *value = rval.rDouble;

  return res;
}

int
remexClientGetInt32Function(char *remexHost, char *command, int *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rINT32, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rInt32;

  return res;
}

int
remexClientGetInt16Function(char *remexHost, char *command, short *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rINT16, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rInt16;

  return res;
}

int
remexClientGetUint32Function(char *remexHost, char *command, unsigned int *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rUINT32, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rUint32;

  return res;
}

int
remexClientGetUint16Function(char *remexHost, char *command, unsigned short *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rExecuteFunction, remexHost, command, rUINT16, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rUint16;

  return res;
}

int
remexClientGetFloat(char *remexHost, char *variable, float *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rGetVariable, remexHost, variable, rFLOAT, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rFloat;

  return res;
}

int
remexClientGetDouble(char *remexHost, char *variable, double *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rGetVariable, remexHost, variable, rDOUBLE, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rDouble;

  return res;
}

int
remexClientGetInt32(char *remexHost, char *variable, int *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rGetVariable, remexHost, variable, rINT32, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rInt32;

  return res;
}

int
remexClientGetInt16(char *remexHost, char *variable, short *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rGetVariable, remexHost, variable, rINT16, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rInt16;

  return res;
}

int
remexClientGetUint32(char *remexHost, char *variable, unsigned int *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rGetVariable, remexHost, variable, rUINT32, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rUint32;

  return res;
}

int
remexClientGetUint16(char *remexHost, char *variable, unsigned short *value)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rGetVariable, remexHost, variable, rUINT16, &rval);
  if(res==-1)
    *value = 0;
  else
    *value = rval.rUint16;

  return res;
}

int
remexClientLoadLibrary(char *remexHost, char *libname)
{
  int res;
  struct varReturnVal rval;

  res = remexClientGet(rLoadLibrary, remexHost, libname, rVOID, &rval);

  return res;
}

void
remexClientUseMutexLock(int useLock)
{
  if(useLock)
    useMutexLock=1;
  else
    useMutexLock=0;
}

int
remexClientDisconnect()
{
  int err, debug=1;

  usleep(10000); // Artificial delay to avoid some cMsg write failure messages
  err = cMsgDisconnect(&domainId);
  if (err != CMSG_OK) {
      if (debug) {
          printf("cMsgDisconnect:\n (%d) %s\n",err, cMsgPerror(err));
      }
  }

  return 0;
}

void
remexPrintErrCode(enum remexErrCode errCode)
{

  switch(errCode)
    {
    case REMEX_ERRCODE_SUCCESS:
      printf("%s: (%d): Success\n",
	     __FUNCTION__,errCode);
      break;

    case REMEX_ERRCODE_DLOPEN_FAILURE:
      printf("%s: (%d): Error calling dlopen(..) to load current symbols.\n",
	     __FUNCTION__,errCode);
      break;

    case REMEX_ERRCODE_DLCLOSE_FAILURE:
      printf("%s: (%d): Error calling dlclose() to close symbol handle\n",
	     __FUNCTION__,errCode);
      break;

    case REMEX_ERRCODE_FUNC_NOT_FOUND:
      printf("%s: (%d): Error calling dlsym(..).  Function not found.\n",
	     __FUNCTION__,errCode);
      break;

    case REMEX_ERRCODE_VAR_NOT_FOUND:
      printf("%s: (%d): Error calling dlsym(..).  Variable not found.\n",
	     __FUNCTION__,errCode);
      break;

    default:
      printf("%s: (%d) Undefined Error Code\n",
	     __FUNCTION__, errCode);

    }

}
