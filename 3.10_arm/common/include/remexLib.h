#ifndef __REMEXLIBH__
#define __REMEXLIBH__
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
 *     A common header file for the remex Host and Client
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#define REDIRECT_MAX_SIZE 200000

enum remexGetType
  {
    rExecuteFunction,
    rGetVariable,
    rLoadLibrary
  };

enum remexErrCode 
  {
    REMEX_ERRCODE_SUCCESS,
    REMEX_ERRCODE_DLOPEN_FAILURE,
    REMEX_ERRCODE_DLCLOSE_FAILURE,
    REMEX_ERRCODE_FUNC_NOT_FOUND,
    REMEX_ERRCODE_VAR_NOT_FOUND
  };

enum varReturnType
  {
    rVOID,
    rFLOAT,
    rDOUBLE,
    rINT32,
    rINT16,
    rUINT32,
    rUINT16
  };

struct varReturnVal
{
  float rFloat;
  double rDouble;
  int rInt32;
  short rInt16;
  unsigned int rUint32;
  unsigned short rUint16;
};

/* Host prototypes */
int  remexAddName(char *sHostname);
void remexPrintNames();
int  remexSetCmsgPassword(char *password);
int  remexSetCmsgServer(char *servername);
int  remexSetRedirect(int choice);
void remexPrintCmsgUDL();
void remexInit(char *sHostname, int useSystemHostname);
void remexClose();

/* Client prototypes */
int  remexClientSetCmsgPassword(char *password);
int  remexClientInit(char *clientName, char *cMsgServer);

int  remexClientExecFunction(char *remexHost, char *command);
int  remexClientGetFloatFunction(char *remexHost, char *command, float *value);
int  remexClientGetDoubleFunction(char *remexHost, char *command, double *value);
int  remexClientGetInt32Function(char *remexHost, char *command, int *value);
int  remexClientGetInt16Function(char *remexHost, char *command, short *value);
int  remexClientGetUint32Function(char *remexHost, char *command, unsigned int *value);
int  remexClientGetUint16Function(char *remexHost, char *command, unsigned short *value);
int  remexClientGetFloat(char *remexHost, char *variable, float *value);
int  remexClientGetDouble(char *remexHost, char *variable, double *value);
int  remexClientGetInt32(char *remexHost, char *variable, int *value);
int  remexClientGetInt16(char *remexHost, char *variable, short *value);
int  remexClientGetUint32(char *remexHost, char *variable, unsigned int *value);
int  remexClientGetUint16(char *remexHost, char *variable, unsigned short *value);
void remexClientUseVmeBusLock(int useLock);
int  remexClientLoadLibrary(char *remexHost, char *libname);

int  remexClientDisconnect();
void remexPrintErrCode(enum remexErrCode errCode);

#endif /* __REMEXLIBH__ */
