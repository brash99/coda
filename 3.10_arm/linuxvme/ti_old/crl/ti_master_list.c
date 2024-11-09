#define ROL_NAME__ "GEN_USER"
#define MAX_EVENT_LENGTH 1024
#define MAX_EVENT_POOL   512
#define GEN_MODE
#define INIT_NAME ti_master_list__init
#define INIT_NAME_POLL ti_master_list__poll
#include <rol.h>
#include <GEN_source.h>
#include "../tiLib.h"
#define TRIG_ADDR 0x100000
#define TRIG_MODE 0
#define BLOCKLEVEL 1
#define BUFFERLEVEL 1
#define USE_PULSER 1
#define READOUT_TI 1
extern int bigendian_out;
int blockLevel=1;
unsigned int *tiData=NULL;
static void __download()
{
    daLogMsg("INFO","Readout list compiled %s", DAYTIME);
#ifdef POLLING___
   rol->poll = 1;
#endif
    *(rol->async_roc) = 0; /* Normal ROC */
  {  /* begin user */
bigendian_out = 0;
{/* inline c-code */
 
{
  /* measured longest fiber length */
  tiSetFiberLatencyOffset_preInit(0x40);

  /* Set crate ID */
  tiSetCrateID_preInit(0x1); /* ROC 1 */

  /* TI Setup */
  tiInit(TRIG_ADDR,TRIG_MODE,0);
  tiDisableBusError();

  if(READOUT_TI==0) /* Disable data readout */
    {
      tiDisableDataReadout();

      /* Disable A32... where that data would have been stored on the TI */
      tiDisableA32();
    }

  tiSetBusySource(TI_BUSY_LOOPBACK,1);

  if(USE_PULSER==1)
    tiSetTriggerSource(TI_TRIGGER_PULSER);
  else
    tiSetTriggerSource(TI_TRIGGER_TSINPUTS);

  /* Set needed TS input bits */
  tiEnableTSInput( TI_TSINPUT_1 | TI_TSINPUT_2 );

  /* Load the trigger table
   *  - 0:
   *    - TS#1,2,3,4,5 generates Trigger1 (physics trigger),
   *    - TS#6 generates Trigger2 (playback trigger),
   *    - No SyncEvent;
   */
  tiLoadTriggerTable(0);

  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

  tiSetBlockBufferLevel(BUFFERLEVEL);


  tiStatus(0);
}
 
 }/*end inline c-code */
{/* inline c-code */
 
{
  /* Perform these at the end of DOWNLOAD */
  tiDisableVXSSignals();
  tiClockReset();
  taskDelay(2);
  tiTrigLinkReset();
}
 
 }/*end inline c-code */
    daLogMsg("INFO","User Download Executed");

  }  /* end user */
} /*end download */     

static void __prestart()
{
CTRIGINIT;
    *(rol->nevents) = 0;
  {  /* begin user */
unsigned long jj, adc_id;
    daLogMsg("INFO","Entering User Prestart");

    GEN_INIT;
    CTRIGRSA(GEN,1,titrig,titrig_done);
    CRTTYPE(1,GEN,1);
{/* inline c-code */
 
{ 
 /* Turn VXS signals on, beginning of prestart */
  tiEnableVXSSignals();

  /* Set number of events per block (broadcasted to all connected TI Slaves)*/
  tiSetBlockLevel(BLOCKLEVEL);
  printf("rocPrestart: Block Level set to %d\n",BLOCKLEVEL);
}
 
 }/*end inline c-code */
{/* inline c-code */
 
{
  /* Send a SyncReset, at the end of prestart */
  tiSyncReset(1);
  taskDelay(2);
}
 
 }/*end inline c-code */
    daLogMsg("INFO","User Prestart Executed");

  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
    *(rol->nevents) = 0;
    rol->recNb = 0;
} /*end prestart */     

static void __end()
{
  {  /* begin user */
{/* inline c-code */
 
{
  int iwait=0, bready=0, hit=0;

  CDODISABLE(GEN,1,0);

  if(USE_PULSER==1)
    {
      tiSoftTrig(1,0,0x1123,1);
      tiDisableRandomTrigger();
    }

  tiStatus(0);
  printf("Interrupt Count: %8d \n",tiGetIntCount());


  /* Free up the allocated memory for the tiData */
  if(tiData!=NULL)
    {
      free(tiData);
      tiData=NULL;
    } 
}

 
 }/*end inline c-code */
    daLogMsg("INFO","User End Executed");

  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
} /* end end block */

static void __pause()
{
  {  /* begin user */
  CDODISABLE(GEN,1,0);
    daLogMsg("INFO","User Pause Executed");

  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
} /*end pause */
static void __go()
{

  {  /* begin user */
    daLogMsg("INFO","Entering User Go");

{/* inline c-code */
 
{
  /* Get the current Block Level */
  blockLevel = tiGetCurrentBlockLevel();
  printf("rocGo: Block Level set to %d\n",blockLevel);

  /* Allocate some memory for TI data */
  tiData = (unsigned int*)malloc((8+5*blockLevel)*sizeof(unsigned int));

  tiStatus(0);
}
 
 }/*end inline c-code */
{/* inline c-code */
 
{
  CDOENABLE(GEN,1,0);

  if(USE_PULSER==1)
    tiSetRandomTrigger(1,0x7);
/*     tiSoftTrig(1,3000,0x1123,1); */

}
 
 }/*end inline c-code */
    daLogMsg("INFO","User Go Executed");

  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
}

void titrig(unsigned int EVTYPE,unsigned int EVSOURCE)
{
    int EVENT_LENGTH;
  {  /* begin user */
unsigned long dCnt, ev_type, ii, tibready, timeout;
  rol->dabufp = (long *) 0;
{/* inline c-code */
 
{
  /* Readout TI to get Event Type */
  ev_type=1; /* Default event type (for example) */
  if(READOUT_TI==1)
    {
      dCnt = tiReadBlock(tiData,8+5*blockLevel,0);
      if(dCnt<=0)
	{
	  logMsg("No data or error.  dCnt = %d\n",dCnt);
	}
      else
	{
	  ev_type = tiDecodeTriggerType(tiData, dCnt, 1);
	  if(ev_type <= 0)
	    {
	      /* Could not find trigger type */
	      ev_type = 1;
	    }

	  /* CODA 2.x only allows for 4 bits of trigger type */
	  ev_type &= 0xF; 
	}
    }
  
}

 
 }/*end inline c-code */
    CEOPEN(ev_type, BT_BANK);
{/* inline c-code */
 
{
  if(READOUT_TI==1)
    {
      /* Open a CODA Bank of 4byte unsigned integers, type=4 (for example) */
      CBOPEN(4,BT_UI4,0);
      if(dCnt<=0)
	{
	  logMsg("No data or error.  dCnt = %d\n",dCnt);
	}
      else
	{
	  for(ii=0; ii<dCnt; ii++)
	    *rol->dabufp++ = tiData[ii];
	}
      CBCLOSE;
    }

  /* Add in some example user data */
  CBOPEN(3,BT_UI4,0);
  *rol->dabufp++ = 0x12345678;
  CBCLOSE;
}
 
 }/*end inline c-code */
    CECLOSE;
  }  /* end user */
} /*end trigger */

void titrig_done()
{
  {  /* begin user */
  }  /* end user */
} /*end done */

void __done()
{
poolEmpty = 0; /* global Done, Buffers have been freed */
  {  /* begin user */
  CDOACK(GEN,1,0);
  }  /* end user */
} /*end done */

static void __status()
{
  {  /* begin user */
  }  /* end user */
} /* end status */

