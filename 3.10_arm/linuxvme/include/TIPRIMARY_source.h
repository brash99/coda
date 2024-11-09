/******************************************************************************
*
* header file for use Linux VME defined rols with CODA
*
*                             DJA   Nov 2000
*
*******************************************************************************/
#ifndef __TIPRIMARY_ROL__
#define __TIPRIMARY_ROL__

static int TIPRIMARY_handlers,TIPRIMARYflag;
static int TIPRIMARY_isAsync;

/* Put any global user defined variables needed here for TIPRIMARY readout */
extern DMA_MEM_ID vmeOUT, vmeIN;

/*----------------------------------------------------------------------------
  tiprimary_trigLib.c -- Dummy trigger routines for GENERAL USER based ROLs

 File : tiprimary_trigLib.h

 Routines:
	   void tiprimarytenable();        enable trigger
	   void tiprimarytdisable();       disable trigger
	   char tiprimaryttype();          return trigger type
	   int  tiprimaryttest();          test for trigger  (POLL Routine)
------------------------------------------------------------------------------*/

static void
tiprimarytenable(int val)
{
  TIPRIMARYflag = 1;
}

static void
tiprimarytdisable(int val)
{
  TIPRIMARYflag = 0;
}

static unsigned long
tiprimaryttype()
{
  return(1);
}

static int
tiprimaryttest()
{
  if(dmaPEmpty(vmeOUT))
    return (0);
  else
    return (1);
}



/* Define CODA readout list specific Macro routines/definitions */

#define TIPRIMARY_TEST  tiprimaryttest

#define TIPRIMARY_INIT { TIPRIMARY_handlers =0;TIPRIMARY_isAsync = 0;TIPRIMARYflag = 0;}

#define TIPRIMARY_ASYNC(code,id)  {printf("*** No Async mode is available for TIPRIMARY ***\n"); \
                              printf("linking sync TIPRIMARY trigger to id %d \n",id); \
			       TIPRIMARY_handlers = (id);TIPRIMARY_isAsync = 0;}

#define TIPRIMARY_SYNC(code,id)   {printf("linking sync TIPRIMARY trigger to id %d \n",id); \
			       TIPRIMARY_handlers = (id);TIPRIMARY_isAsync = 1;}

#define TIPRIMARY_ENA(code,val) tiprimarytenable(val);

#define TIPRIMARY_DIS(code,val) tiprimarytdisable(val);

#define TIPRIMARY_GETID(code) TIPRIMARY_handlers

#define TIPRIMARY_TTYPE tiprimaryttype

#define TIPRIMARY_START(val)	 {;}

#define TIPRIMARY_STOP(val)	 {tiprimarytdisable(val);}


#endif
