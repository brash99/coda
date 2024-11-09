/*----------------------------------------------------------------------------*
 *  Copyright (c) 1991, 1992  Southeastern Universities Research Association, *
 *                            Continuous Electron Beam Accelerator Facility   *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 * CEBAF Data Acquisition Group, 12000 Jefferson Ave., Newport News, VA 23606 *
 *       heyes@cebaf.gov   Tel: (804) 249-7030    Fax: (804) 249-7363         *
 *----------------------------------------------------------------------------*
 * Description: follows this header.
 *
 * Author:
 *	David Abbott
 *	CEBAF Data Acquisition Group
 *
 * Revision History:
 *	  Initial revision
 *
 *
 *----------------------------------------------------------------------------*/

 /* test_trigLib.c -- Dummy trigger routines for UNIX based ROLs

 File : test_trigLib.h

 Routines:
	   void testtenable();        enable trigger
	   void testtdisable();       disable trigger
	   char testttype();          read trigger type
	   int  testttest();          test for trigger (POLL)

------------------------------------------------------------------------------*/


static void 
testtenable(int val)
{
}

static void 
testtdisable(int val)
{
}

static unsigned long 
testttype()
{
  return(1);
}

static int 
testttest()
{
  return(1);
}

