/* sysTempeDma.c - Tempe PCI-X to VME DMA support */

/* Copyright 2004 Motorola, Inc. All Rights Reserved */

/*
modification history
--------------------
01c,13may04,scb Commentary: DMA attribute sample tuned for speed.
01b,30mar04,scb Flush link-list desciptors from cache to DRAM.
01a,03sep03,scb Initial writing
*/

/*
DESCRIPTION

This file contains Tempe PCI-X to VME bridge chip DMA support.
The following functions are supported:

.CS
  sysTempeDmaStart
  sysTempeDmaStatus
  sysTempeDmaAbort
  sysTempeDmaPause
  sysTempeDmaResume
.CE

sysTempeDmaStart():

A Tempe DMA is started by calling sysTempeDmaStart with a channel
specified, DMA descriptor structure, DMA attribute structure and
a DMA status structure which will receive the status of the DMA:

.CS
  STATUS sysTempeDmaStart
      (
      UINT32 chan, 		      /@ channel number (0 or 1) @/
      TEMPE_DMA_DESCRIPTOR * dmaDesc, /@ descriptor @/
      TEMPE_DMA_ATTRIBUTES * dmaAttr, /@ attributes @/
      TEMPE_DMA_STATUS     * dmaStat  /@ DMA status, (or NULL) @/
      )
.CE

The channel parameter (chan) must contain either 0 or 1, depending
on which channel the DMA is to run.  The two channels can be run
concurrently.

The DMA descriptor parameter (dmaDesc) specifies the information which
can change for each link-list block.  This includes a source address,
destination address, byte count and VME bus attributes along with a
'next' pointer which optionally points to another dmaDesc type
parameter.  If the 'next' field of the DMA descriptor parameter is
NULL (0) then a "direct mode" DMA transaction is performed.  If the
'next' field is non-NULL then it must point to another valid DMA
descriptor parameter which will be the second in a linked-list chain.
The second DMA descriptor in a linked-list chain can point to yet
another DMA descriptor and so on, the last DMA descriptor in the chain will
contain NULL in its 'next' pointer.  A linked-list chain can be as
long as desired.

The DMA Attribute parameter (dmaAttr) specifies the information needed 
to program the DMA control register for the particular channel.  Once the
attributes are set for a particular call, they remain unaltered for the
duration of the DMA operation, even if it is a chained-mode operation.
The information in this parameter includes bus usage information 
(DMA block sizes, back-off timers, flush on aborted read options) as well
as an optional 'userHandler' field which points to a user-written function
which gains control when the DMA finishes.  Use of the 'userHandler'
field is optional, if it contains NULL then the sysTempeDmaStart()
function returns to the caller immediately after the DMA is initiated,
allowing for concurrent processing while the DMA is underway.  If the
'userHandler' field contains WAIT_FOREVER then the sysTempeDmaStart()
function will not return until the DMA operation has completed (successfully
or with an error).  While the function is waiting for the DMA to finish
it is blocked on a semaphore so that other tasks may run during DMA
operation.  If 'userHandler' contains something other than NULL or
WAIT_FOREVER, it is assumed to be a pointer to a user written function.
In this case sysTempeDmaStart() will return immediately after initiating
the DMA and the user written handler will be called after the DMA completes.
The user written handler will be operating in the context of an interrupt
service routine so it is important that this function not perform any
blocking calls.  It should execute as quickly as possible since other
interrupt service processing is held off while it executes.

The DMA status parameter will be returned containing information about
the current state of the DMA, including current source address,
current destination address, current link address, decrementing byte
counter and a current upper and lower time base register capture.  The
status information is returned immediately after the DMA is started
and can be subsequently obtained via calls to sysTempeDmaStatus().
The dmaStat parameter can be NULL when calling sysTempeDmaStart(), in
which case no status is returned.

sysTempeDmaStatus():

A Tempe DMA status can be obtained by calling sysTempeDmaStatus with a
channel specified and a structure parameter which receives the status
data.

.CS
  STATUS sysTempeDmaStatus
      (
      UINT32       chan,               /@ Channel to get status @/
      TEMPE_DMA_STATUS *dmaStatus      /@ Current status of channel @/
      )
.CE

The status is read from the Tempe chip at the time of the call to
sysTempeDmaStatus().  Included in the status are values of the
channels status register, current source address, current destination
address, current link address, remaining byte count and an upper and
lower timebase register value.

sysTempeDmaAbort():

An active DMA operation can be terminated by a call to
sysTempeDmaAbort().  The only parameter is a channel number which must
be 0 or 1.  Once a channel has been aborted it cannot be resumed.  A
new call to sysTempeDmaStart() must be made to initiate a new DMA.

.CS
  STATUS sysTempeDmaAbort
      (
      UINT32   chan		/@ Channel to be aborted @/
      )
.CE

Once a channel's DMA has been aborted, it cannot be resumed, a new
call to sysTempeDmaStart() must be made if a new DMA initiation is
desired.

sysTempeDmaPause():

A link-list DMA operation can be paused after the current link-list
descriptor portion of the DMA has completed and before the next descriptor
in the list begins processing.  The only parameter is the channel which is 
to be paused.

.CS
  STATUS sysTempeDmaPause
      (
      UINT32   chan		/@ Channel to be paused @/
      )
.CE

After the channel has been paused, it can be "resumed" vi a call
to sysTempeDmaResume().

sysTempeDmaResume():

A link-list DMA operation which has previously been paused can
be resumed with a call to sysTempeDmaResume().  A channel number
is the only required parameter.

.CS
  STATUS sysTempeDmaResume
      (
      UINT32   chan		/@ Channel to be resumed @/
      )
.CE

Below is a template for setting up a call to sysTempeDmaStart():
See "tempe.h" for a breakdown of these structures.


TEMPE_DMA_DESCRIPTOR dmaDescSample =
    {
    0x01000000,        /@ Source Address 'sourceAddr' @/
    0x81000000,        /@ Destination Address 'destAddr'  @/
    0x00010000,        /@ Number of bytes to transfer 'byteCount' @/
    0,		       /@ 2eSST Broadcast select 'bcastSelect2esst' @/

        /@ The following apply to the source of the DMA @/

        {
        VME_DMA_PCI,   /@ Bus 'srcVmeAttr.bus' @/
        VME_SST320,    /@ 2eSST rate 'srcVmeAttr.vme2esstRate' @/
        VME_MODE_A32,  /@ address mode 'srcVmeAttr.addressMode' @/
        VME_D32,       /@ data width 'srcVmeAttr.dataWidth' @/
        VME_2eSST_OUT, /@ xfer out protocol 'srcVmeAttr.xferProtocolOut' @/
        TRUE,          /@ supervisor access type 'srcVmeAttr.superAccessType' @/
        TRUE	       /@ program access type 'srcVmeAttr.pgmAccessType' @/
        },

        /@ The following apply to the destination of the DMA @/

        {
        VME_DMA_VME,   /@ Bus 'dstVmeAttr.bus' @/
        VME_SST320,    /@ 2eSST rate 'dstVmeAttr.vme2esstRate' @/
        VME_MODE_A32,  /@ address mode 'dstVmeAttr.addressMode' @/
        VME_D32,       /@ data width 'dstVmeAttr.dataWidth' @/
        VME_2eSST_OUT, /@ xfer out protocol 'dstVmeAttr.xferProtocolOut' @/
        TRUE,          /@ supervisor access type 'dstVmeAttr.superAccessType' @/
        TRUE	       /@ program access type 'dstVmeAttr.pgmAccessType' @/
        },
    NULL	       /@ Next descriptor if chained mode 'next' @/
    };

TEMPE_DMA_ATTRIBUTES dmaAttribSample =
    {
        {
        4096,	/@ PCI bus max block size 'busUsg.maxPciBlockSize' @/
        0,	/@ PCI bus back-off timer (usec) 'busUsg.pciBackOffTimer' @/
        2048,	/@ VME bus max block size 'busUsg.maxVmeBlockSize' @/
        0,	/@ VME bus back-off timer 'busUsg.vmeBackOffTimer' @/
        FALSE,	/@ VME flush on aborted read? 'busUsg.vmeFlshOnAbtRead' @/
        FALSE   /@ PCI flush on aborted read? 'busUsg.pciFlshOnAbtRead' @/
        },
    NULL	/@ User-defined completion routine 'userHandler' @/
    };

To start the DMA on channel 0 with the above parameters (turning
on the "don't return until done" option), put the following into 
your code:

    TEMPE_DMA_DESCRIPTOR dmaDesc;
    TEMPE_DMA_ATTRIBUTES dmaAttr;
    TEMPE_DMA_STATUS     dmaStat;
    int 		 chan;	/@ channel number @/
    
    dmaDesc = dmaDescSample;
    dmaAttr = dmaAttrSample;
    chan = 0;
    dmaAttr.userHandler = WAIT_FOREVER;	/@ Don't return till DMA done @/

    if (sysTempeDmaStart (chan, &dmaDesc, &dmaAttr, &dmaStat) == ERROR)
        printf("channel 0: DMA start error\n");

To change the source, destination and byte count and start another
DMA, this time on channel 1 do the following:

    dmaDesc.sourceAddr = 0x02000000;
    dmaDesc.destAddr   = 0x81000000;
    dmaDesc.byteCount  = 0x00020000;
    chan = 1;

    if (sysTempeDmaStart (chan, &dmaDesc, &dmaAttr, &dmaStat) == ERROR)
        printf("channel 1: DMA start error\n");

*/

