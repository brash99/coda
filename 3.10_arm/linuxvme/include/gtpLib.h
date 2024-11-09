/*----------------------------------------------------------------------------*
 *  Copyright (c) 2014        Southeastern Universities Research Association, *
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
 *     Control and Status library for the JLAB Global Trigger Processor
 *     (GTP) module using an ethernet socket interface
 *
 *----------------------------------------------------------------------------*/
#ifndef GTPLIB_H
#define GTPLIB_H

#include "gtpVMELib.h"
#include "gtpSocket.h"


// Structure returned by ReadScalers() tcp server routine
typedef struct
{
  unsigned int SysClk50;
  unsigned int GClk;
  unsigned int Sync;
  unsigned int Trig1;
  unsigned int Trig2;
  unsigned int FpIn[4];
  unsigned int FpOut[4];
  unsigned int Busy;
  unsigned int BusyCycles;
  unsigned int FCalEnergy[32];
  unsigned int BCalEnergy[32];
  unsigned int BCalHitModules[32];
  unsigned int TOF[32];
  unsigned int TagM[32];
  unsigned int TagH[32];
  unsigned int PS[32];
  unsigned int ST[32];
  unsigned int Trig_BCal[32];
  unsigned int Trig_BFCal[32];
  unsigned int Trig_TAGM[32];
  unsigned int Trig_TAGH[32];
  unsigned int Trig_PS[32];
  unsigned int Trig_ST[32];
  unsigned int Trig_TOF[32];
  unsigned int Trig[32];
} GtpScalers;

#define GTP_ADDR		0xF8010000

// Serdes Peripheral: Fiber & VXS serdes controls and monitors
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Ctrl;
  /* 0x0004-0x000F */	unsigned int Reserved0[(0x0010-0x0004)/4];
  /* 0x0010-0x0013 */	unsigned int Status;
  /* 0x0014-0x0017 */	unsigned int Reserved1[(0x0018-0x0014)/4];
  /* 0x0018-0x001B */	unsigned int ErrTile0;
  /* 0x001C-0x001F */	unsigned int Status2;
  /* 0x0020-0x0023 */	unsigned int LaCtrl;
  /* 0x0024-0x0027 */	unsigned int LaStatus;
  /* 0x0028-0x002F */	unsigned int Reserved2[(0x0030-0x0028)/4];
  /* 0x0030-0x0037 */	unsigned int LaData[2];
  /* 0x0038-0x004F */	unsigned int Reserved3[(0x0050-0x0038)/4];
  /* 0x0050-0x0053 */	unsigned int CompareMode;
  /* 0x0054-0x006F */	unsigned int Reserved4[(0x0070-0x0054)/4];
  /* 0x0070-0x0073 */	unsigned int CompareThreshold;
  /* 0x0074-0x008F */	unsigned int Reserved5[(0x0090-0x0074)/4];
  /* 0x0090-0x0097 */	unsigned int MaskEn[2];
  /* 0x0098-0x00AF */	unsigned int Reserved6[(0x00B0-0x0098)/4];
  /* 0x00B0-0x00B7 */	unsigned int MaskVal[2];
  /* 0x00B8-0x00FF */	unsigned int Reserved7[(0x0100-0x00B8)/4];
} GtpSerdes_regs;

// NOTE: SERINDEX should be set to: VXS payload number - 1
#define SS_TAGM_SERINDEX	       14	// PP15
#define SS_TAGH_SERINDEX	       12	// PP13
#define SS_PS_SERINDEX		       10	// PP11
#define SS_ST_SERINDEX			8	// PP9
#define SS_TOF_SERINDEX			6	// PP7
#define SS_BCAL_SERINDEX		4	// PP5
#define SS_FCAL0_SERINDEX		2	// PP3
#define SS_FCAL1_SERINDEX		0	// PP1
#define SS_PORT_MASK               0x5555

