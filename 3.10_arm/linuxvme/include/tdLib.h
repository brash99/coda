/*----------------------------------------------------------------------------*
 *  Copyright (c) 2012        Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Authors: Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     Software driver for the JLab Trigger Distribution Module
 *
 *----------------------------------------------------------------------------*/
#ifndef TDLIB_H
#define TDLIB_H

struct TD_A24RegStruct
{
  /* 0x00000 */ volatile unsigned int boardID;
  /* 0x00004 */ volatile unsigned int fiber;
  /* 0x00008 */ volatile unsigned int intsetup;
  /* 0x0000C */ volatile unsigned int trigDelay;
  /* 0x00010 */ volatile unsigned int adr32;
  /* 0x00014 */ volatile unsigned int blocklevel;
  /* 0x00018 */ volatile unsigned int dataFormat;
  /* 0x0001C */ volatile unsigned int vmeControl;
  /* 0x00020 */ volatile unsigned int trigsrc;
  /* 0x00024 */ volatile unsigned int sync;
  /* 0x00028 */ volatile unsigned int busy;
  /* 0x0002C */ volatile unsigned int fiber_busy;
  /* 0x00030 */ volatile unsigned int trig1Prescale;
  /* 0x00034 */ volatile unsigned int blockBuffer;
  /* 0x00038 */ volatile unsigned int triggerRule;
  /* 0x0003C */ volatile unsigned int triggerWindow;
  /* 0x00040 */          unsigned int blank0;
  /* 0x00044 */ volatile unsigned int tsInput;
  /* 0x00048 */          unsigned int blank1;
  /* 0x0004C */ volatile unsigned int output;
  /* 0x00050 */ volatile unsigned int fiberSyncDelay;
  /* 0x00054 */ volatile unsigned int rocAckRead[2];
  /* 0x0005C */          unsigned int blank2[(0x64-0x5C)/4];
  /* 0x00064 */ volatile unsigned int inputPrescale;
  /* 0x00068 */          unsigned int blank3[(0x74-0x68)/4];
  /* 0x00074 */ volatile unsigned int fp_prescale;
  /* 0x00078 */ volatile unsigned int syncCommand;
  /* 0x0007C */ volatile unsigned int syncDelay;
  /* 0x00080 */ volatile unsigned int syncWidth;
  /* 0x00084 */ volatile unsigned int triggerCommand;
  /* 0x00088 */ volatile unsigned int randomPulser;
  /* 0x0008C */ volatile unsigned int fixedPulser1;
  /* 0x00090 */ volatile unsigned int fixedPulser2;
  /* 0x00094 */ volatile unsigned int nblocks;
  /* 0x00098 */ volatile unsigned int trigCount;
  /* 0x0009C */ volatile unsigned int runningMode;
  /* 0x000A0 */          unsigned int blank4[(0xA8-0xA0)/4];
  /* 0x000A8 */ volatile unsigned int livetime;
  /* 0x000AC */ volatile unsigned int busytime;
  /* 0x000B0 */ volatile unsigned int GTPStatusA;
  /* 0x000B4 */ volatile unsigned int GTPStatusB;
  /* 0x000B8 */ volatile unsigned int GTPtriggerBufferLength;
  /* 0x000BC */ volatile unsigned int inputCounter;
  /* 0x000C0 */ volatile unsigned int blockStatus[4];
  /* 0x000D0 */ volatile unsigned int adr24;
  /* 0x000D4 */ volatile unsigned int syncEventCtrl;
  /* 0x000D8 */ volatile unsigned int eventNumber_hi;
  /* 0x000DC */ volatile unsigned int eventNumber_lo;
  /* 0x000E0 */          unsigned int blank5[(0xEC-0xE0)/4];
  /* 0x000EC */ volatile unsigned int rocEnable;
  /* 0x000F0 */          unsigned int blank6[(0xFC-0xF0)/4];
  /* 0x000FC */ volatile unsigned int blocklimit;
  /* 0x00100 */ volatile unsigned int reset;
  /* 0x00104 */ volatile unsigned int fpDelay[2];
  /* 0x0010C */          unsigned int blank7[(0x128-0x10C)/4];
  /* 0x00128 */ volatile unsigned int fpBusy;
  /* 0x0012C */          unsigned int blank8[(0x138-0x12C)/4];
  /* 0x00138 */ volatile unsigned int triggerRuleMin;
  /* 0x0013C */          unsigned int blank9;
  /* 0x00140 */ volatile unsigned int trigTable[(0x180-0x140)/4];
  /* 0x00180 */ volatile unsigned int busy_scaler1[7];
  /* 0x0019C */ volatile unsigned int busy_scaler2[9];
  /* 0x001C0 */          unsigned int blank10[(0x1D0-0x1C0)/4];
  /* 0x001D0 */ volatile unsigned int hfbr_tiID[8];
  /* 0x001F0 */ volatile unsigned int master_tiID;
  /* 0x001F4 */          unsigned int blank11[(0xDC00-0x1F4)/4];
  /* 0x0DC00 */ volatile unsigned int sysMon[(0xDE00-0xDC00)/4];
  /* 0x0DE00 */          unsigned int blank12[(0xFFFC-0xDE00)/4];
  /* 0x0FFFC */ volatile unsigned int eJTAGLoad;
  /* 0x10000 */ volatile unsigned int JTAGPROMBase[(0x20000-0x10000)/4];
  /* 0x20000 */ volatile unsigned int JTAGFPGABase[(0x30000-0x20000)/4];
};

