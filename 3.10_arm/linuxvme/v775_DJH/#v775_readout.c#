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

#define HISTOGRAM_MEM_BASE_ADDR    0x03000000
#define HISTOGRAM_SIZE  4096
#define HISTOGRAM_MAX_DATA  MAX_EVENT_LENGTH*HISTOGRAM_SIZE

#define TDC_POLL_FOR_GATES  0x00000001

#define KMAX_EVENT_SIZE    9
#define KMAX_MAX_EVENTS  256


/* Requires use of Kmax server */
extern MSG_Q_ID kmaxQID;
extern int v851ProgPulser();
extern void v851ProgExt();
extern int v851Enable();
extern int v851Disable();
extern int v851SetDelay();
extern void v851UpdateDelay();
extern UINT32 *kmaxConfigBuf;

/* Global Variables */
int stopDAQ = 0;
unsigned int *tdcHistData = (unsigned int *)(HISTOGRAM_MEM_BASE_ADDR);
int v775HistOffset = 800;
int tdcHistEventCount = 0;
int tdcEventCount = 0;


void 
v775KmaxDAQ( int id,  int hFlag, int mFlag)
{

  unsigned int control;   /* Online histogram control */
  int nev, pulserON=0, rlen=0;
  unsigned long res, ii;
  unsigned int tdc[MAX_EVENT_LENGTH];  /* Store one full event from v775 */
  volatile int tp[KMAX_EVENT_SIZE];    /* KMAX Event buffer */
  unsigned long tdcchan, tdcval;
  int clearHist = 0;
  int delay_enable=0;
  int delay_set = 0;
  int delay_rate = 10;
    

  /****** Clear TDC ******/

  c775Clear(id);
  /* c775CommonStop(id); */ /* set up for common stop */
  res = c775Dready(id);
  if(res<0) {
    printf("v775KmaxDAQ: ERROR: TDC id %d not available\n",id);
    return;
  }

/****** Initialize Histogram areas *****/

  bzero((char *)tdcHistData,(HISTOGRAM_MAX_DATA<<2));
  tdcHistEventCount=0;
  tdcEventCount = 0;


/****** Clear/Program Pulse Generator ******/

  if(mFlag) {
    v851ProgExt(0);
  }else{
    v851ProgPulser(delay_rate,0);
  }


/****** Readout Loop *******/
  printf(" Polling for Gates...\n");
  do {

    /* check Control Word on what we should do */
    control =      kmaxConfigBuf[0];
    clearHist =    kmaxConfigBuf[1];
    delay_enable = kmaxConfigBuf[2];
    delay_set    = kmaxConfigBuf[3];

    /* Check if we should clear local histograms */
    if(clearHist) {
      bzero((char *)tdcHistData,(HISTOGRAM_MAX_DATA<<2));
      tdcHistEventCount=0;
      kmaxConfigBuf[1] = 0;
    }

    /* Check if we need to enable/disable the Delay Pulser */
    if(delay_enable) {
      if(pulserON == 0) {
        v851Enable(0);
        pulserON=1;
        taskDelay(60);
      }
    }else{
      if(pulserON) {
        v851Disable(0);
        pulserON=0;
      }
    }

    /* Check if we need to update delay settings */
    if(delay_set) {
      for (ii=4;ii<10;ii++) {
        if(kmaxConfigBuf[ii])
           v851SetDelay((ii-3),kmaxConfigBuf[ii],0,0);
      }
      v851UpdateDelay(0);
      delay_set = kmaxConfigBuf[3] = 0;
    }

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
	      tdcchan = ((tdc[ii]&C775_CHANNEL_MASK)>>16);
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
	      tdcchan = (tdc[ii]&C775_CHANNEL_MASK)>>16;
	      tdcval  = (tdc[ii]&C775_TDC_DATA_MASK);
	      tdcHistData[tdcchan*HISTOGRAM_SIZE + tdcval] += 1;
	    }
	    break;

	  case 0: /* print data to stdout */
	  default:
	    printf("TDC DATA: %d words ",rlen);
	    for(ii=0;ii<rlen;ii++) {
	      if ((ii % 4) == 0) printf("\n    ");
	      tdcchan = (tdc[ii]&C775_CHANNEL_MASK)>>16;
	      tdcval  = (tdc[ii]&C775_TDC_DATA_MASK);
	      printf("  Ch %d: %04d (0x%08x)",(int)tdcchan,(int)tdcval,(int)tdc[ii]);
	    }
	    printf("\n");
	    taskDelay(30); /* wait a little before checking again */
	  }
	}


      }
    } else { /* Sleep and check again */
      taskDelay(60);
    }

  } while (stopDAQ == 0);

  printf("Exiting polling loop\n");
  stopDAQ=0;
  return;

}
