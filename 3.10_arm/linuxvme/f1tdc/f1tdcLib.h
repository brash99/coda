/******************************************************************************
*
 *  f1tdcLib.h  -  Driver library for JLAB config and readout of JLAB F1
 *                 TDC v2/v3 using a VxWorks 5.4 or later, or Linux
 *                 based Single Board computer.
 *
 *  Authors: David Abbott 
 *           Jefferson Lab Data Acquisition Group
 *           August 2003
 *
 *           Bryan Moffit
 *           Jefferson Lab Data Acquisition Group
 *           July 2013
 *
 */

#ifndef __F1TDCLIB__
#define __F1TDCLIB__

#define F1_BOARD_ID         0xf10000
#define F1_MAX_BOARDS             20
#define F1_MAX_TDC_CHANNELS       64
#define F1_MAX_HITS_PER_CHANNEL    8
#define F1_MAX_TDC_CHIPS           8
#define F1_MAX_A32_MEM      0x800000   /* 8 Meg */
#define F1_MAX_A32MB_SIZE  0x1000000  /* 16 MB */
#define F1_VME_INT_LEVEL           2     
#define F1_VME_INT_VEC          0xEE

#define F1_SUPPORTED_V2_FIRMWARE   0x33
#define F1_SUPPORTED_V3_FIRMWARE   0x33
#define F1_SUPPORTED_FIRMWARE(x) ((x==2)? F1_SUPPORTED_V2_FIRMWARE : F1_SUPPORTED_V3_FIRMWARE)

struct F1TDC_STATE_REGS
{
  /* 0x000 */ volatile unsigned int level;
  /* 0x004 */ volatile unsigned int csr;
  /* 0x008 */ volatile unsigned int value;
  /* 0x00C */ volatile unsigned int state;
};

struct F1TDC_EB_REGS
{
  /* 0x010 */ volatile unsigned int build_timer;
  /* 0x014 */ volatile unsigned int insert_count;
  /* 0x018 */ volatile unsigned int delete_count;
  /* 0x01C          */ unsigned int spare;
  /* 0x020 */ volatile unsigned int monitor;
  /* 0x024 */ volatile unsigned int progress;
};

/* Define structure for access to F1 config and control registers */
struct f1tdc_struct 
{
  /* 0x0000 */ volatile unsigned int version;
  /* 0x0004 */ volatile unsigned int csr;
  /* 0x0008 */ volatile unsigned int ctrl;
  /* 0x000C */ volatile unsigned int ev_count;
  /* 0x0010 */ volatile unsigned int blocklevel;
  /* 0x0014 */ volatile unsigned int intr;
  /* 0x0018 */ volatile unsigned int adr32;
  /* 0x001C */ volatile unsigned int adr_mb;
  /* 0x0020 */ volatile unsigned short stat[8];
  /* 0x0030 */ volatile unsigned int scaler1;
  /* 0x0034 */ volatile unsigned int scaler2;
  /* 0x0038          */ unsigned int blank0;
  /* 0x003C */ volatile unsigned int config;
  /* 0x0040 */ volatile unsigned int ctrl2;
  /* 0x0044 */ volatile unsigned int config_csr;
  /* 0x0048 */ volatile unsigned int config_data;
  /* 0x004C */ volatile unsigned int serial_eprom;
  /* 0x0050 */ volatile unsigned int trig2_scaler;
  /* 0x0054 */ volatile unsigned int sync_scaler;
  /* 0x0058 */ volatile unsigned int test_scaler;
  /* 0x005C */ volatile unsigned int token_test;
  /* 0x0060 */ volatile unsigned int token_test_scaler;
  /* 0x0064 */ volatile unsigned int pulser_delay;   /* V3 only */
  /* 0x0068 */ volatile unsigned int pulser_dac;     /* V3 only */
  /* 0x006C */ volatile unsigned int pulser_control; /* V3 only */
  /* 0x0070 */ volatile unsigned int block_count;
  /* 0x0074 */ volatile unsigned int block_fifo_count;
  /* 0x0078 */ volatile unsigned int block_word_count_fifo;
  /* 0x007C */ volatile unsigned int status[(0xC0-0x7C)>>2];
  /* 0x00C0 */ volatile unsigned int test_config;
  /* 0x00C4 */ volatile unsigned int clock_scaler;
  /* 0x00C8 */ volatile unsigned int p0_sync_scaler;
  /* 0x00CC */ volatile unsigned int p0_trig1_scaler;
  /* 0x00D0 */ volatile unsigned int p0_trig2_scaler;
  /* 0x00D4          */ unsigned int blank1[(0x100-0xD4)>>2];
  /* 0x0100 */ struct F1TDC_STATE_REGS state;
  /* 0x0110 */ struct F1TDC_EB_REGS eb;
  /* 0x0124 */ volatile unsigned int busy_control;
};

