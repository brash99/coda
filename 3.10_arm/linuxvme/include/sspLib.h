#ifndef __SSPLIB_H
#define __SSPLIB_H
/******************************************************************************
 *
 *  sspLib.h    -  Header for Driver library for JLAB config of JLAB Subsystem
 *                 Processor (SSP).
 *
 */

#ifndef MAX_VME_SLOTS
#define MAX_VME_SLOTS    21
#endif

#define SSP_MAX_FIFO       0x800000   /* 8 Meg */
#define SSP_MAX_A32MB_SIZE 0x800000


#define SSP_SUPPORTED_FIRMWARE 0x0105

/* Config Peripheral: Board information, fpga flash update */
typedef struct
{
  /* 0x0000-0x0003 */ volatile unsigned int BoardId;
  /* 0x0004-0x0007 */ volatile unsigned int FirmwareRev;
  /* 0x0008-0x000B */ volatile unsigned int SpiCtrl;
  /* 0x000C-0x000F */ volatile unsigned int SpiStatus;
  /* 0x0010-0x0013 */ volatile unsigned int ICapCtrl;
  /* 0x0014-0x0017 */ volatile unsigned int ICapDataWr;
  /* 0x0018-0x001B */ volatile unsigned int ICapDataRd;
  /* 0x001C-0x001E */ volatile unsigned int ICapStatus;
  /* 0x0020-0x0023 */ volatile unsigned int Reset;
  /* 0x0024-0x00FF */          unsigned int Reserved0[(0x0100-0x0024)/4];
} SspCfg_regs;

/* Clock Peripheral: Clock configuration interface */
typedef struct
{
  /* 0x0000-0x0003 */ volatile unsigned int Ctrl;
  /* 0x0004-0x0007 */ volatile unsigned int Status;
  /* 0x0008-0x00FF */          unsigned int Reserved0[(0x0100-0x0008)/4];
} Clk_regs;

/* SD Peripheral: Internal signal muxing, scalers, pulser */
typedef struct
{
  /* 0x0000-0x003B */ volatile unsigned int SrcSel[17];
  /* 0x003C-0x007F */          unsigned int Reserved0[(0x0080-0x0044)/4];
  /* 0x0080-0x0083 */ volatile unsigned int PulserPeriod;
  /* 0x0084-0x0087 */ volatile unsigned int PulserLowCycles;
  /* 0x0088-0x008B */ volatile unsigned int PulserNPulses;
  /* 0x008C-0x008F */ volatile unsigned int PulserStart;
  /* 0x0090-0x0093 */ volatile unsigned int PulserDone;
  /* 0x0094-0x00FF */          unsigned int Reserved1[(0x0100-0x0094)/4];
  /* 0x0100-0x0103 */ volatile unsigned int ScalerLatch;
  /* 0x0104-0x018F */ volatile unsigned int Scalers[35];
  /* 0x0190-0x01FF */          unsigned int Reserved2[(0x0200-0x0190)/4];
} Sd_regs;

/* Trigger Peripheral: Trigger configuration, status */
typedef struct
{
  /* 0x0000-0x0003 */ volatile unsigned int Ctrl;
  /* 0x0004-0x0010 */          unsigned int Reserved0[(0x0014-0x0004)/4];
  /* 0x0014-0x0017 */ volatile unsigned int SumHistThr;
  /* 0x0018-0x001B */ volatile unsigned int SumHistWindow;
  /* 0x001C-0x0023 */ volatile unsigned int Reserved1[(0x0024-0x001C)/4];
  /* 0x0024-0x0027 */ volatile unsigned int SumHistData;
  /* 0x0028-0x00FF */          unsigned int Reserved2[(0x0100-0x0028)/4];
} Trg_regs;

