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
 *     Primitive trigger control for VME CPUs using the TJNAF Trigger
 *     Supervisor (TS) card
 *
 *----------------------------------------------------------------------------*/
#ifndef TSLIB_H
#define TSLIB_H

#ifndef VXWORKS
#include <pthread.h>

pthread_mutex_t tsISR_mutex=PTHREAD_MUTEX_INITIALIZER;
#else
/* #include <intLib.h> */
extern int intLock();
extern int intUnlock();
#endif

#ifdef VXWORKS
int intLockKeya;
#define INTLOCK {				\
    intLockKeya = intLock();			\
}

#define INTUNLOCK {				\
    intUnlock(intLockKeya);			\
}
#else
#define INTLOCK {				\
    vmeBusLock();				\
}
#define INTUNLOCK {				\
    vmeBusUnlock();				\
}
#endif

struct ScalerStruct
{
  /* 0x00n80 */ volatile unsigned int fp[4];
  /* 0x00n90 */ volatile unsigned int GTP[8];
  /* 0x00nB0 */ volatile unsigned int gen[8];
  /* 0x00nD0 */ volatile unsigned int blank[(0x334-0x1D0)/4];
};

struct PartitionStruct
{
  /* 0x00n34 */ volatile unsigned int blockBuffer;
  /* 0x00n38 */ volatile unsigned int triggerRuleMin; // Only on part1 struct
  /* 0x00n3C */          unsigned int blank0;
  /* 0x00n40 */ volatile unsigned int fpConfig;
  /* 0x00n44 */ volatile unsigned int gtpConfig;
  /* 0x00n48 */ volatile unsigned int genConfig;
  /* 0x00n4C */ volatile unsigned int triggerTable;
  /* 0x00n50 */ volatile unsigned int data;
  /* 0x00n54 */ volatile unsigned int blockBufferInfo;
  /* 0x00n58 */ volatile unsigned int busyConfig;
  /* 0x00n5C */ volatile unsigned int busytime;
  /* 0x00n60 */ volatile unsigned int hel_livetime;
  /* 0x00n64 */ volatile unsigned int hel_busytime;
  /* 0x00n68 */          unsigned int blank[(0x180-0x168)/4];
};

struct PartTrigTableStruct
{
  /* 0x010n0 */ volatile unsigned int GTPTriggerTable;
  /* 0x010n4 */ volatile unsigned int FPTriggerTable;
};

struct TS_A24RegStruct
{
  /* 0x00000 */ volatile unsigned int boardID;
  /* 0x00004 */ volatile unsigned int fiber;
  /* 0x00008 */ volatile unsigned int intsetup;
  /* 0x0000C */ volatile unsigned int trigDelay;
  /* 0x00010 */ volatile unsigned int adr32;
  /* 0x00014 */ volatile unsigned int blocklevel;
  /* 0x00018 */ volatile unsigned int dataFormat;
  /* 0x0001C */ volatile unsigned int vmeControl;
  /* 0x00020 */ volatile unsigned int trigger;
  /* 0x00024 */ volatile unsigned int sync;
  /* 0x00028 */ volatile unsigned int busy;
  /* 0x0002C */ volatile unsigned int clock;
  /* 0x00030 */ volatile unsigned int trig1Prescale;
  /* 0x00034 */ volatile unsigned int blockBuffer;
  /* 0x00038 */ volatile unsigned int triggerRule;
  /* 0x0003C */ volatile unsigned int triggerWindow;
  /* 0x00040 */ volatile unsigned int GTPtrigger;
  /* 0x00044 */ volatile unsigned int fpInput;
  /* 0x00048 */ volatile unsigned int genInput;
  /* 0x0004C */ volatile unsigned int output;
  /* 0x00050 */ volatile unsigned int fiberSyncDelay;
  /* 0x00054 */ volatile unsigned int GTPprescale[4];
  /* 0x00064 */ volatile unsigned int fpInputPrescale[4];
  /*            Duplication Mode: fpInputPrescale[2] ->  Local Trigger Table                */
  /*            Duplication Mode: fpInputPrescale[3] ->  Local Trigger, Fast Clear Setting  */
  /* 0x00074 */ volatile unsigned int specialEvTypes;
  /* 0x00078 */ volatile unsigned int syncCommand;
  /* 0x0007C */ volatile unsigned int syncDelay;
  /* 0x00080 */ volatile unsigned int syncWidth;
  /* 0x00084 */ volatile unsigned int triggerCommand;
  /* 0x00088 */ volatile unsigned int randomPulser;
  /* 0x0008C */ volatile unsigned int fixedPulser1;
  /* 0x00090 */ volatile unsigned int fixedPulser2;
  /* 0x00094 */ volatile unsigned int nblocks;
  /* 0x00098 */          unsigned int blank0;
  /* 0x0009C */ volatile unsigned int runningMode;
  /* 0x000A0 */ volatile unsigned int duplBusyTime;
  /* 0x000A4 */ volatile unsigned int duplBusyStatus;
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
  /* 0x000E0 */ volatile unsigned int hfbr_tiID[2];
  /* 0x000E8 */ volatile unsigned int master_tiID;
  /* 0x000EC */          unsigned int blank1;
  /* 0x000F0 */ volatile unsigned int fpScaler;
  /* 0x000F4 */ volatile unsigned int gtpScaler;
  /* 0x000F8 */ volatile unsigned int genScaler;
  /* 0x000FC */ volatile unsigned int blocklimit;
  /* 0x00100 */ volatile unsigned int reset;
  /* 0x00104 */ volatile unsigned int fpDelay[11];
  /*            Duplication Mode: fpDelay[5] ->  Fast Clear Delay  */
  /*            Duplication Mode: fpDelay[6] ->  Busy extension settings  */
  /* 0x00130 */          unsigned int blank2;
  /* 0x00134 */ struct   PartitionStruct part1;
  /* 0x00180 */ struct   ScalerStruct Scalers1;
  /* 0x00340 */ struct   PartitionStruct part2;
  /* 0x00380 */ struct   ScalerStruct Scalers2;
  /* 0x00540 */ struct   PartitionStruct part3;
  /* 0x00580 */ struct   ScalerStruct Scalers3;
  /* 0x00740 */ struct   PartitionStruct part4;
  /* 0x00780 */ struct   ScalerStruct Scalers4;
  /* 0x00934 */          unsigned int blank3[(0x1080-0x934)/4];
  /* 0x01080 */ volatile unsigned int GTPTriggerTable[(0x1090-0x1080)/4];
  /* 0x01090 */ volatile unsigned int FPTriggerTable[(0x10A0-0x1090)/4];
  /* 0x010A0 */ struct   PartTrigTableStruct PartTrigTable[4];
  /* 0x010C0 */          unsigned int blank4[(0xFFFC-0x10C0)/4];
  /* 0x0FFFC */ volatile unsigned int eJTAGLoad;
  /* 0x10000 */ volatile unsigned int JTAGPROMBase[(0x20000-0x10000)/4];
  /* 0x20000 */ volatile unsigned int JTAGFPGABase[(0x30000-0x20000)/4];
  /* 0x30000 */ volatile unsigned int SWA[(0x40000-0x30000)/4];
  /* 0x40000 */ volatile unsigned int SWB[(0x50000-0x40000)/4];

};