/* Define version bits */
#define F1_VERSION_FIRMWARE_MASK        0xFF
#define F1_VERSION_BOARDREV_MASK      0xFF00
#define F1_VERSION_BOARDTYPE_MASK 0xFFFF0000

/* Define iFlag bits in f1Init(..) */
#define F1_SRSRC_SOFT       (0<<0)
#define F1_SRSRC_EXT        (1<<0)
#define F1_SRSRC_FP         F1_SRSRC_EXT
#define F1_SRSRC_VXS        F1_SRSRC_EXT
#define F1_SRSRC_MASK         0x1
#define F1_TRIGSRC_SOFT     (0<<1)
#define F1_TRIGSRC_FP       (1<<1)
#define F1_TRIGSRC_VXS      (2<<1)
#define F1_TRIGSRC_MASK       0x6
#define F1_CLKSRC_INT       (0<<4)
#define F1_CLKSRC_FP        (1<<4)
#define F1_CLKSRC_VXS       (2<<4)
#define F1_CLKSRC_MASK       0x30
#define F1_IFLAG_NOINIT    (1<<16)
#define F1_IFLAG_USELIST   (1<<17)
#define F1_IFLAG_NOFWCHECK (1<<18)

/* Define CSR bits */
#define F1_CSR_MASK                0x7ffff
/* #define F1_CSR_ENABLE_INPUTS          0x1 */
/* #define F1_CSR_CONFIG_ERROR           0x2 */
#define F1_CSR_REFCLK_PLL_LOCKED    (1<<0)
#define F1_CSR_BOARDCLK_PLL_LOCKED  (1<<1)
#define F1_CSR_CONFIG_ERROR         (1<<2)
/* #define F1_CSR_CONFIG_ACTIVE          0x4 */
#define F1_CSR_BLOCK_ACCEPTED       (1<<3)
#define F1_CSR_BLOCK_READY          (1<<4)
#define F1_CSR_BERR_STATUS          (1<<5)
#define F1_CSR_TOKEN_STATUS         (1<<6)
#define F1_CSR_MODULE_EMPTY         (1<<7)       
#define F1_CSR_ERROR_MASK           0xFF00
#define F1_CSR_V3_ERROR_MASK        0x3F00
#define F1_CSR_ERROR_TDC0           (1<<8)
#define F1_CSR_ERROR_TDC1           (1<<9)
#define F1_CSR_ERROR_TDC2          (1<<10)
#define F1_CSR_ERROR_TDC3          (1<<11)
#define F1_CSR_ERROR_TDC4          (1<<12)
#define F1_CSR_ERROR_TDC5          (1<<13)
#define F1_CSR_ERROR_TDC6          (1<<14)
#define F1_CSR_ERROR_TDC7          (1<<15)
/* #define F1_CSR_NEXT_BUF_NO        0x10000 */
#define F1_CSR_BUF0_EMPTY          (1<<17)
#define F1_CSR_BUF1_EMPTY          (1<<18)
/* #define F1_CSR_EOB_FLAG           0x80000 */
#define F1_CSR_TOKEN_RETURN        (1<<24)
/* #define F1_CSR_FILLER_FLAG       0x100000 */
/* #define F1_CSR_EXTFIFO_EMPTY     0x200000 */
/* #define F1_CSR_PULSE_SPARE_OUT 0x10000000 */
#define F1_CSR_START               (1<<27)
#define F1_CSR_SYNC_RESET          (1<<28)
#define F1_CSR_TRIGGER             (1<<29)
#define F1_CSR_SOFT_RESET          (1<<30)
#define F1_CSR_HARD_RESET          (1<<31)

