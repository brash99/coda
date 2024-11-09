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


#ifndef _RunObject_hxx
#define _RunObject_hxx


#include <SessionObject.hxx>
#include <RunObject.h>
#include <pthread.h>


namespace codaObject {

using namespace std;
using namespace cmsg;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Extends SessionObject to accept transition commands from run control, but has no interrupt capability.
 * If interrupt capability is needed use InterruptibleObject.
 *
 * User customizes a RunObject via override of some of download,prestart,go,pause,resume,end methods.
 * Alternatively, can specify dll containing download,etc. functions.
 * 
 * If dealing with events, user must set eventCount and dataCount as events are sent to the DAQ system.
 * 
 * @warning User routines should be pthread-cancelable in case reset command is received.
 */
class RunObject : public SessionObject {


public:
	RunObject(const string& UDL, const string& name, const string& descr, const string &codaClass = "USER",
			const cMsgSubscriptionConfig *scfg = NULL) throw(CodaException);
	virtual ~RunObject(void) throw();


protected:
	virtual void setDllUserArg(void *arg) throw();
	virtual void handleConfigure(const cMsgMessage *msg) throw(CodaException);
	virtual void handleDownload(const cMsgMessage *msg) throw(CodaException);
	virtual void handlePrestart(const cMsgMessage *msg) throw(CodaException);
	virtual void handleGo(const cMsgMessage *msg) throw(CodaException);
	virtual void handlePause(const cMsgMessage *msg) throw(CodaException);
	virtual void handleResume(const cMsgMessage *msg) throw(CodaException);
	virtual void handleEnd(const cMsgMessage *msg) throw(CodaException);
	virtual void handleReset(const cMsgMessage *msg) throw(CodaException);


protected:
	virtual void handleSetSession(const string &newSession) throw(CodaException);
	virtual void handleSetSession(const cMsgMessage *msg) throw(CodaException);
	virtual bool download(const string& s) throw(CodaException);
	virtual bool prestart(const string& s) throw(CodaException);
	virtual bool go(const string& s) throw(CodaException);
	virtual bool pause(const string& s) throw(CodaException);
	virtual bool resume(const string& s) throw(CodaException);
	virtual bool end(const string& s) throw(CodaException);
	virtual bool reset(const string& s) throw(CodaException);
	virtual bool configure(const cMsgMessage *msg) throw(CodaException);


protected:
	virtual bool userConfigure(const string& runConfig) throw(CodaException);
	virtual bool userDownload(const string& s) throw(CodaException);
	virtual bool userPrestart(const string& s) throw(CodaException);
	virtual bool userGo(const string& s) throw(CodaException);
	virtual bool userPause(const string& s) throw(CodaException);
	virtual bool userResume(const string& s) throw(CodaException);
	virtual bool userEnd(const string& s) throw(CodaException);
	virtual bool userReset(const string& s) throw(CodaException);


public:
	int getRunNumber(void) const throw();
	string getRunType(void) const throw();
	virtual void startProcessing(void) throw(CodaException);


private:
	void runControlCallback(cMsgMessage *msg, void* userArg) throw(CodaException);
	void runTransitionCallback(cMsgMessage *msg, void* userArg) throw(CodaException);
	void dispatchTransition(bool (RunObject::*mfp)(const string &s), const string &s);
	void *runStatisticsThread(void*);


protected:
	virtual void fillReport(cMsgMessage *m) throw();


protected:
	int runNumber;             /**<Current run number.*/
	string runType;            /**<Current run type.*/
	int runStatisticsInterval; /**<Run statistics calculation interval.*/


public:
	int eventNumber;           /**<Current event count.*/
	double eventRate;          /**<Current event rate.*/
	int dataCount;             /**<Current data count in 4-byte words.*/
	double dataRate;           /**<Current event rate.*/
	float liveTime;            /**<Current live time.*/


protected:
	bool hasDLL;               /**<True if has dll library containing transition functions.*/
	string dllFileName;        /**<dll file name.*/
	void *dllHandle;           /**<dll handle.*/
	dllSysStruct dllSysArg;    /**<For passing information into dll functions.*/
	void *dllUserArg;          /**<dll user arg.*/
	dllFuncPtr dllDownload;    /**<Pointer to download function in dll.*/
	dllFuncPtr dllPrestart;    /**<Pointer to prestart function in dll.*/
	dllFuncPtr dllGo;          /**<Pointer to go function in dll.*/
	dllFuncPtr dllPause;       /**<Pointer to pause function in dll.*/
	dllFuncPtr dllResume;      /**<Pointer to resume function in dll.*/
	dllFuncPtr dllEnd;         /**<Pointer to end function in dll.*/
	dllFuncPtr dllReset;       /**<Pointer to reset function in dll.*/


protected:
	cMsgCallback *runControlCBD;         /**<Run control message callback dispatcher.*/
	cMsgCallback *runTransitionCBD;      /**<Run transition message callback dispatcher.*/


protected:
	pthread_mutex_t runResetMutex;                     /**<Mutex for reset command.*/
	pthread_mutex_t runTransitionMutex;                /**<Mutex for run transitions.*/
	pthread_t transitionThreadId;                      /**<Transition thread id.*/
	pthread_t runStatisticsThreadId;                   /**<Run statistics thread id.*/

	pthreadDispatcher<RunObject,void*,void*> *statisticsThreadDispatcher;    /**<Run statistics pthread dispatcher.*/

        virtual void daLogMsgFill(cMsgMessage &msg) const throw(CodaException);


private:
	void *rcsSubId;                  /**<Session-specific control subscription id.*/
	void *rtsSubId;                  /**<Session-specific transition subscription id.*/

};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject

#endif /* _RunObject_hxx */