#define TD_MAX_VME_SLOTS      21
#define TD_MAX_FIBER_PORTS     8
#define TD_MAX_PORTNAME_CHARS 20

/* Firmware Masks */
#define TD_FIRMWARE_ID_MASK              0xFFFF0000
#define TD_FIRMWARE_TYPE_MASK            0x0000F000
#define TD_FIRMWARE_TYPE_PROD            1
#define TD_FIRMWARE_TYPE_P               3
#define TD_FIRMWARE_MAJOR_VERSION_MASK   0x00000FF0
#define TD_FIRWMARE_MINOR_VERSION_MASK   0x0000000F

#define TD_SUPPORTED_FIRMWARE 0x81
#define TD_SUPPORTED_TYPE     TD_FIRMWARE_TYPE_P

/* 0x0 boardID bits and masks */
#define TD_BOARDID_TYPE_TIDS         0x71D5
#define TD_BOARDID_TYPE_TI           0x7100
#define TD_BOARDID_TYPE_TS           0x7500
#define TD_BOARDID_TYPE_TD           0x7D00
#define TD_BOARDID_TYPE_MASK     0xFF000000
#define TD_BOARDID_PROD_MASK     0x00FF0000
#define TD_BOARDID_GEOADR_MASK   0x00001F00
#define TD_BOARDID_CRATEID_MASK  0x000000FF

/* 0x4 fiber bits and masks */
#define TD_FIBER_1                        (1<<0)
#define TD_FIBER_2                        (1<<1)
#define TD_FIBER_3                        (1<<2)
#define TD_FIBER_4                        (1<<3)
#define TD_FIBER_5                        (1<<4)
#define TD_FIBER_6                        (1<<5)
#define TD_FIBER_7                        (1<<6)
#define TD_FIBER_8                        (1<<7)
#define TD_FIBER_ENABLED(x)           (1<<(x+1))
#define TD_FIBER_MASK                 0x000000FF
#define TD_FIBER_CONNECTED_1             (1<<16)
#define TD_FIBER_CONNECTED_2             (1<<17)
#define TD_FIBER_CONNECTED_3             (1<<18)
#define TD_FIBER_CONNECTED_4             (1<<19)
#define TD_FIBER_CONNECTED_5             (1<<20)
#define TD_FIBER_CONNECTED_6             (1<<21)
#define TD_FIBER_CONNECTED_7             (1<<22)
#define TD_FIBER_CONNECTED_8             (1<<23)
#define TD_FIBER_CONNECTED_TI(x)     (1<<(x+15))
#define TD_FIBER_CONNECTED_MASK       0x00FF0000
#define TD_FIBER_TRIGSRC_ENABLED_1       (1<<24)
#define TD_FIBER_TRIGSRC_ENABLED_2       (1<<25)
#define TD_FIBER_TRIGSRC_ENABLED_3       (1<<26)
#define TD_FIBER_TRIGSRC_ENABLED_4       (1<<27)
#define TD_FIBER_TRIGSRC_ENABLED_5       (1<<28)
#define TD_FIBER_TRIGSRC_ENABLED_6       (1<<29)
#define TD_FIBER_TRIGSRC_ENABLED_7       (1<<30)
#define TD_FIBER_TRIGSRC_ENABLED_8       (1<<31)
#define TD_FIBER_TRIGSRC_ENABLED_TI(x) (1<<(x+23))
#define TD_FIBER_TRIGSRC_ENABLED_MASK 0xFF000000