/* Define Control Register bits */
#define F1_CONTROL_MASK        0xffffffff
#define F1_CTRL_SIGNALS_MASK   0x000003ff
/* #define F1_FB_SEL                  (1<<0) */
/* #define F1_REF_CLK_PCB             (1<<1) */
/* #define F1_REF_CLK_SEL             (1<<2) */
#define F1_REFCLK_SRC_MASK         0x7
#define F1_REFCLK_SRC_INTERNALFP  (1<<0)
#define F1_REFCLK_INTERNAL_ENABLE (1<<1)
#define F1_REFCLK_SRC_FP          (1<<2)
#define F1_REFCLK_VXS             0
#define F1_REFCLK_INTERNAL        3
#define F1_REFCLK_FP              5
#define F1_SYNC_RESET_SRC_MASK     0x18
#define F1_SYNC_RESET_SRC_FP       (1<<3)
#define F1_SYNC_RESET_SRC_P0       (2<<3)
#define F1_SYNC_RESET_SRC_SOFT     (3<<3)
#define F1_TRIGGER_SRC_MASK        0x60
#define F1_TRIGGER_SRC_FP          (1<<5)
#define F1_TRIGGER_SRC_P0          (2<<5)
#define F1_TRIGGER_SRC_SOFT        (3<<5)
#define F1_START_SRC_MASK          0x180
#define F1_START_SRC_FP            (1<<7)
#define F1_START_SRC_P0            (2<<7)
#define F1_START_SRC_SOFT          (3<<7)
/* #define F1_EVENT_LEVEL_INT         0x8 */
/* #define F1_ERROR_INT              0x10 */
#define F1_ENABLE_SOFT_CONTROL     (1<<9)
#define F1_INT_ENABLE              (1<<24)
#define F1_ENABLE_BERR             (1<<25)
#define F1_ENABLE_MULTIBLOCK       (1<<26)
#define F1_FIRST_BOARD             (1<<27)
#define F1_LAST_BOARD              (1<<28)
#define F1_MB_TOKEN_P0             (1<<29)
#define F1_MB_TOKEN_P2             (1<<30)
#define F1_MB_CONFIG_MASK       0x7c000000
/* #define F1_ENABLE_HEADERS        0x400 */
/* #define F1_ENABLE_FP_BUSY        0x800 */
/* #define F1_ENABLE_SPARE_OUT     0x1000 */
/* #define F1_ENABLE_DATA_TDC0    0x10000 */
/* #define F1_ENABLE_DATA_TDC1    0x20000 */
/* #define F1_ENABLE_DATA_TDC2    0x40000 */
/* #define F1_ENABLE_DATA_TDC3    0x80000 */
/* #define F1_ENABLE_DATA_TDC4   0x100000 */
/* #define F1_ENABLE_DATA_TDC5   0x200000 */
/* #define F1_ENABLE_DATA_TDC6   0x400000 */
/* #define F1_ENABLE_DATA_TDC7   0x800000 */

/* Define ev_count bits */
#define F1_EVENT_COUNT_MASK    0x00FFFFFF

/* Define block_count masks */
#define F1_BLOCK_COUNT_MASK    0x000FFFFF

/* Define blocklevel bits */
#define F1_BLOCKLEVEL_MASK  0xFFFF