#define GTP_SER_VXS0	0
#define GTP_SER_VXS1	1
#define GTP_SER_VXS2	2
#define GTP_SER_VXS3	3
#define GTP_SER_VXS4	4
#define GTP_SER_VXS5	5
#define GTP_SER_VXS6	6
#define GTP_SER_VXS7	7
#define GTP_SER_VXS8	8
#define GTP_SER_VXS9	9
#define GTP_SER_VXS10	10
#define GTP_SER_VXS11	11
#define GTP_SER_VXS12	12
#define GTP_SER_VXS13	13
#define GTP_SER_VXS14	14
#define GTP_SER_VXS15	15

#define GTP_SER_CTRL_POWERDN    (1<<0)
#define GTP_SER_CTRL_ERR_RST   (1<<10)
#define GTP_SER_CTRL_ERR_EN    (1<<11)

#define GTP_SER_STATUS_HARDERR0   (1<<0)
#define GTP_SER_STATUS_HARDERR1   (1<<1)
#define GTP_SER_STATUS_LANEUP0    (1<<4)
#define GTP_SER_STATUS_LANEUP1    (1<<5)
#define GTP_SER_STATUS_CHANNELUP  (1<<12)
#define GTP_SER_STATUS_TXLOCK     (1<<13)
#define GTP_SER_STATUS_RXSRCRDYN  (1<<14)

#define GTP_SER_ERRTILE_LANE0_BITERRORS  0x0000FFFF
#define GTP_SER_ERRTILE_LANE1_BITERRORS  0xFFFF0000

#define GTP_SER_STATUS2_LATENCY_MASK 0x00000FFF
#define GTP_SER_STATUS2_CRCPASS      (1<<16);


// GXB Reconfig Peripheral: Serdes debug interface
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Status;
  /* 0x0004-0x0007 */	unsigned int Ctrl;
  /* 0x0008-0x000B */	unsigned int Ctrl2;
  /* 0x000C-0x000F */	unsigned int TxRxIn;
  /* 0x0010-0x0013 */	unsigned int TxRxOut;
  /* 0x0014-0x00FF */	unsigned int Reserved0[(0x0100-0x0014)/4];
} GtpGxbCfg_regs;

// Configuration Peripheral: Gtp configuration interface
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int BoardId;
  /* 0x0004-0x0007 */	unsigned int FirmwareRev;
  /* 0x0008-0x000B */	unsigned int CpuStatus;
  /* 0x000C-0x001B */	unsigned int Hostname[4];
  /* 0x001C-0x00FF */	unsigned int Reserved0[(0x0100-0x001C)/4];
} GtpCfg_regs;

#define GTP_BOARDID                0x47545020
#define GTP_FIRMWAREREV_TYPE_MASK  0xFFFF0000
#define GTP_FIRMWAREREV_MAJOR_MASK 0x0000FF00
#define GTP_FIRMWAREREV_MINOR_MASK 0x000000FF


// Clock Peripheral: Clock configuration interface
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Ctrl;
  /* 0x0004-0x0007 */	unsigned int Status;
  /* 0x0008-0x00FF */	unsigned int Reserved0[(0x0100-0x0008)/4];
} GtpClk_regs;

#define GTP_CLK_CTRL_P0			0x00000001
#define GTP_CLK_CTRL_INT		0x00000002
#define GTP_CLK_CTRL_SRC_MASK           0x00000003
#define GTP_CLK_CTRL_RESET		0x80000000
#define GTP_CLKSRC_NUM                  4

#define GTP_CLK_STATUS_GCLK_LOCK      (1<<0)
#define GTP_CLK_STATUS_L_LOCK         (1<<1)
#define GTP_CLK_STATUS_R_LOCK         (1<<2)
#define GTP_CLK_STATUS_LOCK_MASK  0x00000003


// Trigger Peripheral
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Ctrl;
  /* 0x0004-0x03FF */	unsigned int Reserved0[(0x0400-0x0004)/4];
} GtpTrigger_regs;