/* Define TS Modes of operation:     Ext trigger - Interrupt mode   0
                                     Ext trigger - polling  mode    2 */
#define TS_READOUT_EXT_INT    0
#define TS_READOUT_EXT_POLL   2

/* Firmware Masks */
#define TS_FIRMWARE_ID_MASK              0xFFFF0000
#define TS_FIRMWARE_TYPE_MASK            0x0000F000
#define TS_FIRMWARE_TYPE_PROD            1
#define TS_FIRMWARE_TYPE_P               3
#define TS_FIRMWARE_MAJOR_VERSION_MASK   0x00000FF0
#define TS_FIRWMARE_MINOR_VERSION_MASK   0x0000000F

#define TS_SUPPORTED_FIRMWARE 0x92
#define TS_SUPPORTED_TYPE     TS_FIRMWARE_TYPE_P



/* Macros for tsSetTriggerSource arguments */
#define TS_TRIGSRC_PULSER   5
#define TS_TRIGSRC_EXT      6

/* 0x0 boardID bits and masks */
#define TS_BOARDID_TYPE_TIDS         0x71D5
#define TS_BOARDID_TYPE_TI           0x7100
#define TS_BOARDID_TYPE_TS           0x7500
#define TS_BOARDID_TYPE_TD           0x7D00
#define TS_BOARDID_TYPE_MASK     0xFF000000
#define TS_BOARDID_PROD_MASK     0x00FF0000
#define TS_BOARDID_GEOADR_MASK   0x00001F00
#define TS_BOARDID_CRATEID_MASK  0x000000FF

/* 0x4 fiber bits and masks */
#define TS_FIBER_LOW_POWER     (1<<1)
#define TS_FIBER_I2C_ACTIVE    (1<<2)
#define TS_FIBER_RESET         (1<<3)
#define TS_FIBER_ENABLE_P0     (1<<8)


/* 0x8 intsetup bits and masks */
#define TS_INTSETUP_VECTOR_MASK   0x000000FF
#define TS_INTSETUP_LEVEL_MASK    0x00000F00
#define TS_INTSETUP_ENABLE        (1<<16)

/* 0xC trigDelay bits and masks */
#define TS_TRIGDELAY_TRIG1_DELAY_MASK 0x000000FF
#define TS_TRIGDELAY_TRIG1_WIDTH_MASK 0x0000FF00
#define TS_TRIGDELAY_TRIG2_DELAY_MASK 0x00FF0000
#define TS_TRIGDELAY_TRIG2_WIDTH_MASK 0xFF000000

/* 0x10 adr32 bits and masks */
#define TS_ADR32_MBLK_ADDR_MAX_MASK  0x000003FE
#define TS_ADR32_MBLK_ADDR_MIN_MASK  0x003FC000
#define TS_ADR32_BASE_MASK       0xFF800000

/* 0x14 blocklevel bits and masks */
#define TS_BLOCKLEVEL_MASK           0x000000FF
#define TS_BLOCKLEVEL_CURRENT_MASK   0x00FF0000
#define TS_BLOCKLEVEL_RECEIVED_MASK  0xFF000000

