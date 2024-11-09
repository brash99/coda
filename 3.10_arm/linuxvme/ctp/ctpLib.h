/*----------------------------------------------------------------------------*
 *  Copyright (c) 2010        Southeastern Universities Research Association, *
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
 *     Status and Control library for the JLAB Crate Trigger Processor
 *     (CTP) module using an i2c interface from the JLAB Trigger
 *     Interface (TI) module.
 *
 *----------------------------------------------------------------------------*/
#ifndef CTPLIB_H
#define CTPLIB_H

/* Driver supported U1 & U3 firmware */
#define CTP_SUPPORTED_U1_FIRMWARE      0x0105
#define CTP_SUPPORTED_U3_FIRMWARE      0x0306

/* Driver supported U24 firmware */
#define CTP_SUPPORTED_U24_FIRMWARE  0x0606,0xA06,0xE03,0x1202
#define CTP_SUPPORTED_U24_FIRMWARE_NUMBER 4

#define CTP_SUPPORTED_FIRMWARE_MASK 0xFCFF

#define CTP_FIRMWARE_REVISION_MASK               0x00FF
#define CTP_FIRMWARE_FPGA_MASK                   0x0300
#define CTP_FIRMWARE_PROJECT_MASK                0x7c00
#define CTP_FIRMWARE_BETA_VERSION_MASK           0x8000

#define CTP_FIRMWARE_FPGA_U1                     1
#define CTP_FIRMWARE_FPGA_U3                     3
#define CTP_FIRMWARE_FPGA_U24                    2

#define CTP_FIRMWARE_PROJECT_HALL_D              0
#define CTP_FIRMWARE_PROJECT_BCAL                1
#define CTP_FIRMWARE_PROJECT_MICROSCOPE          2
#define CTP_FIRMWARE_PROJECT_HODOSCOPE           2
#define CTP_FIRMWARE_PROJECT_SPECTROMETER        3
#define CTP_FIRMWARE_PROJECT_TOF                 4


struct CTP_FPGA_U1_Struct
{
  /* 0xn00 */ volatile unsigned int status0;   			/* Address 0 */
  /* 0xn04 */ volatile unsigned int status1;   			/* Address 1 */
  /* 0xn08 */ volatile unsigned int config0;   			/* Address 2 */
  /* 0xn0C */ volatile unsigned int config1;   			/* Address 3 */
  /* 0xn10 */ volatile unsigned int temp;      			/* Address 4 */
  /* 0xn14 */ volatile unsigned int vint;      			/* Address 5 */
  /* 0xn18 */ volatile unsigned int config2;   			/* Address 6 */
  /* 0xn1C */ volatile unsigned int config3;   			/* Address 7 */
  /* 0xn20 */ volatile unsigned int config4;   			/* Address 8 */
  /* 0xn24 */ volatile unsigned int config5;   			/* Address 9 */
  /* 0xn28 */ volatile unsigned int config6;   			/* Address 10 */
  /* 0xn2C */ volatile unsigned int status3;   			/* Address 11 */
  /* 0xn30 */          unsigned int blank0[(0x44-0x30)/4];   	/* Address 12-16 */
  /* 0xn44 */ volatile unsigned int status2;   			/* Address 17 */
};

struct CTP_FPGA_U3_Struct
{
  /* 0xn00 */ volatile unsigned int status0;   			/* Address 0 */
  /* 0xn04 */ volatile unsigned int status1;   			/* Address 1 */
  /* 0xn08 */ volatile unsigned int config0;   			/* Address 2 */
  /* 0xn0C */ volatile unsigned int config1;   			/* Address 3 */
  /* 0xn10 */ volatile unsigned int temp;      			/* Address 4 */
  /* 0xn14 */ volatile unsigned int vint;      			/* Address 5 */
  /* 0xn18 */ volatile unsigned int config2;   			/* Address 6 */
  /* 0xn1C */ volatile unsigned int config3;   			/* Address 7 */
  /* 0xn20 */          unsigned int blank0[(0x2C-0x20)/4];   	/* Address 8-10 */
  /* 0xn2C */ volatile unsigned int status3;   			/* Address 11 */
  /* 0xn30 */          unsigned int blank1[(0x44-0x30)/4];   	/* Address 12-16 */
  /* 0xn44 */ volatile unsigned int status2;   			/* Address 17 */
};

