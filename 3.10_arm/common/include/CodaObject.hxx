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

#ifndef _CodaObject_hxx
#define _CodaObject_hxx


#include <cMsg.hxx>
#include <CodaException.hxx>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <algorithm>


#define MIN(a,b) ((a)<(b))?(a):(b)
#define MAX(a,b) ((a)>(b))?(a):(b)


// for daLogMsg
#define DALOG_INFO   1
#define DALOG_WARN   5
#define DALOG_ERROR  9
#define DALOG_SEVERE 13


/**
 * The entire codaObject package resides in the codaObject namespace.
 */
namespace codaObject {

using namespace std;
using namespace cmsg;


/** 
 * @mainpage codaObject Package
 * @author Elliott Wolin
 * @version 1.0
 * @date 21-Feb-2007
 *
 * @section intro Introduction.
 * The codaObject package is an object-oriented framework for building DAQ components that can interact with the 
 * CODA run control facility.
 *
 * Software compontents participating in a run control session can be
 * built from one of a number of coda objects in an inheritance
 * hierarchy, depending on the functionality required.  The hierarchy contains the following objects:
 *
 * CodaObject implements basic communications with run control, but cannot be owned or controlled.  
 * As far as run control is concerned a CodaObject can only respond to queries.
 *
 * SessionObject adds the notion of session ownership and basic control to CodaObject.
 *
 * RunObject is a SessionObject that can participate in run control transitions.
 *
 * InterruptibleObject introduces interrupt capability to RunObject via an InterruptService object.  A number of
 * kinds of InterrruptService classes are provided.
 *
 * No main program is provided.  Developers write classes that extend the
 * classes in this package, and provide their own main program.
 * See the example programs in the test and exesrc directories.
 */


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/** 
 * Allows pthread_create() to dispatch to an object member function that takes a single argument, used internally.  
 *
 * Static function dispatchIt() is actually given to pthread_create().  The pthread_create() user arg is a pointer to
 * this dispatcher object, which contains all info needed to dispatch correctly.
 */
template <class C, typename R, typename T> class pthreadDispatcher {
public:
  C *c;            /**<Pointer to object of type C having the member function.*/
  R (C::*mfp)(T);  /**<Pointer to member function of object of type C, return is of type R.*/
  T mfArg;         /**<Member function arg is of type T.*/

  /** 
   * Constructor stores object pointer, member function pointer, member function arg.
   *
   * @param c      Pointer to object
   * @param mfp    Pointer to member function 
   * @param mfArg  Arg to give to member function when invoked
   */
  pthreadDispatcher(C *c, R (C::*mfp)(T), T mfArg) : c(c), mfp(mfp), mfArg(mfArg) {}


  /**
   * Static function given to pthread_create dispatches to member function.
   *
   * @param pthreadArg Pointer to dispatcher object
   */
  static void *dispatchIt(void *pthreadArg) {
    pthreadDispatcher *pd = static_cast<pthreadDispatcher*>(pthreadArg);
    pthread_exit((void*)(((pd->c)->*(pd->mfp))(pd->mfArg)));
    return(NULL);
  }
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Base class for all CODA objects implements standard functionality.
 * Connects to run control system, sets up basic callbacks, etc.  
 *
 * CodaObjects have a state than can spontaneously change due to external causes.
 *
 * NOTE: CodaObjects are by design NOT controllable by the run control
 * system!  Use SessionObject if you need to own and control the object.
 * As far as run control is concerned, a CodaObject can only respond
 * to queries.
 */
class CodaObject {
  
public:
  CodaObject(const string& UDL, const string& name, const string& descr, const string &codaClass = "user", 
             const cMsgSubscriptionConfig *scfg = NULL, int connTimeout = 10) throw(CodaException);
  virtual ~CodaObject(void) throw();


protected:
  virtual void sendResponse(const cMsgMessage *msg, const string& subject, const string& type, 
                            const string& text, int userInt) const throw(CodaException);
  virtual void userMsgHandler(cMsgMessage *msg, void *userArg) throw(CodaException);
  virtual string userStatus(void) const throw();
  

public:
  int daLogMsg(const string& text, int sevid, const string& daLogSubj="") const throw(CodaException);
  virtual void startProcessing(void) throw(CodaException);
  virtual void stopProcessing(void) throw(CodaException);

  string getObjectType(void) const throw();
  string getCodaClass(void) const throw();
  string getHostName(void) const throw();
  string getUserName(void) const throw();
  string getState(void) const throw();
  string getStatus(void) const throw();
  string getConfig(void) const throw();


public:
  void changeState(const string& newState) throw(CodaException);
  void changeStatus(const string& newStatus) throw(CodaException);


private:
  void codaObjectCallback(cMsgMessage *msg, void* userArg) throw(CodaException);
  string checkCodaClass(const string &cClass) const throw();


protected:
  virtual void daLogMsgFill(cMsgMessage &msg) const throw(CodaException);


protected:
  string UDL;          /**<cMsg UDL to connect to run control.*/
  string name;         /**<Coda object name.*/
  string descr;        /**<Coda object description.*/

  string objectType;   /**<Type of this coda object.*/
  string codaClass;    /**<Coda class, specified by user.*/
  string hostName;     /**<Host name.*/
  string userName;     /**<User name.*/
  string state;        /**<Current state, set via changeState() method.*/
  string status;       /**<Current status, set via changeStatus() method.*/
  string config;       /**<Current config, set via changeConfig() method.*/

  const cMsgSubscriptionConfig *myscfg;    /**<cMsg connection subscription config.*/


public:
  cMsg *rcConn;           /**<cMsg run control connection object.*/


private:
  cMsgCallback *codaCBD;  /**<Callback dispatcher dispatches messages to member function.*/
  int connTimeout;        /**<RC connection timeout in secs.*/


public:
  string CodaObjectDaLogSubject;    /**<optional daLog message subject.*/
  static int debug;                 /**<Global debug flag.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------



}  // namespace codaObject

#endif /* _CodaObject_hxx */
