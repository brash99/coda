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
 *     Source code for the remex standalone command-line client
 *
 *----------------------------------------------------------------------------*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include "remexLib.h"

/* Function prototypes */
void usage();

int
main(int argc, char *argv[]) {

    int res;
    struct varReturnVal value;
    char *command = "-";
    static int getVariable=0;
    static int libLoad=0;
    static int useMutexLock=0;
    static char *cMsgHost = NULL;
    static char *cMsgPassword = NULL;
    static int Verbose=0;
    char *cMsgHost_env = NULL;
    char *cMsgPassword_env = NULL;
    char remexHost[253];
    int opt_param, option_index = 0;
    static enum varReturnType rType = rINT32;
    static struct option long_options[] =
      {
	/* {const char *name, int has_arg, int *flag, int val} */
	{"var", 0, &getVariable, 1},
	{"dlopen", 0, NULL, 'd'},
	{"void", 0, (int *)&rType, rVOID},
	{"float", 0, (int *)&rType, rFLOAT},
	{"double", 0, (int *)&rType, rDOUBLE},
	{"int32", 0, (int *)&rType, rINT32},
	{"int16", 0, (int *)&rType, rINT16},
	{"uint32", 0, (int *)&rType, rUINT32},
	{"uint16", 0, (int *)&rType, rUINT16},
	{"lock", 0, &useMutexLock, 1},
	{"help", 0, 0, 'h'},
	{"cmsghost", 1, NULL, 'c'},
	{"password", 1, NULL, 'p'},
	{"verbose", 0, &Verbose, 1},
	{0, 0, 0, 0}
      };


    /* Parse the commandline parameters */
    while(1)
      {
	option_index = 0;
	opt_param = getopt_long (argc, argv, "vdlhc:p:",
			      long_options, &option_index);

	if (opt_param == -1) /* No more option parameters left */
	  break;

	switch (opt_param)
	  {
	  case 0:
	    break;

	  case 'v': /* Get Variable (instead of Function) */
	    getVariable=1;
	    if(Verbose) printf("-- Get Variable\n");
	    break;

	  case 'd': /* Load library */
	    libLoad=1;
	    if(Verbose) printf("-- Load library\n");
	    break;

	  case 'l': /* use mutex Lock/Unlock */
	    useMutexLock=1;
	    if(Verbose) printf("-- Use mutex Lock\n");
	    break;

	  case 'c': /* cMsg Host */
	    cMsgHost = optarg;
	    if(Verbose) printf("-- CMSG Host (%s)\n",cMsgHost);
	    break;

	  case 'p': /* cMsg Password */
	    cMsgPassword = optarg;
	    if(Verbose) printf("-- CMSG Password (%s)\n",cMsgPassword);
	    break;

	  case 'h': /* help */
	  case '?': /* Invalid Option */
	  default:
	    usage();
	    exit(1);
	  }

      }

    if(cMsgHost==NULL)
      { /* Check for environment variable */
	cMsgHost_env = getenv("REMEX_CMSG_HOST");
	if(cMsgHost_env != NULL)
	  {
	    cMsgHost = cMsgHost_env;
	    if(Verbose) printf("-- env: CMSG Host (%s)\n",cMsgHost);
	  }

      }

    if(cMsgPassword==NULL)
      { /* Check for environment variable */
	cMsgPassword_env = getenv("REMEX_CMSG_PASSWORD");
	if(cMsgPassword_env != NULL)
	  {
	    cMsgPassword = cMsgPassword_env;
	    if(Verbose) printf("-- env: CMSG Password (%s)\n",cMsgPassword);
	  }

      }

    /* Get the command/variable to grab from the host */
    if (optind < argc)
      {
	if((argc-optind)>=2) /* user supplied remex hostname first */
	  {
	    strncpy(remexHost,argv[optind],253);
	    optind++;
	  }
	else
	  {
	    gethostname(remexHost,253);
	  }
	command = argv[optind];
	if(Verbose) printf("-- command (%s)\n",command);
	/* The rest of the arguments are ignored, if they're there */
      }

    if(strcmp(command,"-")==0)
      {
	usage();
	exit(1);
      }

    if(cMsgPassword != NULL)
      {
	remexClientSetCmsgPassword(cMsgPassword);
      }


    res = remexClientInit(NULL,cMsgHost);
    if(res==-1)
      {
	if(Verbose) printf("-- Client Initialization Failed\n");
	return 0;
      }

    remexClientUseMutexLock(useMutexLock);

    if(getVariable)
      {
	if(Verbose) printf("-- Variable ");
	switch(rType)
	  {
	  case rFLOAT:
	    if(Verbose) printf("(Float)\n");
	    res = remexClientGetFloat(remexHost,command,&(value.rFloat));
	    break;

	  case rDOUBLE:
	    if(Verbose) printf("(Double)\n");
	    res = remexClientGetDouble(remexHost,command,&(value.rDouble));
	    break;

	  case rINT16:
	    if(Verbose) printf("(Int16)\n");
	    res = remexClientGetInt16(remexHost,command,&(value.rInt16));
	    break;

	  case rUINT32:
	    if(Verbose) printf("(UInt32)\n");
	    res = remexClientGetUint32(remexHost,command,&(value.rUint32));
	    break;

	  case rUINT16:
	    if(Verbose) printf("(UInt16)\n");
	    res = remexClientGetUint16(remexHost,command,&(value.rUint16));
	    break;

	  case rINT32:
	  default:
	    if(Verbose) printf("(Int32)\n");
	    res = remexClientGetInt32(remexHost,command,&(value.rInt32));
	  }
      }
    else if(libLoad)
      {
	res = remexClientLoadLibrary(remexHost, command);
      }
    else
      {
	if(Verbose) printf("-- Function ");
	switch(rType)
	  {
	  case rFLOAT:
	    if(Verbose) printf("(Float)\n");
	    res = remexClientGetFloatFunction(remexHost,command,&(value.rFloat));
	    break;

	  case rDOUBLE:
	    if(Verbose) printf("(Double)\n");
	    res = remexClientGetDoubleFunction(remexHost,command,&(value.rDouble));
	    break;

	  case rINT32:
	    if(Verbose) printf("(Int32)\n");
	    res = remexClientGetInt32Function(remexHost,command,&(value.rInt32));
	    break;

	  case rINT16:
	    if(Verbose) printf("(Int16)\n");
	    res = remexClientGetInt16Function(remexHost,command,&(value.rInt16));
	    break;

	  case rUINT32:
	    if(Verbose) printf("(UInt32)\n");
	    res = remexClientGetUint32Function(remexHost,command,&(value.rUint32));
	    break;

	  case rUINT16:
	    if(Verbose) printf("(UInt16)\n");
	    res = remexClientGetUint16Function(remexHost,command,&(value.rUint16));
	    break;

	  default:
	    if(Verbose) printf("(Void)\n");
	    res = remexClientExecFunction(remexHost,command);
	  }
      }

    if(!res)
      {
	switch(rType)
	  {
	  case rFLOAT:
	    printf("%s = %lf\n",command,value.rFloat);
	    break;

	  case rDOUBLE:
	    printf("%s = %lf\n",command,value.rDouble);
	    break;

	  case rINT32:
	    printf("%s = 0x%x (%d)\n",command,value.rInt32,value.rInt32);
	    break;

	  case rINT16:
	    printf("%s = 0x%x (%d)\n",command,value.rInt16,value.rInt16);
	    break;

	  case rUINT32:
	    printf("%s = 0x%x (%u)\n",command,value.rUint32,value.rUint32);
	    break;

	  case rUINT16:
	    printf("%s = 0x%x (%u)\n",command,value.rUint16,value.rUint16);
	    break;

	  default:
	    printf("%s void return\n",command);
	  }
      }
    else
      {
	printf(" Host call returned ERROR\n");
      }

    remexClientDisconnect();

    exit(0);
}