struct CTP_FPGA_U24_Struct
{
  /* 0xn00 */ volatile unsigned int status0;             /* Address 0 */
  /* 0xn04 */ volatile unsigned int status1;             /* Address 1 */
  /* 0xn08 */ volatile unsigned int config0;             /* Address 2 */
  /* 0xn0C */ volatile unsigned int config1;             /* Address 3 */
  /* 0xn10 */ volatile unsigned int temp;                /* Address 4 */
  /* 0xn14 */ volatile unsigned int vint;                /* Address 5 */
  /* 0xn18 */          unsigned int blank0;              /* Address 6 */
  /* 0xn1C */          unsigned int blank1;              /* Address 7 */
  /* 0xn20 */ volatile unsigned int sum_threshold_lsb;   /* Address 8 */
  /* 0xn24 */ volatile unsigned int sum_threshold_msb;   /* Address 9 */
  /* 0xn28 */ volatile unsigned int history_buffer_lsb;  /* Address 10 */
  /* 0xn2C */ volatile unsigned int history_buffer_msb;  /* Address 11 */
  /* 0xn30 */ volatile unsigned int scaler_ctrl;         /* Address 12 */
  /* 0xn34 */ volatile unsigned int clock_scaler;        /* Address 13 */
  /* 0xn38 */ volatile unsigned int sync_scaler;         /* Address 14 */
  /* 0xn3C */ volatile unsigned int trig1_scaler;        /* Address 15 */
  /* 0xn40 */ volatile unsigned int trig2_scaler;        /* Address 16 */
  /* 0xn44 */ volatile unsigned int status2;             /* Address 17 */
  /* 0xn48 */ volatile unsigned int config2;             /* Address 18 */
  /* 0xn4C */ volatile unsigned int config3;             /* Address 19 */
  /* 0xn50 */ volatile unsigned int config4;             /* Address 20 */
  /* 0xn54 */ volatile unsigned int config5;             /* Address 21 */
  /* 0xn58 */ volatile unsigned int config6;             /* Address 22 */
  /* 0xn5C */ volatile unsigned int config7;             /* Address 23 */
  /* 0xn60 */ volatile unsigned int config8;             /* Address 24 */
  /* 0xn64 */ volatile unsigned int config9;             /* Address 25 */
  /* 0xn68 */ volatile unsigned int status3;             /* Address 26 */
  /* 0xn6C */ volatile unsigned int status4;             /* Address 27 */
  /* 0xn70 */ volatile unsigned int status5;             /* Address 28 */
  /* 0xn74 */ volatile unsigned int status6;             /* Address 29 */
  /* 0xn7C */ volatile unsigned int status7;             /* Address 30 */
  /* 0xn80 */ volatile unsigned int status8;             /* Address 31 */
  /* 0xn84 */ volatile unsigned int status9;             /* Address 32 */
  /* 0xn88 */ volatile unsigned int status10;            /* Address 33 */
  /* 0xn8C */ volatile unsigned int status11;            /* Address 34 */
  /* 0xn90 */ volatile unsigned int status12;            /* Address 35 */
  /* 0xn94 */ volatile unsigned int status13;            /* Address 36 */
};

struct CTPStruct
{
  /* 0x0000 */          unsigned int blank0[(0x3C00-0x0000)/4];
  /* 0x3C00 */ struct   CTP_FPGA_U1_Struct fpga1;    /* I2C Device 1 */
  /* 0x3C48 */          unsigned int blank1[(0x5C00-0x3C48)/4];
  /* 0x5C00 */ struct   CTP_FPGA_U3_Struct fpga3;    /* I2C Device 2 */
  /* 0x5C48 */          unsigned int blank2[(0x7C00-0x5C48)/4];
  /* 0x7C00 */ struct   CTP_FPGA_U24_Struct fpga24;  /* I2C Device 3 */
  /* 0x7C98 */          unsigned int blank3[(0x10000-0x7C98)/4];
};

