/*----------------------------------------------------------------------------*
*  Copyright (c) 2005        Southeastern Universities Research Association, *
*                            Thomas Jefferson National Accelerator Facility  *
*                                                                            *
*    This software was developed under a United States Government license    *
*    described in the NOTICE file included as part of this distribution.     *
*                                                                            *
*    E.Wolin, 25-Feb-2005, Jefferson Lab                                     *
*                                                                            *
*    Authors: Elliott Wolin                                                  *
*             wolin@jlab.org                    Jefferson Lab, MS-6B         *
*             Phone: (757) 269-7365             12000 Jefferson Ave.         *
*             Fax:   (757) 269-5519             Newport News, VA 23606       *
*
*----------------------------------------------------------------------------*/


#ifndef _RunObject_h
#define _RunObject_h


#ifndef VXWORKS
#include <dlfcn.h>
#endif


namespace codaObject {

using namespace std;


/**
 * Used internally to access dll functions.
 */ 
typedef int (*dllFuncPtr)(const char *s, void *sysArg, void *userArg);

/**
 * Used internally to access dll functions.
 */ 
typedef int (*dllIntFuncPtr)(unsigned int flag, void *sysArg, void *userArg);

/**
 * Used internally to pass information to the dll.
 */ 
struct dllSysStruct {
  int *pRunNumber;
  unsigned int *pDataBuffer;
};


}  // namespace codaObject

#endif /* _RunObject_h */
