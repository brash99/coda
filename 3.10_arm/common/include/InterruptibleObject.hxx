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


#ifndef _InterruptibleObject_hxx
#define _InterruptibleObject_hxx


#include <RunObject.hxx>
#include <InterruptService.hxx>


namespace codaObject {

using namespace std;
using namespace cmsg;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Extends RunObject to add interrupt capability.  Has an InterruptService.
 *
 * @warning User routines should be pthread-cancelable in case abort/reset command is received.
 */
class InterruptibleObject : public RunObject {
  
public:
  InterruptibleObject(const string& UDL, const string& name, const string& descr, InterruptService *intSvc = NULL,
                      const string &codaClass = "USER", const cMsgSubscriptionConfig *scfg = NULL) throw(CodaException);
  ~InterruptibleObject(void) throw();


protected:
  virtual bool download(const string& s) throw(CodaException);
  virtual bool prestart(const string& s) throw(CodaException);
  virtual bool go(const string& s) throw(CodaException);
  virtual bool pause(const string& s) throw(CodaException);
  virtual bool resume(const string& s) throw(CodaException);
  virtual bool end(const string& s) throw(CodaException);
  virtual bool reset(const string& s) throw(CodaException);


public:
  virtual bool interrupt(unsigned int flag) throw(CodaException);


protected:
  virtual bool userInterrupt(unsigned int flag) throw(CodaException);


protected:
  dllIntFuncPtr dllInterrupt;    /**<Pointer to interrupt function in dll.*/
  InterruptService *intSvc;      /**<Pointer to interrupt service.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject

#endif /* _InterruptibleObject_hxx */