/* 0x18 dataFormat bits and masks */
#define TS_DATAFORMAT_TWOBLOCK_PLACEHOLDER (1<<0)
#define TS_DATAFORMAT_TIMING_WORD          (1<<1)
#define TS_DATAFORMAT_HIGHERBITS_WORD      (1<<2)
#define TS_DATAFORMAT_WORDS_MASK           0x00000007
#define TS_DATAFORMAT_GTPINPUT_READOUT     (1<<3)
#define TS_DATAFORMAT_FPINPUT_READOUT      (1<<4)
#define TS_DATAFORMAT_BEFORE_PRESCALE_READOUT (1<<5)

/* 0x1C vmeControl bits and masks */
#define TS_VMECONTROL_BERR              (1<<0)
#define TS_VMECONTROL_TOKEN             (1<<1)
#define TS_VMECONTROL_MBLK              (1<<2)
#define TS_VMECONTROL_A32M              (1<<3)
#define TS_VMECONTROL_A32               (1<<4)
#define TS_VMECONTROL_INT               (1<<7)
#define TS_VMECONTROL_I2CDEV_HACK       (1<<8)
#define TS_VMECONTROL_TOKEN_HI          (1<<9)
#define TS_VMECONTROL_FIRST_BOARD       (1<<10)
#define TS_VMECONTROL_LAST_BOARD        (1<<11)
#define TS_VMECONTROL_BUFFER_DISABLE    (1<<12)
#define TS_VMECONTROL_DRIVE_TSIO_EN     (1<<20)
#define TS_VMECONTROL_BLOCKLEVEL_UPDATE (1<<21)
#define TS_VMECONTROL_SLOWER_TRIGGER_RULES (1<<31)

/* 0x20 trigger bits and masks */
#define TS_TRIGGER_SOURCEMASK       0x000003FF
#define TS_TRIGGER_P0               (1<<0)
#define TS_TRIGGER_HFBR1            (1<<1)
#define TS_TRIGGER_LOOPBACK         (1<<2)
#define TS_TRIGGER_FPTRG            (1<<3)
#define TS_TRIGGER_VME              (1<<4)
#define TS_TRIGGER_TSINPUTS         (1<<5)
#define TS_TRIGGER_TSREV2           (1<<6)
#define TS_TRIGGER_PULSER           (1<<7)
#define TS_TRIGGER_ENABLE           (1<<8)
#define TS_TRIGGER_P2BUSY           (1<<9)
#define TS_TRIGGER_PART(pID)        (1<<(pID+11))
#define TS_TRIGGER_MONITOR_MASK     0xFFFF0000

/* 0x24 sync bits and masks */
#define TS_SYNC_SOURCEMASK      0x0000FFFF
#define TS_SYNC_P0               (1<<0)
#define TS_SYNC_HFBR1            (1<<1)
#define TS_SYNC_HFBR5            (1<<2)
#define TS_SYNC_FP               (1<<3)
#define TS_SYNC_LOOPBACK         (1<<4)
#define TS_SYNC_USER_SYNCRESET_ENABLED  (1<<7)
#define TS_SYNC_MONITOR_MASK     0xFFFF0000

/* 0x28 busy bits and masks */
/* Enable/Disable bits */
#define TS_BUSY_SOURCEMASK      0x0000FFFF
#define TS_BUSY_SWA              (1<<0)
#define TS_BUSY_SWB              (1<<1)
#define TS_BUSY_P2               (1<<2)
#define TS_BUSY_FP_FTDC          (1<<3)
#define TS_BUSY_FP_FADC          (1<<4)
#define TS_BUSY_FP               (1<<5)
#define TS_BUSY_P2_TRIGGER_INPUT (1<<6)
#define TS_BUSY_LOOPBACK         (1<<7)
#define TS_BUSY_TI_A             (1<<8)
#define TS_BUSY_TI_B             (1<<9)
#define TS_BUSY_HFBR_MASK        ((1<<9) | (1<<8))
#define TS_BUSY_INT              (1<<10)
#define TS_BUSY_ALL_BRANCHES     (1<<11)
#define TS_BUSY_BRANCH1          (1<<12)
#define TS_BUSY_BRANCH2          (1<<13)
#define TS_BUSY_BRANCH3          (1<<14)
#define TS_BUSY_BRANCH4          (1<<15)
/* More specific Monitor bits */
#define TS_BUSY_MONITOR_SWA      (1<<16)
#define TS_BUSY_MONITOR_SWB      (1<<17)
#define TS_BUSY_MONITOR_P2       (1<<18)
#define TS_BUSY_MONITOR_FP_FTDC  (1<<19)
#define TS_BUSY_MONITOR_FP_FADC  (1<<20)
#define TS_BUSY_MONITOR_FP       (1<<21)
#define TS_BUSY_MONITOR_LOOPBACK (1<<23)
/* MONITOR_TS includes busy from data buffer level */
#define TS_BUSY_MONITOR_TS       (1<<27)
/* MONITOR TI_? is the actual busy level from the fiber, not resulting from buffer level */
#define TS_BUSY_MONITOR_TI_A     (1<<28)
#define TS_BUSY_MONITOR_TI_B     (1<<29)
#define TS_BUSY_MONITOR_FIBER_BUSY(x) (1<<(x+27))
#define TS_BUSY_MONITOR_DUPL     (1<<31)
#define TS_BUSY_MONITOR_MASK     0xFFFF0000

