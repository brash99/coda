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

#ifndef _CodaException_hxx
#define _CodaException_hxx


#include <exception>
#include <string>
#include <sstream>


namespace codaObject {

using namespace std;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


/**
 *  Basic codaObject exception class contains description and code.
 */
class CodaException : public std::exception {

public:
  CodaException(const string &c);
  CodaException(const string &c, int i);
  virtual ~CodaException(void) throw();

  virtual void setReturnCode(int i) throw();
  virtual int getReturnCode(void) const throw();
  virtual string toString(void) const throw();
  virtual const char* what(void) const throw();


private:
  string descr;     /**<Description.*/
  int returnCode;   /**<Return code.*/
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------


} // namespace codaObject

#endif /* _CodaException_hxx */