/* includes */
#include <intLib.h>
    
/* defines */

/* This is extracted from the MV6100 BSP (vxWorks 5.5.1)  */
#define TEMPE_INT_NUM_DMA0   117
#define TEMPE_INT_NUM_DMA1   118

/* typedefs */

typedef struct
    {
    INT_HANDLER userHandler;    /* user supplied notification routine */
    SEM_ID  semDrvId;           /* driver access semaphore */
    SEM_ID  semIntId;           /* interrupt interface semaphore */
    } TEMPE_DMA_DRV_CNTL;

/* globals */

/* Offsets to the DMA registers, indexed by channel number (0 or 1) */

UINT32 dctlx  [] = { TEMPE_DCTL0,  TEMPE_DCTL1 };	/* Control */
UINT32 dstax  [] = { TEMPE_DSTA0,  TEMPE_DSTA1 };	/* Status */
UINT32 dcsaUx [] = { TEMPE_DCSAU0, TEMPE_DCSAU1 };	/* Cur src upper */
UINT32 dcsaLx [] = { TEMPE_DCSAL0, TEMPE_DCSAL1 };	/* Cur src lower */
UINT32 dcdaUx [] = { TEMPE_DCDAU0, TEMPE_DCDAU1 };	/* Cur dst upper */
UINT32 dcdaLx [] = { TEMPE_DCDAL0, TEMPE_DCDAL1 };	/* Cur dst lower */
UINT32 dclaUx [] = { TEMPE_DCLAU0, TEMPE_DCLAU1 };	/* Cur lnk uower */
UINT32 dclaLx [] = { TEMPE_DCLAL0, TEMPE_DCLAL1 };	/* Cur lnk lower */
UINT32 dsaUx  [] = { TEMPE_DSAU0,  TEMPE_DSAU1 };	/* Src upper */
UINT32 dsaLx  [] = { TEMPE_DSAL0,  TEMPE_DSAL1 };	/* Src lower */
UINT32 ddaUx  [] = { TEMPE_DDAU0,  TEMPE_DDAU1 };	/* Dst upper */
UINT32 ddaLx  [] = { TEMPE_DDAL0,  TEMPE_DDAL1 };	/* Dst lower */
UINT32 dsatx  [] = { TEMPE_DSAT0,  TEMPE_DSAT1 };	/* Src attrib */
UINT32 ddatx  [] = { TEMPE_DDAT0,  TEMPE_DDAT1 };	/* Dst attrib */
UINT32 dnlaUx [] = { TEMPE_DNLAU0, TEMPE_DNLAU1 };	/* Next lnk upper */
UINT32 dnlaLx [] = { TEMPE_DNLAL0, TEMPE_DNLAL1 };	/* Next lnk lower */
UINT32 dcntx  [] = { TEMPE_DCNT0,  TEMPE_DCNT1 };	/* Count */
UINT32 ddbsx  [] = { TEMPE_DDBS0,  TEMPE_DDBS1 };	/* Bcst select */

