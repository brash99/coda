//  Trigger polling service for old TI from linux single board computer

//  EJW, 12-Jul-2012



#ifndef _linuxTITrigPollingService_hxx
#define _linuxTITrigPollingService_hxx


#include <InterruptibleObject.hxx>
#include <time.h>
#include <pthread.h>


// settable by switches on old TI module
#define TI_BASE_ADDR          0x0ed0


// TI commands
#define VME_ACCESS_MODE       0x29
#define TI_ENABLE_TRIG        0x7
#define TI_DISABLE            0x0
#define TI_PAUSE              0x5
#define TI_RESET              0x80






namespace codaObject {


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Implements service to check if new TI has received trigger.
 */
class linuxTITrigPollingService : public InterruptService {
  
public:
  explicit linuxTITrigPollingService(timespec *timeout);
  virtual bool setupInterrupt(void);
  virtual bool enableInterrupt(void);
  virtual bool pauseInterrupt(void);
  virtual bool resumeInterrupt(void);
  virtual bool deleteInterrupt(void);

  virtual void* dispatchInterrupt(void*);


private:
  pthread_t linuxTITrigPollingThreadId;        /**<Polling thread id.*/
  timespec *linuxTITrigPollingTimeout;         /**<Polling interval.*/

  pthreadDispatcher<linuxTITrigPollingService,void*,void*> *linuxTITrigPollingPthreadDispatcher;  /**<Pthread dispatcher for polling thread.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject

#endif