/* Initialization flags */
#define CTP_INIT_IGNORE_VERSION         (1<<0)

/* CTP Register bits and masks */
/* Lane_up mask shifts by 2 bits for each channel (two lanes/channel) */

/* Common bits and masks for all FPGAs */
#define CTP_FPGA_STATUS0_CHAN_UP(chan)    (1<<(chan+12))
#define CTP_FPGA_STATUS0_LANE0_UP(chan)   (1<<(2*chan))
#define CTP_FPGA_STATUS0_LANE1_UP(chan)   (1<<(2*chan+1))

#define CTP_FPGA_STATUS1_CHANUP_EXTRA1           (1<<0)
#define CTP_FGPA_STATUS1_ALLCHANUP               (1<<1)
#define CTP_FPGA_STATUS1_CHANUP_EXTRA2           (1<<2)

#define CTP_FGPA_CONFIG1_INIT_ALL_MGT            (1<<1)
#define CTP_FPGA_CONFIG1_ALIGN_AT_SYNCRESET      (1<<3)

#define CTP_FPGA_TEMP_SLOPE        (503.975/1024.)
#define CTP_FPGA_TEMP_YINT         (-273.15)

#define CTP_FPGA_STATUS2_FIRMWARE_VERSION_MASK   0x7FFF


/* U1: config2 bits */
#define CTP_FPGA1_CONFIG2_PROG_DATA_U1    (0x0<<3)
#define CTP_FPGA1_CONFIG2_PROG_DATA_U3    (0x1<<3)
#define CTP_FPGA1_CONFIG2_PROG_DATA_U24   (0x2<<3)
#define CTP_FPGA1_CONFIG2_READ_DATA_U1    (0x3<<3)
#define CTP_FPGA1_CONFIG2_READ_DATA_U3    (0x4<<3)
#define CTP_FPGA1_CONFIG2_READ_DATA_U24   (0x5<<3)
#define CTP_FPGA1_CONFIG2_READ_EPROM_U1   (0x6<<3)
#define CTP_FPGA1_CONFIG2_READ_EPROM_U3   (0x7<<3)
#define CTP_FPGA1_CONFIG2_READ_EPROM_U24  (0x8<<3)
#define CTP_FPGA1_CONFIG2_ERASE_EPROM_U1  (0x9<<3)
#define CTP_FPGA1_CONFIG2_ERASE_EPROM_U3  (0xA<<3)
#define CTP_FPGA1_CONFIG2_ERASE_EPROM_U24 (0xB<<3)
#define CTP_FPGA1_CONFIG2_SRAM_WRITE        (1<<7)
#define CTP_FPGA1_CONFIG2_EXEC_OPCODE       (1<<8)
#define CTP_FPGA1_CONFIG2_U1_CONFIG       (0x1<<9)
#define CTP_FPGA1_CONFIG2_U3_CONFIG       (0x2<<9)
#define CTP_FPGA1_CONFIG2_U24_CONFIG      (0x3<<9)

/* U1: config3 masks */
#define CTP_FPGA1_CONFIG3_ERASE_ROM_BLOCK_NUM  0x3FF

/* U1: config4 masks */
#define CTP_FPGA1_CONFIG4_SRAM_DATA_MASK      0xFFFF

/* U1: config5 masks */
#define CTP_FPGA1_CONFIG5_SRAM_ADDR_MASK      0xFFFF

/* U1: config6 bits and masks */
#define CTP_FPGA1_CONFIG6_SRAM_ADDR_MASK       0x3F
#define CTP_FPGA1_CONFIG6_SRAM_READ         (1<<14)
#define CTP_FPGA1_CONFIG6_SRAM_WRITE        (1<<15)

/* U1: status3 masks */
#define CTP_FPGA1_STATUS3_SRAM_DATA_MASK    0xFFFF

/* U1: status2 bits */
#define CTP_FPGA1_STATUS2_READY_FOR_OPCODE      (1<<15)

/* U3: config2 bits and masks */
#define CTP_FPGA3_CONFIG2_SROM_ADDR_MASK    0x3FF
#define CTP_FPGA3_CONFIG2_SROM_READ       (1<<15)

