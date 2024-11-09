/******************************************************************************
*
*  c775Lib.h  -  Driver library header file for readout of a C.A.E.N. multiple
*                Model 775 TDCs using a VxWorks 5.2 or later based single board 
*                computer.
*
*  Author: David Abbott 
*          Jefferson Lab Data Acquisition Group
*          March 2002
*
*  SVN: $Rev: 401 $
*
*/
#ifndef __C775LIB__
#define __C775LIB__

#define C775_MAX_CHANNELS   32
#define C775_MAX_WORDS_PER_EVENT  34

/* Define a Structure for access to TDC*/
struct c775_struct {
  volatile unsigned long  data[512];
  unsigned long blank1[512];
  volatile unsigned short rev;
  volatile unsigned short geoAddr;
  volatile unsigned short cbltAddr;
  volatile unsigned short bitSet1;
  volatile unsigned short bitClear1;
  volatile unsigned short intLevel;
  volatile unsigned short intVector;
  volatile unsigned short status1;
  volatile unsigned short control1;
  volatile unsigned short aderHigh;
  volatile unsigned short aderLow;
  volatile unsigned short ssReset;
  unsigned short blank2;
  volatile unsigned short cbltControl;
  unsigned short blank3[2];
  volatile unsigned short evTrigger;
  volatile unsigned short status2;
  volatile unsigned short evCountL;
  volatile unsigned short evCountH;
  volatile unsigned short incrEvent;
  volatile unsigned short incrOffset;
  volatile unsigned short loadTest;
  unsigned short blank4;
  volatile unsigned short fclrWindow;
  volatile unsigned short bitSet2;
  volatile unsigned short bitClear2;
  volatile unsigned short wMemTestAddr;
  volatile unsigned short memTestWordH;
  volatile unsigned short memTestWordL;
  volatile unsigned short crateSelect;
  volatile unsigned short testEvWrite;
  volatile unsigned short evCountReset;
  unsigned short blank5[15];
  volatile unsigned short iped;
  unsigned short blank6;
  volatile unsigned short rTestAddr;
  unsigned short blank7;
  volatile unsigned short swComm;
  volatile unsigned short slideConst;
  unsigned short blank8[2];
  volatile unsigned short AAD;
  volatile unsigned short BAD;
  unsigned short blank9[6];
  volatile unsigned short threshold[C775_MAX_CHANNELS];
};

struct c775_ROM_struct {
  volatile unsigned short OUI_3;
  unsigned short blank1;
  volatile unsigned short OUI_2;
  unsigned short blank2;
  volatile unsigned short OUI_1;
  unsigned short blank3;
  volatile unsigned short version;
  unsigned short blank4;
  volatile unsigned short ID_3;
  unsigned short blank5;
  volatile unsigned short ID_2;
  unsigned short blank6;
  volatile unsigned short ID_1;
  unsigned short blank7[7];
  volatile unsigned short revision;
};


#define C775_BOARD_ID   0x00000307

/* Define Address offset for 68K based A24/D32 VME addressing */
/* default VMEChip2 programming only supports A24/D16 */
#define C775_68K_A24D32_OFFSET   0xe0000000

/* Define default interrupt vector/level */
#define C775_INT_VEC      0xaa
#define C775_VME_INT_LEVEL   4

#define C775_MIN_FSR       140  /* nsec (High resoulution) */
#define C775_MAX_FSR      1200  /* nsec (Low resoulution) */

#define C775_ROM_OFFSET    0x8026

/* Register Bits */
#define C775_VME_BUS_ERROR 0x8
#define C775_SOFT_RESET    0x80
#define C775_DATA_RESET    0x4

#define C775_BUFFER_EMPTY  0x2
#define C775_BUFFER_FULL   0x4

#define C775_DATA_READY    0x1
#define C775_BUSY          0x4

#define C775_BLK_END       0x04
#define C775_BERR_ENABLE   0x20
#define C775_ALIGN64       0x40

#define C775_MEM_TEST            0x1
#define C775_OFFLINE             0x2
#define C775_OVERFLOW_SUP        0x8
#define C775_UNDERFLOW_SUP      0x10
#define C775_INVALID_SUP        0x20
#define C775_TEST_MODE          0x40
#define C775_SLIDE_ENABLE       0x80
#define C775_COMMON_STOP       0x400
#define C775_AUTO_INCR         0x800
#define C775_INC_HEADER       0x1000
#define C775_SLIDE_SUB_ENABLE 0x2000
#define C775_INCR_ALL_TRIG    0x4000


