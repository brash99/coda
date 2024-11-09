/******************************************************************************
*
* header file for use Linux VME defined rols with CODA
*
*                             DJA   Nov 2000
*
* SVN: $Rev$
*
*******************************************************************************/
#ifndef __TSPRIMARY_ROL__
#define __TSPRIMARY_ROL__

static int TSPRIMARY_handlers,TSPRIMARYflag;
static int TSPRIMARY_isAsync;
static unsigned int *TSPRIMARYPollAddr = NULL;
static unsigned int TSPRIMARYPollMask;
static unsigned int TSPRIMARYPollValue;
static unsigned long TSPRIMARY_prescale = 1;
static unsigned long TSPRIMARY_count = 0;


/* Put any global user defined variables needed here for TSPRIMARY readout */
extern DMA_MEM_ID vmeOUT, vmeIN;

/*----------------------------------------------------------------------------
  tsprimary_trigLib.c -- Dummy trigger routines for GENERAL USER based ROLs

 File : tsprimary_trigLib.h

 Routines:
	   void tsprimarytenable();        enable trigger
	   void tsprimarytdisable();       disable trigger
	   char tsprimaryttype();          return trigger type 
	   int  tsprimaryttest();          test for trigger  (POLL Routine)
------------------------------------------------------------------------------*/

static void 
tsprimarytenable(int val)
{
  TSPRIMARYflag = 1;
}

static void 
tsprimarytdisable(int val)
{
  TSPRIMARYflag = 0;
}

static unsigned long 
tsprimaryttype()
{
  return(1);
}

static int 
tsprimaryttest()
{
  if(dmaPEmpty(vmeOUT)) 
    return (0);
  else
    return (1);
}



/* Define CODA readout list specific Macro routines/definitions */

#define TSPRIMARY_TEST  tsprimaryttest

#define TSPRIMARY_INIT { TSPRIMARY_handlers =0;TSPRIMARY_isAsync = 0;TSPRIMARYflag = 0;}

#define TSPRIMARY_ASYNC(code,id)  {printf("*** No Async mode is available for TSPRIMARY ***\n"); \
                              printf("linking sync TSPRIMARY trigger to id %d \n",id); \
			       TSPRIMARY_handlers = (id);TSPRIMARY_isAsync = 0;}

#define TSPRIMARY_SYNC(code,id)   {printf("linking sync TSPRIMARY trigger to id %d \n",id); \
			       TSPRIMARY_handlers = (id);TSPRIMARY_isAsync = 1;}

#define TSPRIMARY_SETA(code) TSPRIMARYflag = code;

#define TSPRIMARY_SETS(code) TSPRIMARYflag = code;

#define TSPRIMARY_ENA(code,val) tsprimarytenable(val);

#define TSPRIMARY_DIS(code,val) tsprimarytdisable(val);

#define TSPRIMARY_CLRS(code) TSPRIMARYflag = 0;

#define TSPRIMARY_GETID(code) TSPRIMARY_handlers

#define TSPRIMARY_TTYPE tsprimaryttype

#define TSPRIMARY_START(val)	 {;}

#define TSPRIMARY_STOP(val)	 {tsprimarytdisable(val);}

#define TSPRIMARY_ENCODE(code) (code)


#endif