/* Define intr bits */
#define F1_INT_VEC_MASK            0xff
#define F1_INT_LEVEL_MASK         0x700
#define F1_SLOT_ID_MASK        0x1f0000

/* Define adr32 bits */
#define F1_A32_ENABLE            (1<<0)
#define F1_A32_ADDR_MASK        0xffc0

/* define adr_mb bits */
#define F1_AMB_ENABLE            (1<<0)
#define F1_AMB_MIN_MASK          0xffc0
#define F1_AMB_MAX_MASK      0xffc00000


/* Define Chip status register Bits */
#define F1_CHIP_RES_LOCKED             (1<<0)
#define F1_CHIP_HITFIFO_OVERFLOW       (1<<1)
#define F1_CHIP_TRIGFIFO_OVERFLOW      (1<<2)
#define F1_CHIP_OUTFIFO_OVERFLOW       (1<<3)
#define F1_CHIP_EXTFIFO_FULL           (1<<4)
#define F1_CHIP_EXTFIFO_ALMOST_FULL    (1<<5)
#define F1_CHIP_EXTFIFO_EMPTY          (1<<6)
#define F1_CHIP_INITIALIZED            (1<<7)
#define F1_CHIP_LOSS_OF_LOCK_OCCURRED  (1<<8)
#define F1_CHIP_CLEAR_STATUS          (1<<15)
#define F1_CHIP_ERROR_COND             0x1f1e
#define F1_CHIP_STAT_MASK              0x007e
#define F1_CHIP_LATCH_STAT_MASK        0x1f00

/* Define CONFIG REGISTER Bits */
#define F1_HIREZ_MODE           0x8000

#define F1_CONFIG_CHIP_MASK     0x00E00000
#define F1_CONFIG_COMMON        (1<<20)
#define F1_CONFIG_REG_MASK      0x000F0000
#define F1_CONFIG_DATA_MASK     0x0000FFFF

/* Define ctrl2 bits */
#define F1_GO_DATA                 (1<<0)
#define F1_FORCE_CHIP_HEADERS      (1<<1)
#define F1_FORCE_BUSY             (1<<15)
#define F1_SYSTEM_TEST_MODE       (1<<16)
#define F1_SINGLE_BOARD_TEST_MODE (1<<17)
#define F1_TRIGGER_LED_ENABLE     (1<<18)
#define F1_STATUS_LED_ENABLE      (1<<19)

/* Define config_csr bits */
#define F1_CONFIG_CSR_WRITE_EN       (1<<31)
#define F1_CONFIG_CSR_BULK_ERASE     (1<<30)
#define F1_CONFIG_CSR_SECTOR_ERASE   (1<<29)
#define F1_CONFIG_CSR_SECTOR_BUSY    (1<<8)
#define F1_CONFIG_CSR_LAST_DATA_MASK 0xFF

/* Define config_data bits */
#define F1_CONFIG_DATA_ADDR_MASK 0xFFFFFF00
#define F1_CONFIG_DATA_DATA_MASK 0x000000FF

/* Define Bit Masks */
#ifdef NOTSURE
/* #define F1_VERSION_MASK        0x00ff */
/* #define F1_BOARD_MASK          0xff00 */
/* #define F1_CSR_ERROR_MASK      0x0000ff00 */
#define F1_CONTROL_SEL_MASK    0x7
#define F1_EVENT_LEVEL_MASK    0xffff
#define F1_INT_ENABLE_MASK     0x18
#define F1_MODULE_EMPTY_MASK   (F1_CSR_ZERO_EVENTS_FLAG | F1_CSR_BUF0_EMPTY | F1_CSR_BUF1_EMPTY | F1_CSR_EXTFIFO_EMPTY)
#endif/*  NOTSURE */

/* #define F1_SDC_MASK          0xfff0 */
/* #define F1_BDC_MASK          0x0008 */

