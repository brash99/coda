/*----------------------------------------------------------------------------*
*  Copyright (c) 2005        Southeastern Universities Research Association, *
*                            Thomas Jefferson National Accelerator Facility  *
*                                                                            *
*    This software was developed under a United States Government license    *
*    described in the NOTICE file included as part of this distribution.     *
*                                                                            *
*    E.Wolin, 31-Oct-2006, Jefferson Lab                                     *
*                                                                            *
*    Authors: Elliott Wolin                                                  *
*             wolin@jlab.org                    Jefferson Lab, MS-6B         *
*             Phone: (757) 269-7365             12000 Jefferson Ave.         *
*             Fax:   (757) 269-5519             Newport News, VA 23606       *
*
*----------------------------------------------------------------------------*/

#ifndef _PollingService_hxx
#define _PollingService_hxx


#include <InterruptibleObject.hxx>
#include <time.h>
#include <pthread.h>


namespace codaObject {


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Implements polling interrupt service.
 */
class PollingService : public InterruptService {
  
public:
  explicit PollingService(int timeout);
  virtual bool setupInterrupt(void);
  virtual bool enableInterrupt(void);
  virtual bool pauseInterrupt(void);
  virtual bool resumeInterrupt(void);
  virtual bool deleteInterrupt(void);

  virtual void* dispatchInterrupt(void*);


private:
  pthread_t pollingThreadId;        /**<Polling thread id.*/
  int pollingTimeout;               /**<Polling interval in milliseconds.*/

  pthreadDispatcher<PollingService,void*,void*> *pollingPthreadDispatcher;  /**<Pthread dispatcher for polling thread.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject

#endif /* PollingService_hxx */