/* U3: config3 bits */
#define CTP_FPGA3_CONFIG3_REBOOT_ALL_FPGA  (5<<0)
#define CTP_FPGA3_CONFIG3_REBOOT_SAFETY    (2<<0)

/* U3: status3 bits and masks */
#define CTP_FPGA3_STATUS3_SROM_DATA_MASK     0xFF
#define CTP_FPGA3_STATUS3_SROM_DATA_VALID (1<<15)

/* U24: status1 bits */
#define CTP_FPGA24_STATUS1_HISTORY_BUFFER_READY   (1<<3)
#define CTP_FPGA24_STATUS1_FIBER_LANE_CHAN_ALIGN  (1<<4)
#define CTP_FPGA24_STATUS1_FIBER_LANE_BYTE_ALIGN  (1<<5)
#define CTP_FPGA24_STATUS1_FIBER_LANE_REMOTE_UP   (1<<6)
#define CTP_FPGA24_STATUS1_FIBER_CHAN_READY       (1<<7)
#define CTP_FPGA24_STATUS1_ERROR_LATCH_FS         (1<<8)
#define CTP_FPGA24_STATUS1_ALIGNMENT_SUCCESS      (1<<9)

/* U24: config1 bits */
#define CTP_FPGA24_CONFIG1_RESET_FIBER_MGT         (1<<2)
#define CTP_FPGA24_CONFIG1_ARM_HISTORY_BUFFER      (1<<0)
#define CTP_FPGA24_CONFIG1_SPECTROMETER_FP_OUTPUT  (1<<4)


/* U24: status2 bits */
#define CTP_FPGA24_STATUS2_FIRMWARE_VERSION_MASK   0xFFFF

/* U24: config3 masks */
#define CTP_FPGA24_CONFIG3_FP_INPUT_MASK         0xF000
#define CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK      0x003F
#define CTP_FPGA24_CONFIG3_BCAL_SCALERS_INHIBIT_WINDOW_MASK 0x0F00
#define CTP_FPGA24_CONFIG3_BCAL_WINDOW_MASK      0x003F
#define CTP_FPGA24_CONFIG3_HODOSCOPE_WIDTH_MASK  0x0780
#define CTP_FPGA24_CONFIG3_MICROSCOPE_WIDTH_MASK 0x0780
#define CTP_FPGA24_CONFIG3_HODOSCOPE_SELECT      (1<<11)

#define CTP_FPGA24_CONFIG3_SPECTROMETER_COUNTER_MODE_MASK           0xC000
#define CTP_FPGA24_CONFIG3_SPECTROMETER_COUNTER_MODE_PSS            (1<<14)
#define CTP_FPGA24_CONFIG3_SPECTROMETER_COUNTER_MODE_PSCT           (1<<15)
#define CTP_FPGA24_CONFIG3_SPECTROMETER_COUNTER_MODE_STCT           0
#define CTP_FPGA24_CONFIG3_SPECTROMETER_WINDOW_WIDTH_MASK           0x000F
#define CTP_FPGA24_CONFIG3_SPECTROMETER_COUNT_INHIBIT_WIDTH_MASK    0x000F

/* U24: config4 bits and masks */
#define CTP_FPGA24_CONFIG3_TOF_WIDTH_MASK        0x000F
#define CTP_FPGA24_CONFIG4_BCAL_THRESHOLD        0xFFFF

/* U24: config9 masks */
#define CTP_FPGA24_CONFIG9_SCALER4_MSB_MASK      0xF000
#define CTP_FPGA24_CONFIG9_SCALER3_MSB_MASK      0x0F00
#define CTP_FPGA24_CONFIG9_SCALER2_MSB_MASK      0x00F0
#define CTP_FPGA24_CONFIG9_SCALER1_MSB_MASK      0x000F

/* U24: Scaler bits and masks */
#define CTP_SCALER_CTRL_RESET_SYNC               (1<<0)
#define CTP_SCALER_CTRL_RESET_TRIG1              (1<<1)
#define CTP_SCALER_CTRL_RESET_TRIG2              (1<<2)
#define CTP_SCALER_CTRL_BCAL_RISING_EDGE_LATCH         (1<<14)
#define CTP_SCALER_CTRL_BCAL_CLEAR_COUNTERS_AFTER_READ (1<<15)
#define CTP_SCALER_CTRL_SPECTROMETER_RISING_EDGE_LATCH           (1<<14)
#define CTP_SCALER_CTRL_SPECTROMETER_CLEAR_COUNTERS_AFTER_READ   (1<<15)

