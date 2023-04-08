/**
 * 
 * \file main.h
 * \brief The header file for the main part of the program.
 *
 * 
 * $Id: main.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __MAIN_H__
# define __MAIN_H__

//---------------------------------------------------------------
// Libraries
//---------------------------------------------------------------
// Enable Visual Styles
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
#ifndef _WIN32_DCOM
#define _WIN32_DCOM
#endif 

typedef unsigned char			UINT8;

# define REDRAW_TIMER_PERIOD	50							// Scope's update interval in milliseconds

// Drawing constants
# define CONVERSION_FACTOR		1000000.0f					// uV/V

//
// GUI definitions
//
// battery status indicater constants
# define BATLOW_TIMER_PERIOD	1000						// battery low flash time-period (in milliseconds)

// Toolbar constants
#define TB_NBUTTONS				11							// number of toolbar buttons

// Rebar constants
# define RBB_SENSITIVITY_WIDTH	1.8							// width of the sensitivity band (in logical in)
# define RBB_TIMEBASE_WIDTH		1.8							// width of the timebase band (in logical in)
# define RBB_LP_WIDTH			1.8							// width of the low-pass filter band (in logical in)
# define RBB_STRSTA_WIDTH		1.8							// width of the streaming status band (in logical in)

// Annotiation display window constants
# define ADW_STARTPOS_X			200
# define ADW_STARTPOS_Y			120

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
// Annotations enumeration
typedef enum {CommunicationFailure,
			  CommunicationResume,
			  CoordinatorRemoved,
			  CoordinatorInserted,
			  Now,
			  Regular
} AnnotationType;

// Stop codes used by the IDM_SAMPLE_STOP "function"
typedef enum
{
	Stop_Normal = 0,			// user-started stop
	Stop_Abort,					// 1: recording needs to be aborted due to major error; no EDF+ file saved
	Stop_Cancel					// 2: recording needs to be stopped due to minor error; EDF+ file saved
} StopCode;

// return codes used when checking for the existence of the coordinator and of the measurement device
typedef enum
{
	WEEGSystem_INVALID = -1,	// invalid value used to initialize variables
	WEEGSystem_OK = 0,			// both the coordinator and measurement device were found and are functioning properly
	WEEGSystem_COORD_NCOM,		// 1 = more than 1 COM port found for coordinator (should not happend under normal circumstances)
	WEEGSystem_COORD_COM,		// 2 = could not open the WEEG COM port
	WEEGSystem_COORD_DC,		// 3 = the coordinator is not connected to the PC
	WEEGSystem_COORD_NA,		// 4 = the coordinator is not answering to the polling request
	WEEGSystem_MEASDEV_CONFIG,	// 5 = the measurement device could not be configured
	WEEGSystem_MEASDEV_TIMEOUT,	// 6 = the measurement device is not answering
	WEEGSystem_MEASDEV_CHECKSUM	// 7 = the measurement device is communicating but there are a lot of checksum errors
} WEEGSystemCheckCode;

// Signal-type enumeration
typedef enum {SM_EEG,
			  SM_aEEG} SignalMode;

//---------------------------------------------------------------------------
//   								Constants
//---------------------------------------------------------------------------
// EDF File Editor executable name
const TCHAR * EDFFILEEDITOREXE = TEXT("EDFFileEditor.exe");
const TCHAR * LIBRARIESRTF = TEXT("libraries.rtf");
const TCHAR * LICENSESTXT = TEXT("licenses.txt");

// invalid data samples
const short INVALID_ACC_SAMPLE	= 0x01FF;
const short INVALID_EEG_SAMPLE = 0x7FFF;

//**************
// GUI Constants
//**************
// statusbar status strings
const TCHAR * mc_strStateStrs [] = {TEXT("Stopped."), TEXT("Initializing..."), TEXT("Sampling.")};

// statusbar battery state strings
const TCHAR * mc_strBatteryStates [] = {TEXT("Battery OK"), TEXT("Battery LOW!")};

// Scale factors for EEG channel rendering (uV/cm)
const int mc_intSensitivityFactors[NSENSFACTORS] = {10, 20, 30, 50, 70, 100, 150, 200, 300, 500, 700, 1000, 2000, 5000};

// Scale factors for EEG channel rendering (mm/sec)
const float mc_fltTimebaseFactors[NTBFACTORS] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 5.0f, 10.0f};

//*******************
// Hardware Constants
//*******************
// Samples per sampled channel as a function of channel count,
// i.e. when there are 6 sampled channels, one packet contains
// 8 groups of 6 channels
const int mc_intSampleLengths [9] = {0, 50, 25, 16, 12, 10, 8, 7, 6};

// number of valid packets that must be received from the measurement device
// during the system check
const unsigned int mc_uintNTestPacketsRequired = 3;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
static LRESULT APIENTRY		AnnotationsWndProc (HWND hWnd, UINT message, UINT wParam, LONG lParam);
static LRESULT CALLBACK		ASCIIMaskedEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK		Dialog_About (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static DWORD CALLBACK		Dialog_About_EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);
static BOOL CALLBACK		Dialog_Annotation (HWND hWndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK		Dialog_InternetConnectionConfig (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK		Dialog_PatientInfo (HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void					Dialog_PatientInfo_SetNCharsLeft(HWND hwndDlg);
static BOOL CALLBACK		Dialog_RecordingInfo (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam);
static void					Dialog_RecordingInfo_SetNCharsLeft1 (HWND hwndDlg);
static void					Dialog_RecordingInfo_SetNCharsLeft2 (HWND hwndDlg);
static BOOL CALLBACK		Dialog_ScreenSize (HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void					Dialog_ScreenSize_DrawRuler(HDC hdcRuler, RECT rcRuler, int intHorizontalResolution, int intVerticalResolution, int intHorizontalSize, int intVerticalSize, BOOL blnVertical);
static BOOL CALLBACK		Dialog_Settings (HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static int CALLBACK			Dialog_Settings_SHBrowseForFolder(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);
static BOOL CALLBACK		Dialog_SSHConfig (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK		Dialog_StreamingConfig (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam);
static DWORD				GUI_Init(HWND hWnd, GUIElements * pgui);
static void					GUI_DisplayAnnotationsContextMenu(HWND hwndOwner, int intXPos, int intYPos, GUIElements gui, HWND hwndMainWindow);
static void					GUI_DisplayPropertySheet (HWND hwndOwner, GUIElements gui);
static void					GUI_SetEnabledCommands(HWND hwndOwner, BOOL blnIsRecording, GUIElements gui);
static void					GUI_SetFullScreenMode(HWND hWnd, GUIElements gui);
static void					GUI_SetStatusBarPartSize(HWND hwndStatusBar, int intNewClientWidth);
static LRESULT APIENTRY		MainWndProc (HWND hWnd, UINT message, UINT wParam, LONG lParam);
static BOOL					Sample_DetectedCommunicationFailure(SampleDataRecord * pdrCurrentDataRecord, SampleThreadData * pstd, HWND hwndMainWindow);
static SampleThreadState	Sample_GetMainFSMState(void);
static BOOL					Sample_ProcessDataPacket (tPacket_DATA * ptpMeasurementData, SampleDataRecord * pdrCurrentDataRecord, SampleThreadData * pstd, HWND hwndMainWindow);
static void					Sample_RecordingFSM(SampleThreadData * pstd, HWND hwndMainWnd);
static void					Sample_SimulationFSM(SampleThreadData * pstd, HWND	hwndMainWnd);
static BOOL					Sample_StoreAndTransmitDataRecord(SampleDataRecord * pdrCurrentDataRecord, SampleThreadData * pstd);
static long WINAPI			Sample_Thread (LPARAM lParam);
static void					Sample_WEEGSystemCheckFSM(SampleThreadData * pstd);
# endif