#define F1_ALL_CHIPS          0xff
#define F1_OFFSET_MASK        0x3f3f
#define F1_ENABLE_EDGES       0x4040   /* Rising edges only - default */
#define F1_ENABLE_DUAL_EDGES  0xc0c0   /* Both rising and falling edges */
#define F1_DISABLE_EDGES_ODD  0xff3f
#define F1_DISABLE_EDGES_EVEN 0x3fff
#define F1_DISABLE_EDGES      0x3f3f

/* Define pulser register bits and masks - V3 only */
#define F1_PULSER_DELAY_MASK     0x00000fff
#define F1_PULSER_DAC_RESET      0x002f0000
#define F1_PULSER_DAC_INT_REF    0x003f0001
#define F1_PULSER_DAC_MASK       0x0000fff0
#define F1_PULSER_DAC_A_VALUE    0x00000000
#define F1_PULSER_DAC_B_VALUE    0x00010000
#define F1_PULSER_DAC_BOTH_VALUE 0x00070000
#define F1_PULSER_PULSE_OUT      (1<<0)
#define F1_PULSER_TRIGGER_OUT    (1<<1)

/* define test_config register bits */
#define F1_TEST_TRIG_OUT              (1<<0)
#define F1_TEST_BUSY_OUT              (1<<1)
#define F1_TEST_SDLINK_OUT            (1<<2)
#define F1_TEST_TOKEN_OUT             (1<<3)
#define F1_TEST_STATBITB_IN           (1<<8)
#define F1_TEST_TOKEN_IN              (1<<9)
#define F1_TEST_CLOCK_COUNTER_STATUS  (1<<15)

/* define scaler register bits */
#define F1_CLOCK_SCALER_RESET     0
#define F1_CLOCK_SCALER_START     0
#define F1_SYNC_SCALER_RESET      0
#define F1_TRIG1_SCALER_RESET     0
#define F1_TRIG2_SCALER_RESET     0

/* Define State registers bits and masks */
#define F1_STATE_LEVEL_CAPTURE_MASK    0x000000FF

#define F1_STATE_CSR_ARM_BUFFER        (1<<31)
#define F1_STATE_CSR_BUFFER_FULL       (1<<27)
#define F1_STATE_CSR_BUFFER_EMPTY      (1<<26)
#define F1_STATE_CSR_BUFFER_WORDS_MASK 0x000001FF

#define F1_STATE_VALUE_MASK            0x0003FFFF

/* Define busy_control bits and masks */
#define F1_BUSYCONTROL_FIFO_WORD_CNT_EN          (1<<31)
#define F1_BUSYCONTROL_EVENTS_NOT_BUILT_CNT_EN   (1<<30)
#define F1_BUSYCONTROL_BUSY_STATUS               (1<<24)
#define F1_BUSYCONTROL_WORD_COUNT_BUSY_STATUS    (1<<23)
#define F1_BUSYCONTROL_EVENT_COUNT_BUSY_STATUS   (1<<22)
#define F1_BUSYCONTROL_LATCHED_WORD_COUNT_BUSY   (1<<21)
#define F1_BUSYCONTROL_LATCHED_EVENT_COUNT_BUSY  (1<<20)
#define F1_BUSYCONTROL_MIN_EVENT_BUILT_BUSY_MASK 0xFFFF

#define F1_DATA_TYPE_DEFINE       0x80000000
#define F1_DATA_TYPE_MASK         0x78000000
#define F1_DATA_SLOT_MASK         0x07c00000

#define F1_DATA_BLOCK_HEADER      0x00000000
#define F1_DATA_BLOCK_TRAILER     0x08000000
#define F1_DATA_EVENT_HEADER      0x10000000
#define F1_DATA_TRIGGER_TIME      0x18000000
#define F1_DATA_WINDOW_RAW        0x20000000
#define F1_DATA_WINDOW_SUM        0x28000000
#define F1_DATA_PULSE_RAW         0x30000000
#define F1_DATA_PULSE_INTEGRAL    0x38000000
#define F1_DATA_PULSE_TIME        0x40000000
#define F1_DATA_STREAM            0x48000000
#define F1_DATA_INVALID           0x70000000
#define F1_DATA_FILLER            0x78000000

