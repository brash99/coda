#define ROL_NAME__ "FRED"
#define MAX_EVENT_LENGTH 40960
#define MAX_EVENT_POOL   100
/* POLLING_MODE */
#define POLLING___
#define POLLING_MODE
#define TEST_MODE
#define INIT_NAME test_list_v3__init
#define INIT_NAME_POLL test_list_v3__poll
#include <rol.h>
#include <TEST_source.h>
int blklevel = 1;
int maxdummywords = 200;
int trigBankType = 0xff11;
const int TIR_ADDR = 0x0ed0;

const int TIR_SOURCE;
const int TIR_MODE;
const int TIR_EXT_POLL;

const int ADC_ID = 0;
const int MAX_ADC_DATA = 34;

const int TDC_ID = 0;
const int MAX_TDC_DATA = 34;

#include "tirLib.c"
#include "caen792Lib.c"
#include "caen775Lib.c"
#include "c792Lib.h"
#include "c775Lib.h"

unsigned long laddr;
extern int bigendian_out;

static void __download()
{
    daLogMsg("INFO","Readout list compiled %s", DAYTIME);
#ifdef POLLING___
   rol->poll = 1;
#endif
    *(rol->async_roc) = 0; /* Normal ROC */
  {  /* begin user */
unsigned long res;
  bigendian_out = 1;

/* insert code from v792_v775/c792_linux_list.c */

    int dmaMode;
    vmeDmaConfig(1,3,0);
    c792Init(0x110000,0,1,0);
    c775Init(0xa10000,0,1,0);

    daLogMsg("INFO","User Download Executed");

  }  /* end user */
} /*end download */     

static void __prestart()
{
CTRIGINIT;
    *(rol->nevents) = 0;
  {  /* begin user */
unsigned long jj;
    daLogMsg("INFO","Entering User Prestart");

    TEST_INIT;
    CTRIGRSS(TEST,1,usrtrig,usrtrig_done);
    CRTTYPE(1,TEST,1);
  TESTflag = 0;
  TEST_prescale = 10;

/* insert code from v792_v775/c792_linux_list.c */

  unsigned short iflag;
  int stat;

  /* Program/Init VME Modules Here */
  /* Setup ADCs (no sparsification, enable berr for block reads) */
  c792Sparse(ADC_ID,0,0);
  c792Clear(ADC_ID);
  c792DisableBerr(ADC_ID); // Disable berr - multiblock read
/*  c792EnableBerr(ADC_ID); /\* for 32bit block transfer *\/ */

  c792Status(ADC_ID,0,0);

/* Program/Init VME Modules Here */
  /* Setup TDCs (no sparcification, enable berr for block reads) */
  c775Clear(TDC_ID);
  c775DisableBerr(TDC_ID); // Disable berr - multiblock read
/*   c775EnableBerr(TDC_ID); /\* for 32bit block transfer *\/ */
  c775CommonStop(TDC_ID);
  //c775CommonStart(TDC_ID);

  c775Status(TDC_ID,0,0);

    daLogMsg("INFO","User Prestart Executed");
  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
    *(rol->nevents) = 0;
    rol->recNb = 0;
} /*end prestart */     

static void __end()
{
  {  /* begin user */
  TESTflag = 0;
    daLogMsg("INFO","User End Executed");

  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
} /* end end block */

static void __pause()
{
  {  /* begin user */
  TESTflag = 0;
    daLogMsg("INFO","User Pause Executed");

  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
} /*end pause */
static void __go()
{

  {  /* begin user */
    daLogMsg("INFO","Entering User Go");

  TESTflag = 1;
  }  /* end user */
    if (__the_event__) WRITE_EVENT_;
}

void usrtrig(unsigned int EVTYPE,unsigned int EVSOURCE)
{
    int EVENT_LENGTH;
  {  /* begin user */
unsigned long ii, evtnum;
  evtnum = *(rol->nevents);
 CEOPEN(ROCID,BT_BANK,blklevel);
{/* inline c-code */
 
CBOPEN(trigBankType,BT_SEG,blklevel);
for(ii=0;ii<blklevel;ii++) {
   if(trigBankType == 0xff11) {
     *rol->dabufp++ = (EVTYPE<<24)|(0x01<<16)|(3);
   }else{
     *rol->dabufp++ = (EVTYPE<<24)|(0x01<<16)|(1);
   }
   *rol->dabufp++ = (blklevel*(evtnum-1) + (ii+1));
   if(trigBankType == 0xff11) {
      *rol->dabufp++ = 0x12345678;
      *rol->dabufp++ = 0;
   }
}
CBCLOSE;
 
 }/*end inline c-code */
CBOPEN(1,BT_UI4,blklevel);
    CBWRITE32(0xda000011); 
{/* inline c-code */
 
  for(ii=0;ii<maxdummywords;ii++) {
     *rol->dabufp++ = ii;
  }
 
 }/*end inline c-code */
    CBWRITE32(0xda0000ff); 
CBCLOSE;
CECLOSE;
  }  /* end user */
} /*end trigger */

void usrtrig_done()
{
  {  /* begin user */
  }  /* end user */
} /*end done */

void __done()
{
poolEmpty = 0; /* global Done, Buffers have been freed */
  {  /* begin user */
  }  /* end user */
} /*end done */

void __reset () 
{
{/* inline c-code */
 
/* This is a test */
 
 }/*end inline c-code */
} /* end of user codes */