/* 0x2C clock bits and mask  */
#define TS_CLOCK_INTERNAL    (0)
#define TS_CLOCK_EXTERNAL    (1)
#define TS_CLOCK_INPUT_MIX_CONTROL_MASK 0x00000030
#define TS_CLOCK_INPUT_MIX_ENABLE       (1<<4)

/* 0x30 trig1Prescale bits and masks */
#define TS_TRIG1PRESCALE_MASK          0x0000FFFF

/* 0x34 blockBuffer bits and masks */
#define TS_BLOCKBUFFER_BUFFERLEVEL_MASK      0x000000FF
#define TS_BLOCKBUFFER_BLOCKS_READY_MASK     0x0000FF00
#define TS_BLOCKBUFFER_TRIGGERS_IN_BLOCK     0x00FF0000
#define TS_BLOCKBUFFER_BLOCKS_NEEDACK_MASK   0x7F000000
#define TS_BLOCKBUFFER_BREADY_INT_MASK       0x0F000000
#define TS_BLOCKBUFFER_BUSY_ON_BLOCKLIMIT    (1<<28)
#define TS_BLOCKBUFFER_SYNCRESET_REQUESTED   (1<<30)
#define TS_BLOCKBUFFER_SYNCEVENT             (1<<31)

/* 0x38 triggerRule bits and masks */
#define TS_TRIGGERRULE_RULE1_MASK 0x000000FF
#define TS_TRIGGERRULE_RULE2_MASK 0x0000FF00
#define TS_TRIGGERRULE_RULE3_MASK 0x00FF0000
#define TS_TRIGGERRULE_RULE4_MASK 0xFF000000

/* 0x3C triggerWindow bits and masks */
#define TS_TRIGGERWINDOW_COINC_MASK        0x000000FF
#define TS_TRIGGERWINDOW_INHIBIT_MASK      0x0000FF00
#define TS_TRIGGERWINDOW_TRIG21_DELAY_MASK 0x00FF0000

/* 0x4C output bits and masks */
#define TS_OUTPUT_FP_MASK             0x0000003F
#define TS_OUTPUT_FP_1                (1<<0)
#define TS_OUTPUT_FP_2                (1<<1)
#define TS_OUTPUT_FP_3                (1<<2)
#define TS_OUTPUT_FP_4                (1<<3)
#define TS_OUTPUT_FP_5                (1<<4)
#define TS_OUTPUT_FP_6                (1<<5)
#define TS_OUTPUT_BLOCKS_FOR_INT_MASK 0xFFFF0000

/* 0x50 fiberSyncDelay bits and masks */
#define TS_FIBERSYNCDELAY_HFBR1_SYNCDELAY_MASK    0x0000FF00
#define TS_FIBERSYNCDELAY_LOOPBACK_SYNCDELAY_MASK 0x00FF0000
#define TS_FIBERSYNCDELAY_HFBR5_SYNCDELAY_MASK    0xFF000000

/* 0x70 fpInputPrescale[3] Masks (Duplication Mode) */
#define TS_DUPL_LOCAL_TRIG_RULE_MASK       0x0000007F
#define TS_DUPL_LOCAL_TRIG_WIDTH_MASK      0x0000FF00
#define TS_DUPL_FAST_CLEAR_WIDTH_MASK      0x00FF0000
#define TS_DUPL_FAST_CLEAR_VETO_WIDTH_MASK 0xFF000000

/* 0x74 specialEvTypes masks */
#define TS_SPECIALEVTYPES_MULT_GTP_OR_FP_MASK  0x000000FF
#define TS_SPECIALEVTYPES_GTP_AND_FP_MASK      0x0000FF00
#define TS_SPECIALEVTYPES_VME_MASK             0x00FF0000
#define TS_SPECIALEVTYPES_PULSER_MASK          0xFF000000

/* 0x78 syncCommand bits and masks */
#define TS_SYNCCOMMAND_VME_CLOCKRESET      0x11
#define TS_SYNCCOMMAND_CLK250_RESYNC       0x22
#define TS_SYNCCOMMAND_AD9510_RESYNC       0x33
#define TS_SYNCCOMMAND_GTP_STATUSB_RESET   0x44
#define TS_SYNCCOMMAND_TRIGGERLINK_ENABLE  0x55
#define TS_SYNCCOMMAND_TRIGGERLINK_DISABLE 0x77
#define TS_SYNCCOMMAND_SYNCRESET_HIGH      0x99
#define TS_SYNCCOMMAND_TRIGGER_READY_RESET 0xAA
#define TS_SYNCCOMMAND_RESET_EVNUM         0xBB
#define TS_SYNCCOMMAND_SYNCRESET_LOW       0xCC
#define TS_SYNCCOMMAND_SYNCRESET           0xDD
#define TS_SYNCCOMMAND_SYNCRESET_4US       0xEE
#define TS_SYNCCOMMAND_SYNCCODE_MASK       0x000000FF

/* 0x7C syncDelay bits and masks */
#define TS_SYNCDELAY_MASK              0x0000007F

