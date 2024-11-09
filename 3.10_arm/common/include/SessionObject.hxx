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

#ifndef _SessionObject_hxx
#define _SessionObject_hxx


#include <CodaObject.hxx>


#define MIN_REPORTING_INTERVAL 0.5
#define MAX_REPORTING_INTERVAL 10.0



namespace codaObject {

using namespace std;
using namespace cmsg;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Extends CodaObject to allow for session ownership and control.
 * A SessionObject is owned by a session and unavailable to other sessions until released.
 *
 * User customizes SessionObject via override of configure, start, stop, and/or exit methods.
 */
class SessionObject : public CodaObject {
  
public:
  SessionObject(const string& UDL, const string& name, const string& descr, const string &codaClass = "USER",
                const cMsgSubscriptionConfig *scfg = NULL) throw(CodaException);
  virtual ~SessionObject(void) throw();


protected:
  virtual void handleSetSession(const string &newSession) throw(CodaException);
  virtual void handleSetSession(const cMsgMessage *msg) throw(CodaException);
  virtual void handleSessionConfigure(const cMsgMessage *msg) throw(CodaException);
  virtual void handleExit(const cMsgMessage *msg) throw(CodaException);
  virtual void handleSessionReset(const cMsgMessage *msg) throw(CodaException);
  virtual void handleStartReporting(const cMsgMessage *msg) throw(CodaException);
  virtual void handleStopReporting(const cMsgMessage *msg) throw();
  virtual void fillReport(cMsgMessage *msg) throw();


public:
  string getSession(void) const throw() ;
  string getConfig(void) const throw() ;


private:
  void sessionControlCallback(cMsgMessage *msg, void* userArg) throw(CodaException);
  void *reportingThread(void *arg);


protected:
  virtual bool setSession(const string& newSession) throw(CodaException);

  virtual bool sessionConfigure(const string& fileName,const string& fileContent) throw(CodaException);
  virtual void exit(const string& s) throw(CodaException);
  virtual void sessionReset(const string& s) throw(CodaException);


private:
  string sessionFileContent;       /**<Current session file content.*/
  cMsgCallback *sessionControlCBD; /**<Control callback dispatcher.*/
  cMsgCallback *sessionOptionCBD;  /**<Option callback dispatcher.*/
  void *scSubId;                   /**<Session-specific control subscription id.*/
  double reportingInterval;        /**<Reporting interval in secs.*/
  pthread_t reportingThreadId;     /**<Reporting thread id.*/
  pthreadDispatcher<SessionObject,void*,void*> *reportingThreadDispatcher;  /**<Reporting thread dispatcher.*/

protected:
  string session;                  /**<Current session.*/
  virtual void daLogMsgFill(cMsgMessage &msg) const throw(CodaException);
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject


#endif /* _SessionObject_hxx */