/* 0x8 intsetup bits and masks */
#define TD_INTSETUP_VECTOR_MASK   0x000000FF
#define TD_INTSETUP_LEVEL_MASK    0x00000F00
#define TD_INTSETUP_ENABLE        (1<<16)

/* 0xC trigDelay bits and masks */
#define TD_TRIGDELAY_TRIG1_DELAY_MASK 0x000000FF
#define TD_TRIGDELAY_TRIG1_WIDTH_MASK 0x0000FF00
#define TD_TRIGDELAY_TRIG2_DELAY_MASK 0x00FF0000
#define TD_TRIGDELAY_TRIG2_WIDTH_MASK 0xFF000000

/* 0x10 adr32 bits and masks */
#define TD_ADR32_MBLK_ADDR_MAX_MASK  0x000003FE
#define TD_ADR32_MBLK_ADDR_MIN_MASK  0x003FC000
#define TD_ADR32_BASE_MASK       0xFF800000

/* 0x14 blocklevel bits and masks */
#define TD_BLOCKLEVEL_MASK           0x000000FF

/* 0x18 dataFormat bits and masks */
#define TD_DATAFORMAT_TWOBLOCK_PLACEHOLDER (1<<0)
#define TD_DATAFORMAT_TIMING_WORD          (1<<1)
#define TD_DATAFORMAT_STATUS_WORD          (1<<2)

/* 0x1C vmeControl bits and masks */
#define TD_VMECONTROL_BERR           (1<<0)
#define TD_VMECONTROL_TOKEN          (1<<1)
#define TD_VMECONTROL_MBLK           (1<<2)
#define TD_VMECONTROL_A32M           (1<<3)
#define TD_VMECONTROL_A32            (1<<4)
#define TD_VMECONTROL_ERROR_INT      (1<<7)
#define TD_VMECONTROL_I2CDEV_HACK    (1<<8)
#define TD_VMECONTROL_TOKEN_HI       (1<<9)
#define TD_VMECONTROL_FIRST_BOARD    (1<<10)
#define TD_VMECONTROL_LAST_BOARD     (1<<11)
#define TD_VMECONTROL_BUFFER_DISABLE (1<<15)

/* 0x20 trigsrc bits and masks */
#define TD_TRIGSRC_SOURCEMASK       0x0000F3FF
#define TD_TRIGSRC_P0               (1<<0)
#define TD_TRIGSRC_HFBR1            (1<<1)
#define TD_TRIGSRC_LOOPBACK         (1<<2)
#define TD_TRIGSRC_FPTRG            (1<<3)
#define TD_TRIGSRC_VME              (1<<4)
#define TD_TRIGSRC_TSINPUTS         (1<<5)
#define TD_TRIGSRC_TSREV2           (1<<6)
#define TD_TRIGSRC_PULSER           (1<<7)
#define TD_TRIGSRC_ENABLE           (1<<8)
#define TD_TRIGSRC_P2BUSY           (1<<9)
#define TD_TRIGSRC_PART_1           (1<<12)
#define TD_TRIGSRC_PART_2           (1<<13)
#define TD_TRIGSRC_PART_3           (1<<14)
#define TD_TRIGSRC_PART_4           (1<<15)
#define TD_TRIGSRC_MONITOR_MASK     0xFFFF0000

/* 0x24 sync bits and masks */
#define TD_SYNC_SOURCEMASK      0x0000FFFF
#define TD_SYNC_P0               (1<<0)
#define TD_SYNC_HFBR1            (1<<1)
#define TD_SYNC_HFBR5            (1<<2)
#define TD_SYNC_FP               (1<<3)
#define TD_SYNC_LOOPBACK         (1<<4)
#define TD_SYNC_MONITOR_MASK     0xFFFF0000