/* Serdes Peripheral: Fiber & VXS serdes controls and monitors */
typedef struct
{
  /* 0x0000-0x0003 */ volatile unsigned int Ctrl;
  /* 0x0004-0x0007 */ volatile unsigned int CtrlTile0;
  /* 0x0008-0x000B */ volatile unsigned int CtrlTile1;
  /* 0x000C-0x000F */ volatile unsigned int DrpCtrl;
  /* 0x0010-0x0013 */ volatile unsigned int Status;
  /* 0x0014-0x0017 */ volatile unsigned int DrpStatus;
  /* 0x0018-0x001B */ volatile unsigned int ErrTile0;
  /* 0x001C-0x001F */ volatile unsigned int ErrTile1;
  /* 0x0020-0x0023 */ volatile unsigned int CrateId;
  /* 0x0024-0x002F */          unsigned int Reserved0[(0x0030-0x0024)/4];
  /* 0x0030-0x0033 */ volatile unsigned int MonCtrl;
  /* 0x0034-0x0037 */ volatile unsigned int MonStatus;
  /* 0x0038-0x003F */          unsigned int Reserved1[(0x0040-0x0038)/4];
  /* 0x0040-0x004B */ volatile unsigned int MonMask[3];
  /* 0x004C-0x005F */          unsigned int Reserved2[(0x0060-0x004C)/4];
  /* 0x0060-0x006B */ volatile unsigned int MonVal[3];
  /* 0x006C-0x007F */          unsigned int Reserved3[(0x0080-0x006C)/4];
  /* 0x0080-0x0083 */ volatile unsigned int MonThr[1];
  /* 0x0084-0x008F */          unsigned int Reserved4[(0x0090-0x0084)/4];
  /* 0x0090-0x009B */ volatile unsigned int MonData[3];
  /* 0x009C-0x00FF */          unsigned int Reserved5[(0x0100-0x009C)/4];
} Serdes_regs;

/* EB: Adr32M bits */
#define SSP_EB_FIRST_BOARD  (1<<26)
#define SSP_EB_LAST_BOARD   (1<<27)

#define SSP_DUMMY_DATA             0xf800fafa

/* Event Builder */
typedef struct
{
  /* 0x0000-0x0003 */ volatile unsigned int Lookback;
  /* 0x0004-0x0007 */ volatile unsigned int WindowWidth;
  /* 0x0008-0x000B */ volatile unsigned int BlockCfg;
  /* 0x000C-0x000F */ volatile unsigned int AD32;
  /* 0x0010-0x0013 */ volatile unsigned int Adr32M;
  /* 0x0014-0x0017 */ volatile unsigned int Interrupt;
  /* 0x0018-0x001B */ volatile unsigned int ReadoutCfg;
  /* 0x001C-0x001F */ volatile unsigned int ReadoutStatus;
  /* 0x0020-0x0023 */ volatile unsigned int FifoBlockCnt;
  /* 0x0024-0x0027 */ volatile unsigned int FifoWordCnt;
  /* 0x0028-0x002B */ volatile unsigned int FifoEventCnt;
  /* 0x002C-0x00FF */          unsigned int Reserved1[(0x0100-0x002C)/4];
} EB_regs;

/* SSP memory structure */
typedef struct
{
  /* 0x0000-0x00FF */ SspCfg_regs  Cfg;
  /* 0x0100-0x01FF */ Clk_regs     Clk;
  /* 0x0200-0x03FF */ Sd_regs      Sd;
  /* 0x0400-0x04FF */ Trg_regs     Trg;
  /* 0x0500-0x0FFF */ unsigned int Reserved0[(0x1000-0x0500)/4];
  /* 0x1000-0x19FF */ Serdes_regs  Ser[10];
  /* 0x1A00-0xFFFF */ unsigned int Reserved1[(0x10000-0x1A00)/4];
  /* 0x2000-0x20FF */ EB_regs      EB;
} SSP_regs;