/* 0x80 syncWidth bits and masks */
#define TS_SYNCWIDTH_MASK              0x7F
#define TS_SYNCWIDTH_LONGWIDTH_ENABLE  (1<<7)

/* 0x84 triggerCommand bits and masks */
#define TS_TRIGGERCOMMAND_VALUE_MASK      0x000000FF
#define TS_TRIGGERCOMMAND_CODE_MASK       0x00000F00
#define TS_TRIGGERCOMMAND_TRIG1           0x00000100
#define TS_TRIGGERCOMMAND_TRIG2           0x00000200
#define TS_TRIGGERCOMMAND_SYNC_EVENT      0x00000300
#define TS_TRIGGERCOMMAND_SET_BLOCKLEVEL  0x00000800
#define TS_TRIGGERCOMMAND_SET_BUFFERLEVEL 0x00000C00


/* 0x88 randomPulser bits and masks */
#define TS_RANDOMPULSER_TRIG1_RATE_MASK 0x0000000F
#define TS_RANDOMPULSER_TRIG1_ENABLE    (1<<7)
#define TS_RANDOMPULSER_TRIG2_RATE_MASK 0x00000F00
#define TS_RANDOMPULSER_TRIG2_ENABLE    (1<<15)

/* 0x8C fixedPulser1 bits and masks */
#define TS_FIXEDPULSER1_NTRIGGERS_MASK 0x0000FFFF
#define TS_FIXEDPULSER1_PERIOD_MASK    0x7FFF0000
#define TS_FIXEDPULSER1_PERIOD_RANGE   (1<<31)

/* 0x90 fixedPulser2 bits and masks */
#define TS_FIXEDPULSER2_NTRIGGERS_MASK 0x0000FFFF
#define TS_FIXEDPULSER2_PERIOD_MASK    0x7FFF0000
#define TS_FIXEDPULSER2_PERIOD_RANGE   (1<<31)

/* 0x94 nblocks bits and masks */
#define TS_NBLOCKS_COUNT_MASK           0x00FFFFFF
#define TS_NBLOCKS_EVENTS_IN_BLOCK_MASK 0xFF000000

/* 0xA0 duplBusyTime bits and masks */

/* 0xA4 duplBusyStatus bits and masks */

/* 0xB0 GTPStatusA bits and masks */
#define TS_GTPSTATUSA_RESET_DONE_MASK 0x000000FF
#define TS_GTPSTATUSA_PLL_LOCK_MASK   0x00000F00

/* 0xB4 GTPStatusB bits and masks */
#define TS_GTPSTATUSB_CHANNEL_BONDING_MASK         0x000000FF
#define TS_GTPSTATUSB_DATA_ERROR_MASK              0x0000FF00
#define TS_GTPSTATUSB_DISPARITY_ERROR_MASK         0x00FF0000
#define TS_GTPSTATUSB_DATA_NOT_IN_TABLE_ERROR_MASK 0xFF000000

/* 0xB8 GTPtriggerBufferLength bits and masks */
#define TS_GTPTRIGGERBUFFERLENGTH_GLOBAL_LENGTH_MASK  0x000007FF
#define TS_GTPTRIGGERBUFFERLENGTH_SUBSYS_LENGTH_MASK  0x07FF0000
#define TS_GTPTRIGGERBUFFERLENGTH_IO_DELAY_CTRL_READY (1<<26)
#define TS_GTPTRIGGERBUFFERLENGTH_HFBR1_MGT_ERROR     (1<<28)
#define TS_GTPTRIGGERBUFFERLENGTH_CLK250_DCM_LOCK     (1<<29)
#define TS_GTPTRIGGERBUFFERLENGTH_CLK125_DCM_LOCK     (1<<30)
#define TS_GTPTRIGGERBUFFERLENGTH_VMECLK_DCM_LOCK     (1<<31)

/* 0xC0 blockStatus bits and masks */
#define TS_BLOCKSTATUS_NBLOCKS_READY0    0x000000FF
#define TS_BLOCKSTATUS_NBLOCKS_NEEDACK0  0x0000FF00
#define TS_BLOCKSTATUS_NBLOCKS_READY1    0x00FF0000
#define TS_BLOCKSTATUS_NBLOCKS_NEEDACK1  0xFF000000

/* 0xD0 adr24 bits and masks */
#define TS_ADR24_ADDRESS_MASK         0x0000001F
#define TS_ADR24_HARDWARE_SET_MASK    0x000003E0
#define TS_ADR24_TM_NBLOCKS_READY1    0x00FF0000
#define TS_ADR24_TM_NBLOCKS_NEEDACK1  0xFF000000

/* 0xD4 syncEventCtrl bits and masks */
#define TS_SYNCEVENTCTRL_NBLOCKS_MASK 0x00FFFFFF

/* 0xD8 eventNumber_hi bits and masks */
#define TS_EVENTNUMBER_FIBER_A_ENABLE              (1<<0)
#define TS_EVENTNUMBER_FIBER_B_ENABLE              (1<<1)
#define TS_EVENTNUMBER_FIBER_A_TRANSCEIVER_MISSING (1<<4)
#define TS_EVENTNUMBER_FIBER_A_INT_NOT_REQUESTED   (1<<5)
#define TS_EVENTNUMBER_FIBER_B_TRANSCEIVER_MISSING (1<<6)
#define TS_EVENTNUMBER_FIBER_B_INT_NOT_REQUESTED   (1<<7)
#define TS_EVENTNUMBER_HI_MASK                 0xFFFF0000