// GtpTrigger_regs->Ctrl bits (for Hall D GTP definitions)
//    '1' - enables subsystem data stream
//    '0' - disables subsystem data stream
#define GTP_TRG_CTRL_BCAL_E	 0x00000001
#define GTP_TRG_CTRL_BCAL_H	 0x00000002
#define GTP_TRG_CTRL_FCAL	 0x00000004
#define GTP_TRG_CTRL_TAGM	 0x00000008
#define GTP_TRG_CTRL_TAGH	 0x00000010
#define GTP_TRG_CTRL_PS		 0x00000020
#define GTP_TRG_CTRL_ST		 0x00000040
#define GTP_TRG_CTRL_TOF	 0x00000080
#define GTP_TRG_CTRL_ENABLE_MASK 0x000000FF

// SD Peripheral
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int ScalerLatch;
  /* 0x0004-0x003F */	unsigned int Scalers[15];
  /* 0x0040-0x00FF */	unsigned int Reserved0[(0x0100-0x0040)/4];
  /* 0x0100-0x0103 */	unsigned int PulserPeriod;
  /* 0x0104-0x0107 */	unsigned int PulserLowCycles;
  /* 0x0108-0x010B */	unsigned int PulserNPulses;
  /* 0x010C-0x010F */	unsigned int PulserDone;
  /* 0x0110-0x0113 */	unsigned int PulserStart;
  /* 0x0114-0x011F */	unsigned int Reserved1[(0x0120-0x0114)/4];
  /* 0x0120-0x0137 */	unsigned int SrcSel[6];
  /* 0x0138-0x01FF */	unsigned int Reserved2[(0x0200-0x0138)/4];
} GtpSd_regs;


// GtpSd_regs->SrcSel[] IDs
#define GTP_SD_SRC_TRIG				0
#define GTP_SD_SRC_SYNC				1
#define GTP_SD_SRC_LVDSOUT0			2
#define GTP_SD_SRC_LVDSOUT1			3
#define GTP_SD_SRC_LVDSOUT2			4
#define GTP_SD_SRC_LVDSOUT3			5
#define GTP_SD_SRC_LVDSOUT4			6
#define GTP_SRC_NUM                             7

// GtpSd_regs->SrcSel[] values
#define GTP_SD_SRC_SEL_0			0
#define GTP_SD_SRC_SEL_1			1
#define GTP_SD_SRC_SEL_SYNC			2
#define GTP_SD_SRC_SEL_TRIG1			3
#define GTP_SD_SRC_SEL_TRIG2			4
#define GTP_SD_SRC_SEL_LVDSIN(n)		(5+n)
#define GTP_SD_SRC_SEL_PULSER			9
#define GTP_SD_SRC_SEL_BUSY			10
#define GTP_SD_SRC_SEL_TRIGGER(n)	        (32+n)

#define GTP_SRC_SEL_NUM                         64

// GtpSd_regs->Scalers[] IDs
#define GTP_SD_SCALER_50MHZ			0
#define GTP_SD_SCALER_GCLK			1
#define GTP_SD_SCALER_SYNC			2
#define GTP_SD_SCALER_TRIG1			3
#define GTP_SD_SCALER_TRIG2			4
#define GTP_SD_SCALER_BUSY			5
#define GTP_SD_SCALER_BUSYCYCLES		6
#define GTP_SD_SCALER_FPIN(n)			(7+n)
#define GTP_SD_SCALER_FPOUT(n)		        (11+n)


// Logic Analyzer Peripheral
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Ctrl;
  /* 0x0004-0x0007 */	unsigned int Status;
  /* 0x0008-0x001F */	unsigned int Reserved0[(0x0020-0x0008)/4];
  /* 0x0020-0x003B */	unsigned int Data[7];
  /* 0x003C-0x003F */	unsigned int Reserved1[(0x0040-0x003C)/4];
  /* 0x0040-0x004B */	unsigned int CompareMode[3];
  /* 0x004C-0x005F */	unsigned int Reserved2[(0x0060-0x004C)/4];
  /* 0x0060-0x006B */	unsigned int CompareThreshold[3];
  /* 0x006C-0x007F */	unsigned int Reserved3[(0x0080-0x006C)/4];
  /* 0x0080-0x009B */	unsigned int MaskEn[7];
  /* 0x009C-0x009F */	unsigned int Reserved4[(0x00A0-0x009C)/4];
  /* 0x00A0-0x00BB */	unsigned int MaskVal[7];
  /* 0x00BC-0x00FF */	unsigned int Reserved5[(0x0100-0x00BC)/4];
} GtpLa_regs;