/* locals */

LOCAL TEMPE_DMA_DRV_CNTL tempeDmaCntl [TEMPE_DMA_CHANNEL_COUNT];
LOCAL BOOL isTempeInitialized = FALSE;

/* forward declarations */

STATUS sysTempeDmaStart (UINT32 chan, TEMPE_DMA_DESCRIPTOR * dmaDesc,
    			 TEMPE_DMA_ATTRIBUTES * dmaAttr, 
    			 TEMPE_DMA_STATUS * dmaStat);

STATUS sysTempeDmaStatus (UINT32 chan, TEMPE_DMA_STATUS *dmaStatus);
STATUS sysTempeDmaAbort (UINT32 chan);
STATUS sysTempeDmaPause (UINT32 chan);
STATUS sysTempeDmaResume (UINT32 chan);

LOCAL void sysTempeDmaInit (void);

LOCAL void sysTempeDmaGo (UINT32 chan, UINT32 dctl, VME_LL_DESC * dmaOut);

LOCAL UINT32 sysTempeDmaCtlReg (TEMPE_DMA_ATTRIBUTES * dmaAttr);

LOCAL void sysTempeDmaRegImg   (UINT32 chan, TEMPE_DMA_DESCRIPTOR *dmaDesc, 
			        VME_LL_DESC * dmaOut);


LOCAL void sysTempeDmaHandleInt (UINT32 chan);

/* externals */

IMPORT UINT32 sysTimeBaseUGet(void);	/* Time base Upper fetch */
IMPORT UINT32 sysTimeBaseLGet(void);	/* Time base Lower fetch */

IMPORT UINT32 sysTempeBaseAdrsGet(void);

/*****************************************************************************
*
* sysTempeDmaStart - Configure and start the DMA controller.
*
* Set up the DMA controller for a block mode or chain mode DMA transfer.
* The user must fill out the DMA descriptor and DMA attribute parameters
* according to device specifications.
*
* Control register options are set according to dmaAttr. The descriptor 
* registers are programmed according dmaDesc and the transfer is initiated.
*
* Both direct mode and chain mode are supported.  To invoke chain mode,
* 'next' of dmaDesc must have a nonzero value and point to a legitimate
* "next" DMA descriptor - the chain can be as long as desired.  Otherwise, 
* if 'next' of dmaDesc is zero, the DMA in "direct mode" is started.
*
* The user has three options for being notified of the completion of a
* DMA transaction depending on the contents of the userHandler field
* in the dmaAttr parameter.  The first is to wait for the DMA to
* completely finish before returning (userHandler == WAIT_FOREVER).
* The second is to provide a routine for the DMA interrupt handler to
* call upon completion which must follow the rules for executing
* inside an ISR by not calling waits, prints, etc.  (userHander ==
* user_routine_address).  The third is to return immediately so the
* user task can proceed with execution.  If this option is selected,
* sysTempeDmaStatus() can subsequently be called to test for DMA competion.
*
* RETURNS: OK for successful start of DMA, ERROR if DMA channel is invalid
* or if the channel is busy.
*
*/ 