/* 0xE0-0xE8 TI ID bits and masks */
#define TS_ID_TRIGSRC_ENABLE_MASK     0x000000FF
#define TS_ID_CRATEID_MASK            0x0000FF00

/* 0x100 reset bits and masks */
#define TS_RESET_I2C                  (1<<1)
#define TS_RESET_JTAG                 (1<<2)
#define TS_RESET_SFM                  (1<<3)
#define TS_RESET_SOFT                 (1<<4)
#define TS_RESET_BUSYACK              (1<<7)
#define TS_RESET_CLK250               (1<<8)
#define TS_RESET_CLK200               (1<<8)
#define TS_RESET_CLK125               (1<<9)
#define TS_RESET_MGT                  (1<<10)
#define TS_RESET_AUTOALIGN_HFBR1_SYNC (1<<11)
#define TS_RESET_AUTOALIGN_HFBR5_SYNC (1<<12)
#define TS_RESET_RAM_WRITE            (1<<12)
#define TS_RESET_IODELAY              (1<<14)
#define TS_RESET_MEASURE_LATENCY      (1<<15)
#define TS_RESET_TAKE_TOKEN           (1<<16)
#define TS_RESET_FORCE_SYNCEVENT      (1<<20)
#define TS_RESET_SYNCRESET_REQUEST    (1<<23)
#define TS_RESET_LATCH_TIMERS         (1<<24)
#define TS_RESET_RESET_L1A_NUMBER     (1<<25)
#define TS_RESET_PART_ACK(pID)        (1<<(pID+25))
#define TS_RESET_FILL_TO_END_BLOCK    (1<<31)

/* 0x104 fpDelay Masks */
#define TS_FPDELAY_MASK(x) (0x1FF<<(10*(x%3)))

/* 0x118 fpDelay[5] -> Duplication Mode fast clear delay */
#define TS_DUPL_FAST_CLEAR_DELAY_MASK  0x0007fc00

/* 0x11C fpDelay[6] -> Duplication Mode busy extension setting */
#define TS_DUPL_LOCAL_TRIG_BUSY_MASK  0x000003FF
#define TS_DUPL_FAST_CLEAR_BUSY_MASK  0x3FF00000

/* 0x138 triggerRuleMin In partition struct part1 */
#define TS_TRIGGERRULEMIN_MIN2_MASK  0x00007F00
#define TS_TRIGGERRULEMIN_MIN2_EN    (1<<15)
#define TS_TRIGGERRULEMIN_MIN3_MASK  0x007F0000
#define TS_TRIGGERRULEMIN_MIN3_EN    (1<<23)
#define TS_TRIGGERRULEMIN_MIN4_MASK  0x7F000000
#define TS_TRIGGERRULEMIN_MIN4_EN    (1<<31)

/* Define default Interrupt vector and level */
#define TS_INT_VEC      0xc8
#define TS_INT_LEVEL    5

/* i2c data masks - 16bit data default */
#define TS_I2C_DATA_MASK             0x0000ffff
#define TS_I2C_8BIT_DATA_MASK        0x000000ff

/* Scaler registers */
#define TS_SCALER_SCALE_HI_MSB_MASK 0x0000007f
#define TS_SCALER_SCALE_HI_LSB_MASK 0x7fffff80
#define TS_SCALER_SCALE_HI          (1<<31)

/* Partition registers */
#define TS_PART_BLOCKBUFFER_EVENTS_LEFT_IN_BLOCK_MASK 0xFF000000
#define TS_PART_BLOCKBUFFER_BLOCKS_READY_MASK         0x00FFFF00
#define TS_PART_BLOCKBUFFER_BLOCKLEVEL_MASK           0x000000FF

#define TS_PART_BLOCKBUFFERINFO_BLOCKS_READY_MASK     0x00FFFF00

#define TS_PART_BUSYCONFIG_BUFFERLEVEL_MASK   0xFF000000
#define TS_PART_BUSYCONFIG_BUSYSRC_MASK       0x00F00000
#define TS_PART_BUSYCONFIG_TI_B               (1<<23)
#define TS_PART_BUSYCONFIG_TI_A               (1<<22)
#define TS_PART_BUSYCONFIG_FP                 (1<<21)
#define TS_PART_BUSYCONFIG_SWB                (1<<20)
#define TS_PART_BUSYCONFIG_BUFFERLEVEL_ENABLE (1<<17)
#define TS_PART_BUSYCONFIG_ALMOSTFULL_ENABLE  (1<<16)
#define TS_PART_BUSYCONFIG_TD_INPUT_MASK      0x0000FFFF

#define TS_PART_BUSYTIME_MASK  0x0000FFFF


/* Data buffer bits and masks */
#define TS_EMPTY_FIFO                      0xF545BAD5
#define TS_BLOCK_HEADER_CRATEID_MASK       0xFF000000
#define TS_BLOCK_HEADER_SLOTS_MASK         0x001F0000
#define TS_BLOCK_TRAILER_CRATEID_MASK      0x00FF0000
#define TS_BLOCK_TRAILER_SLOTS_MASK        0x1F000000
#define TS_DATA_BLKNUM_MASK                0x0000FF00
#define TS_DATA_BLKLEVEL_MASK              0x000000FF