#define F1_DATA_BLKNUM_MASK       0x0000003f
#define F1_DATA_WRDCNT_MASK       0x003fffff
#define F1_DATA_TRIGNUM_MASK      0x07ffffff


#define F1_DATA_TDC_MASK     0x00ffffff
#define F1_DATA_FLAG_MASK    0x07000000
/* #define F1_DATA_SLOT_MASK    0xf8000000 */
/* #define F1_DATA_INVALID      0xf0000000 */

/* #define F1_DATA_TYPE_MASK    0x00800000 */
#define F1_DATA_CHIP_MASK    0x00380000
#define F1_DATA_CHAN_MASK    0x00070000
#define F1_DATA_TIME_MASK    0x0000ffff

#define F1_HEAD_DATA         0x00000000
#define F1_TAIL_DATA         0x00000007
#define F1_DUMMY_DATA        0xf800ffff   /* Filler word (Type 15) */

#define F1_HT_DATA_MASK      0x00800007
#define F1_HT_CHAN_MASK      0x00000007
#define F1_HT_CHIP_MASK      0x00000038
#define F1_HT_XOR_MASK       0x00000040
#define F1_HT_TRIG_MASK      0x0000ff80
#define F1_HT_EVENT_MASK     0x003f0000
#define F1_HT_TRIG_OVF_MASK  0x00400000

/* f1BlockError values */
#define F1_BLOCKERROR_NO_ERROR          0
#define F1_BLOCKERROR_TERM_ON_WORDCOUNT 1
#define F1_BLOCKERROR_UNKNOWN_BUS_ERROR 2
#define F1_BLOCKERROR_ZERO_WORD_COUNT   3
#define F1_BLOCKERROR_DMADONE_ERROR     4
#define F1_BLOCKERROR_NTYPES            5

/* Define some macros */


/* Define Prototypes */
int  f1Init (unsigned int addr, unsigned int addr_inc, int ntdc, int iFlag);
int  f1CheckAddresses(int id);
int  f1Slot(unsigned int i);
int  f1ConfigWrite(int id, int *config_data, int chipMask);
int  f1GConfigWrite(int *config_data, int chipMask);
int  f1SetConfig(int id, int iflag, int chipMask);
int  f1ConfigRead(int id, unsigned int *config_data, int chipID);
int  f1ConfigReadFile(char *filename);
void f1ConfigShow(int id, int chipMask);
int  f1GetSerialNumber(int id, char **rval);
int  f1GetFirmwareVersion(int id, int pflag);
void f1Status(int id, int sflag);
void f1GStatus(int sFlag);
void f1ChipStatus(int id, int pflag);
int  f1ReadBlock(int id, volatile unsigned int *data, int nwrds, int rflag);
int  f1GetBlockError(int pflag);
int  f1PrintEvent(int id, int rflag);
int  f1FlushEvent(int id);
int  f1GPrintEvent(int rflag);
void f1Clear(int id);
void f1GClear();
void f1ClearStatus(int id, unsigned int chipMask);
void f1GClearStatus(unsigned int chipMask);
unsigned int f1ErrorStatus(int id, int sflag);
unsigned int f1GErrorStatus(int sflag);
int  f1CheckLock(int id);
int  f1GCheckLock(int pflag);
void f1SoftReset(int id);
void f1GSoftReset();
void f1Reset(int id, int iFlag);
void f1SyncReset(int id);
void f1GSyncReset();
void f1Trig(int id);
void f1GTrig();
void f1Start(int id);
void f1GStart();
int  f1Dready(int id);
unsigned int f1GBready();
int  f1DataScan(int pflag);
unsigned int f1ScanMask();
int  f1GetRez(int id);
int  f1SetWindow(int id, int window, int latency, int chipMask);
void f1GSetWindow(int window, int latency, int chipMask);
unsigned int f1ReadCSR(int id);
int  f1WriteControl(int id, unsigned int val);
void f1GWriteControl(unsigned int val);
int  f1Enable(int id);
int  f1GEnable();
int  f1Disable(int id);
int  f1GDisable();
int  f1Enabled(int id);
int  f1EnableData(int id, int chipMask);
void f1GEnableData(int chipMask);
int  f1DisableData(int id);
int  f1DisableChannel(int id, int input);
int  f1EnableChannel(int id, int input);
void f1DisableChannelMask(int id, unsigned long long int mask);
void f1EnableChannelMask(int id, unsigned long long int mask);
void f1EnableClk(int id, int cflag);
void f1DisableClk(int id);
unsigned int f1EnableLetra(int id, int chipMask);
unsigned int f1DisableLetra(int id, int chipMask);
void f1EnableSoftTrig(int id);
void f1GEnableSoftTrig();
void f1DisableSoftTrig(int id);
void f1EnableBusError(int id);
void f1GEnableBusError();
void f1DisableBusError(int id);
int  f1SetBlockLevel(int id, int level);
void f1GSetBlockLevel(int level);
int  f1SetClkSource(int id, int source);