/* 0x28 busy bits and masks */
#define TD_BUSY_SOURCEMASK      0x0000FFFF
#define TD_BUSY_P2               (1<<2)
#define TD_BUSY_TRIGGER_LOCK     (1<<6)
#define TD_BUSY_LOOPBACK         (1<<7)
#define TD_BUSY_HFBR1            (1<<8)
#define TD_BUSY_HFBR2            (1<<9)
#define TD_BUSY_HFBR3            (1<<10)
#define TD_BUSY_HFBR4            (1<<11)
#define TD_BUSY_HFBR5            (1<<12)
#define TD_BUSY_HFBR6            (1<<13)
#define TD_BUSY_HFBR7            (1<<14)
#define TD_BUSY_HFBR8            (1<<15)
#define TD_BUSY_HFBR_MASK        0x0000FF00
#define TD_BUSY_MONITOR_MASK     0xFFFF0000
#define TD_BUSY_MONITOR_P2       (1<<18)
#define TD_BUSY_MONITOR_TRIG_LOST (1<<22)
#define TD_BUSY_MONITOR_LOOPBACK (1<<23)
#define TD_BUSY_MONITOR_HFBR1    (1<<24)
#define TD_BUSY_MONITOR_HFBR2    (1<<25)
#define TD_BUSY_MONITOR_HFBR3    (1<<26)
#define TD_BUSY_MONITOR_HFBR4    (1<<27)
#define TD_BUSY_MONITOR_HFBR5    (1<<28)
#define TD_BUSY_MONITOR_HFBR6    (1<<29)
#define TD_BUSY_MONITOR_HFBR7    (1<<30)
#define TD_BUSY_MONITOR_HFBR8    (1<<31)
#define TD_BUSY_MONITOR_FIBER_BUSY(i)   (1<<(23+i))

/* 0x2c fiber_busy bits and masks */
#define TD_FIBER_BUSY_HFBR1    (1<<16)
#define TD_FIBER_BUSY_HFBR2    (1<<17)
#define TD_FIBER_BUSY_HFBR3    (1<<18)
#define TD_FIBER_BUSY_HFBR4    (1<<19)
#define TD_FIBER_BUSY_HFBR5    (1<<20)
#define TD_FIBER_BUSY_HFBR6    (1<<21)
#define TD_FIBER_BUSY_HFBR7    (1<<22)
#define TD_FIBER_BUSY_HFBR8    (1<<23)
#define TD_FIBER_BUSY(i)   (1<<(15+i))

/* 0x34 blockBuffer bits and masks */
#define TD_BLOCKBUFFER_BUFFERLEVEL_MASK      0x0000FFFF
#define TD_BLOCKBUFFER_BLOCKS_READY_MASK     0x0000FF00
#define TD_BLOCKBUFFER_BLOCKS_NEEDACK_MASK   0xFF000000

/* 0x4C output bits and masks */
#define TD_OUTPUT_MASK                 0x0000FFFF
#define TD_OUTPUT_BLOCKS_READY_MASK    0x00FF0000
#define TD_OUTPUT_EVENTS_IN_BLOCK_MASK 0xFF000000

/* 0x78 syncCommand bits and masks */
#define TD_SYNCCOMMAND_VME_CLOCKRESET      0x11
#define TD_SYNCCOMMAND_CLK250_RESYNC       0x22
#define TD_SYNCCOMMAND_AD9510_RESYNC       0x33
#define TD_SYNCCOMMAND_GTP_STATUSB_RESET   0x44
#define TD_SYNCCOMMAND_TRIGGERLINK_ENABLE  0x55
#define TD_SYNCCOMMAND_TRIGGERLINK_DISABLE 0x77
#define TD_SYNCCOMMAND_TRIGGER_READY_RESET 0xAA
#define TD_SYNCCOMMAND_SYNCRESET           0xDD
#define TD_SYNCCOMMAND_SYNCCODE_MASK       0x000000FF

/* 0x7C syncDelay bits and masks */
#define TD_SYNCDELAY_MASK              0x0000007F

/* 0x80 syncWidth bits and masks */
#define TD_SYNCWIDTH_MASK              0x7F
#define TD_SYNCWIDTH_LONGWIDTH_ENABLE  (1<<7)

/* 0x94 nblocks bits and masks */
#define TD_NBLOCKS_BLOCK_COUNT_MASK    0x00FFFFFF

/* 0x9C runningMode settings */
#define TD_RUNNINGMODE_ENABLE          0x71
#define TD_RUNNINGMODE_DISABLE         0x0

/* 0xA0 fiberLatencyMeasurement bits and masks */
#define TD_FIBERLATENCYMEASUREMENT_CARRYCHAIN_MASK 0x0000FFFF
#define TD_FIBERLATENCYMEASUREMENT_IODELAY_MASK    0x00FF0000
#define TD_FIBERLATENCYMEASUREMENT_DATA_MASK       0xFF000000