/* Sd_regs->SrcSel[] IDs */
#define SD_SRC_LVDSOUT0			0
#define SD_SRC_LVDSOUT1			1
#define SD_SRC_LVDSOUT2			2
#define SD_SRC_LVDSOUT3			3
#define SD_SRC_LVDSOUT4			4
#define SD_SRC_GPIO0			5
#define SD_SRC_GPIO1			6
#define SD_SRC_P2_LVDSOUT0		7
#define SD_SRC_P2_LVDSOUT1		8
#define SD_SRC_P2_LVDSOUT2		9
#define SD_SRC_P2_LVDSOUT3		10
#define SD_SRC_P2_LVDSOUT4		11
#define SD_SRC_P2_LVDSOUT5		12
#define SD_SRC_P2_LVDSOUT6		13
#define SD_SRC_P2_LVDSOUT7		14
#define SD_SRC_TRIG			15
#define SD_SRC_SYNC			16

#define SD_SRC_NUM			17

/* Sd_regs->SrcSel[] values */
#define SD_SRC_SEL_0		0
#define SD_SRC_SEL_1		1
#define SD_SRC_SEL_SYNC		2
#define SD_SRC_SEL_TRIG1	3
#define SD_SRC_SEL_TRIG2	4
#define SD_SRC_SEL_LVDSIN0	5
#define SD_SRC_SEL_LVDSIN1	6
#define SD_SRC_SEL_LVDSIN2	7
#define SD_SRC_SEL_LVDSIN3	8
#define SD_SRC_SEL_LVDSIN4	9
#define SD_SRC_SEL_P2LVDSIN0	10
#define SD_SRC_SEL_P2LVDSIN1	11
#define SD_SRC_SEL_P2LVDSIN2	12
#define SD_SRC_SEL_P2LVDSIN3	13
#define SD_SRC_SEL_P2LVDSIN4	14
#define SD_SRC_SEL_P2LVDSIN5	15
#define SD_SRC_SEL_P2LVDSIN6	16
#define SD_SRC_SEL_P2LVDSIN7	17
#define SD_SRC_SEL_PULSER	18
#define SD_SRC_SEL_BUSY		19
#define SD_SRC_SEL_TRIGGER0	20
#define SD_SRC_SEL_TRIGGER1	21
#define SD_SRC_SEL_TRIGGER2	22
#define SD_SRC_SEL_TRIGGER3	23
#define SD_SRC_SEL_TRIGGER4	24
#define SD_SRC_SEL_TRIGGER5	25
#define SD_SRC_SEL_TRIGGER6	26
#define SD_SRC_SEL_TRIGGER7	27

#define SD_SRC_SEL_NUM		28

#define SD_SRC_SEL_MASK         0x0000001F

/* Sd_regs->Scalers[] IDs */
#define SD_SCALER_SYSCLK	0
#define SD_SCALER_GCLK		1
#define SD_SCALER_SYNC		2
#define SD_SCALER_TRIG1		3
#define SD_SCALER_TRIG2		4
#define SD_SCALER_GPIO0		5
#define SD_SCALER_GPIO1		6
#define SD_SCALER_LVDSIN0	7
#define SD_SCALER_LVDSIN1	8
#define SD_SCALER_LVDSIN2	9
#define SD_SCALER_LVDSIN3	10
#define SD_SCALER_LVDSIN4	11
#define SD_SCALER_LVDSOUT0	12
#define SD_SCALER_LVDSOUT1	13
#define SD_SCALER_LVDSOUT2	14
#define SD_SCALER_LVDSOUT3	15
#define SD_SCALER_LVDSOUT4	16
#define SD_SCALER_BUSY		17
#define SD_SCALER_BUSYCYCLES	18
#define SD_SCALER_P2_LVDSIN0	19
#define SD_SCALER_P2_LVDSIN1	20
#define SD_SCALER_P2_LVDSIN2	21
#define SD_SCALER_P2_LVDSIN3	22
#define SD_SCALER_P2_LVDSIN4	23
#define SD_SCALER_P2_LVDSIN5	24
#define SD_SCALER_P2_LVDSIN6	25
#define SD_SCALER_P2_LVDSIN7	26
#define SD_SCALER_P2_LVDSOUT0	27
#define SD_SCALER_P2_LVDSOUT1	28
#define SD_SCALER_P2_LVDSOUT2	29
#define SD_SCALER_P2_LVDSOUT3	30
#define SD_SCALER_P2_LVDSOUT4	31
#define SD_SCALER_P2_LVDSOUT5	32
#define SD_SCALER_P2_LVDSOUT6	33
#define SD_SCALER_P2_LVDSOUT7	34

