#ifndef __TSUTIL__
#define __TSUTIL__
/*
 * Module: tsUtil.h
 *
 * Description: Trigger Supervisor (version 2) Program
 *              Function Library header file.
 *
 * Author:
 *	David Abbott
 *	CEBAF Data Acquisition Group
 *
 * Revision History:
 *	  Revision 1.0  Sept 2007 abbottd
 *
 *        Revision 2.0  2007/07              abbottd
 *
 *        Revision 2.1  2010/01              moffit
 *           - Added Linux compatibility
 */

#include <pthread.h>

pthread_mutex_t tsISR_mutex=PTHREAD_MUTEX_INITIALIZER;

#ifdef VXWORKS
#define TSINTLOCK {				\
    if(pthread_mutex_lock(&tsISR_mutex)<0)	\
      perror("pthread_mutex_lock");		\
}

#define TSINTUNLOCK {				\
    if(pthread_mutex_unlock(&tsISR_mutex)<0)	\
      perror("pthread_mutex_unlock");		\
}
#else
#define TSINTLOCK {				\
    vmeBusLock();				\
}
#define TSINTUNLOCK {				\
    vmeBusUnlock();				\
}
#endif

/* Define TS Version 2 Memory structure */
volatile struct vme_ts2
{
  /* 0x00 */ volatile unsigned int csr;
  /* 0x04 */ volatile unsigned int csr2;
  /* 0x08 */ volatile unsigned int trig;
  /* 0x0C */ volatile unsigned int roc;
  /* 0x10 */ volatile unsigned int sync;
  /* 0x14 */ volatile unsigned int trigCount;
  /* 0x18 */ volatile unsigned int trigData;
  /* 0x1C */ volatile unsigned int lrocData;
  /* 0x20 */ volatile unsigned int prescale[8];
  /* 0x40 */ volatile unsigned int timer[5];
  /* 0x54 */ volatile unsigned int intVec;
  /* 0x58 */ volatile unsigned int rocBufStatus;
  /* 0x5C */ volatile unsigned int lrocBufStatus;
  /* 0x60 */ volatile unsigned int rocAckStatus;
  /* 0x64 */ volatile unsigned int userData[2];
  /* 0x6C */ volatile unsigned int state;
  /* 0x70 */ volatile unsigned int test;
  /* 0x74 */ volatile unsigned int blank1;
  /* 0x78 */ volatile unsigned int scalAssign;
  /* 0x7C */ volatile unsigned int scalControl;
  /* 0x80 */ volatile unsigned int scaler[18];
  /* 0xC8 */ volatile unsigned int scalEvent;
  /* 0xCC */ volatile unsigned int scalLive[2];
  /* 0xD4 */ volatile unsigned int id;
} VME_TS2;


struct ts_state {
  char go[12];
  char trigger[12];
  int ready;
  int busy;
  int seq;
  int sync;
  int feb;
  int inhibit;
  int clear;
  int l1e;
  char buffers[8];
  int branch[5];
  int strobe[5];
  int ack[5];
};


/* Default Loacal Base address for the Trigger Supervisor (A24/D32 or A24/D16) */
#define TS_BASE_ADDR    0xed0000
#define TS_MEM_OFFSET     0x4000
#define TS_BOARD_ID       0x002a

/* Define TS Branch 5 Modes of operation:     Interrupt mode   0
                                              Polling  mode    1 */
#define TS_INT    0
#define TS_POLL   1


/* Define Register Masks */
#define TS_CSR_MASK            0x007f01ff
#define TS_CSR2_MASK           0x0000dfff
#define TS_TRIG_MASK           0x0000ffff
#define TS_TRIG_COUNT_MASK     0x0000ffff
#define TS_TRIG_DATA_MASK      0x00000fff
#define TS_ROC_MASK            0xffffffff
#define TS_SYNC_MASK           0x0000ffff
#define TS_TEST_MASK           0x00007f00
#define TS_STATE_MASK          0x000fffff
#define TS_PRESCALE1_4MASK     0x00ffffff
#define TS_PRESCALE5_8MASK     0x0000ffff
#define TS_TIMER1_4MASK        0x0000ffff
#define TS_TIMER5_MASK         0x000000ff
#define TS_INTVEC_MASK         0x000000ff
#define TS_ROC_STATUS_MASK     0xcfcfcfcf
#define TS_LROC_STATUS_MASK    0x000080cf
#define TS_LROC_DATA_MASK      0x000000ff
#define TS_ACK_STATUS_MASK     0xffffffff
#define TS_SCAL_ASSIGN_MASK    0x00ffffff
#define TS_PROG_EVENT_MASK     0x000000bf
#define TS_SCALER_MASK         0xffffffff
#define TS_SCALER_CONTROL_MASK 0x00ffffff
#define TS_MEM_MASK            0x00ffffff
#define TS_ID_MASK             0x0000ffff