#define TS_DATAFORMAT_DATA_TYPE_WORD          (1<<31)
#define TS_DATAFORMAT_TYPE_MASK               0x78000000
#define TS_DATAFORMAT_FILLER_WORD_TYPE        0x78000000
#define TS_DATAFORMAT_TYPE_BLOCK_HEADER       0
#define TS_DATAFORMAT_BLOCKHEADER_SLOT_MASK   0x07C00000
#define TS_DATAFORMAT_BLOCKHEADER_MODID_MASK  0x003C0000
#define TS_DATAFORMAT_BLOCKHEADER_BLKNUM_MASK 0x0003FF00
#define TS_DATAFORMAT_BLOCKHEADER_NEVTS_MASK  0x000000FF
#define TS_DATAFORMAT_TYPE_BLOCK_TRAILER      1
#define TS_DATAFORMAT_BLOCKTRAILER_SLOT_MASK   0x07C00000
#define TS_DATAFORMAT_BLOCKTRAILER_NWORDS_MASK 0x003FFFFF

/* tsInit initialization flag bits */
#define TS_INIT_NO_INIT                 (1<<0)
#define TS_INIT_SKIP_FIRMWARE_CHECK     (1<<2)
#define TS_INIT_DUPLICATION_MODE        (1<<3)


/* Some pre-initialization routine prototypes */
int  tsSetCrateID_preInit(int cid);

/* Function prototypes */
int  tsInit(unsigned int tAddr, unsigned int mode, int force);
unsigned int tsFind();
int  tsCheckAddresses();
void tsStatus(int pflag);
void tsSlaveStatus(int pflag);
int  tsGetPortTrigSrcEnabled(int port);
int  tsGetTrigSrcEnabledFiberMask();
int  tsGetFirmwareVersion();
int  tsReset();
int  tsReload();
unsigned int tsGetSerialNumber(char **rSN);
int  tsClockResync();
int  tsSetCrateID(unsigned int crateID);
int  tsGetCrateID(int port);
int  tsSetBlockLevel(int blockLevel);
int  tsBroadcastNextBlockLevel(int blockLevel);
int  tsGetNextBlockLevel();
int  tsGetCurrentBlockLevel();
int  tsSetInstantBlockLevelChange(int enable);
int  tsGetInstantBlockLevelChange();
int  tsSetInputMix(int enable);
int  tsGetInputMix();
int  tsSetGTPInput(unsigned int inputmask);
int  tsSetFPInput(unsigned int inputmask);
int  tsSetTriggerSource(int trig);
int  tsSetTriggerSourceMask(int trigmask);
int  tsDisableTriggerSource(int fflag);
int  tsSetSyncSource(unsigned int sync);
int  tsSetEventFormat(int format);
int  tsSoftTrig(int trigger, unsigned int nevents, unsigned int period_inc, int range);
int  tsSetRandomTrigger(int trigger, int setting);
int  tsDisableRandomTrigger();
int  tsReadBlock(volatile unsigned int *data, int nwrds, int rflag);
int  tsReadTriggerBlock(volatile unsigned int *data);
int  tsReadScalers(volatile unsigned int *data, int choice);
int  tsPrintScalers(int choice);
int  tsSetBusySource(unsigned int sourcemask, int rFlag);
void tsEnableBusError();
void tsDisableBusError();
int  tsPayloadPort2VMESlot(int payloadport);
int  tsPayloadPortMask2VMESlotMask(int payloadport_mask);
int  tsVMESlot2PayloadPort(int vmeslot);
int  tsVMESlotMask2PayloadPortMask(int vmeslot_mask);
int  tsSetPrescale(int prescale);
int  tsGetPrescale();
int  tsSetTriggerPrescale(int type, int chan, unsigned int prescale);
int  tsGetTriggerPrescale(int type, int chan);

int  tsSetTriggerPulse(int trigger, int delay, int width);
void tsTrigLinkReset();
void tsSetSyncDelayWidth(unsigned int delay, unsigned int width, int widthstep);
void tsSyncReset(int blflag);
void tsResetEB();
void tsSyncResetResync();
void tsClockReset();
void tsUserSyncReset(int enable);
int  tsSetUserSyncResetReceive(int enable);
void tsTriggerReadyReset();
void tsTriggerLinkErrorReset();
unsigned int tsGetTriggerLinkStatus(int pflag);
int  tsSetAdr32(unsigned int a32base);
int  tsResetEventCounter();
unsigned long long int tsGetEventCounter();
int  tsSetBlockLimit(unsigned int limit);
unsigned int tsGetBlockLimit();
int  tsGetBlockLimitStatus();
int  tsSetGTPInputReadout(int enable);
int  tsSetFPInputReadout(int enable);
int  tsSetBeforePrescaleReadout(int enable);