#define SD_SCALER_NUM			35

#define SD_PULSER_DONE			0x1
#define SD_PULSER_FREQ_MIN		0.01
#define SD_PULSER_FREQ_MAX		25E6

#define CLK_CTRL_DRPDWE				0x00200000
#define CLK_CTRL_DRPDEN				0x00400000
#define CLK_CTRL_SERDES_MASK                    0x03000000
#define CLK_CTRL_SERDES_DISABLED                (0<<24)
#define CLK_CTRL_SERDES_VXS                     (1<<24)
#define CLK_CTRL_SERDES_P2                      (2<<24)
#define CLK_CTRL_SERDES_LOCAL                   (3<<24)
#define CLK_CTRL_LOGIC_MASK                     0x0C000000
#define CLK_CTRL_LOGIC_DISABLED                 (0<<26)
#define CLK_CTRL_LOGIC_VXS                      (1<<26)
#define CLK_CTRL_LOGIC_P2                       (2<<26)
#define CLK_CTRL_LOGIC_LOCAL                    (3<<26)
#define CLK_CTRL_GCLKRST			0x80000000

#define CLK_STATUS_DRPRDY			0x00010000
#define CLK_STATUS_GCLKLOCKED   		0x00020000

#define SSPCFG_SPI_NCSSET			0x00000100
#define SSPCFG_SPI_NCSCLR			0x00000200
#define SSPCFG_SPI_START			0x00000400
#define SSPCFG_SPI_DONE				0x00000800

#define TRG_CTRL_FIBER_EN0			0x00000001
#define TRG_CTRL_FIBER_EN1			0x00000002
#define TRG_CTRL_FIBER_EN2			0x00000004
#define TRG_CTRL_FIBER_EN3			0x00000008
#define TRG_CTRL_FIBER_EN4			0x00000010
#define TRG_CTRL_FIBER_EN5			0x00000020
#define TRG_CTRL_FIBER_EN6			0x00000040
#define TRG_CTRL_FIBER_EN7			0x00000080

#define TRG_CTRL_GTPSRC_FIBER0	(0 << 24)
#define TRG_CTRL_GTPSRC_FIBER1	(1 << 24)
#define TRG_CTRL_GTPSRC_FIBER2	(2 << 24)
#define TRG_CTRL_GTPSRC_FIBER3	(3 << 24)
#define TRG_CTRL_GTPSRC_FIBER4	(4 << 24)
#define TRG_CTRL_GTPSRC_FIBER5	(5 << 24)
#define TRG_CTRL_GTPSRC_FIBER6	(6 << 24)
#define TRG_CTRL_GTPSRC_FIBER7	(7 << 24)
#define TRG_CTRL_GTPSRC_SUM	(8 << 24)



#define TRG_CTRL_GTPSRC_NUM	9

#define TRG_SUMHISTCTRL_EN	0x00000001

#define TRG_SUMHISTWINDOW_NSA_MASK  0x00FF0000
#define TRG_SUMHISTWINDOW_NSB_MASK  0x000000FF

#define SSP_SER_FIBER0		0
#define SSP_SER_FIBER1		1
#define SSP_SER_FIBER2		2
#define SSP_SER_FIBER3		3
#define SSP_SER_FIBER4		4
#define SSP_SER_FIBER5		5
#define SSP_SER_FIBER6		6
#define SSP_SER_FIBER7		7
#define SSP_SER_VXS0		8
#define SSP_SER_VXSGTP		9

#define SSP_SER_NUM		10