#define CTP_CLOCK_SCALER_COUNT_MASK              0xFF
#define CTP_SCALER_COUNT_MASK                    0xFFFF

#define CTP_DATA_MASK                            0xFFFFFFFF


int  ctpInit(int flag);
int  ctpStatus();
int  ctpSetFinalSumThreshold(unsigned int threshold, int arm);
int  ctpGetFinalSumThreshold();
int  ctpSetHistoryBufferTriggerMask(unsigned int mask, int arm);
unsigned int ctpGetHistoryBufferTriggerMask(int pflag);
int  ctpSetPayloadEnableMask(int enableMask);
int  ctpSetVmeSlotEnableMask(unsigned int vmemask);
int  ctpEnableSlotMask(unsigned int inMask);
int  ctpResetGTP();
int  ctpGetAllChanUp();
int  ctpGetErrorLatchFS(int pflag);
int  ctpGetAlignmentStatus(int pflag, int ntries);
int  ctpAlignAtSyncReset(int enable);
int  ctpArmHistoryBuffer();
int  ctpDReady();
int  ctpReadEvent(volatile unsigned int *data, int nwrds);
void ctpFiberReset();
void ctpPayloadReset();

int  ctpResetScalers();
int  ctpResetSyncScaler();
int  ctpResetTrig1Scaler();
int  ctpResetTrig2Scaler();
int  ctpGetClockScaler();
int  ctpGetSyncScaler();
int  ctpGetTrig1Scaler();
int  ctpGetTrig2Scaler();

int  ctpGetSerialNumber(char **rval);
int  ctpSetCrateID(int crateID);
int  ctpGetCrateID();
int  ctpGetFirmwareVersion(int fpga);
unsigned int ctpGetChannelUpMask(int fType);
unsigned int ctpGetLaneUpMask(int lane, int fType);
float ctpGetFPGATemperature(int fpga, int pflag);

unsigned int ctpGetChannelUpMask(int fType);
unsigned int ctpGetLaneUpMask(int lane, int fType);

int  ctpSetFPInputMask(int mask);
int  ctpGetFPInputMask();
int  ctpSetBCALWindowWidth(int width);
int  ctpGetBCALWindowWidth();
int  ctpSetBCALThreshold(int thres);
int  ctpGetBCALThreshold();
int  ctpSetBCALScalerThresholds(int thres1, int thres2, int thres3, int thres4);
int  ctpGetBCALScalerThreshold(int scal);
int  ctpSetBCALScalerInhibitWindow(int width);
int  ctpGetBCALScalerInhibitWindow(int width);
int  ctpReadBCALData(int latch, int clear, volatile unsigned int *data);
int  ctpPrintBCALData(int latch, int clear);

int  ctpSetScopeTrigger(int type, int windowwidth);
int  ctpGetScopeTrigger(int pflag);
int  ctpGetScopeTriggerWidth();


int  ctpSetSpectrometerMode(int mode, int width);
int  ctpGetSpectrometerMode();
int  ctpGetSpectrometerWidth();
int  ctpReadSpectrometerData(int latch, int clear, volatile unsigned int *data);
int  ctpPrintSpectrometerData(int latch, int clear);
int  ctpGetSpectrometerFPOutput();
int  ctpSetSpectrometerFPOutput(int enable);

int  ctpSetTOFCoincWidth(int windowwidth);
int  ctpGetTOFCoincWidth();

int  ctpSetFWSize(int size);
int  ctpSetNSectorErase(int nsectors);
int  ctpReadFirmwareFile(char *fw_filename);
int  ctpFirmwareRevFromFpgaID(int pflag);
int  ctpFirmwareChipFromFpgaID(int pflag);
int  ctpFirmwareUpload(int ifpga, int reboot);
#endif /* CTPLIB_H */
