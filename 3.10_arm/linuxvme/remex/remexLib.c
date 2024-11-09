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
 *     Server side routines for the remex Host.
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>
#include "remexLib.h"
#include "cMsg.h"

/* #define DEBUG */

#define NARGS 8

#define FUNCT_WITH_ARGS(_FUNC,A0,A1,A2,A3,A4,A5,A6,A7)			\
  (*(_FUNC##_funcPtr)) (A7##args[0],A6##args[1],A5##args[2],A4##args[3],A3##args[4],A2##args[5],A1##args[6],A0##args[7])

#define FUNCTION_CALL(_FUNC) FUNCT_WITH_ARGS(_FUNC,i,i,i,i,i,i,i,i)

#define VARIABLE_CALL(_FUNC) (*(_FUNC##_funcPtr))

/* test function */
/* NOTE: Cannot handle doubles for arguments... must use floats */
float
plinko(float fl,int il)
{
  printf("float  = %f\n",fl);
  printf("int    = %d\n",il);

  return (float)fl;
}

/* Another test function */
void
antivegas(int times)
{
  int i;
  for(i=0; i<times; i++)
    printf("%5d: What happens in %s...\n",i,__FUNCTION__);
}

int
ourOldSow()
{
  return 4;
}

/* test variable */
float dreamsare = 12341.12314;
double thelays  = 741781.1278741248791;

/* host names to respond to */
#define MAX_RESPONDER_NAMES 5
#define MAX_RESPONDER_CHARS 253
static char responderName[MAX_RESPONDER_NAMES][MAX_RESPONDER_CHARS] =
  {"","","","",""};
static int nresponderNames = 0;

/* cMsg items */
static void *domainId;
static void *unSubHandle[MAX_RESPONDER_NAMES];
static char cmsgUDL[300] = "";
static char cmsgPassword[300] = "";

/* Symbol table handler */
static void *handler;

/* has remex been initialized? yes=1 */
static int remexInitd = 0;

/* rediection-pipe globals */
static int remexDoRedirect=0;
static pthread_t      remexRedirectPth;
static int remexPipeFD[2];
static int remexBufFD[2];
static void remexStartRedirectionThread(void);
static void remexRedirect(void);

/* static prototypes */
static void remexCallback(void *msg, void *arg);
static int remexExecuteFunction(char *string, int lockFlag,
				enum varReturnType retType,
				struct varReturnVal *retVal,
				enum remexErrCode *errCode);
static int remexGetVar(char *string, int lockFlag,
		       enum varReturnType retType,
		       struct varReturnVal *retVal,
		       enum remexErrCode *errCode);
static int remexLoadLibrary(char *string, int lockFlag,
			    enum varReturnType retType,
			    struct varReturnVal *retVal,
			    enum remexErrCode *errCode);

typedef void            (*VOID_FUNCTIONPTR) ();
typedef int             (*INT32_FUNCTIONPTR) ();
typedef long            (*LONG_FUNCTIONPTR) ();
typedef short           (*INT16_FUNCTIONPTR) ();
typedef unsigned int    (*UINT32_FUNCTIONPTR) ();
typedef unsigned short  (*UINT16_FUNCTIONPTR) ();
typedef double          (*DOUBLE_FUNCTIONPTR) ();
typedef float           (*FLOAT_FUNCTIONPTR) ();

/* mutex Lock/Unlock function pointers */
static LONG_FUNCTIONPTR remexMutexLock;
static LONG_FUNCTIONPTR remexMutexUnlock;
static int lockPresent=0;

/*
 * remexAddName
 *    - Add a name that the "host" will respond to
 *
 *   RETURNS: the current number of valid host names, -1 if error.
 */

int
remexAddName(char *sHostname)
{
  int err, debug=1;
  int iresp;

  if(sHostname== NULL)
    {
      printf("%s: ERROR: Name is NULL\n",__FUNCTION__);
      return -1;
    }

  if(nresponderNames>MAX_RESPONDER_NAMES)
    {
      printf("%s: ERROR: Already have the maximum number of names (%d)\n",
	     __FUNCTION__,MAX_RESPONDER_NAMES);
      return -1;
    }

  if(strlen(sHostname) > MAX_RESPONDER_CHARS)
    {
      printf("%s: %s will be truncated to fit %d characters\n",
	     __FUNCTION__,sHostname,MAX_RESPONDER_CHARS);
    }

  char tmprName[MAX_RESPONDER_CHARS];
  strncpy(tmprName,sHostname,MAX_RESPONDER_CHARS);

  /* Check to see if this entry already exists in the list */
  for(iresp = 0; iresp<nresponderNames; iresp++)
    {
      if(strcmp(tmprName,responderName[iresp])==0)
	{
	  printf("%s: WARN: %s already added (ignoring this one)\n",__FUNCTION__,tmprName);
	  return nresponderNames;
	}
    }

  strncpy(responderName[nresponderNames],tmprName,253);
  nresponderNames++;

  if(remexInitd==1)
    {
      char tmpName[255] = "to";
      strncat(tmpName,responderName[nresponderNames-1],MAX_RESPONDER_CHARS+2);
      err = cMsgSubscribe(domainId,
			  "*",
			  tmpName,
			  remexCallback,
			  NULL, NULL, &unSubHandle[nresponderNames-1]);
      if (err != CMSG_OK) {
	if (debug) {
          printf("cMsgSubscribe: %s\n",cMsgPerror(err));
	}
	exit(1);
      }
    }

  return nresponderNames;
}

/*
 * remexPrintNames
 *    - Print the names in the responderName array
 *
 */

void
remexPrintNames()
{
  int iresp;

  if(nresponderNames==0)
    {
      printf("%s: No names added\n",__FUNCTION__);
      return;
    }

  printf("%s: ",__FUNCTION__);
  if(remexInitd==0)
    printf(" NOT YET listening for (must run remexInit)... \n");
  else
    printf(" Listening for...\n");
  for(iresp = 0; iresp < nresponderNames; iresp++)
    {
      printf("  %2d: %s\n", iresp, responderName[iresp]);
    }

}

/*
 * remexSetCmsgPassword
 *   - Set the cMsg Password based on the supplied password
 *
 */

int
remexSetCmsgPassword(char *password)
{
  if(password==NULL)
    {
      printf("%s: ERROR: password is NULL\n",__FUNCTION__);
      return -1;
    }

  if(remexInitd==1)
    {
      printf("%s: ERROR: This routine must be called prior to remexInit\n",
	     __FUNCTION__);
      return -1;
    }

  strncat(cmsgPassword,password,253);

  return 0;
}

/*
 * remexSetCmsgServer
 *   - Set the cMsg UDL based on the supplied cMsg server name
 *
 */

int
remexSetCmsgServer(char *servername)
{
  if(servername==NULL)
    {
      printf("%s: ERROR: servername is NULL\n",__FUNCTION__);
      return -1;
    }

  if(remexInitd==1)
    {
      printf("%s: ERROR: This routine must be called prior to remexInit\n",
	     __FUNCTION__);
      return -1;
    }

  strcpy(cmsgUDL,"cMsg://");
  strncat(cmsgUDL,servername,253);
  strcat(cmsgUDL,"/cMsg/remex");

  return 0;
}

/*
 * remexSetRedirect
 * - Set the decision to redirect the standard output to the client
 *  choice = 1 : output to client
 *           0 : output on server
 *
 */

int
remexSetRedirect(int choice)
{
  int stat;
  if(choice<0 || choice>1)
    {
      printf("%s: ERROR: Invalid choice (%d).  Must be 0 (server) or 1 (client).",
	     __FUNCTION__,choice);
      return -1;
    }

  if(choice==1)
    {
      if(remexDoRedirect==1)
	{
#ifdef REDIRECTERROR
	  printf("%s: ERROR: Redirection already enabled\n",__FUNCTION__);
#endif
	  return -1;
	}

      printf("%s: Enabling Redirection\n",__FUNCTION__);
      /* Create the pipe file descriptors */
      stat = pipe(remexPipeFD);
      if(stat==-1)
	{
	  printf("%s: ERROR: Creating redirection pipe\n",__FUNCTION__);
	  perror("pipe");
	  return -1;
	}

      stat = pipe(remexBufFD);
      if(stat == -1)
	{
	  printf("%s: ERROR: Creating redirection buffer pipe\n",__FUNCTION__);
	  perror("pipe");
	  return -1;
	}

      /* Spawn the redirection-pipe pthread */
      remexStartRedirectionThread();

    }
  else
    {
      if(remexDoRedirect==0)
	{
#ifdef REDIRECTERROR
	  printf("%s: ERROR: Redirection already disabled\n",__FUNCTION__);
#endif
	  return -1;
	}

      printf("%s: Disabling Redirection\n",__FUNCTION__);

      /* Kill the redirection-pipe pthread, if it's running */
      void *res=0;

      if(pthread_cancel(remexRedirectPth)<0)
	perror("pthread_cancel");

      write(remexPipeFD[0],"",0);

      if(pthread_join(remexRedirectPth,&res)<0)
	perror("pthread_join");
      if (res == PTHREAD_CANCELED)
	printf("%s: Redirecion thread canceled\n",__FUNCTION__);
      else
	printf("%s: ERROR: Redirection thread NOT canceled\n",__FUNCTION__);
    }

  remexDoRedirect=choice;

  return 0;
}

static void
remexStartRedirectionThread(void)
{
  int status;

  status =
    pthread_create(&remexRedirectPth,
		   NULL,
		   (void*(*)(void *)) remexRedirect,
		   (void *)NULL);

  if(status!=0)
    {
      printf("%s: ERROR: Redirection Thread could not be started.\n",
	     __FUNCTION__);
      printf("\t pthread_create returned: %d\n",status);
    }

}

static void
remexRedirect(void)
{
  int rBytes=0, remB=0;
  int stat=0;
  char msg_buf[REDIRECT_MAX_SIZE];

  prctl(PR_SET_NAME,"remexRedirection");


#ifdef DEBUG
  printf("%s: Started..\n",__FUNCTION__);
#endif

  /* Do this until we're told to stop */
  while(1)
    {
      remB=0; rBytes=0;
      memset(msg_buf,0,REDIRECT_MAX_SIZE*sizeof(char));

#ifdef DEBUG
      printf("%s: Waiting for output\n",__FUNCTION__);
#endif
      while( (rBytes = read(remexPipeFD[0], (char *)&msg_buf[remB], 100)) > 0)
        {
	  if(remB<REDIRECT_MAX_SIZE*sizeof(char))
	    remB += rBytes;
#ifdef DEBUG
	  else
	    {
	      printf("%s: Got REDIRECT_MAX_SIZE (%d >= %d)\n",
		     __FUNCTION__,remB, REDIRECT_MAX_SIZE);
	    }
#endif
        }

#ifdef DEBUG
      printf("%s: <<%s>>\n",__FUNCTION__,msg_buf);
#endif

      pthread_testcancel();

      close(remexPipeFD[0]);

      /* Write the message back to the routine that creates the payload */
      write(remexBufFD[1],msg_buf,remB);
      close(remexBufFD[1]);

      /* Reopen the redirection pipe for future ExecuteFunction commands */
      stat = pipe(remexPipeFD);
      if(stat == -1)
	perror("pipe");

    }
}

void
remexPrintCmsgUDL()
{
  printf("%s: UDL = %s",__FUNCTION__,cmsgUDL);
}

void
remexInit(char *sHostname, int useSystemHostname)
{
  char myName[253];
  char *myDescription = "Executer of Commands from a Remote Sender";
  char *UDL     = "cMsg://multicast/cMsg/remex"; /* default to multicast */
  int  err, errHN, debug=1;
  long res;
  int iresp;

  /* Set the cMsg client name... should be unique */
  /* Try and get the system host name.. */
  char tmpHost[253];
  errHN = gethostname(tmpHost,253);
  if(errHN)
    {
      perror("gethostname");
      printf("%s: Unable to use system host name for cMsg client\n",__FUNCTION__);
      if(sHostname==NULL)
	{
	  printf("%s: ERROR: gethostname failed.  Must specify hostname\n",
		 __FUNCTION__);
	  return;
	}
      printf(" .. Will use %s\n",sHostname);
      strncpy(myName,sHostname,253);
    }
  else
    {
      strncpy(myName,tmpHost,253);
    }

  /* Now set the cMsg UDL */
  if(strcmp(cmsgUDL,"")==0)
    strncpy(cmsgUDL,UDL,300);

  strcat(cmsgUDL,"?multicastTO=5");

  if(strcmp(cmsgPassword,"")!=0)
    {
      strcat(cmsgUDL,"&cmsgpassword=");
      strncat(cmsgUDL,cmsgPassword,253);
    }

  /* Connect to cMsg server */
  err = cMsgConnect(cmsgUDL, (char *)myName, myDescription, &domainId);
  if (err != CMSG_OK) {
    if (debug) {
      printf("cMsgConnect: %s\n",cMsgPerror(err));
    }
    return;
  }

  /* start receiving messages */
  cMsgReceiveStart(domainId);

  /* add the argument sHostname to the host list */
  if(sHostname != NULL)
    remexAddName(sHostname);

  if(useSystemHostname==1)
    {
      char tmpHost[253];
      err = gethostname(tmpHost,253);
      if(err)
	{
	  perror("gethostname");
	  printf("%s: Unable to use system host name\n",__FUNCTION__);
	}
      else
	{
	  remexAddName(tmpHost);
	}
    }
  /* Loop through the host list and subscribe to each */
  char tmpName[255];
  for(iresp = 0 ; iresp < nresponderNames; iresp++)
    {
      strncpy(tmpName,"to",255);
      strncat(tmpName,responderName[iresp],MAX_RESPONDER_CHARS+2);
      err = cMsgSubscribe(domainId,
			  "*",
			  tmpName,
			  remexCallback,
			  NULL, NULL, &unSubHandle[iresp]);
      if (err != CMSG_OK) {
	if (debug) {
          printf("cMsgSubscribe: %s\n",cMsgPerror(err));
	}
	return;
      }

    }

  remexInitd=1;
  remexPrintNames();

  /* Open up the symbol table now... */
  handler = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
  if(handler == 0)
    {
      perror("dlopen");
      printf("%s: ERROR: dlopen failed on >%s<\n",__FUNCTION__,dlerror());
    }

  /* Check to see if we have jvme library loaded by checking for the
     vmeBusLock()/vmeBusUnlock() routines */
  res = (long)dlsym((void *)handler, "vmeBusLock");

  if((res != (-1)) && (res != 0))
    {
      printf("%s: Found vmeBusLock()\n",__FUNCTION__);
      remexMutexLock = (LONG_FUNCTIONPTR) res;

      res = (long)dlsym((void *)handler, "vmeBusUnlock");
      if((res != (-1)) && (res != 0))
	{
	  printf("%s: Found vmeBusUnlock()\n",__FUNCTION__);
	  remexMutexUnlock = (LONG_FUNCTIONPTR) res;
	  printf("%s: VME Bus Locking will be used if requested\n",__FUNCTION__);
	  lockPresent=1;
	}
      else
	{
	  printf("%s: ERROR: >%s()< routine not found\n",__FUNCTION__,"vmeBusUnlock()");
	  remexMutexUnlock=NULL;
	  remexMutexLock=NULL;
	  lockPresent=0;
	}
    }
  else
    {
      /* Check to see if we have VTP library loaded by checking for the
	 vtpLock()/vtpUnlock() routines */
      res = (long)dlsym((void *)handler, "vtpLock");

      if((res != (-1)) && (res != 0))
	{
	  printf("%s: Found vtpLock()\n",__FUNCTION__);
	  remexMutexLock = (LONG_FUNCTIONPTR) res;

	  res = (long)dlsym((void *)handler, "vtpUnlock");
	  if((res != (-1)) && (res != 0))
	    {
	      printf("%s: Found vtpUnlock()\n",__FUNCTION__);
	      remexMutexUnlock = (LONG_FUNCTIONPTR) res;
	      printf("%s: VTP Locking will be used if requested\n",__FUNCTION__);
	      lockPresent=1;
	    }
	  else
	    {
	      printf("%s: ERROR: >%s()< routine not found\n",__FUNCTION__,"vtpUnlock()");
	      remexMutexUnlock=NULL;
	      remexMutexLock=NULL;
	      lockPresent=0;
	    }
	}
      else
	{
	  printf("%s: WARNING: Shared Mutex routines not found\n",__FUNCTION__);
	  remexMutexUnlock=NULL;
	  remexMutexLock=NULL;
	  lockPresent=0;
	}
    }

  if(dlclose((void *)handler) != 0)
    {
      perror("dlclose");
    }

}


void
remexClose()
{
  int err, debug=1;

  if(remexInitd==1)
    {
      /* Close the redirection thread */
      remexSetRedirect(0);

      /* Disconnect from cMsg */
      err = cMsgDisconnect(&domainId);
      if (err != CMSG_OK) {
	if (debug) {
          printf("err = %d, %s\n",err, cMsgPerror(err));
	}
      }
    }

  remexInitd=0;

}

/*************************************************************
 * remexCallback
 *   - cMsg callback routine
 *     Handles all incoming requests
 *
 */

static void
remexCallback(void *msg, void *arg)
{
  char *msubject = "Unknown";
  char *command;
  enum varReturnType rettype = rVOID;
  struct varReturnVal myretVal;
  int res=0;
  enum remexErrCode errCode = REMEX_ERRCODE_SUCCESS;
  int useMutexLock=0;
  int useRedirection=0;

  /* Create a response to the incoming message */
  void *sendMsg = cMsgCreateResponseMessage(msg);
  /*
   * If we've run out of memory, msg is NULL, or
   * msg was not sent from a sendAndGet() call,
   * sendMsg will be NULL.
   */
  if (sendMsg == NULL)
    {
      printf("%s: not me... return\n",__FUNCTION__);
      cMsgFreeMessage(&msg);
      return;
    }

  myretVal.rFloat = 0.;
  myretVal.rDouble = 0.;
  myretVal.rInt32 = 0;
  myretVal.rInt16 = 0;
  myretVal.rUint32 = 0;
  myretVal.rUint16 = 0;

  cMsgGetSubject(msg, (const char **)&msubject);

  cMsgGetString(msg, "variable", (const char **)&command);
  cMsgGetInt32(msg, "return_type", (int *)&rettype);
  cMsgGetInt32(msg, "useMutexLock", (int *)&useMutexLock);

  if(strcmp(msubject,"ExecuteFunction")==0)
    {
      res =
	remexExecuteFunction(command, useMutexLock, rettype, &myretVal, &errCode);

      if(remexDoRedirect)
	useRedirection=1;

    }
  else if(strcmp(msubject,"GetVar")==0)
    {
      res =
	remexGetVar(command, useMutexLock, rettype, &myretVal, &errCode);
    }
  else if(strcmp(msubject,"LoadLibrary")==0)
    {
      res =
	remexLoadLibrary(command, useMutexLock, rettype, &myretVal, &errCode);
    }

  if((res != -1) && (errCode == REMEX_ERRCODE_SUCCESS))
    {
      /* Add return values to the cmsg payload */
      switch(rettype)
	{
	case rFLOAT:
	  printf("%s = %lf\n",command,myretVal.rFloat);
	  cMsgAddFloat(sendMsg, "return_value", myretVal.rFloat);
	  break;

	case rDOUBLE:
	  printf("%s = %lf\n",command,myretVal.rDouble);
	  cMsgAddDouble(sendMsg, "return_value", myretVal.rDouble);
	  break;

	case rINT32:
	  printf("%s = 0x%x (%d)\n",command,myretVal.rInt32,myretVal.rInt32);
	  cMsgAddInt32(sendMsg, "return_value", myretVal.rInt32);
	  break;

	case rINT16:
	  printf("%s = 0x%x (%d)\n",command,myretVal.rInt16,myretVal.rInt16);
	  cMsgAddInt16(sendMsg, "return_value", myretVal.rInt16);
	  break;

	case rUINT32:
	  printf("%s = 0x%x (%d)\n",command,myretVal.rUint32,myretVal.rUint32);
	  cMsgAddUint32(sendMsg, "return_value", myretVal.rUint32);
	  break;

	case rUINT16:
	  printf("%s = 0x%x (%d)\n",command,myretVal.rUint16,myretVal.rUint16);
	  cMsgAddUint16(sendMsg, "return_value", myretVal.rUint16);
	  break;

	case rVOID:
	default:
	  printf("%s: VOID Return\n",command);
	}

      /* Add redirected output to the cmsg payload (if enabled) */
      if(useRedirection)
	{
	  int rBytes=0, remB=0, stat;
	  char msg_buf[REDIRECT_MAX_SIZE];
	  memset(msg_buf,0,REDIRECT_MAX_SIZE*sizeof(char));

	  /* Open up the read pipe from the Redirection thread */
#ifdef DEBUG
	  printf("%s: Waiting for output\n",__FUNCTION__);
#endif
	  while( (rBytes = read(remexBufFD[0], (char *)&msg_buf[remB], 1)) > 0)
	    {
	      if(remB<REDIRECT_MAX_SIZE*sizeof(char))
		remB += rBytes;
	    }

#ifdef DEBUG
	  printf("%s: <<%s>>\n",__FUNCTION__,msg_buf);
#endif
	  close(remexBufFD[0]);

	  /* Reopen the buffer pipe for future redirection */
	  stat = pipe(remexBufFD);
	  if(stat == -1)
	    perror("pipe");

	  if(strlen(msg_buf))
	    cMsgAddString(sendMsg,"redirect_output", msg_buf);
	}

    }
  else /* Error from Function or Variable call */
    {
      remexPrintErrCode(errCode);
    }

  cMsgAddInt32(sendMsg, "error_code", errCode);
  cMsgSetSubject(sendMsg,"remex Host response");
  cMsgSetType(sendMsg,"remexResponse");


  cMsgSend(domainId, sendMsg);
  cMsgFlush(domainId, NULL);

  /* free messages passed to the callback */
  cMsgFreeMessage(&msg);
  /* free messages created in this callback */
  cMsgFreeMessage(&sendMsg);

}

/***********************************************************************
 *
 * remexExecuteFunction
 *     Execute a routine with it's arguments, provided that it exists
 *     as a symbol loaded within memory.
 *
 *   args:  *string  - funtion with it's arguments (as a char string)
 *                   e.g.  printf("Hello World")
 *
 *          lockFlag - Whether (1) or not (0) to lock the
 *                     Shared Mutex before executing the function
 */

static int
remexExecuteFunction(char *string, int lockFlag,
		     enum varReturnType retType,
		     struct varReturnVal *retVal,
		     enum remexErrCode *errCode)
{
  int ii, len;
  long res;
  char *saveptr;
  char *command, *arguments;
  char *args[NARGS];
  int nargs;
  VOID_FUNCTIONPTR   void_funcPtr;
  INT32_FUNCTIONPTR  int32_funcPtr;
  INT16_FUNCTIONPTR  int16_funcPtr;
  UINT32_FUNCTIONPTR uint32_funcPtr;
  UINT16_FUNCTIONPTR uint16_funcPtr;
  DOUBLE_FUNCTIONPTR double_funcPtr;
  FLOAT_FUNCTIONPTR  float_funcPtr;
  char str[256];
  void *handler;
  float fargs[NARGS];
  int *iargs = (int *)fargs;
  char **sargs = (char **)fargs;
  int initial_io=0;

  /* parsing*/
  strncpy(str,string,255); /*strtok will modify input string, let it be local and non-constant one*/
#ifdef DEBUG
  printf("DEBUG: str >%s<\n",str);
#endif

  command = strtok_r(str,"(",&saveptr);
  if(command!=NULL)
    {
#ifdef DEBUG
      printf("DEBUG: command >%s<\n",command);
#endif
    }
  else
    {
      printf("%s: no command found in >%s<\n",__FUNCTION__,str);
      return(-1);
    }

  arguments = strtok_r(NULL,")",&saveptr);
  if(arguments!=NULL)
    {
#ifdef DEBUG
      printf("DEBUG: arguments >%s<\n",arguments);
#endif
      args[0] = strtok_r(arguments,",",&saveptr);
      nargs = 1;

      while( (nargs<NARGS) && (args[nargs]=strtok_r(NULL,",",&saveptr)) != NULL ) nargs ++;

      for(ii=0; ii<nargs; ii++)
	{
	  if( (strchr(args[ii],'"')!=NULL) || (strchr(args[ii],'\'')!=NULL) ) /*string*/
	    {
	      sargs[ii] = args[ii];
	      while(sargs[ii][0]==' ') sargs[ii] ++; /*remove leading spaces*/
	      len = strlen(sargs[ii]);
	      while(sargs[ii][len-1]==' ') len--; /*remove trailing spaces*/
	      sargs[ii][len] = '\0';
#ifdef DEBUG
	      printf("111: sargs[%2d] >%s<\n",ii,sargs[ii]);
#endif
	      sargs[ii] ++; /* remove leading quote */
	      len = strlen(sargs[ii]);
	      sargs[ii][len-1] = '\0'; /* remove trailing quote */
#ifdef DEBUG
	      printf("222: sargs[%2d] >%s<\n",ii,sargs[ii]);
#endif
	    }
	  else if(strchr(args[ii],'.')!=NULL) /*float*/
	    {
	      sscanf(args[ii],"%f",&fargs[ii]);
#ifdef DEBUG
	      printf("flo: args[%2d] >%s< %f\n",ii,args[ii],fargs[ii]);
#endif
	    }
	  else if(strchr(args[ii],'x')!=NULL) /*hex*/
	    {
	      sscanf(args[ii],"%x",&iargs[ii]);
#ifdef DEBUG
	      printf("hex: args[%2d] >%s< %d (0x%x)\n",ii,args[ii],iargs[ii],iargs[ii]);
#endif
	    }
	  else /*decimal*/
	    {
	      sscanf(args[ii],"%i",&iargs[ii]);
#ifdef DEBUG
	      printf("dec: args[%2d] >%s< %d\n",ii,args[ii],iargs[ii]);
#endif
	    }
	}
    }

#ifdef DEBUG
  printf("ints-> %d(0x%x) %d(0x%x) %d(0x%x) %d(0x%x)\n",
	 iargs[0],iargs[0],iargs[1],iargs[1],iargs[2],iargs[2],iargs[3],iargs[3]);
  printf("floats-> %f %f %f %f\n",fargs[0],fargs[1],fargs[2],fargs[3]);

  printf("%s: Executing >%s<\n",__FUNCTION__,command);
#endif

  handler = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
  if(handler == 0)
    {
      printf("%s: ERROR: dlopen failed on >%s<\n",__FUNCTION__,dlerror());
      *errCode = REMEX_ERRCODE_DLOPEN_FAILURE;
      return(-1);
    }

  /* find symbol */
  command = strtok_r(command," ",&saveptr); /*remove leading and trailing spaces if any*/
#ifdef DEBUG
  printf("DEBUG: command1 >%s<\n",command);
#endif
  if(command==NULL)
    {
      printf("%s: no command found in >%s<\n",__FUNCTION__,command);
      return(-1);
    }

  res = (long)dlsym((void *)handler, (const char *)command);

  if((res != (-1)) && (res != 0))
    {
      /*printf("INFO: >%s()< routine found\n",command)*/;
    }
  else
    {
      printf("%s: ERROR: >%s()< routine not found\n",__FUNCTION__,command);
      *errCode = REMEX_ERRCODE_FUNC_NOT_FOUND;
      return(-1);
    }

  if( (lockFlag==1) && (lockPresent==1) )
    {
#ifdef DEBUG
      printf("DEBUG: Shared mutex Lock\n");
#endif
      remexMutexLock();
    }

  fflush(stdout);
  initial_io = dup(STDOUT_FILENO);
  if(remexDoRedirect)
    {
      dup2(remexPipeFD[1],STDOUT_FILENO);
    }


  switch(retType)
    {
    case rINT32:
      int32_funcPtr = (INT32_FUNCTIONPTR) res;
      retVal->rInt32 = FUNCTION_CALL(int32);
      break;

    case rINT16:
      int16_funcPtr = (INT16_FUNCTIONPTR) res;
      retVal->rInt16 = FUNCTION_CALL(int16);
      break;

    case rUINT32:
      uint32_funcPtr = (UINT32_FUNCTIONPTR) res;
      retVal->rUint32 = FUNCTION_CALL(uint32);
      break;

    case rUINT16:
      uint16_funcPtr = (UINT16_FUNCTIONPTR) res;
      retVal->rUint16 = FUNCTION_CALL(uint16);
      break;

    case rDOUBLE:
      double_funcPtr = (DOUBLE_FUNCTIONPTR) res;
      retVal->rDouble = FUNCTION_CALL(double);
      break;

    case rFLOAT:
      float_funcPtr = (FLOAT_FUNCTIONPTR) res;
      retVal->rFloat = FUNCTION_CALL(float);
      break;

    case rVOID:
    default:
      void_funcPtr = (VOID_FUNCTIONPTR) res;
      FUNCTION_CALL(void);
    }

  fflush(stdout);
  if(remexDoRedirect)
    {
      close(remexPipeFD[1]);
    }
  dup2(initial_io,STDOUT_FILENO);

  if( (lockFlag==1) && (lockPresent==1) )
    {
#ifdef DEBUG
      printf("DEBUG: Shared mutex Unlock\n");
#endif
      remexMutexUnlock();
    }

  /* close symbol table */
  if(dlclose((void *)handler) != 0)
    {
      printf("ERROR: failed to unload >%s<\n",command);
      *errCode = REMEX_ERRCODE_DLCLOSE_FAILURE;
      return(-1);
    }

  return(0);
}

static int
remexGetVar(char *string, int lockFlag,
	    enum varReturnType retType,
	    struct varReturnVal *retVal,
	    enum remexErrCode *errCode)
{
  long res;
  void *handler;

  handler = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
  if(handler == 0)
    {
      printf("my_execute ERROR: dlopen failed on >%s<\n",dlerror());
      *errCode = REMEX_ERRCODE_DLOPEN_FAILURE;
      return(-1);
    }

  /* Not sure if the (int) should be here */
  res = (long)dlsym((void *)handler, (const char *)string);
  if((res != (-1)) && (res != 0))
    {
      /*printf("INFO: >%s()< routine found\n",string)*/;
    }
  else
    {
      printf("ERROR: >%s< variable not found\n",string);
      *errCode = REMEX_ERRCODE_VAR_NOT_FOUND;
      return(-1);
    }

  if( (lockFlag==1) && (lockPresent=1) )
    {
#ifdef DEBUG
      printf("DEBUG: Shared Mutex Lock\n");
#endif
      remexMutexLock();
    }

  switch(retType)
    {
    case rINT32:
      retVal->rInt32 = ((int *)res)[0];
      break;

    case rINT16:
      retVal->rInt16 = ((short *)res)[0];
      break;

    case rUINT32:
      retVal->rUint32 = ((unsigned int*)res)[0];
      break;

    case rUINT16:
      retVal->rUint16 = ((unsigned short*)res)[0];
      break;

    case rDOUBLE:
      retVal->rDouble = ((double *)res)[0];
      break;

    case rFLOAT:
      retVal->rFloat = ((float *)res)[0];
      break;

    case rVOID:
    default:
      break;
    }


  if( (lockFlag==1) && (lockPresent==1) )
    {
#ifdef DEBUG
      printf("DEBUG: Shared Mutex Unlock\n");
#endif
      remexMutexUnlock();
    }

  /* close symbol table */
  if(dlclose((void *)handler) != 0)
    {
      *errCode = REMEX_ERRCODE_DLCLOSE_FAILURE;
      return(-1);
    }

  return(0);
}

static int
remexLoadLibrary(char *string, int lockFlag,
		 enum varReturnType retType,
		 struct varReturnVal *retVal,
		 enum remexErrCode *errCode)
{
  int rval=0;
  void *myHandle;

  *errCode = REMEX_ERRCODE_SUCCESS;

  myHandle = dlopen(string, RTLD_NOW | RTLD_GLOBAL);
  if(myHandle==0)
    {
      perror("dlopen");
      printf("%s: ERROR: dlopen failed on >%s<\n",__FUNCTION__,dlerror());
      *errCode = REMEX_ERRCODE_DLOPEN_FAILURE;
      rval = -1;
    }

  return(rval);
}