/* Define CSR1 Register Bits */
#define TS_CSR_GO                  0x1
#define TS_CSR_PONS                0x2
#define TS_CSR_PAS                 0x4
#define TS_CSR_SYNC                0x8
#define TS_CSR_P1_EVENT           0x10
#define TS_CSR_P2_EVENT           0x20
#define TS_CSR_ENABLE_L1          0x40
#define TS_CSR_OI                 0x80
#define TS_CSR_TEST_MODE         0x100
#define TS_CSR_RESET            0x4000
#define TS_CSR_INIT             0x8000

#define TS_CSR_SYNC_OCCURED         0x10000
#define TS_CSR_P1_EVENT_OCCURED     0x20000
#define TS_CSR_P2_EVENT_OCCURED     0x40000
#define TS_CSR_LATE_FAIL_OCCURED    0x80000
#define TS_CSR_INHIBIT_OCCURED     0x100000
#define TS_CSR_WFIFO_ERR_OCCURED   0x200000
#define TS_CSR_RFIFO_ERR_OCCURED   0x400000

#define TS_CSR_CLEAR_STATUS 0x80000000
/* Define CSR2 Register Bits */
#define TS_CSR2_ENABLE_SYNC         0x1
#define TS_CSR2_ENABLE_CP_TIMER     0x2
#define TS_CSR2_ENABLE_FB_TIMER     0x4
#define TS_CSR2_ENABLE_CH_TIMER     0x8
#define TS_CSR2_ENABLE_EXT_BUSY    0x10
#define TS_CSR2_LOCK_BRANCH1       0x20
#define TS_CSR2_LOCK_BRANCH2       0x40
#define TS_CSR2_LOCK_BRANCH3       0x80
#define TS_CSR2_LOCK_BRANCH4      0x100
#define TS_CSR2_LOCK_BRANCH5      0x200
#define TS_CSR2_ENABLE_FP_P1      0x400
#define TS_CSR2_ENABLE_INT        0x800
#define TS_CSR2_ENABLE_LROC      0x1000
#define TS_CSR2_DISABLE_9_12     0x4000
#define TS_CSR2_ENABLE_FAST      0x8000
#define TS_CSR2_LOCK_ALL         0x03e0

/* Define Other Register Bits */
#define TS_TRIG_NON_STROBE          0x1
#define TS_TRIG_LEVEL_9_10       0x2000
#define TS_TRIG_LEVEL_11_12      0x4000
#define TS_TRIG_OPEN_PRESCALE    0x8000

#define TS_STATE_L1_ACCEPT            0x1
#define TS_STATE_START_L2             0x2
#define TS_STATE_L2_PASS              0x4
#define TS_STATE_L2_FAIL              0x8
#define TS_STATE_L2_ACCEPT           0x10
#define TS_STATE_START_L3            0x20
#define TS_STATE_L3_PASS             0x40
#define TS_STATE_L3_FAIL             0x80
#define TS_STATE_L3_ACCEPT          0x100
#define TS_STATE_CLEAR              0x200
#define TS_STATE_FE_BUSY            0x400
#define TS_STATE_INHIBIT            0x800
#define TS_STATE_LATCHED           0x1000
#define TS_STATE_BUSY              0x2000
#define TS_STATE_ACTIVE            0x4000
#define TS_STATE_READY             0x8000
#define TS_STATE_MAIN_SEQ_ACTIVE  0x10000
#define TS_STATE_SYNC_SEQ_ACTIVE  0x20000
#define TS_STATE_P1_SEQ_ACTIVE    0x40000
#define TS_STATE_P2_SEQ_ACTIVE    0x80000

#define TS_MEM_L1OK                 0x1
#define TS_MEM_CLASS_MASK           0xe
#define TS_MEM_TTYPE_MASK          0xf0
#define TS_MEM_L1A_MASK          0xff00

#define TS_LROC_SYNC                0x1
#define TS_LROC_LATEFAIL            0x2
#define TS_LROC_CODE_MASK          0xfc
#define TS_LROC_ACK               0x100
#define TS_LROC_STROBE           0x8000
#define TS_LROC_BUF_MASK         0x00cf