#define C775_DATA           0x00000000
#define C775_HEADER_DATA    0x02000000
#define C775_TRAILER_DATA   0x04000000
#define C775_INVALID_DATA   0x06000000


/* Register Masks */
#define C775_BITSET1_MASK   0x0098
#define C775_INTLEVEL_MASK  0x0007
#define C775_INTVECTOR_MASK 0x00ff
#define C775_STATUS1_MASK   0x01ff
#define C775_CONTROL1_MASK  0x0074
#define C775_STATUS2_MASK   0x00f6
#define C775_BITSET2_MASK   0x7fff
#define C775_EVTRIGGER_MASK 0x001f
#define C775_FSR_MASK       0x00ff

#define C775_DATA_ID_MASK    0x07000000
#define C775_WORDCOUNT_MASK  0x00003f00
#define C775_CHANNEL_MASK    0x003f0000
#define C775_CRATE_MASK      0x00ff0000
#define C775_EVENTCOUNT_MASK 0x00ffffff
#define C775_GEO_ADDR_MASK   0xf8000000
#define C775_TDC_DATA_MASK   0x00000fff

/* Macros */
#define C775_EXEC_SOFT_RESET(id) {					\
    c775Write(&c775p[id]->bitSet1, C775_SOFT_RESET);			\
    c775Write(&c775p[id]->bitClear1, C775_SOFT_RESET);}

#define C775_EXEC_DATA_RESET(id) {					\
    c775Write(&c775p[id]->bitSet2, C775_DATA_RESET);			\
    c775Write(&c775p[id]->bitClear2, C775_DATA_RESET);}

#define C775_EXEC_READ_EVENT_COUNT(id) {				\
    volatile unsigned short s1, s2;					\
    s1 = c775Read(&c775p[id]->evCountL);				\
    s2 = c775Read(&c775p[id]->evCountH);				\
    c775EventCount[id] = (c775EventCount[id]&0xff000000) +		\
      (s2<<16) +							\
      (s1);}
#define C775_EXEC_SET_EVTREADCNT(id,val) {				\
    if(c775EvtReadCnt[id] < 0)						\
      c775EvtReadCnt[id] = val;						\
    else								\
      c775EvtReadCnt[id] = (c775EvtReadCnt[id]&0x7f000000) + val;}

#define C775_EXEC_CLR_EVENT_COUNT(id) {		\
    c775Write(&c775p[id]->evCountReset, 1);	\
    c775EventCount[id] = 0;}
#define C775_EXEC_INCR_EVENT(id) {			\
    c775Write(&c775p[id]->incrEvent, 1);		\
    c775EvtReadCnt[id]++;}
#define C775_EXEC_INCR_WORD(id) {		\
    c775Write(&c775p[id]->incrOffset, 1);}
#define C775_EXEC_GATE(id) {			\
    c775Write(&c775p[id]->swComm, 1);}

/* Function Prototypes */
STATUS c775Init (UINT32 addr, UINT32 addr_inc, int nadc, UINT16 crateID);
void   c775Status( int id, int reg, int sflag);
int    c775PrintEvent(int id, int pflag);
int    c775ReadEvent(int id, UINT32 *data);
int    c775FlushEvent(int id, int fflag);
int    c775ReadBlock(int id, volatile UINT32 *data, int nwrds);
STATUS c775IntConnect (VOIDFUNCPTR routine, int arg, UINT16 level, UINT16 vector);
STATUS c775IntEnable (int id, UINT16 evCnt);
STATUS c775IntDisable (int iflag);
STATUS c775IntResume (void);
UINT16 c775Sparse(int id, int over, int under);
int    c775Dready(int id);
int    c775SetFSR(int id, UINT16 fsr);
INT16  c775BitSet2(int id, UINT16 val);
INT16  c775BitClear2(int id, UINT16 val);
void   c775ClearThresh(int id);
void   c775Gate(int id);
void   c775IncrEventBlk(int id, int count);
void   c775IncrEvent(int id);
void   c775IncrWord(int id);
void   c775Enable(int id);
void   c775Disable(int id);
void   c775CommonStop(int id);
void   c775CommonStart(int id);
void   c775Clear(int id);
void   c775Reset(int id);

#endif /* __C775LIB__ */
