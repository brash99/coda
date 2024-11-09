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

#ifndef _InterruptService_hxx
#define _InterruptService_hxx


namespace codaObject {

class InterruptibleObject;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 * Pure virtual class defines interrupt service functionality.
 */
class InterruptService {
  
public:
  InterruptService(void) : myIntObj(NULL) {}
  virtual bool setupInterrupt(void)    = 0;   /**<Sets up interrupt, called during prestart transition.*/
  virtual bool enableInterrupt(void)   = 0;   /**<Enables interrupt, called during go transition.*/
  virtual bool pauseInterrupt(void)    = 0;   /**<Pauses interrupt, called during pause transition.*/
  virtual bool resumeInterrupt(void)   = 0;   /**<Resumes interrupt, called during resume transition.*/
  virtual bool deleteInterrupt(void)   = 0;   /**<Deletes interrupt, called during end transition.*/

  virtual void setInterruptibleObject(InterruptibleObject *o) { myIntObj=o; } /**<Registers InterruptibleObject using this service*/


protected:
  InterruptibleObject *myIntObj;  /**<InterruptibleObject using this service*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject

#endif /* _InterruptService_hxx */
