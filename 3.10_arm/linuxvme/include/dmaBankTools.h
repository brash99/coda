
#ifndef __DMABANKTOOLS__
#define __DMABANKTOOLS__

unsigned int *dStartOfEvent;

#define EVENTOPEN(bnum, btype, nev) {					\
    dStartOfEvent = (unsigned int *)(dma_dabufp);			\
    *(++(dma_dabufp)) = (syncFlag<<28) | ((bnum) << 16) | ((btype##_ty) << 8) | (0xff & (nev<0)?1:nev); \
    ((dma_dabufp))++;}

#define EVENTCLOSE {							\
    unsigned int eventlen;						\
    eventlen = (unsigned int) (((char *) (dma_dabufp)) - ((char *) dStartOfEvent)); \
    *dStartOfEvent = (eventlen);					\
    if ((eventlen & 1) != 0) {                                          \
      (dma_dabufp) = ((unsigned int *)((char *) (dma_dabufp))+1);	\
      eventlen += 1;                                                    \
      *dStartOfEvent = (eventlen);					\
    };                                                                  \
    if ((eventlen & 2) !=0) {                                           \
      eventlen = eventlen + 2;                                          \
      *dStartOfEvent = (eventlen);					\
      (dma_dabufp) = ((unsigned int *)((short *) (dma_dabufp))+1);;	\
    };                                                                  \
    eventlen = ( (eventlen)>>2) - 1;};


#define BANKOPEN(bnum, btype, code) {					\
    unsigned int *StartOfBank;						\
    StartOfBank = (unsigned int *)(dma_dabufp);			\
    *(++(dma_dabufp)) = (((bnum) << 16) | (btype##_ty) << 8) | (code);	\
    ((dma_dabufp))++;


#define BANKCLOSE							\
  unsigned int banklen;						\
  banklen = (unsigned int) (((char *) (dma_dabufp)) - ((char *) StartOfBank));	\
  *StartOfBank = banklen;						\
  if ((banklen & 1) != 0) {						\
    (dma_dabufp) = ((unsigned int *)((char *) (dma_dabufp))+1);	\
    banklen += 1;							\
    *StartOfBank = banklen;						\
  };									\
  if ((banklen & 2) !=0) {						\
    banklen = banklen + 2;						\
    *StartOfBank = banklen;						\
    (dma_dabufp) = ((unsigned int *)((short *) (dma_dabufp))+1);;	\
  };									\
  *StartOfBank = ( (banklen) >> 2) - 1;					\
  };

#define DMAWRITE32(dat) {*(dma_dabufp)++ = LSWAP(dat);}
#define DMAWRITE16(dat) {*((short *) dma_dabufp)++ = LSWAP(dat);}
#define DMAWRITE8(dat)  {*((char  *) dma_dabufp)++ = LSWAP(dat);}

#endif
