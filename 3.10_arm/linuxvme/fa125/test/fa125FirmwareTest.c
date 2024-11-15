/*
 * File:
 *    adc125LibTest.c
 *
 * Description:
 *    Test ADC15 with TI interrupts with GEFANUC Linux Driver, 
 *    adc125, and TIR library
 *
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "jvme.h"
#include "fa125Lib.h"
#include "tiLib.h"
#include "sdLib.h"

void myISR(int arg);

DMA_MEM_ID vmeIN,vmeOUT;
extern DMANODE *the_event;
extern unsigned int *dma_dabufp;
extern int fa125BlockError;
extern int vmeBerrStatus;


#define DO_READOUT

#define FA125_ADDR (15<<19)
#define TI_ADDR (21<<19)

#define BLOCKLEVEL 1

FILE *outfile=NULL;

int 
main(int argc, char *argv[]) 
{
  char hostname[50];
  char filename[70];
  int stat;

  stat = gethostname((char *)&hostname,50);
  strcat((char*)&filename,"data/");
  strcat((char*)&filename,hostname);
  strcat((char*)&filename,"_fa125.dat");


  printf("\nFA125 Library Tests (%s)\n",filename);
  printf("----------------------------\n");

  outfile = fopen(filename,"w");
  if(outfile==NULL)
    {
      perror("fopen");
      exit(1);
    }

  vmeOpenDefaultWindows();
    
  /* Setup Address and data modes for DMA transfers
   *   
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1);

  /* INIT dmaPList */

  dmaPFreeAll();
/*   vmeIN  = dmaPCreate("vmeIN",1024*100,5,0); */
  vmeIN  = dmaPCreate("vmeIN",4*72*100*14+100,1,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);
    
  dmaPStatsAll();

  dmaPReInitAll();

  /*     gefVmeSetDebugFlags(vmeHdl,0x0); */
  /* Set the TI structure pointer */
  /*     tiInit((2<<19),TI_READOUT_EXT_POLL,0); */
  tiInit(TI_ADDR,TI_READOUT_EXT_POLL,TI_INIT_SKIP_FIRMWARE_CHECK);
  tiCheckAddresses();

  tiSetBlockLimit(10000);

  char mySN[20];
  printf("0x%08x\n",tiGetSerialNumber((char **)&mySN));
  printf("mySN = %s\n",mySN);

#ifndef DO_READOUT
  tiDisableDataReadout();
  tiDisableA32();
#endif

  tiLoadTriggerTable(0);
    
  tiSetTriggerHoldoff(1,1,1);
  tiSetTriggerHoldoff(2,4,1);

  tiSetPrescale(0);
  tiSetBlockLevel(BLOCKLEVEL);

  stat = tiIntConnect(TI_INT_VEC, myISR, 0);
  if (stat != OK) 
    {
      printf("ERROR: tiIntConnect failed \n");
      goto CLOSE;
    } 
  else 
    {
      printf("INFO: Attached TI Interrupt\n");
    }

  /*     tiSetTriggerSource(TI_TRIGGER_TSINPUTS); */
  tiSetTriggerSource(TI_TRIGGER_PULSER);
  tiEnableTSInput(0x1);

  /*     tiSetFPInput(0x0); */
  /*     tiSetGenInput(0xffff); */
  /*     tiSetGTPInput(0x0); */

  tiSetBusySource(TI_BUSY_LOOPBACK,1);

  tiSetBlockBufferLevel(4);

  tiSetFiberDelay(1,2);
  tiSetSyncDelayWidth(1,0x3f,1);
    
  tiClockReset();
  taskDelay(1);
  tiTrigLinkReset();
  taskDelay(1);
  tiEnableVXSSignals();
  taskDelay(1);
  //  tiSyncReset(1);

  taskDelay(1);
    
  tiStatus(1);

  extern unsigned int fa125AddrList[FA125_MAX_BOARDS];

  fa125AddrList[0] = (5<<19);
  fa125AddrList[1] = (6<<19);
  fa125AddrList[2] = (7<<19);
  int nmods=18;

  int iFlag=0;