STATUS sysTempeDmaStart
    (
    UINT32 chan, 		    /* channel number (0 or 1) */
    TEMPE_DMA_DESCRIPTOR * dmaDesc, /* descriptor */
    TEMPE_DMA_ATTRIBUTES * dmaAttr, /* attributes */
    TEMPE_DMA_STATUS     * dmaStat  /* DMA status */
    )
    {
    STATUS status = OK;
    TEMPE_DMA_DRV_CNTL * dmaChanCntl = NULL;
    TEMPE_DMA_DESCRIPTOR * dmaDescStart;
    TEMPE_DMA_DESCRIPTOR * dmaDescCurrent;
    TEMPE_DMA_DESCRIPTOR * loopAddr;
    VME_LL_DESC * dmaOut;
    UINT32 dctrl;

    /*ppcTimeBaseGet(&tb1);*/

    if (isTempeInitialized == FALSE)
	sysTempeDmaInit ();

    if (chan > TEMPE_DMA_CHANNEL_COUNT)
	return (ERROR);

    dmaChanCntl = &tempeDmaCntl[chan];
	    
    /* Compose control register image */

    dctrl = sysTempeDmaCtlReg (dmaAttr);

    /* Now set "direct mode" bit if required */

    if (dmaDesc->next == NULL)
        {

        /* We have direct mode DMA */

        FSET32 (dctrl, 1, TEMPE_DCTLx_MOD_BIT);
        }

    /*
     * Compose the register image (possibly linked) with which to 
     * program the DMA controller.
     */

    dmaDescStart = dmaDesc;		/* Save 1st descriptor ptr */

    if (((UINT32)&dmaDesc->linkDesc & 0x7) == 0)
	dmaOut = (VME_LL_DESC *)&dmaDesc->linkDesc;   /* align to 8-byte */
    else
        dmaOut = (VME_LL_DESC *)&dmaDesc->linkDesc2;	/* packet image */
    loopAddr = dmaDesc;			/* Save for loop detection */

    /*
     * Compute linked packet images inside of dmaOut chain 
     * We will follow the linked-list and at the same time have
     * a trailing address pointer (loopAddr) advancing 1/2 as fast 
     * as we are processing the chain.  If we ever hit the loopAddr
     * value with a new packet to process, we know that we have
     * detected a loop in the chain and we will stop processing.
     */

    sysTempeDmaRegImg (chan, loopAddr, dmaOut);

    /* Flush the descriptor into DRAM */

    cacheFlush (DATA_CACHE, (void *)dmaOut, sizeof(VME_LL_DESC));

    dmaDescCurrent = loopAddr->next;

    while (dmaDescCurrent != NULL)
        {
        if (dmaDescCurrent == loopAddr)
            break;		/* Detected circular chain so exit */

	/* Build machine's descriptor block */

        if (((UINT32)&dmaDescCurrent->linkDesc & 0x7) == 0)
	    dmaOut = (VME_LL_DESC *)&dmaDescCurrent->linkDesc; 
        else
            dmaOut = (VME_LL_DESC *)&dmaDescCurrent->linkDesc2;

        sysTempeDmaRegImg (chan, dmaDescCurrent, dmaOut);

	/* If this is the last one in the chain, set the last link bit */

	if (dmaDescCurrent->next == NULL)
	    {

	    /* Set last link bit */

	    dmaOut->dnlaLx = TEMPE_DNLALx_LLA_MASK;
	    }

	/* Flush the descriptor into DRAM */

	cacheFlush (DATA_CACHE, (void *)dmaOut, sizeof(VME_LL_DESC));

        dmaDescCurrent = dmaDescCurrent->next;  /* Look at next in list */

	if (dmaDescCurrent == NULL)
	    break;		/* End of list - we're done */

	if (dmaDescCurrent == loopAddr)
	    break;		/* Detetcted circular list - exit */

	/* Build machine's descriptor block */

        if (((UINT32)&dmaDescCurrent->linkDesc & 0x7) == 0)
	    dmaOut = (VME_LL_DESC *)&dmaDescCurrent->linkDesc; 
        else
            dmaOut = (VME_LL_DESC *)&dmaDescCurrent->linkDesc2;

        sysTempeDmaRegImg (chan, dmaDescCurrent, dmaOut);

	/* If this is the last one in the chain, set the last link bit */

	if (dmaDescCurrent->next == NULL)
	    {

	    /* Set last link bit */

	    dmaOut->dnlaLx = TEMPE_DNLALx_LLA_MASK;
	    }

	/* Flush the descriptor into DRAM */

	cacheFlush (DATA_CACHE, (void *)dmaOut, sizeof(VME_LL_DESC));

        dmaDescCurrent = dmaDescCurrent->next;	/* Look at next in list */
	loopAddr = loopAddr->next;		/* Advance loop detection */
        }

    /* Protect access to this resource via semaphore "take" */

/*    semTake (dmaChanCntl->semDrvId, WAIT_FOREVER);   */

    /* Make sure channel is not busy (active) */

    if ((TEMPE_READ32(TEMPE_REG_BASE,dstax[chan]) & 
         TEMPE_DSTAx_BSY_MASK) == 0)
	{

        /* Save the handler associated with this DMA invocation */

        dmaChanCntl->userHandler = dmaAttr->userHandler;

        /* 
         * Write the composed register image & control reg to the part 
         * to start the DMA.
         */

        if (((UINT32)&dmaDescStart->linkDesc & 0x7) == 0)
	    dmaOut = (VME_LL_DESC *)&dmaDescStart->linkDesc; 
        else
            dmaOut = (VME_LL_DESC *)&dmaDescStart->linkDesc2;
        dmaOut = (VME_LL_DESC *)&dmaDescStart->linkDesc;   /* 1st in chain */

	/*ppcTimeBaseGet(&tb2);*/

        sysTempeDmaGo (chan, dctrl, dmaOut);

	/*ppcTimeBaseGet(&tb3);*/

/*
        if (dmaStat != NULL)
            sysTempeDmaStatus (chan, dmaStat);

        if ((int)dmaChanCntl->userHandler == WAIT_FOREVER)
            {
            semTake (dmaChanCntl->semIntId, WAIT_FOREVER);
            }
*/

        }
    else
        {
        status = ERROR;		/* Channel was busy */
        }

    /*    semGive (dmaChanCntl->semDrvId);  */

    /*ppcTimeBaseGet(&tb4);*/

    return (status);
    }