/*************************************************************
 *
 * usage
 *     - just print usage info to standard out
 *
 */

void
usage()
{
  printf("\n");
  printf("Usage: remex <options> <remexHost> <command>\n");
  printf("\n");

  printf("Execute a function, or retreive a global variable from a remex Host.\n");
  printf(" If <remexHost> is omitted, the current hostname will be used.\n");
  printf(" If <options> is omitted, the Execute Function will be used.\n");
  printf("\n");

  printf("<options>:\n");

  printf("  -v, --var                      Specified <command> is a global variable.\n");
  printf("\n");

  printf("  -d, --dlopen                   Load shared library named <command>.\n");
  printf("                                  Filename must satisfy requirements of filename\n");
  printf("                                  argument of dlopen(3).\n");
  printf("\n");

  printf("  -l, --lock                     Lock and Unlock the shared VME Bus Mutex during\n");
  printf("                                  the operation.\n");
  printf("\n");

  printf("  -c, --cmsghost <cmsgHostName>  Specify <cmsgHostName> as the cMsg server.\n");
  printf("                                  If not specified, localhost will be used,\n");
  printf("                                  or whatever is specified in the\n");
  printf("                                  REMEX_CMSG_HOST environment variable.\n");
  printf("\n");

  printf("  -p, --password <cmsgPassword>  Specify <cmsgPassword> to the cMsg server.\n");
  printf("                                  If not specified, whatever is specified in\n");
  printf("                                  the REMEX_CMSG_PASSWORD environment variable\n");
  printf("                                  will be used. Otherwise, no password will be\n");
  printf("                                  used. For CODA 3.x, use the EXPID as the \n");
  printf("                                  cMsg Password.\n");
  printf("\n");

  printf("  --verbose                      Be verbose on the program execution and \n");
  printf("                                  parsing of command line arguments.\n");
  printf("\n");

  printf(" Return Values:\n");
  printf("  --void                         Do not attempt to return any value from the\n");
  printf("                                   function execution.\n");
  printf("                                   Ignored for global variables.\n");
  printf("\n");
  printf("  --float\n");
  printf("  --double\n");
  printf("  --int32\n");
  printf("  --int16\n");
  printf("  --uint32\n");
  printf("  --uint16\n");
  printf("\n");


  printf("\n");

}