/*   iFlag  = (1<<17); /\* use fa125AddrList *\/ */
  /*
    bits 2-1:  defines Trigger source
    (0) 0 0  VXS (P0)
    (1) 0 1  Internal Timer
    (2) 1 0  Internal Multiplicity Sum
    (3) 1 1  P2 Connector (Backplane)
    bit    3:  NOT USED WITH THIS FIRMWARE VERSION
    bits 5-4:  defines Clock Source
    (0) 0 0  P2 Connector (Backplane)
    (1) 0 1  VXS (P0)
    (2) 1 0  Internal 125MHz Clock
  */
  iFlag |= (0<<1);  /* Trigger Source */
  iFlag |= (1<<4);  /* Clock Source */
  iFlag |= (1<<18); /*skip fw check?*/

  //fa125SetByteSwap(0,0);
  int iadc=0, faslot=0;
  int rval=0;
  extern int nfa125;



  stat = fa125Init(3<<19,1<<19,nmods,iFlag);
  //stat = fa125Init(9<<19,1<<19,1,iFlag);
  fa125GStatus(0);

  if (stat != OK) 
    {
      printf("ERROR: fa125Init failed \n");
      goto CLOSE;
    } 


  sdInit(1);
  sdSetActiveVmeSlots( fa125ScanMask());
  sdStatus(1);

  getchar();


  fa125ResetToken(0);
  for(iadc=0; iadc<nfa125; iadc++)
    {
      faslot = fa125Slot(iadc);
      fa125PowerOn(faslot);

      int ichan=0;
      for(ichan=0; ichan<72; ichan++)
	{
	  fa125SetOffset(faslot,ichan,0x4000);
	}
      int i=0;
      for(i=0; i<3 ; i++)
	{
	  /*adc125SetPulserAmplitude(0,i,key_value);*/
	  fa125SetPulserAmplitude(0,i,0x8000);
	}

      fa125SetCommonThreshold(faslot,0x0);

      //fa125SetChannelEnableMask(fa125Slot(iadc),0x0,0x0,0x0);
      //fa125SetChannelEnableMask(fa125Slot(iadc),0x414141,0x414141,0x414141);
      fa125SetChannelEnableMask(fa125Slot(iadc),0xFFFFFF,0xFFFFFF,0xFFFFFF);
      fa125SetScaleFactors(faslot,0,0,4);


      fa125PrintTemps(faslot);
/*       fa125Clear(faslot); */

      fa125SetBlocklevel(faslot, BLOCKLEVEL);

      fa125Reset(faslot, 0);

      rval = fa125SetProcMode(faslot,"CDC_long",100,100,40,4,1,4,4);

      if(rval==ERROR)
        {
          printf("ERROR!\n");
          goto CLOSE;
        }
      fa125Enable(faslot);
      fa125Status(faslot,1);
    }

  fa125ResetToken(0);
  fa125GStatus(0);
  tiSyncReset(1);

  printf("Hit any key to enable Triggers...\n");
  getchar();
  fa125GStatus(0);

  /* Enable the TI and clear the trigger counter */

  tiIntEnable(0);
  tiStatus(1);
#define SOFTTRIG
#ifdef SOFTTRIG
  tiSetRandomTrigger(1,0x3);
  taskDelay(10);
  //tiSoftTrig(1,0x1,0x700,0);
#endif


  printf("Hit any key to Disable TIR and exit.\n");
  getchar();

  tiSetBlockLimit(1);
  tiStatus(1);


#ifdef SOFTTRIG
  /* No more soft triggers */
  /*     tidSoftTrig(0x0,0x8888,0); */
  //  tiSoftTrig(1,0,0x700,0);
  tiDisableRandomTrigger();
#endif

  fa125GStatus(faslot);

  for(iadc=0; iadc<nfa125; iadc++)
    {
      faslot = fa125Slot(iadc);
      fa125PrintTemps(faslot);
      fa125PowerOff(faslot);
      fa125Reset(faslot,1);
      fa125Status(faslot,1);
    }
  printf("berr_count = %d\n",fa125GetBerrCount());

/*   fa125Clear(0); */

  tiIntDisable();

  tiIntDisconnect();


 CLOSE:

  if(outfile!=NULL)
    fclose(outfile);

  vmeCloseDefaultWindows();

  exit(0);
}