// BCal Subsystem Peripheral
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Delay;
  /* 0x0004-0x0007 */	unsigned int Width;
  /* 0x0008-0x000F */	unsigned int Reserved0[(0x0010-0x0008)/4];
  /* 0x0010-0x0013 */	unsigned int HistDataEnergy;
  /* 0x0014-0x0017 */	unsigned int HistDataHits;
  /* 0x0018-0x00FF */	unsigned int Reserved1[(0x0100-0x0018)/4];
} GtpBCal_regs;

// FCal Subsystem Peripheral
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Delay;
  /* 0x0004-0x0007 */	unsigned int Width;
  /* 0x0008-0x000F */	unsigned int Reserved0[(0x0010-0x0008)/4];
  /* 0x0010-0x0013 */	unsigned int HistDataEnergy;
  /* 0x0014-0x00FF */	unsigned int Reserved1[(0x0100-0x0014)/4];
} GtpFCal_regs;

// Hit pattern subsystems (TAGM,TAGH,ST,TOF,PS)
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Delay;
  /* 0x0004-0x0007 */	unsigned int Width;
  /* 0x0008-0x007F */	unsigned int Reserved0[(0x0080-0x0008)/4];
  /* 0x0080-0x00FF */	unsigned int Scalers[32];
} GtpHitPattern_regs;

enum GTP_TRIG_PERIPHERAL
  {
    GTP_TRIGPER_BCAL = 0,
    GTP_TRIGPER_FCAL,
    GTP_TRIGPER_TAGM,
    GTP_TRIGPER_TAGH,
    GTP_TRIGPER_PS,
    GTP_TRIGPER_ST,
    GTP_TRIGPER_TOF,
    GTP_TRIGPER_NUM
  };

#define GTP_TRIGPER_DELAY_MASK  0x000000FF
#define GTP_TRIGPER_WIDTH_MASK  0x000000FF

// Hall D Trigger Bit
typedef struct
{
  /* 0x0000-0x0003 */	unsigned int Ctrl;
  /* 0x0004-0x0007 */	unsigned int TrigOutCtrl;
  /* 0x0008-0x000B */	unsigned int TrigOutStatus;
  /* 0x000C-0x000F */	unsigned int Reserved0[(0x0010-0x000C)/4];
  /* 0x0010-0x0013 */	unsigned int BCalCtrl0;
  /* 0x0014-0x0017 */	unsigned int BCalCtrl1;
  /* 0x0018-0x001F */	unsigned int Reserved1[(0x0020-0x0018)/4];
  /* 0x0020-0x0023 */	unsigned int FCalCtrl;
  /* 0x0024-0x002F */	unsigned int Reserved2[(0x0030-0x0024)/4];
  /* 0x0030-0x0033 */	unsigned int BFCalCtrl;
  /* 0x0034-0x003F */	unsigned int Reserved3[(0x0040-0x0034)/4];
  /* 0x0040-0x0043 */	unsigned int PSCtrl;
  /* 0x0044-0x004F */	unsigned int Reserved4[(0x0050-0x0044)/4];
  /* 0x0050-0x0053 */	unsigned int STCtrl0;
  /* 0x0054-0x0057 */	unsigned int STCtrl1;
  /* 0x0058-0x005F */	unsigned int Reserved5[(0x0060-0x0058)/4];
  /* 0x0060-0x0063 */	unsigned int TOFCtrl0;
  /* 0x0064-0x0067 */	unsigned int TOFCtrl1;
  /* 0x0068-0x006F */	unsigned int Reserved6[(0x0070-0x0068)/4];
  /* 0x0070-0x0073 */	unsigned int TagMCtrl;
  /* 0x0074-0x0077 */	unsigned int TagHCtrl;
  /* 0x0078-0x007F */	unsigned int Reserved7[(0x0080-0x0078)/4];
  /* 0x0080-0x009F */	unsigned int Scalers[8];
  /* 0x00A0-0x00FF */	unsigned int Reserved8[(0x0100-0x00A0)/4];
} GtpTrigbit_regs;