/*****************************************************************************
*
* sysTempeDmaStatus - Read and return DMA status.
*
* Capture the current status of a DMA.  This function can be called during
* active DMA operation or after DMA has completed to determine the DMa
* status.
*
* RETURNS:
* OK is returned if the dmaStatus is valid.
* ERROR is returned if the driver is not initialized, or the
* channel is invalid or the dmaStatus reference is null. The
* dmaStatus contents will not be valid.
*/

STATUS sysTempeDmaStatus
    (
    UINT32       chan,               /* Channel to get status */
    TEMPE_DMA_STATUS *dmaStatus      /* Current status of channel */
    )
    {
    STATUS status = OK;


    /*ppcTimeBaseGet(&tb5);*/

    if (isTempeInitialized && 
       (chan < TEMPE_DMA_CHANNEL_COUNT) && dmaStatus != NULL)
	{
        dmaStatus->dstax  = TEMPE_READ32(TEMPE_REG_BASE, dstax[chan]);
	/*        dmaStatus->dcsaUx = TEMPE_READ32(TEMPE_REG_BASE, dcsaUx[chan]);  */
        dmaStatus->dcsaLx = TEMPE_READ32(TEMPE_REG_BASE, dcsaLx[chan]);
	/*        dmaStatus->dcdaUx = TEMPE_READ32(TEMPE_REG_BASE, dcdaUx[chan]);   */
        dmaStatus->dcdaLx = TEMPE_READ32(TEMPE_REG_BASE, dcdaLx[chan]);
	/*        dmaStatus->dclaUx = TEMPE_READ32(TEMPE_REG_BASE, dclaUx[chan]);   */
        dmaStatus->dclaLx = TEMPE_READ32(TEMPE_REG_BASE, dclaLx[chan]);
        dmaStatus->dcntx  = TEMPE_READ32(TEMPE_REG_BASE, dcntx[chan]);
/*	dmaStatus->timeBaseL   = sysTimeBaseLGet ();
	dmaStatus->timeBaseU   = sysTimeBaseUGet ();  */
    
        if (dmaStatus->dstax & TEMPE_DSTAx_DON_MASK) 
            dmaStatus->dmaDone = TRUE;
        else
    	    dmaStatus->dmaDone = FALSE;
	}
    else
	{
	status = ERROR;
	}

    /* ppcTimeBaseGet(&tb6);*/

    return (status);
    }

/******************************************************************************
*
* sysTempeDmaAbort - Abort the current Tempe DMA operation.
*
* For the given valid channel, if the channel is busy, set the "abort"
* bit in the channel's control register to abort the channel's DMA. 
* After setting the "abort" bit in the control register, this function 
* immediately returns.  If it is desired to wait for the abort to actually take
* effect, repeated calls to sysTempeDmaStatus and monitoring of the
* following bits the dstax (status register) should be done:  ABT: (bit 27)
* for a value of 1.
*
* RETURNS: 
* OK is returned if abort succeeded.
* ERROR is returned if the driver is not initialized, or the channel
* is invalid, or the channel was not active.
*/

STATUS sysTempeDmaAbort
    (
    UINT32   chan		/* Channel to be aborted */
    )
    {
    TEMPE_DMA_DRV_CNTL *dmaChanCntl = NULL;
    UINT32 statReg = 0;
    UINT32 cntlReg = 0;
    STATUS status = OK;

    if (isTempeInitialized && (chan < TEMPE_DMA_CHANNEL_COUNT))
        {
        dmaChanCntl = &tempeDmaCntl[chan];

        semTake (dmaChanCntl->semDrvId, WAIT_FOREVER);

	/* Read the status register */

        statReg = TEMPE_READ32 (TEMPE_REG_BASE, dstax[chan]);

        /* If the channel is active, abort the channel */

	if ((statReg & TEMPE_DSTAx_BSY_MASK) != 0)
            {

	    cntlReg = TEMPE_READ32 (TEMPE_REG_BASE, dctlx[chan]);
	    cntlReg |= TEMPE_DCTLx_ABT_MASK;
	    TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dctlx[chan], cntlReg);

	    }
        else
            {

            /* Channel already stopped */

            status = ERROR;
            }

        semGive (dmaChanCntl->semDrvId);
        }
    else
        {

        /* Driver not initialized or invalid parameter */

        status = ERROR;
        }

    return (status);
    }

