/*      MVME16X mvme_dma.c                              */
/*                                                      */
/* This code transfers data in long word  mode across   */
/* the VMEbus to the MVME167 single board computer.     */
/* The data transfer may be performed in block transfer */
/* mode B32 or B64.                                     */
/*                                                      */
/*      Dave Barker.                                    */
/*      17th May 1993.                                  */
/*      CEBAF.                                          */      
/*                                                      */
/*      Modified: David Abbott                          */
/*                1995                                  */
/* For reference, please see pages 2-47 to 2-61 of the  */
/* MVME16X         single board computers programmer's  */
/* reference guide. Also page 42 of the VMEbus          */
/* Specification manual.                                */

#define BYTES_PER_WORD   4

int 
mvme_dma(loc_addr, loc_inc, vme_addr, vme_inc, num_words, blk_flag)
long  loc_addr;
int   loc_inc;
long  vme_addr;
int   vme_inc;
long  num_words;
int   blk_flag;

{
int ii, done;
long *ptr;

/* Code will transfer NUM_BYTES of data from         */
/* VME address VME_ADDRESS to the local address      */
/* LOCAL_ADDRESS in transfer mode specified by       */
/* blk_flag.                                         */

    /* Program local bus address counter at          */
    /* location $fff40038                            */

    ptr = (long *)0xfff40038;
    *ptr = (long)loc_addr;

    /* Program the VME address counter at            */
    /* location 0xfff4003c                           */
  
    ptr = (long *)0xfff4003c;
    *ptr = (long)vme_addr;

    /* Program the DMAC Byte Counter at              */	
    /* location 0xfff40040                           */
  
    ptr = (long *)0xfff40040;
    *ptr = num_words * BYTES_PER_WORD;

    /* Program the DMAC Control Register 2 at        */
    /* location 0xfff40034                           */
    /* The control register 2 fields are;            */
    /* bits 0-5    VME address modifier codes.       */
    /*             use 0x39                          */
    /* bits 6-7    0x01 for D32 block transfer.      */
    /* bit 8       low for D32 cycles on the VMEbus  */
    /* bit 9       low for data direction from the   */
    /*             VMEbus to the local memory.       */
    /* bit 10      high, increment the local address */
    /*             counter during transfer.          */
    /* bit 11      high, increment the VMEbus address*/
    /*             counter during transfer.          */
    /* bit 12      not used.                         */
    /* bits 13-14  0x4, snoop enabled  .             */
    /* bit 15      low, not used in direct transfer  */
    /* bits 16-31  low, not used.                    */

    ptr = (long *)0xfff40034;

    if (blk_flag == 1)     /* A24  BLK32  DMA transfers */
       {
       *ptr = (0x00002079) | (loc_inc<<10) | (vme_inc<<11);
       }
    else if (blk_flag == 2) /* A24  BLK64  DMA transfers */
       {
       *ptr = (0x000020F9) | (loc_inc<<10) | (vme_inc<<11);
       }
    else              /* A24/D32  Program I/O transfers */
       {
       *ptr = (0x00002039) | (loc_inc<<10) | (vme_inc<<11);
       }

    /* Program the DMAC Control Register 1 at        */
    /* location 0xfff40030                           */

    /* The control register 1 bit fields are;        */
    /* bits 0-1     0x0, request level 0             */
    /* bits 2-3     0x0, release on timeout.         */
    /* bit  4       high, requester in fair mode.    */
    /* bit  5       low, direct transfer mode.       */
    /* bit  6       high, Enable Block Transfer.     */
    /* bit  7       low, not used in direct transfer */
    /* bits 8-31    low, no used.                    */

    /* Note, on power up, the long word at 0xfff40030*/
    /* is defaulted to 0x00040700. Therefore, bits   */
    /* 8-31 are retained at the default values.      */

    ptr = (long *)0xfff40030;
    *ptr = 0x00040750;

    /* Check for DMA transfer Done or Error           */

    ptr = (long *)0xfff40048;
    for(ii=0;ii<1000;ii++) {
      done = *ptr&0x3;
      if (done == 1) {
	return (0);   /* OK */
      }else{
	return (-1); /* Bus Error */
      }
    }
    /* Transfer Timeout */
    logMsg("DMA ERROR: DMA Timeout douring transfer\n",0,0,0,0,0,0);
    return (-1);

}