/* Interrupt Service routine */
/* void */
/* myISR(int arg) */
/* { */
/*   volatile unsigned short reg; */
/*   unsigned int timeout=0; */
/*   volatile UINT32 data[72*10]; */
/*   int dCnt=0; */
/*   unsigned int i; */
/*   static unsigned int intCount=0; */

/*   /\*   if(tirIntCount%1000 == 0) *\/ */
/*   /\*     printf("Got %d interrupts (Data = 0x%x)\n",tirIntCount,reg); *\/ */

/*   intCount++; */

/* /\*   reg = tirReadData(); *\/ */
/*   do {timeout++;} while((!fa125Poll(0)) && timeout<100000); */
/*   { */
/*     if(timeout==100000)  */
/*       printf("timeout\n"); */
/*     else */
/*       dCnt = fa125ReadEvent(0,(volatile UINT32 *)&data,72*2,(2)<<3); */
/*     /\* 	  printf("%4d: timeout = %d, dCnt = %d\n",intCount,timeout,dCnt); *\/ */
/*   } */

/* /\*   tirIntOutput(0); *\/ */

/*   if(tirIntCount%10==0 && dCnt>0) */
/*     { */
/*       printf("Received %d triggers... dCnt = %d\n",tirIntCount, dCnt); */
/*       for(i=0; i<100;i++) */
/* 	{ */
/* 	  printf(" 0x%08x  ",data[i]); */
/* 	  if((i+1)%4==0) printf("\n"); */
/* 	} */
/*     } */
/* } */