/* Define Timers and Resoulutions (Nanoseconds) */
#define TS_TIMER_CP     1
#define TS_TIMER_L2A    2
#define TS_TIMER_L3A    3
#define TS_TIMER_FB     4
#define TS_TIMER_CH     5
#define TS_TIMER_RES   40

/* Define Scaler IDs */
#define TS_SCALER_EVENT     0
#define TS_SCALER_USER1    13
#define TS_SCALER_USER2    14
#define TS_SCALER_USER3    15
#define TS_SCALER_USER4    16
#define TS_SCALER_USER5    17
#define TS_SCALER_USER6    18
#define TS_SCALER_EVENT_ID  0x40000
#define TS_SCALER_LATCH    0x800000

/* User Scaler Assign Codes */
#define TS_SCALER_OR_TRIGGER       0
#define TS_SCALER_LATCHED_TRIGGER  1
#define TS_SCALER_L1_ACCEPT        2
#define TS_SCALER_L2_ACCEPT        3
#define TS_SCALER_L3_ACCEPT        4
#define TS_SCALER_FAST_RESET       5
#define TS_SCALER_CLEAR            6
#define TS_SCALER_L2_PASS          7
#define TS_SCALER_L2_FAIL          8
#define TS_SCALER_L3_PASS          9
#define TS_SCALER_L3_FAIL         10
#define TS_SCALER_LATE_FAIL       11
#define TS_SCALER_SYNC_SHED       12
#define TS_SCALER_SYNC_FORCED     13
#define TS_SCALER_P1_EVENT        14
#define TS_SCALER_P2_EVENT        15

/* Test Register Bits */
#define TS_TEST_LEVEL1_TRIGGER_PULSE (1 << 0)
#define TS_TEST_LEVEL2_PASS_PULSE    (1 << 1)
#define TS_TEST_LEVEL3_PASS_PULSE    (1 << 2)
#define TS_TEST_LEVEL2_FAIL_PULSE    (1 << 3)
#define TS_TEST_LEVEL3_FAIL_PULSE    (1 << 4)
#define TS_TEST_FE_BUSY              (1 << 8)
#define TS_TEST_EXT_INHIBIT          (1 << 9)
#define TS_TEST_BRANCH1_ROC_ACK      (1 << 10)
#define TS_TEST_BRANCH2_ROC_ACK      (1 << 11)
#define TS_TEST_BRANCH3_ROC_ACK      (1 << 12)
#define TS_TEST_BRANCH4_ROC_ACK      (1 << 13)
#define TS_TEST_TEST_ROC_ACK_ENABLE  (1 << 14)


/* Function Prototypes */
int tsInit(unsigned int addr, int iflag);
int tsMemInit();
int tsLive(int sflag);
void tsLiveClear();
unsigned int tsCsr(unsigned int Val);
unsigned int tsCsr2Set(unsigned int cval);
unsigned int tsCsr2Clear(unsigned int cval);
void tsClearStatus();
unsigned int tsEnableInput(unsigned int Val, int tflag);
unsigned int tsRoc(unsigned char b4,unsigned char b3,unsigned char b2,unsigned char b1);
unsigned int tsSync(unsigned int Val);
unsigned int tsPrescale(int Num, unsigned int Val);
unsigned int tsTimerWrite(int Num, unsigned int Val);
unsigned int tsMemWrite(int Num, unsigned int Val);
void tsGo(int iflag);
void tsStop(int iflag);
void tsReset(int iflag);
unsigned int tsStatus (int iflag);
void tsTimerStatus();
unsigned int tsState(int iflag);
unsigned int tsFifoRead(int rflag);
unsigned int tsFifoCount();
unsigned int tsScalRead(int Num, int sflag);
void tsScalClear(unsigned int mask);
unsigned int tsScalAssign(unsigned short u13, unsigned short u14, unsigned short u15,
			  unsigned short u16, unsigned short u17, unsigned short u18);
void tsScalLatch();
void tsScalUnLatch();
void tsScalClearAll();

int  tsIntConnect (unsigned int vector, VOIDFUNCPTR routine, int arg, int poll);
void tsIntDisconnect();
int  tsIntEnable(int iflag);
void tsIntDisable();
void tsIntAck();
unsigned int tsIntType();
int  tsIntTrigData(unsigned int *itype, unsigned int *isyncFlag, unsigned int *ilateFail);
int  tsDecodeTrigData(unsigned int idata, unsigned int *itype, unsigned int *isyncFlag, unsigned int *ilateFail);
int  tsIntTest();
unsigned int tsGetIntCount();

#endif /* __TSUTIL__ */