void f1EnableMultiBlock(int tflag);
void f1DisableMultiBlock();
void f1ResetToken(int id);
int  f1SetForceChipHeaders(int id, int enable);
void f1GSetForceChipHeaders(int enable);
int  f1ResetPulser(int id);
int  f1SetPulserTriggerDelay(int id, int delay);
int  f1SetPulserDAC(int id, int output, int dac);
int  f1SoftPulser(int id, int output);
unsigned int f1GetA32(int id);
unsigned int f1GetA32M();
unsigned int f1GetTokenStatus(int pflag);

void f1TestSetSystemTestMode(int id, int mode);
void f1TestSetTrigOut(int id, int mode);
void f1TestSetBusyOut(int id, int mode);
void f1TestSetSdLink(int id, int mode);
void f1TestSetTokenOut(int id, int mode);
int  f1TestGetStatBitB(int id);
int  f1TestGetTokenIn(int id);
int  f1TestGetClockCounterStatus(int id);
unsigned int f1TestGetClockCounter(int id);
unsigned int f1TestGetSyncCounter(int id);
unsigned int f1TestGetTrig1Counter(int id);
unsigned int f1TestGetTrig2Counter(int id);
void f1TestResetClockCounter(int id);
void f1TestResetSyncCounter(int id);
void f1TestResetTrig1Counter(int id);
void f1TestResetTrig2Counter(int id);

int  f1StateArmBuffer(int id, int enable);
int  f1StateReadBuffer(int id, volatile unsigned int *data, int nwords);
int  f1StatePrintBuffer(int id);
int  f1EBControl(int id, int fake_on_timeout, int fake_on_skipped, int delete_on_timeout, int timeout_val);
unsigned int f1EBGetInsertedDataCount(int id);
int  f1EBGetDeletedDataCount(int id);
int  f1EBGetMonitor(int id);
int  f1EBResetCounters(int id, int flag);
int  f1EBStatus(int id);
int  f1BusyControl(int id, int en_fifo_word_count, int en_event_not_built_count, int min_not_built_count);
void f1GBusyControl(int en_fifo_word_count, int en_event_not_built_count, int min_not_built_count);
void f1GBusyStatus();

void f1DataDecode(int id, unsigned int data);

/* Firmware tools prototypes */
int  f1FirmwareReadFile(char *filename);
int  f1FirmwareEraseEPROM(int id);
int  f1FirmwareGEraseEPROM();
int  f1FirmwareDownloadConfigData(int id, int print_header);
int  f1FirmwareGDownloadConfigData();
int  f1FirmwareVerifyDownload(int id, int print_header);
int  f1FirmwareGVerifyDownload();

#endif