/* Interrupt Service routine */
void
myISR(int arg)
{
  volatile unsigned short reg;
  int dCnt, idata;
  static int len=0;
  DMANODE *outEvent;
  int tibready=0, timeout=0;

  unsigned int tiIntCount = tiGetIntCount();

#ifdef DO_READOUT
  GETEVENT(vmeIN,tiIntCount);

#ifdef DOINT
  tibready = tiBReady();
  if(tibready==ERROR)
    {
      printf("%s: ERROR: tiIntPoll returned ERROR.\n",__FUNCTION__);
      return;
    }

  if(tibready==0 && timeout<10000)
    {
      printf("NOT READY!\n");
      tibready=tiBReady();
      timeout++;
    }

  if(timeout>=100)
    {
      printf("TIMEOUT!\n");
      return;
    }
#endif

  *dma_dabufp++;
  extern int nfa125;

  dCnt = tiReadBlock(dma_dabufp,(490*nfa125)+200,1);
  if(dCnt<=0)
    {
      printf("No data or error.  dCnt = %d\n",dCnt);
    }
  else
    {
/*       dma_dabufp += dCnt; */
/*       printf("FROM TI: dCnt = %d\n",dCnt); */
    
    }
#else /* DO_READOUT */
  tiResetBlockReadout();
#endif /* DO_READOUT */

  int iread=0, iadc=0, faslot=0;;
  timeout=100000;

#ifdef OLDREADOUT
  for(iadc=0; iadc<nfa125; iadc++)
#else
    iadc=0;
#endif
    {
      faslot = fa125Slot(iadc);
/*       fa125SoftTrigger(faslot); */
/*       sleep(1); */
    
#ifdef OLDREADOUT
      iread=0;
      do {iread++;} while((!fa125Poll(faslot)) && iread<timeout);
      {
	if(iread==timeout) 
	  {
	    printf("fa125 (%d) timeout\n",faslot);
	    fa125Status(faslot);
	  }
	else
	  {
	    dCnt = fa125ReadEvent(faslot,(volatile UINT32 *)dma_dabufp,72*3,(2)<<3);
	    /* 	  printf("%4d: timeout = %d, dCnt = %d\n",intCount,timeout,dCnt); */
	    if(dCnt<=0)
	      {
		printf("No fa125 (%d) data or error.  dCnt = %d\n",faslot,dCnt);
	      }
	    else
	      {
		dma_dabufp += dCnt;
	      }
	  }
      }
#else // OLDREADOUT
      int rflag=1;
      if(nfa125>1) rflag=2;
/*       printf("Check for BReady\n"); */
      for(iread=0; iread<timeout; iread++)
	{
	  if(fa125GBready(faslot)==fa125ScanMask())
	    break;
	}
/*       printf("Check for Timeout\n"); */
      if(iread==timeout)
	{
	  printf("fa125 (%d) timeout  (fa125GBready: 0x%08x != 0x%08x)\n",faslot,
		 fa125GBready(faslot), fa125ScanMask());
/* 	  fa125SGtatus(); */
	}
      else
	{
/* 	  printf("Readout\n"); */
/* 	  for(iadc=0; iadc<nfa125; iadc++) */
/* 	    { */
/* 	      rflag=1; */
	      faslot = fa125Slot(iadc);
	      //dCnt = fa125ReadBlock(faslot,(volatile UINT32 *)dma_dabufp,0x6000,rflag);
	      dCnt = fa125ReadBlock(faslot,(volatile UINT32 *)dma_dabufp,0xF000,rflag);

/* 	      if(fa125BlockError) */
/* 		{ */
/* 		  printf("vmeBerrStatus = %d\n", vmeBerrStatus=0); */

/* 		  vmeReadDMARegs(); */
/* 		  dmaPStatsAll(); */
/* 		  for(iadc=0; iadc<nfa125; iadc++) */
/* 		    { */
/* 		      faslot = fa125Slot(iadc); */
/* 		      fa125Status(faslot); */
/* 		    } */
/* 		  tiSetBlockLimit(1); */
/* 		} */
	      if(dCnt<=0)
		{
		  printf("No fa125 (%d) data or error.  dCnt = %d\n",faslot,dCnt);
		}
	      else
		{
		  if(abs(dCnt-0x6000)<2)
		    {
		      printf("%s: dCnt = %d   int = %d\n",
			     __FUNCTION__,dCnt,tiGetIntCount());
		    }
		  dma_dabufp += dCnt;
		}
/* 	    } */
	}
/*   for(iadc=0; iadc<nfa125; iadc++) */
/*     { */
/*       fa125Status(fa125Slot(iadc)); */
/*     } */
  fa125ResetToken(fa125Slot(0));
#endif // OLDREADOUT

    }

/*   for(iadc=0; iadc<nfa125; iadc++) */
/*     { */
/*       fa125Status(fa125Slot(iadc)); */
/*     } */
  PUTEVENT(vmeOUT);
  
  outEvent = dmaPGetItem(vmeOUT);
#define READOUT
#ifdef READOUT
  if(tiIntCount%1==0)
    {
      if(((len!=outEvent->length) && (tiIntCount!=1)) || (fa125BlockError==1))
	{
	  //	  printf("%6d:  prev event length = %d  now = %d   fa125BlockError = %d\n",
	  //		 tiIntCount,len,outEvent->length,fa125BlockError);

	  
#define PRINTCRAP
#ifdef PRINTCRAP
	  	  for(idata=0;idata<outEvent->length;idata++)
	  {
	    //	    fa125DecodeData(LSWAP(outEvent->data[idata]));
	    }
		  //tiSetBlockLimit(1);

#endif
	}

      len = outEvent->length;
	  
#ifdef FOUT      
      fprintf(outfile,"%8d %d\n",tiIntCount,len);
	  

      for(idata=0;idata<len;idata++)
	{
#define OLDPRINTOUT
#ifdef OLDPRINTOUT
	  if((idata%8)==0) fprintf(outfile,"\n\t");
	  fprintf(outfile,"  0x%08x ",(unsigned int)LSWAP(outEvent->data[idata]));
#else
	  fa125DecodeData(LSWAP(outEvent->data[idata]));
#endif // OLDPRINTOUT
	}
      fprintf(outfile,"\n");
#endif //FOUT
    }

/*   printf("<Enter> to continue\n"); */
/*   getchar(); */

#endif
  dmaPFreeItem(outEvent);

  if(tiIntCount%1000==0)
    {
      printf("Received %d triggers...\n",
	     tiIntCount);
      fa125GStatus(faslot);
    }
/*     sleep(1); */
}