/******************************************************************************
*
* sysTempeDmaPause - Initiate a pause of the current Tempe DMA operation.
*
* For the given valid channel, if the channel is busy, set the "pause"
* bit in the channel's control register to pause the channel.  Note that
* the Tempe bridge DMA pause only applies to linked-list transfers.  The
* pause will take effect after completion the DMA operation associated
* with the currently active descriptor is done.  After setting the
* "pause" bit in the control register, this function immediately returns.
* Upon return from this function, it is possible that the DMA will still
* be in progress for some time until the operation associated with the
* currently active descriptor has completed.  If it is desired to wait
* for the pause to actually take effect, repeated calls to
* sysTempeDmaStatus and monitoring of the following bits the dstax
* (status register) should be done: PAU: (bit 26) for a value of 1.
*
* RETURNS: 
* OK is returned if pause succeeded.
* ERROR is returned if the driver is not initialized, or the channel
* is invalid, or the channel was not active.
*/

STATUS sysTempeDmaPause
    (
    UINT32   chan		/* Channel to be paused */
    )
    {
    TEMPE_DMA_DRV_CNTL *dmaChanCntl = NULL;
    UINT32 statReg = 0;
    UINT32 cntlReg = 0;
    STATUS status = OK;

    if (isTempeInitialized && (chan < TEMPE_DMA_CHANNEL_COUNT))
        {
        dmaChanCntl = &tempeDmaCntl[chan];

        semTake (dmaChanCntl->semDrvId, WAIT_FOREVER);

	/* Read the status register */

        statReg = TEMPE_READ32 (TEMPE_REG_BASE, dstax[chan]);

        /* If the channel is active, pause the channel */

	if ((statReg & TEMPE_DSTAx_BSY_MASK) != 0)
            {

	    /* Set the "pause" bit in the control register */
	    cntlReg = TEMPE_READ32 (TEMPE_REG_BASE, dctlx[chan]);
	    cntlReg |= TEMPE_DCTLx_PAU_MASK;
	    TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dctlx[chan], cntlReg);

            }
        else
            {

            /* Channel already stopped */

            status = ERROR;
            }

        semGive (dmaChanCntl->semDrvId);
        }
    else
        {

        /* Driver not initialized or invalid parameter */

        status = ERROR;
        }

    return (status);
    }

/******************************************************************************
*
* sysTempeDmaResume - Resume a previously paused Tempe DMA operation.
*
* For a given valid channel, verify that the channel is actually paused.
* If so, write the channel control register's resume bit and the channel 
* will continue the link-list transfer.
*
* RETURNS: 
* OK is returned if the resume succeeded.
* ERROR is returned if the driver is not initialized, or the channel
* is invalid, or the channel is not paused.
*/

STATUS sysTempeDmaResume
    (
    UINT32   chan		/* Channel to be resumed */
    )
    {
    TEMPE_DMA_DRV_CNTL *dmaChanCntl = NULL;
    UINT32 cntlReg = 0;
    STATUS status = OK;

    if (isTempeInitialized && (chan < TEMPE_DMA_CHANNEL_COUNT))
        {
        dmaChanCntl = &tempeDmaCntl[chan];
 
        semTake (dmaChanCntl->semDrvId, WAIT_FOREVER);
 
	/* If the channel is paused, resume it - else return ERROR. */

	if ((TEMPE_READ32(TEMPE_REG_BASE, dstax[chan]) &
		          TEMPE_DSTAx_PAU_MASK) != 0 )
            {
	    cntlReg = TEMPE_READ32 (TEMPE_REG_BASE, dctlx[chan]);
	    cntlReg |= TEMPE_DCTLx_DGO_MASK;	/* Set "go" bit */
	    TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dctlx[chan], cntlReg);
            }
        else
            {
            status = ERROR;	/* Error, channel is not paused */
            }

        semGive (dmaChanCntl->semDrvId);
        }
    else
        {
        status = ERROR;		/* Error, not initialized or bad channel */
        }

    return (status);
    }

/******************************************************************************
*
* sysTempeDmaInit - Initialize DMA and attach DMA Interrupt Handler.
* 
* Initialize the environment to allow for Tempe DMA activity.  The
* initialization includes allocating semaphores for protecting access
* to DMA setup and interrupt handler synchronization.  This routine
* also connects an interrupt handler to handle completion interrupts.
*
* RETURNS: NA
*/

LOCAL void sysTempeDmaInit (void)
    {
    TEMPE_DMA_DRV_CNTL *dmaChanCntl = NULL;
    UINT32 chan = 0;

    if (isTempeInitialized == TRUE)
	return;

    while (chan < TEMPE_DMA_CHANNEL_COUNT)
        {
        dmaChanCntl = &tempeDmaCntl[chan];

        /*
         * Allocate semaphores for protecting access to channel and
	 * for ISR synchronization with completion routine
	 */

        dmaChanCntl->semDrvId = semBCreate (SEM_Q_PRIORITY, SEM_FULL);
        dmaChanCntl->semIntId = semBCreate (SEM_Q_FIFO, SEM_EMPTY);

	chan++;
	}

    /* Connect and enable interrupt service routine for each channel */

    intConnect (INUM_TO_IVEC (TEMPE_INT_NUM_DMA0), sysTempeDmaHandleInt, 0);
    intConnect (INUM_TO_IVEC (TEMPE_INT_NUM_DMA1), sysTempeDmaHandleInt, 1);

    intEnable (TEMPE_INT_NUM_DMA0);
    intEnable (TEMPE_INT_NUM_DMA1);

    isTempeInitialized = TRUE;

    }

/*****************************************************************************
*
* sysTempeDmaGo - Write control values and start the DMA
*
* Writes the "composed" images into the DMA registers to start the DMA
* operation.
*
* RETURNS: NA
*/