/* 0xA4 fiberAlignment bits and masks */
#define TD_FIBERALIGNMENT_HFBR1_IODELAY_MASK   0x000000FF
#define TD_FIBERALIGNMENT_HFBR1_SYNCDELAY_MASK 0x0000FF00
#define TD_FIBERALIGNMENT_HFBR5_IODELAY_MASK   0x00FF0000
#define TD_FIBERALIGNMENT_HFBR5_SYNCDELAY_MASK 0xFF000000

/* 0xB4 GTPStatusB bits and masks */
#define TD_GTPSTATUSB_CHANNEL_BONDING_MASK         0x000000FF
#define TD_GTPSTATUSB_DATA_ERROR_MASK              0x0000FF00
#define TD_GTPSTATUSB_DISPARITY_ERROR_MASK         0x00FF0000
#define TD_GTPSTATUSB_DATA_NOT_IN_TABLE_ERROR_MASK 0xFF000000

/* 0xC0 blockStatus bits and masks */
#define TD_BLOCKSTATUS_NBLOCKS_READY0    0x000000FF
#define TD_BLOCKSTATUS_NBLOCKS_NEEDACK0  0x0000FF00
#define TD_BLOCKSTATUS_NBLOCKS_READY1    0x00FF0000
#define TD_BLOCKSTATUS_NBLOCKS_NEEDACK1  0xFF000000

/* 0xD0 adr24 bits and masks */
#define TD_ADR24_ADDRESS_MASK         0x0000001F
#define TD_ADR24_HARDWARE_SET_MASK    0x000003E0
#define TD_ADR24_TM_NBLOCKS_READY1    0x00FF0000
#define TD_ADR24_TM_NBLOCKS_NEEDACK1  0xFF000000

/* 0xEC rocEnable bits and masks */
#define TD_ROCENABLE_MASK                           0x000000FF
#define TD_ROCENABLE_ROC(x)                         (1<<(x))
#define TD_ROCENABLE_SYNCRESET_REQUEST_ENABLE_MASK  0x0007FC00
#define TD_ROCENABLE_SYNCRESET_REQUEST_MONITOR_MASK 0x1FF00000


/* 0x100 reset bits and masks */
#define TD_RESET_I2C                  (1<<1)
#define TD_RESET_JTAG                 (1<<2)
#define TD_RESET_SFM                  (1<<3)
#define TD_RESET_SOFT                 (1<<4)
#define TD_RESET_FIBER                (1<<5)
#define TD_RESET_BUSYACK              (1<<7)
#define TD_RESET_CLK250               (1<<8)
#define TD_RESET_CLK200               (1<<8)
#define TD_RESET_CLK125               (1<<9)
#define TD_RESET_MGT                  (1<<10)
#define TD_RESET_AUTOALIGN_P0_SYNC    (1<<11)
#define TD_RESET_AUTOALIGN_HFBR1_SYNC (1<<11)
#define TD_RESET_AUTOALIGN_HFBR5_SYNC (1<<12)
#define TD_RESET_RAM_WRITE            (1<<12)
#define TD_RESET_FIBER_AUTO_ALIGN     (1<<13)
#define TD_RESET_IODELAY              (1<<14)
#define TD_RESET_MEASURE_LATENCY      (1<<15)
#define TD_RESET_TAKE_TOKEN           (1<<16)
#define TD_RESET_BLOCK_READOUT        (1<<17)
#define TD_RESET_MGT_RX_RESET         (1<<22)
#define TD_RESET_LATCH_TIMERS         (1<<24)

/* 0x1D0-0x1EC TI ID bits and masks */
#define TD_ID_TRIGSRC_ENABLE_MASK     0x000000FF
#define TD_ID_CRATEID_MASK            0x0000FF00
#define TD_ID_BLOCKLEVEL_MASK         0x00FF0000
#define TD_ID_BLOCK_BUFFERLEVEL_MASK  0xFF000000


/* Slaves, used by tdAddSlaveMask */
#define TD_SLAVE_1 (1<<1)
#define TD_SLAVE_2 (1<<2)
#define TD_SLAVE_3 (1<<3)
#define TD_SLAVE_4 (1<<4)
#define TD_SLAVE_5 (1<<5)
#define TD_SLAVE_6 (1<<6)
#define TD_SLAVE_7 (1<<7)
#define TD_SLAVE_8 (1<<8)