#define GTP_TRIGBIT_CTRL_ENABLE                 (1<<0)
#define GTP_TRIGBIT_CTRL_BCAL_HITMODULES_EN	(1<<1)
#define GTP_TRIGBIT_CTRL_BFCAL_EN		(1<<2)
#define GTP_TRIGBIT_CTRL_TAGM_PATTERN_EN	(1<<3)
#define GTP_TRIGBIT_CTRL_TAGH_PATTERN_EN	(1<<4)
#define GTP_TRIGBIT_CTRL_PS_COIN_EN		(1<<5)
#define GTP_TRIGBIT_CTRL_ST_NHITS_EN		(1<<6)
#define GTP_TRIGBIT_CTRL_TOF_NHITS_EN		(1<<7)
#define GTP_TRIGBIT_CTRL_ENABLE_MASK            0xff

#define GTP_TRIGBIT_TRIGOUTCTRL_LATENCY_MASK    0x00000FFF
#define GTP_TRIGBIT_TRIGOUTCTRL_WIDTH_MASK      0x00FF0000

#define GTP_TRIGBIT_TRIGOUTSTATUS_MASK          0x1

#define GTP_TRIGBIT_BCALCTRL0_MASK              0x000000FF
#define GTP_TRIGBIT_BCALCTRL1_MASK              0x0000000F

#define GTP_TRIGBIT_FCALCTRL_MASK              0x000000FF

#define GTP_TRIGBIT_BFCALCTRL_MASK             0xFFFFFFFF

#define GTP_TRIGBIT_PSCTRL_MASK                 0x0000FFFF

#define GTP_TRIGBIT_STCTRL0_MASK                0xFFFFFFFF
#define GTP_TRIGBIT_STCTRL1_MASK                0x0000001F

#define GTP_TRIGBIT_TOFCTRL0_MASK               0xFFFFFFFF
#define GTP_TRIGBIT_TOFCTRL1_MASK               0x0000001F

#define GTP_TRIGBIT_TAGMCTRL_MASK               0xFFFFFFFF

#define GTP_TRIGBIT_TAGHCTRL_MASK               0xFFFFFFFF



enum TRIGOUT_PARAMETERS
  {
    GTP_BCAL_ENERGY_SCALE = 0,
    GTP_BCAL_HITMODULES_THRESHOLD,
    GTP_FCAL_ENERGY_SCALE,
    GTP_BFCAL_ENERGY_THRESHOLD,
    GTP_PS_MASK,
    GTP_ST_MASK,
    GTP_ST_HITCOUNT_THRESHOLD,
    GTP_TOF_MASK,
    GTP_TOF_HITCOUNT_THRESHOLD,
    GTP_TAGM_MASK,
    GTP_TAGH_MASK,
    GTP_TRIGBIT_PARAM_NUM
  };

#define GTP_TRIGBIT_SCALER_BCALHIT	0
#define GTP_TRIGBIT_SCALER_BFCAL	1
#define GTP_TRIGBIT_SCALER_TAGM		2
#define GTP_TRIGBIT_SCALER_TAGH		3
#define GTP_TRIGBIT_SCALER_PS		4
#define GTP_TRIGBIT_SCALER_ST		5
#define GTP_TRIGBIT_SCALER_TOF		6
#define GTP_TRIGBIT_SCALER_TRIGOUT	7
#define GTP_TRIGBIT_SCALER_NUM          8

/* gtpStatus pflag bits */
enum GTP_STATUS_PFLAGS
  {
    GTP_STATUS_SHOW_PAYLOAD_INPUT  = (1<<0),
    GTP_STATUS_SHOW_TRIG_PERIPHEAL = (1<<1),
    GTP_STATUS_SHOW_TRIGOUT_BITS   = (1<<2)
  };