#define SER_CRATEID_MASK        0x0000FFFF

#define SSP_SER_CTRL_POWERDN	0x00000001
#define SSP_SER_CTRL_GTXRST	0x00000002
#define SSP_SER_CTRL_LINKRST	0x00000200
#define SSP_SER_CTRL_ERRCNT_RST	0x00000400
#define SSP_SER_CTRL_ERRCNT_EN	0x00000800

#define SSP_SER_STATUS_HARDERR(x)       (1<<x)
#define SSP_SER_STATUS_LANEUP(x)        (1<<(x+4))
#define SSP_SER_STATUS_POL_REVERSED(x)  (1<<(x+8))
#define SSP_SER_STATUS_CHUP             (1<<12)
#define SSP_SER_STATUS_TXLOCK           (1<<13)
#define SSP_SER_STATUS_SRCRDYN          (1<<14)

#define SSP_SER_MON_CTRL_RESET_CRATEID  (1<<0)

#define SYSCLK_FREQ   50000000
#define GCLK_FREQ     250000000

#define SSP_CLKSRC_DISABLED             0
#define SSP_CLKSRC_SWB			1
#define SSP_CLKSRC_P2			2
#define SSP_CLKSRC_LOCAL		3
#define SSP_CLKSRC_NUM                  4

#define SSP_CFG_BOARDID			0x53535020	// "SSP "

#define SSP_CFG_FIRMWAREREV_MASK           0x0000FFFF
#define SSP_CFG_FIRMWAREREV_MAJOR_MASK     0x0000FF00
#define SSP_CFG_FIRMWAREREV_MINOR_MASK     0x000000FF
#define SSP_CFG_SSPTYPE_MASK               0x007F0000
#define SSP_CFG_TEST_RELEASE_MASK          0x00800000
#define SSP_CFG_SLOTID_MASK                0x1F000000

#define SPI_CMD_RD		       	0x03
#define SPI_CMD_GETID			0x9F
#define SPI_CMD_GETSTATUS		0xD7
#define SPI_CMD_WRBUF1			0x84
#define SPI_CMD_PGBUF1ERASE	0x83

#define SPI_BYTE_LENGTH			8*1024*1024
#define SPI_MFG_ATMEL			0x1F
#define SPI_DEVID_AT45DB642D	0x2800

/* sspInit iFlag options */
#define SSP_INIT_MODE_DISABLED         0
#define SSP_INIT_MODE_P2               1
#define SSP_INIT_MODE_FP               2
#define SSP_INIT_MODE_VXS              3
#define SSP_INIT_MODE_MASK             0x3
#define SSP_INIT_SKIP_SOURCE_SETUP     (1<<12)
#define SSP_INIT_SKIP_FIRMWARE_CHECK   (1<<13)
#define SSP_INIT_NO_INIT               (1<<14)
#define SSP_INIT_USE_ADDRLIST          (1<<15)
#define SSP_INIT_FIBER0_ENABLE         (1<<16)
#define SSP_INIT_FIBER1_ENABLE         (1<<17)
#define SSP_INIT_FIBER2_ENABLE         (1<<18)
#define SSP_INIT_FIBER3_ENABLE         (1<<19)
#define SSP_INIT_FIBER4_ENABLE         (1<<20)
#define SSP_INIT_FIBER5_ENABLE         (1<<21)
#define SSP_INIT_FIBER6_ENABLE         (1<<22)
#define SSP_INIT_FIBER7_ENABLE         (1<<23)
#define SSP_INIT_FIBER_ENABLE_MASK     0x00FF0000

#define SSP_INIT_GTP_FIBER0_ENABLE     (0<<24)
#define SSP_INIT_GTP_FIBER1_ENABLE     (1<<24)
#define SSP_INIT_GTP_FIBER2_ENABLE     (2<<24)
#define SSP_INIT_GTP_FIBER3_ENABLE     (3<<24)
#define SSP_INIT_GTP_FIBER4_ENABLE     (4<<24)
#define SSP_INIT_GTP_FIBER5_ENABLE     (5<<24)
#define SSP_INIT_GTP_FIBER6_ENABLE     (6<<24)
#define SSP_INIT_GTP_FIBER7_ENABLE     (7<<24)
#define SSP_INIT_GTP_FIBERSUM_ENABLE   (8<<24)
#define SSP_INIT_GTP_FIBER_ENABLE_MASK 0xFF000000