/* Define default Interrupt vector and level */
#define TD_INT_VEC      0xc8
#define TD_INT_LEVEL    5

/* i2c data masks - 16bit data default */
#define TD_I2C_DATA_MASK             0x0000ffff
#define TD_I2C_8BIT_DATA_MASK        0x000000ff

/* tdInit initialization flag bits */
#define TD_INIT_NO_INIT                 (1<<0)
#define TD_INIT_SKIP_FIRMWARE_CHECK     (1<<2)
#define TD_INIT_USE_ADDR_LIST           (1<<3)

#ifndef MAX_VME_SLOTS
/** This is either 20 or 21 */
#define MAX_VME_SLOTS 21
#endif

/* Function prototypes */
STATUS tdInit(UINT32 addr, UINT32 addr_inc, int nfind, int iFlag);
int    tdSlot(unsigned int i);
unsigned int tdSlotMask();
int    tdCheckAddresses();
void   tdStatus(int id, int pflag);
void   tdInitPortNames();
int    tdSetPortName(int id, int iport, char *name);
int    tdGetPortName(int id, int iport, char **name);
int    tdSavePortNames(char *filename);
int    tdLoadPortNames(char *filename);
void   tdPrintPortNames();
void   tdGStatus(int pflag);
int    tdSetBlockLevel(int id, unsigned int blockLevel);
int    tdGSetBlockLevel(unsigned int blockLevel);
int    tdSetBlockBufferLevel(int id, unsigned int level);
int    tdGSetBlockBufferLevel(unsigned int level);
int    tdGetFirmwareVersion(int id);
int    tdEnableFiber(int id, unsigned int fiber);
int    tdDisableFiber(int id, unsigned int fiber);
int    tdSetFiberMask(int id, unsigned int fibermask);
int    tdSetBusySource(int id, unsigned int sourcemask, int rFlag);
int    tdSetTriggerLock(int id, int enable);
int    tdGSetTriggerLock(int enable);
int    tdGetTriggerLock(int id);
int    tdSetSyncSource(int id, unsigned int sync);
int    tdSetTriggerSource(int id, int trigmask);
int    tdResetSlaveConfig(int id);
int    tdAddSlave(int id, unsigned int fiber);
int    tdRemoveSlave(int id, unsigned int fiber);
int    tdAddSlaveMask(int id, unsigned int fibermask);
int    tdAutoAlignSync(int id);
unsigned int tdGetSerialNumber(int id, char **rSN);
int    tdLatchTimers(int id);
unsigned int tdGetLiveTime(int id);
unsigned int tdGetBusyTime(int id);
int    tdEnableSyncResetRequest(int id, unsigned int portMask);
int    tdSyncResetRequestStatus(int id, int pflag);
int    tdGetConnectedFiberMask(int id);
int    tdGetTrigSrcEnabledFiberMask(int id);
int    tdTriggerReadyReset(int id);
int    tdGetCrateID(int id, int port);
int    tdSetPortNamesFromCrateID(int id);
void   tdGSetPortNamesFromCrateID();
int    tdGetPortTrigSrcEnabled(int id, int port);
int    tdGetSlaveBlocklevel(int id, int port);
int    tdResetMGT(int id);
int    tdResetMGTRx(int id);
int    tdResetFiber(int id);

void   tdSlaveStatus(int id, int pflag);
unsigned int tdGetBlockStatus(int id, int port, int pflag);
int    tdGetBusyStatus(int id, int port, int pflag);
int    tdSetOutputPort(int id, unsigned int set1, unsigned int set2,
		       unsigned int set3, unsigned int set4);
unsigned int tdGetTrigCount(int id);
unsigned int tdGetBusyCounter(int id, int busysrc);
int    tdPrintBusyCounters(int id);
int    tdGFiberBusyStatus(int nsec);
int    tdReadFiberFifo(int id, int fiber, volatile unsigned int *data,
		       int maxwords, int rflag);
int    tdPrintFiberFifo(int id, int fiber);
int    tdClearFiberFifo(int id, int fiber);
unsigned int tdGetTriggerLinkStatus(int id, int pflag);
int    tdGPrintTriggerLinkStatus();

int    tdGetTranceiverStatus(int id, unsigned int *data, int maxwords);
int    tdPrintTranceiverStatus(int id);


#endif /* TDLIB_H */