LOCAL void sysTempeDmaGo 
    (
    UINT32 chan, 		   /* Channel number (0 or 1) */
    UINT32 dctrl,		   /* Control register image */
    VME_LL_DESC * dmaOut	   /* DMA register output image */
    )
    {
    UINT64 reg64;
    UINT32 reg32;
    UINT32 upper = 0;
    UINT32 lower = 0;

    /* Turn on the "start" bit in control register image */

    dctrl |= TEMPE_DCTLx_DGO_MASK;

    /* If this is link-mode transfer ... */

    if ((dmaOut->dnlaUx | dmaOut->dnlaLx) != 0)
	{

	/* Link list - point part's link addresses to this descriptor */

	reg32 = (UINT32)dmaOut;
        reg64 = reg32;
	UNMAKE64(reg64, upper, lower);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dnlaUx [chan], upper);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dnlaLx [chan], lower);

	/* Clear direct mode bit - making sure it is link-list mode */

	dctrl &= ~TEMPE_DCTLx_MOD_MASK;
	}
    else
	{

	/* Not linked list - load up the DMA registers */

        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dsaUx [chan], dmaOut->dsaUx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dsaLx [chan], dmaOut->dsaLx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, ddaUx [chan], dmaOut->ddaUx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, ddaLx [chan], dmaOut->ddaLx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dsatx [chan], dmaOut->dsatx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, ddatx [chan], dmaOut->ddatx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dnlaUx [chan], dmaOut->dnlaUx);
	TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dnlaLx [chan], dmaOut->dnlaLx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dcntx  [chan], dmaOut->dcntx);
        TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, ddbsx [chan], dmaOut->ddbsx);

	/* Set direct mode bit - making sure it is NOT link-list mode */

	dctrl |= TEMPE_DCTLx_MOD_MASK;

	}

    /* Finally write the control register which starts the whole shebang */

    TEMPE_WRITE32_PUSH (TEMPE_REG_BASE, dctlx [chan], dctrl);

    }

/*****************************************************************************
*
* sysTempeDmaCtlReg - Build Tempe DMA control register image.
*
* This routine uses the DMA attributes structure (one of the parameters
* passed in to start a DMA transfer) to build the control register image
* for one of Tempe DMA channel control registers.  Note that the control
* register is set once per transfer even if the transfer involves multiple
* (linked) packets.
*
* RETURNS: Tempe DMA control register image.
*/

LOCAL UINT32 sysTempeDmaCtlReg
    (
    TEMPE_DMA_ATTRIBUTES * dmaAttr /* attributes */
    )
    {
    UINT32 dctrl;		    /* Return control register image here */
    UINT32 field;

    /*
     * Construct the control register image,
     * start with mode bit which indicates direct mode or linked
     * list mode.
     */

    dctrl = 0;		/* Start with cleared register image */

    /* Control register: VME flush on aborted read */

    if (dmaAttr->busUsg.vmeFlshOnAbtRead == TRUE)
	{
	FSET32 (dctrl, 1, TEMPE_DCTLx_VFAR_BIT);
	}

    /* Control register: PCI flush on aborted read */

    if (dmaAttr->busUsg.pciFlshOnAbtRead == TRUE)
	{
	FSET32 (dctrl, 1, TEMPE_DCTLx_PFAR_BIT);
	}

    /* Control register: VME bus block size */

    for (field = 0; field <= 7; field++)
	{
	if ((32 * (1 << field)) >= (dmaAttr->busUsg.maxVmeBlockSize ))
	    break;
	}

    FSET32 (dctrl, field, TEMPE_DCTLx_VBKS_BIT);

    /* Control register: VME back off timer */

    if (dmaAttr->busUsg.vmeBackOffTimer == 0)
	field = 0;
    else
	{
        for (field = 1; field <= 7; field++)
	    {
	    if ((1 << (field - 1)) >= dmaAttr->busUsg.vmeBackOffTimer)
	        break;
	    }
	}

    FSET32 (dctrl, field, TEMPE_DCTLx_VBOT_BIT);

    /* Control register: PCI bus block size */

    for (field = 0; field <= 7; field++)
	{
	if ((32 * (1 << field)) >= (dmaAttr->busUsg.maxPciBlockSize))
	    break;
	}

    FSET32 (dctrl, field, TEMPE_DCTLx_PBKS_BIT);

    /* Control register: PCI back off timer */

    if (dmaAttr->busUsg.pciBackOffTimer == 0)
	field = 0;
    else
	{
        for (field = 1; field <= 7; field++)
	    {
	    if ((1 << (field - 1)) >= dmaAttr->busUsg.pciBackOffTimer)
	        break;
	    }
	}

    FSET32 (dctrl, field, TEMPE_DCTLx_PBOT_BIT);

    return (dctrl);

    }

/*****************************************************************************
*
* sysTempeDmaRegImg - Build DMA descriptor.
*
* From the Tempe DMA descriptor (one of the parameters passed in to start a
* DMA transer) build the 10-word DMA descriptor.  This descriptor can
* ultimtely be used to program a direct mode transfer or stand as one of the
* memory-resident descriptors in a linked DMA operation.
*
* RETURNS: NA
*/