/* sspStatus rflag options */
#define SSP_STATUS_SHOWREGS   (1<<0)

#define SSP_GSTATUS_MODULES   (1<<0)
#define SSP_GSTATUS_PORTS     (1<<1)

/* Global arrays of strings of names of ports/signals */
extern const char *ssp_ioport_names[SD_SRC_NUM];
extern const char *ssp_signal_names[SD_SRC_SEL_NUM];
extern const char *ssp_gtpsrc_names[TRG_CTRL_GTPSRC_NUM];
extern const char *ssp_scaler_name[SD_SCALER_NUM];
extern const char *ssp_clksrc_name[SSP_CLKSRC_NUM];
extern const char *ssp_serdes_names[SSP_SER_NUM];

/* SSP configuration */
int  sspInit(unsigned int addr, unsigned int addr_inc, int nfind, int iFlag);
void sspCheckAddresses(int id);
int  sspSlot(unsigned int i);
int  sspSetMode(int id, int iFlag, int pflag);
int  sspStatus(int id, int rflag);
void sspGStatus(int rflag);
int  sspResetCrateId(int id, int port);
int  sspPrintCrateId(int id, int port);

/* SSP CLK Functions */
int  sspSetClkSrc(int id, int src);
int  sspGetClkStatus(int id);
int  sspGetClkSrc(int id, int pflag);

/* SSP SD.IO Functions */
int  sspSetIOSrc(int id, int ioport, int signal);
int  sspGetIOSrc(int id, int ioport, int pflag);
void sspPrintIOSrc(int id, int pflag);

/* Pulser routines */
int  sspTriggerSetup(int id, int fiber_mask, int gtp_src, int pflag);
int  sspPulserStatus(int id);
void sspPulserStart(int id);
void sspPulserSetup(int id, float freq, float duty, unsigned int npulses);

/* SSP serdes */
void sspPortEnable(int id, int mask, int pflag);
void sspPortResetErrorCount(int id, int mask);
int  sspPortGetErrorCount(int id, int port, int lane);
void sspPortPrintStatus(int id, int mask);
int  sspGetConnectedFiberMask(int id);
int  sspGetCrateID(int id, int port);
void sspSerdesEnable(int id, int mask, int pflag);
int  sspSerdesGetErrorCount(int id, int ser, int lane);
void sspSerdesResetErrorCount(int id, int mask);
void sspSerdesPrintStatus(int id, int mask);

/* Scaler routines */
void sspPrintScalers(int id);

/* Firmware update routines */
int  sspFirmwareUpdateVerify(int id, const char *filename);
int  sspFirmwareUpdate(int id, const char *filename);
int  sspFirmwareRead(int id, const char *filename);
int  sspFirmwareVerify(int id, const char *filename);

/* Firmware update utility routines */
int  sspGetSerialNumber(int id, char *mfg, int *sn);
unsigned int sspGetFirmwareVersion(int id);

/* SSP Readout and Readout configuration */
unsigned int sspSlotMask();
int  sspReadBlock(int id, unsigned int *data, int nwrds, int rflag);
int  sspBReady(int id);
unsigned int sspGBReady();
int  sspSetBlockLevel(int id, int block_level);
int  sspGetBlockLevel(int id);
int  sspEnableBusError(int id);
int  sspDisableBusError(int id);
int  sspGetBusError(int id);
int  sspSetWindowWidth(int id, int window_width);
int  sspGetWindowWidth(int id);
int  sspSetWindowOffset(int id, int window_offset);
int  sspGetWindowOffset(int id);

#endif