int  tsIntPoll();
unsigned int  tsGetIntCount();
int  tsIntConnect(unsigned int vector, VOIDFUNCPTR routine, unsigned int arg);
int  tsIntDisconnect();
int  tsAckConnect(VOIDFUNCPTR routine, unsigned int arg);
void tsIntAck();
int  tsIntEnable(int iflag);
void tsIntDisable();
unsigned int  tsBReady();
int  tsGetSyncEventFlag();
int  tsGetSyncEventReceived();
int  tsSetBlockBufferLevel(unsigned int level);
int  tsSetupFiberLoopback();



int  tsTriggerTableConfig(unsigned int **itable);
int  tsGetTriggerTable(unsigned int **otable);
int  tsDefineEventType(int inputType, unsigned int trigMask, int hwTrig, int evType);
int  tsDefineSpecialEventType(int trigOpt, int evType);
int  tsGetSpecialEventType(int trigOpt);
void tsTriggerTableDefault();
int  tsLoadTriggerTable();
void tsPrintTriggerTable(int inputType, int subGroup, int showbits);

unsigned int tsGetDaqStatus();
int  tsSetOutputPort(unsigned int set1, unsigned int set2, unsigned int set3,
		     unsigned int set4, unsigned int set5, unsigned int set6);
int  tsSetClockSource(unsigned int source);
void tsResetIODelay();
int  tsVmeTrigger1();
int  tsVmeTrigger2();
void tsSetFiberDelay(unsigned int number, unsigned int offset);
int  tsResetSlaveConfig();
int  tsAddSlave(unsigned int fiber);
int  tsRemoveSlave(unsigned int fiber);
int  tsSetTriggerHoldoff(int rule, unsigned int value, int timestep);
int  tsGetTriggerHoldoff(int rule);
int  tsSetTriggerHoldoffMin(int rule, unsigned int value);
int  tsGetTriggerHoldoffMin(int rule, int pflag);
int  tsLoadTriggerTable();
unsigned int tsGetLiveTime();
unsigned int tsGetBusyTime();
unsigned int tsGetLiveTime_InputHigh();
unsigned int tsGetBusyTime_InputHigh();
int  tsLive(int sflag);
unsigned int tsBlockStatus(int fiber, int pflag);
int  tsGetBusyStatus(int pflag);
int  tsSetSyncEventInterval(int blk_interval);
int  tsForceSyncEvent();
int  tsSyncResetRequest();
int  tsGetSyncResetRequest();
int  tsFillToEndBlock();
int  tsCurrentBlockFilled(unsigned short npoll);
int  tsResetMGT();
int  tsSetFPDelay(int chan, int delay);
int  tsGetFPDelay(int chan);
int  tsPrintFPDelay();
int  tsSetTSIODrive(int enable);
int  tsGetTSIODrive();
int  tsGetDriverSupportedVersion();
int  tsReadScalersMon(volatile unsigned int *data);
int  tsSetTrigCoinWindow(unsigned int size);
int  tsGetTrigCoinWindow();
int  tsSetTrigInhibitWindow(unsigned int size);
int  tsGetTrigInhibitWindow();

/* Partition Mode routine prototypes */
int  tsPartInit(int pID, unsigned int tAddr, unsigned int mode, int iFlag);
int  tsPartSetBusySource(int busysrc);
int  tsPartSetBlockBufferLevel(unsigned int bufferlevel);
int  tsPartSetTDInput(unsigned int tdinput);
int  tsPartSetFPInput(unsigned short input1, unsigned short input2, unsigned short input3);
int  tsPartSetGTPInput(unsigned short input1, unsigned short input2, unsigned short input3, unsigned short input4, unsigned short input5);
int  tsPartLoadTriggerTable();
int  tsPartReadBlock(volatile unsigned int *data, int nwrds);
unsigned int tsPartBReady();
void tsPartIntAck();
int  tsPartIntEnable(int iflag);
void tsPartIntDisable();
int  tsPartIntConnect(VOIDFUNCPTR routine, unsigned int arg);
int  tsPartIntDisconnect();

/* Duplication Mode routine prototypes */
int  tsDuplMode(int set);
int  tsDuplSetBranchEnable(int b1, int b2, int b3, int b4);
int  tsDuplSetLocalTrigComboMask(unsigned int mask);
unsigned int tsDuplGetLocalTrigComboMask();
int  tsDuplSetLocalTrigCombo(unsigned int mask, int set);
int  tsDuplSetTriggerHoldoff(unsigned int value);
int  tsDuplGetTriggerHoldoff();
int  tsPrintTriggerHoldoff(int dflag);
int  tsDuplSetLocalTriggerWidth(int width);
int  tsDuplGetLocalTriggerWidth();
int  tsDuplSetFastClearWidth(int width);
int  tsDuplGetFastClearWidth();
int  tsDuplSetFastClearDelay(int delay);
int  tsDuplGetFastClearDelay();
int  tsDuplSetFastClearVetoWidth(int width);
int  tsDuplGetFastClearVetoWidth();
int  tsDuplSetLocalTrigBusy(int value);
int  tsDuplGetLocalTrigBusy();
int  tsDuplSetFastClearBusy(int value);
int  tsDuplGetFastClearBusy();
int  tsDuplGetBusyTime();
unsigned int tsDuplGetBusyStatus();
int  tsDuplPrintBusyStatus();

#endif /* TSLIB_H */
