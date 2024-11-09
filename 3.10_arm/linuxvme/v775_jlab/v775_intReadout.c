/*************************************************************** 
 * Test code for reading out a A CAEN V775 TDC                 *
 *                                                             *
 *    Written By : David Abbott                                *
 *                 Data Acquisition Group                      *
 *                 Jefferson Lab                               *
 *    Date: January 2003                                       *
 *                                                             *
 *    Inputs:  slot  -  Fastbus slot number (0-25)             *
 *             count -  number of Block Read                   *
 *                      cycles to execute                      * 
 ***************************************************************/

#include <vxWorks.h>
#include <stdio.h>
#include <string.h>
#include <sysLib.h>
#include <taskLib.h>
#include <msgQLib.h>


/* External library declarations */
#include "c775Lib.h"

#define MAX_EVENT_LENGTH 34
#define DEFAULT_VME_ADDR 0x200000

/* Requires use of Kmax server */
extern struct c775_struct *c775p[20];


/* Global Variables */
int stopDAQ = 0;
int tdcHistEventCount = 0;
int tdcEventCount = 0;
UINT32 v775RawData[MAX_EVENT_LENGTH*32];

void 
v775KmaxInt (int tdcID)
{
  int res=0;
  int nevts, nwords;
  int maxData = MAX_EVENT_LENGTH*32;

  /* Note that the Argument to the ISR is the Module ID */

  /* Check number of events available */
  nevts = c775Dready(tdcID);
  if(nevts <= 0) {
    c775Clear(tdcID);
    logMsg("v775KmaxInt: ERROR: Bad Event Ready Count from TDC, TDC Cleared",0,0,0,0,0,0);
    return;
  }

  /* Readout All available events */
  res = c775ReadBlock(tdcID, v775RawData, maxData);
  if(res < 0) {
    logMsg("v775KmaxInt: ERROR: TDC Error in Block Read",0,0,0,0,0,0);
  }
  
  /* determine the number of events read out */

 

}



void 
v775KmaxIDAQ( int id,  int hFlag)
{

  unsigned int control;   /* Online histogram control */
  int nev, rlen=0;
  unsigned long res, ii;
  unsigned int tdc[MAX_EVENT_LENGTH];  /* Store one full event from v775 */
  volatile int tp[KMAX_EVENT_SIZE];    /* KMAX Event buffer */
  unsigned long tdcchan, tdcval;
    

  /****** Clear/Program TDC ******/

  c775Clear(id);
  c775p[id]->bitSet2 = 0x400; /* set up for common stop */
  res = c775Dready(id);
  if(res<0) {
    printf("v775KmaxDAQ: ERROR: TDC id %d not available\n",id);
    return;
  }

  /****** Initialize Histogram areas *****/

  tdcHistEventCount=0;
  tdcEventCount = 0;


  /******* Initialize Interrupts *******/

  /****** Readout Loop *******/


    if(control&TDC_POLL_FOR_GATES) {
    
      nev = c775Dready(id);
 
      if (nev > 0) {
	bzero((char *)tdc,sizeof(tdc));
	res = c775ReadEvent(id,tdc);
	if (res < 0) {
	  printf("v775KmaxDAQ: ERROR: in TDC ReadEvent \n");
	  return;
	} else {
	  
	  switch (hFlag) {
	  case 2: /* Store KMAX Event stream */
	    bzero((char *) tp, sizeof(tp));
	    /* Loop over all TDC Channels*/
	    rlen = res - 2;
	    for(ii=1;ii<(rlen+1);ii++) { /* skip over header */
	      tdcchan = ((tdc[ii]&C775_CHANNEL_MASK)>>16) - 4; /* Look at 5,6,7,8 */
	      tdcval  = (tdc[ii]&C775_TDC_DATA_MASK);
	      if(tdcchan< (KMAX_EVENT_SIZE)) /* store only max Kmax parameters */
		tp[tdcchan] = tdcval;        /* Raw TDC Values */
	    }
	    tp[4] = (tp[0] + tp[1])>>1;   /* Meantime 1 */
	    tp[5] = (tp[2] + tp[3])>>1;   /* Meantime 2 */
	    tp[6] = v775HistOffset + tp[1] - tp[0]; /* Difference 1 */
	    tp[7] = v775HistOffset + tp[3] - tp[2]; /* Difference 2 */
	    tp[8] = v775HistOffset + tp[5] - tp[4]; /* Mean Difference */ 

	    /* For use with kmax_server */
	    res = msgQSend(kmaxQID, (char *)tp,(9*4),NO_WAIT,MSG_PRI_NORMAL);
	    if(res<0)
	      printf("tdc_readout: WARNING: dropped event\n");
	    else
	      tdcEventCount++;

	    break;

	  case 1: /* Histogram the data locally */
	    rlen = res - 2;  /* discount header and trailer */
	    tdcHistEventCount++;
	    for(ii=1;ii<(rlen+1);ii++) { /* skip over header */
	      tdcchan = (tdc[ii]&0x3f0000)>>16;
	      tdcval  = (tdc[ii]&0xfff);
	      tdcHistData[tdcchan*4096 + tdcval] += 1;
	    }
	    break;

	  case 0: /* print data to stdout */
	  default:
	    printf("TDC DATA: %d words ",rlen);
	    for(ii=0;ii<rlen;ii++) {
	      if ((ii % 4) == 0) printf("\n    ");
	      tdcchan = (tdc[ii]&0x3f0000)>>16;
	      tdcval  = (tdc[ii]&0xfff);
	      printf("  Ch %d: %04d (0x%08x)",(int)tdcchan,(int)tdcval,(int)tdc[ii]);
	    }
	    printf("\n");
	    taskDelay(30); /* wait a little before checking again */
	  }
	}


      }

  } while (stopDAQ == 0);


  printf("Exiting polling loop\n");
  stopDAQ=0;
  return;

}