LOCAL void sysTempeDmaRegImg
    (
    UINT32 chan, 		    /* channel number (0 or 1) */
    TEMPE_DMA_DESCRIPTOR * dmaDesc, /* descriptor */
    VME_LL_DESC * dmaOut  	    /* Return program image here */   
    )
    {
    TEMPE_DMA_DESCRIPTOR * dmaDescNext; /* descriptor */
    UINT64 reg64;
    UINT32 reg32;

    /* Construct source and destination addresses (upper and lower) */

    UNMAKE64 (dmaDesc->sourceAddr, 
	      dmaOut->dsaUx, dmaOut->dsaLx);

    UNMAKE64 (dmaDesc->destAddr, 
	      dmaOut->ddaUx, dmaOut->ddaLx);

    /* Broadcast Select */

    dmaOut->ddbsx = dmaDesc->bcastSelect2esst;

    /* Byte count */

    dmaOut->dcntx = dmaDesc->byteCount;

    /* Now compute the source attribute */

    dmaOut->dsatx = dmaDesc->srcVmeAttr.bus;

    /* Source attribute: 2esst mode rate */

    FSET32 (dmaOut->dsatx, dmaDesc->srcVmeAttr.vme2esstRate, 
            TEMPE_DSATx_2eSSTMx_BIT); 

    /* Source attribute: Transfer mode (2eSST, MBLT, etc.) */

    FSET32 (dmaOut->dsatx, dmaDesc->srcVmeAttr.xferProtocolOut, 
	    TEMPE_DSATx_TMx_BIT);

    /* Source attribute: Data bus width (16 or 32-bit) */

    FSET32 (dmaOut->dsatx, dmaDesc->srcVmeAttr.dataWidth, 
	    TEMPE_DSATx_DBWx_BIT);

    /* Source attribute: Supervisor or nonprivilege access type */

    if (dmaDesc->srcVmeAttr.superAccessType == TRUE)
        FSET32 (dmaOut->dsatx, 1, TEMPE_DSATx_SUP_BIT);

    /* Source attribute: Program or Data access mode */

    if (dmaDesc->srcVmeAttr.pgmAccessType == TRUE)
        FSET32 (dmaOut->dsatx, 1, TEMPE_DSATx_PGM_BIT);
	
    /* Source attribute: Address mode (A32, A24 etc.) */

    FSET32 (dmaOut->dsatx, dmaDesc->srcVmeAttr.addressMode, 0);

    /* Now for the destination attributes */

    dmaOut->ddatx = dmaDesc->dstVmeAttr.bus;

    /* Destination attribute: 2esst mode rate */

    FSET32 (dmaOut->ddatx, dmaDesc->dstVmeAttr.vme2esstRate, 
            TEMPE_DDATx_2eSSTMx_BIT); 

    /* Destination attribute: Transfer mode (2eSST, MBLT, etc.) */

    FSET32 (dmaOut->ddatx, dmaDesc->dstVmeAttr.xferProtocolOut, 
	    TEMPE_DDATx_TMx_BIT);

    /* Destination attribute: Data bus width (16 or 32-bit) */

    FSET32 (dmaOut->ddatx, dmaDesc->dstVmeAttr.dataWidth, 
	    TEMPE_DDATx_DBWx_BIT);

    /* Destination attribute: Supervisor or nonprivilege access type */

    if (dmaDesc->dstVmeAttr.superAccessType == TRUE)
        FSET32 (dmaOut->ddatx, 1, TEMPE_DDATx_SUP_BIT);

    /* Destination attribute: Program or Data access mode */

    if (dmaDesc->dstVmeAttr.pgmAccessType == TRUE)
        FSET32 (dmaOut->ddatx, 1, TEMPE_DDATx_PGM_BIT);
	
    /* Destination attribute: Address mode (A32, A24 etc.) */

    FSET32 (dmaOut->ddatx, dmaDesc->dstVmeAttr.addressMode, 0);

    /* Construct the link addresses */

    if (dmaDesc->next == NULL)
	{
	dmaOut->dnlaUx = 0;	/* Not linked, but instead direct mode */
	dmaOut->dnlaLx = 0;
	}
    else
	{
	dmaDescNext = dmaDesc->next;
	reg32 = (UINT32)&dmaDescNext->linkDesc;
	reg32 &= ~0x7;		/* Round down to 4-byte boundary */
	reg64 = reg32;
        UNMAKE64 (reg64, dmaOut->dnlaUx, dmaOut->dnlaLx);
	}

    }

/******************************************************************************
*
* sysTempeDmaHandleInt - Interrupt handler for DMA completions
*
* Upon completion of a DMA operation, finish the operation by releasing
* the channel semaphore if necessary and optionally calling a user
* supplied handler.
*
* RETURNS: NA
*/

LOCAL void sysTempeDmaHandleInt
    (
    UINT32 chan
    )
    {
    TEMPE_DMA_DRV_CNTL *dmaChanCntl = &tempeDmaCntl[chan];

    /*
     * If the user handler for the channel is set to WAIT_FOREVER
     * then give the semaphore to the waiting DMA start. If the user
     * handler is not WAIT_FOREVER, but non-null, invoke the user
     * routine with the channel number. If the handler is null
     * continue directly to clear the interrupt.
     */

    /*    logMsg("Tempe DMA Interrupt\n",0,0,0,0,0,0);  */

    if ((int)(dmaChanCntl->userHandler) == WAIT_FOREVER)
        {
        semGive (dmaChanCntl->semIntId);
        }
    else if (dmaChanCntl->userHandler != NULL)
        {
        (dmaChanCntl->userHandler) (chan);
        }
    }
