/*******************************************************************************
 *  Floppy Dsik Controller for BBK/YuXing
 *
 *  Author:  fanoble <87430545@qq.com>
 *
 *  Create:   Nov 6, 2022, by fanoble
 *******************************************************************************
 */

#ifndef __NES_FDC_H__
#define __NES_FDC_H__

#define FDC_IMAGE_ADDRESS 0x80E00000

typedef struct tagFDC FDC;

//class FDC {
//public:
//	FDC();
//	~FDC();
	void FdcInit(FDC* thiz);
	void FdcDone(FDC* thiz);

	// Basic
	int FdcLoadDiskImage(FDC* thiz, const char* fname);
	int FdcSaveDiskImage(FDC* thiz);
	int FdcIsDirty(FDC* thiz);// { return bDirty; }
	int FdcIsPresent(FDC* thiz);// { return (pDiskImage != 0); }

	// IO: nPort: 0-7
	unsigned char FdcRead(FDC* thiz, unsigned char nPort);
	void FdcWrite(FDC* thiz, unsigned char nPort, unsigned nData);

	// to do
	int FdcEject(FDC* thiz);// { return 0; }
	int FdcSetWriteProtect(FDC* thiz, int bWP);// { return 0; }

	int FdcCheckIRQ(FDC* thiz);// { return bFdcIrq; }

	void FdcHardReset(FDC* thiz);
	void FdcSoftReset(FDC* thiz);

	// Commands
	void FdcNop(FDC* thiz);
	void FdcReadTrack(FDC* thiz);
	void FdcSpecify(FDC* thiz);
	void FdcSenseDriveStatus(FDC* thiz);
	void FdcWriteData(FDC* thiz);
	void FdcReadData(FDC* thiz);
	void FdcRecalibrate(FDC* thiz);
	void FdcSenseIntStatus(FDC* thiz);
	void FdcWriteDeletedData(FDC* thiz);
	void FdcReadID(FDC* thiz);
	void FdcReadDeletedData(FDC* thiz);
	void FdcFormatTrack(FDC* thiz);
	void FdcSeek(FDC* thiz);
	void FdcScanEqual(FDC* thiz);
	void FdcScanLowOrEqual(FDC* thiz);
	void FdcScanHighOrEqual(FDC* thiz);

	typedef struct tagFDC_CMD_DESC {
		unsigned char bWLength;
		unsigned char bRLength;
		void	(*pFun)(FDC*);
	} FDC_CMD_DESC;

//protected:
struct tagFDC {
	// for FDC
	int		bFdcIrq;
	int		bFdcHwReset;
	int		bFdcSoftReset;

	int		bFdcDataBytes;

	int		bFdcDmaInt;
	int		nFdcDrvSel;
	int		nFdcMotor;

#define FDC_MS_BUSYS0		0x01	// FDD0 in SEEK mode
#define FDC_MS_BUSYS1		0x02	// FDD1 in SEEK mode
#define FDC_MS_BUSYS2		0x04	// FDD2 in SEEK mode
#define FDC_MS_BUSYS3		0x08	// FDD3 in SEEK mode
#define FDC_MS_BUSYRW		0x10	// Read or Write in progress
#define FDC_MS_EXECUTION	0x20	// Execution Mode
#define FDC_MS_DATA_IN		0x40	// Data input or output
#define FDC_MS_RQM			0x80	// Request for Master, Ready
	unsigned char nFdcMainStatus;

#define FDC_S0_US0			0x01	// Unit Select 0
#define FDC_S0_US1			0x02	// Unit Select 1
#define FDC_S0_HD			0x04	// Head Address
#define FDC_S0_NR			0x08	// Not Ready
#define FDC_S0_EC			0x10	// Equipment Check
#define FDC_S0_SE			0x20	// Seek End
#define FDC_S0_IC0			0x40	// Interrupt Code
#define FDC_S0_IC1			0x80	// NT/AT/IC/XX

#define FDC_S1_MA			0x01	// Missing Address Mark
#define FDC_S1_NW			0x02	// Not Writable
#define FDC_S1_ND			0x04	// No Data
#define FDC_S1_OR			0x10	// Over Run
#define FDC_S1_DE			0x20	// Data Error
#define FDC_S1_EN			0x80	// End of Cylinder

#define FDC_S2_MD			0x01	// Missing Address Mark in Data Field
#define FDC_S2_BC			0x02	// Bad Cylinder
#define FDC_S2_SN			0x04	// Scan Not Satisfied
#define FDC_S2_SH			0x08	// Scan Equal Hit
#define FDC_S2_WC			0x10	// Wrong Cylinder
#define FDC_S2_DD			0x20	// Data Error in Data Field
#define FDC_S2_CM			0x40	// Control Mark

#define FDC_S3_US0			0x01	// Unit Select 0
#define FDC_S3_US1			0x02	// Unit Select 1
#define FDC_S3_HD			0x04	// Side Select
#define FDC_S3_TS			0x08	// Two Side
#define FDC_S3_T0			0x10	// Track 0
#define FDC_S3_RY			0x20	// Ready
#define FDC_S3_WP			0x40	// Write Protect
#define FDC_S3_FT			0x80	// Fault
	unsigned char nFDCStatus[4];

#define FDC_CC_MASK						0x1F
#define FDC_CF_MT						0x80
#define FDC_CF_MF						0x40
#define FDC_CF_SK						0x20

#define FDC_CC_READ_TRACK				0x02
#define FDC_CC_SPECIFY					0x03
#define FDC_CC_SENSE_DRIVE_STATUS		0x04
#define FDC_CC_WRITE_DATA				0x05
#define FDC_CC_READ_DATA				0x06
#define FDC_CC_RECALIBRATE				0x07
#define FDC_CC_SENSE_INTERRUPT_STATUS	0x08
#define FDC_CC_WRITE_DELETED_DATA		0x09
#define FDC_CC_READ_ID					0x0A
#define FDC_CC_READ_DELETED_DATA		0x0C
#define FDC_CC_FORMAT_TRACK				0x0D
#define FDC_CC_SEEK						0x0F
#define FDC_CC_SCAN_EQUAL				0x11
#define FDC_CC_SCAN_LOW_OR_EQUAL		0x19
#define FDC_CC_SCAN_HIGH_OR_EQUAL		0x1D
	unsigned char bFdcCycle;
	unsigned char bFdcLastCommand;
	unsigned char bFdcCommands[10];
	unsigned char bFdcResults[8];
	const FDC_CMD_DESC* pFdcCmd;

#define FDC_PH_IDLE						0
#define FDC_PH_COMMAND					1
#define FDC_PH_EXECUTION				2
#define FDC_PH_RESULT					3
	unsigned char bFdcPhase;

	unsigned char  nFdcCylinder;
	unsigned char* pFdcDataPtr;

	int			   nCurrentLBA;

	int            bDirty;
	unsigned char* pDiskImage;

	int            nDiskSize;
	char           szDiskName[2048];

//private:
};

#endif

/*******************************************************************************
                           E N D  O F  F I L E
*******************************************************************************/