// GTP memory structure
typedef struct
{
  /* 0x0000-0x00FF */	GtpCfg_regs		Cfg;
  /* 0x0100-0x01FF */	GtpClk_regs		Clk;
  /* 0x0200-0x03FF */	GtpSd_regs		Sd;
  /* 0x0400-0x04FF */	GtpLa_regs		La;
  /* 0x0500-0x05FF */	GtpGxbCfg_regs		GxbCfgOdd;
  /* 0x0600-0x06FF */	GtpGxbCfg_regs		GxbCfgEven;
  /* 0x0700-0x0FFF */	unsigned int		Reserved1[(0x1000-0x0700)/4];
  /* 0x1000-0x1FFF */	GtpSerdes_regs		Ser[16];
  /* 0x2000-0x23FF */	GtpTrigger_regs		Trg;
  /* 0x2400-0x2FFF */	unsigned int		Reserved2[(0x3000-0x2400)/4];
  /* 0x3000-0x30FF */	GtpBCal_regs		BCal;
  /* 0x3100-0x31FF */	GtpFCal_regs		FCal;
  /* 0x3200-0x32FF */	GtpHitPattern_regs	TagM;
  /* 0x3300-0x33FF */	GtpHitPattern_regs	TagH;
  /* 0x3400-0x34FF */	GtpHitPattern_regs	PS;
  /* 0x3500-0x35FF */	GtpHitPattern_regs	ST;
  /* 0x3600-0x36FF */	GtpHitPattern_regs	TOF;
  /* 0x3700-0x3FFF */	unsigned int		Reserved3[(0x4000-0x3700)/4];
  /* 0x4000-0x4FFF */	GtpTrigbit_regs		Trigbits[16];
  /* 0x5000-0xFFFF */	unsigned int		Reserved4[(0x10000-0x6000)/4];
} Gtp_regs;

/* Routine prototypes */
int  gtpInit(int rFlag, char* gHostname);
int  gtpStatus(int pflag);
int  gtpCheckAddresses();

int  gtpSetClockSource(int clksrc);
int  gtpGetClockSource();
int  gtpGetClockPLLStatus();
int  gtpSetSyncSource(int srsrc);
int  gtpGetSyncSource();
int  gtpSetTrig1Source(int trig1src);
int  gtpGetTrig1Source();

int  gtpEnablePayloadPort(int port);
int  gtpDisablePayloadPort(int port);
int  gtpEnableVmeSlot(int vmeslot);
int  gtpDisableVmeSlot(int vmeslot);
int  gtpEnablePayloadPortMask(int portmask);
int  gtpEnableVmeSlotMask(unsigned int vmeslotmask);

int  gtpGetChannelUpMask();
int  gtpEnableBitErrorCounters(int port);
int  gtpEnableBitErrorCountersMask(int portmask);

int  gtpSetTriggerEnableMask(int trigmask);
int  gtpGetTriggerEnableMask();

int  gtpSetSubsystemDelayWidth(int ss, int delay, int width);
int  gtpGetSubsystemDelay(int ss);
int  gtpGetSubsystemWidth(int ss);

int  gtpSetTriggerBitEnableMask(int trigout, int trigbitMask);
int  gtpGetTriggerBitEnableMask(int trigout);
int  gtpSetTrigoutLatencyWidth(int trigout, int latency, int width);
int  gtpGetTrigoutLatency(int trigout);

int  gtpGetTrigoutWidth(int trigout);
int  gtpGetTrigoutStatus(int trigout);

int  gtpSetTrigoutParameter(int trigout, int param, int value);
unsigned int gtpGetTrigoutParameter(int trigout, int param);

extern const char *gtp_clksrc_name[GTP_CLKSRC_NUM];
extern const char *gtp_ioport_names[GTP_SRC_NUM];
extern const char *gtp_signal_name[GTP_SRC_SEL_NUM];
extern const char *gtp_trigout_param_names[GTP_TRIGBIT_PARAM_NUM];
extern const char *gtp_pp_name[16];
extern const char *gtp_trig_peripheral_name[GTP_TRIGPER_NUM];

#endif /* GTPLIB_H */
