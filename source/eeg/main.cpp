/**
 * 
 * \file main.cpp
 * \brief This file implements the whole program to measure data from WEEG2/WEEG3 
 * device, and to save it in EDF+ file.
 *
 * 
 * $Id: main.cpp 78 2013-02-21 17:23:21Z jakab $
 */
//---------------------------------------------------------------------------
//   					  Windows-related definitions
//---------------------------------------------------------------------------
// this macro prevents windows.h from including winsock.h for version 1.1
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// library requires at least Windows XP SP2
#define WINVER			0x0502
#define _WIN32_WINNT	0x0502
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//---------------------------------------------------------------------------
//   								Includes
//---------------------------------------------------------------------------
// Windows libaries
# include <objbase.h>
# include <shlobj.h>
# include <shlwapi.h>
# include <windows.h>
# include <CommDlg.h>
# include <commctrl.h>
# include <richedit.h>
# include <wincred.h>
# include <wincrypt.h>
# include <dbt.h>
# include <windowsx.h>
# include <Shellapi.h>
# include <Ras.h>
# include <RasError.h>

// CRT libraries
# include <eh.h>
# include <io.h>
# include <stdarg.h>
# include <stdio.h>
# include <tchar.h>
# include <time.h>

// custom libraries
# include <eegem_beep.h>
# include <libEDF.h>
# include <libmath.h>

// application headers
# include "globals.h"
# include "annotations.h"
# include "applog.h"
# include "config.h"
# include "edfPlus.h"
# include "graphics.h"
# include "linkedlist.h"
# include "resource.h"
# include "serialV4.h"
# include "sigproc.h"
# include "thread_stream.h"
# include "thread_storage.h"
# include "thread_sample.h"
# include "thread_upload.h"
# include "thread_WiFi.h"
# include "util.h"
# include "main.h"

//---------------------------------------------------------------------------
//									Libraries
//---------------------------------------------------------------------------
// Windows
#pragma comment(lib, "Rasapi32.lib")

// custom
#pragma comment(lib, "libEDF.lib")

//---------------------------------------------------------------------------
//   								Constants
//---------------------------------------------------------------------------
const SampleDataRecord			mc_sdrEmpty = {NULL, 0, NULL};		///< empty SampleDataRecord struct used to initialize all variables of this type

//---------------------------------------------------------------------------
//   								Global variables
//---------------------------------------------------------------------------
static BOOL						m_blnCommunicationBlackout, m_blnBatteryLow, m_blnBatteryLowBlink;
static BOOL						m_blnScreenSaverActive;
static BOOL						m_blnUploadComplete;
static CONFIGURATION			m_cfgConfiguration;
static HANDLE					m_hEDFPlusFile;
static HINSTANCE				m_hinMain;
static int						m_intNSamplesDatarecord;	// Number of samples in current data record
static PatientIdentification	m_piPatientInfo;
static RecordingIdentification	m_riRecordingInfo;
static short					** m_pshrSampleBuffer;														// Buffer for measurement data
static SignalMode				m_smCurrentSignalMode;
static unsigned int				m_uintNNewSamples;
static unsigned int				m_uintSampleBufferLength;

// Annotations
static char						m_strCurrentDRAnnotations[ANNOTATION_TOTAL_NCHARS];		///< string buffer that stores the annotations for the current data record (NOTE: has to be global variable since it is accessed by both the main and sampling threads)
static unsigned int				m_uintTimeKeepingTAL;									///< annotation time-stamp
static int						m_intNAnnotationsBuffered;								///< amount of annotations stored in m_strCurrentDRAnnotations
static int						m_intNNowAnnotations;									///< amount of 'Now' annotations since the start of the current recording

// WEEG-related variables
static WEEGSystemCheckCode		m_wsccCheckCode;

// Graphics engine
static BOOL						m_blnDrawAccelerometerTraces;
static double					m_dblEEGYScale;
double 							** m_pdblEEGDisplayBuffer;
double		 					** m_pdblAEEGDisplayBuffer;
double 							* m_pdblDisplayBufferTemp;
static unsigned int				m_uintEEGDisplayBufferID;											// m_dblEEGDisplayBuffer index from where new samples should be inserted
static unsigned int				m_uintEEGDisplayBufferLength;
static unsigned int				m_uintAEEGDisplayBufferID;											// m_dblEEGDisplayBuffer index from where new samples should be inserted
static unsigned int				m_uintAEEGDisplayBufferLength;
static unsigned int				m_uintNMaxSamples;

// GUI Variables
static BOOL						m_blnIsAnnotationsMenuDisplayed;
static BOOL						m_blnIsFullScreen;
static BOOL						m_blnPortraitOrientation;
static struct tm				m_tmRecordingTime;
static HWND						m_hwndAnnotationsDisplay;

// Statistics Variables
static long						m_lngNPacketsReceived;			// # packets received
static long						m_lngNPacketChecksumErrors;		// # packets received w/ wrong checksum
static long						m_intNPacketsLost;				// # packets lost
static int						m_intNDataRecords;				// Number of current data records

// multithreading variables
static SampleThreadState		m_stsCurrentSampleThreadState;	// Sampling thread's current mode
static HANDLE					m_hMutexAnnotation;				// mutex used to prevent the GUI and Sample threads from using the annotation buffer at the same time
static HANDLE					m_hMutexSampleBuffer;			// mutex used to prevent the GUI and Sample threads from using the data sample buffer at the same time

//---------------------------------------------------------------------------------------------------------------------------------
//   								Utility Functions
//---------------------------------------------------------------------------------------------------------------------------------
/**
 * \brief Tests whether the WEEG system is ready for recording.
 *
 * \param[out]	strErrorBuffer		pointer to NULL-terminated string that will contain the error code (if any)
 * \param[in]	uintErrorBufferLen	length of strErrorBuffer, in TCHARs
 * \param[in]	std					struct containing values passed to the sampling thread
 * \param[in]	gui					struct containing handles to the elements of the main window's GUI
 *
 * \return TRUE if system passed all tests and is ready for recording, FALSE otherwise.
 */
static BOOL main_CheckWEEGSystem(TCHAR * strErrorBuffer, unsigned int uintErrorBufferLen, SampleThreadData * pstd, GUIElements gui)
{
	BOOL blnSystemOK = FALSE;
	TCHAR strBuffer[128];

#ifdef _DEBUG
	// make sure buffer is actually as long as advertised
	memset(strErrorBuffer, 0xAE, uintErrorBufferLen*sizeof(TCHAR));
#endif

	// warn user that system check is about to begin by displaying message in status bar
	_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("WEEG system in progress..."));
	SendMessage(gui.hwndStatusBar, SB_SETTEXT, SB_ANNOTATIONS_PART, (LPARAM) strBuffer);
		
	// turn on testing and wait for completion
	pstd->Mode = SampleThreadMode_WEEGSystemCheck;
	SignalObjectAndWait(pstd->hevSampleThread_Start, pstd->hevSampleThread_Idle, INFINITE, FALSE);

	// check result of system check
	switch(m_wsccCheckCode)
	{
		case WEEGSystem_OK:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("WEEG system is ready."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("WEEG system is ready."));
			
			blnSystemOK = TRUE;
		break;
		
		// the coordinator is not connected to the PC
		case WEEGSystem_COORD_DC:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("The coordinator communication port could not be detected. Please ensure that the\nUSB driver is correctly installed and that the WEEG device is securely plugged into an USB port."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("The WEEG COM port could not be detected. Check: USB driver is installed; coordinator plugged in."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): The WEEG COM port could not be detected."), 0, TRUE);
		break;
		
		// more than 1 COM port found for coordinator (should not happen under normal circumstances)
		case WEEGSystem_COORD_NCOM:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("More than one possible WEEG COM port was detected. Please check that the correct COM port is selected in the 'Settings' window."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("More than one possible WEEG COM port was detected. Select correct COM port in 'Settings' window."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): More than one possible WEEG COM port was detected."), 0, TRUE);
		break;

		// could not open the WEEG COM port
		case WEEGSystem_COORD_COM:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("Could not open the WEEEG COM port. Please ensure that no other software is communicating\nwith the WEEG system."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Could not open the WEEEG COM port. Check: no other software is acccessing the WEEG system."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): Could not open the WEEEG COM port."), 0, TRUE);
		break;

		// the coordinator is not answering to the polling request
		case WEEGSystem_COORD_NA:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("Could not communicate with the WEEG coordinator. Please ensure that the\ncoordinator is securely plugged in."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Could not communicate with the WEEG coordinator. Check: coordinator plugged in."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): Could not communicate with the WEEG coordinator."), 0, TRUE);
		break;
		
		// the measurement device could not be configured
		case WEEGSystem_MEASDEV_CONFIG:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("The WEEG measurement device could not be configured.  Please check the following:\n- device is within range\n- device's batteries are full\n- device is turned on."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("The WEEG measurement device could not be configured. Check: device is turned on; device within range; batteries OK."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): The WEEG measurement device could not be configured."), 0, TRUE);
		break;

		// the measurement device is not answering
		case WEEGSystem_MEASDEV_TIMEOUT:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("Communication with the WEEG measurement device timed-out. Please check the following:\n- device is within range\n- device's batteries are full\n- device is turned on."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Communication with the WEEG measurement device timed-out. Check: device is turned on; device within range; batteries OK."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): Communication with the WEEG measurement device timed-out."), 0, TRUE);
		break;
		
		// the measurement device is communicating but there are a lot of checksum errors
		case WEEGSystem_MEASDEV_CHECKSUM:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("The measurement device is communicating, but there are a lot of data errors. Please check the following:\n- device is within range\n- device and coordinator are not close to sources of interference\n(e.g. microwave ovens, 2.4GHz phones, etc.)."));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Multiple checksum errors. Check: sources of interference; device within range."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): Multiple checksum errors."), 0, TRUE);
		break;
		
		// not supposed to occur
		default:
			if(uintErrorBufferLen > 0 && strErrorBuffer != NULL)
				_sntprintf_s(strErrorBuffer, uintErrorBufferLen, _TRUNCATE, TEXT("This ExistenceCheckCode is not supposed to occur. Check code!"));
			
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Weird ExistenceCheckCode."));
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("main_CheckWEEGSystem(): Unknown WEEG ExistenceCheckCode."), 0, TRUE);
	}
	
	// adjust the icon of the check system toolbar button depending on the result of the system check
	if(blnSystemOK)
		SendMessage(gui.hwndToolbar, TB_CHANGEBITMAP, IDM_SYSTEMCHECK, MAKELPARAM (8, 0)); 
	else
		SendMessage(gui.hwndToolbar, TB_CHANGEBITMAP, IDM_SYSTEMCHECK, MAKELPARAM (7, 0)); 

	// display results in status bar
	SendMessage(gui.hwndStatusBar, SB_SETTEXT, SB_ANNOTATIONS_PART, (LPARAM) strBuffer);
	
	return blnSystemOK;
}

/**
 * \brief Computes the maximum amount of possible recording time (in hours) based on the largest EDF+ file that can be stored in the given path.
 *
 * \param[in]	strPath						path where EDF+ file would be stored
 * \param[out]	uintAvailableRecordingTime	pointer to two-element unsigned int array where amount of free space will be stored (element 0 stores the hours and element 1 the minutes)
 *
 * \return ERROR_SUCCESS if successfull, system error code otherwise.
 */
static DWORD main_GetMaxAvailableRecordingTime(TCHAR * strPath, unsigned int * uintAvailableRecordingTime)
{
	UINT64 lngFreeBytesAvailable;
	unsigned int uintNBytesDataRecord = -1;
	
	// initialize return values
	uintAvailableRecordingTime[0] = 0;
	uintAvailableRecordingTime[1] = 0;

	// check if the given save path location is valid
	if(PathFileExists(strPath))
	{
		// retrieve amount of free disk space
		if(GetDiskFreeSpaceEx(strPath,(PULARGE_INTEGER) &lngFreeBytesAvailable, NULL, NULL))
		{
			uintNBytesDataRecord = m_cfgConfiguration.SamplingFrequency*(EEGCHANNELS + ACCCHANNELS)*sizeof(short) + ANNOTATION_TOTAL_NCHARS*sizeof(char);
			uintAvailableRecordingTime[0] = (unsigned int) (lngFreeBytesAvailable/(uintNBytesDataRecord*3600));
			uintAvailableRecordingTime[1] = (unsigned int) ((lngFreeBytesAvailable%(uintNBytesDataRecord*3600))/(uintNBytesDataRecord*60));
		}
		else
			return GetLastError();
	}
	else
		return GetLastError();
	
	return ERROR_SUCCESS;
}

/**
* \brief Generates the full path of the final EDF+ file.
*
* \param[in]	StartDateTime				date and time at which the recording of the temporary EDF+ file was started
* \param[in]	strTempEDFFilePath			pointer to NULL-terminated string containing the full path of the temporary EDF+ file
* \param[out]	strFinalEDFFilePath			pointer to NULL-terminated string where full path of the final EDF+ file is returned
* \param[in]	uintFinalEDFFilePathLen		size of strFinalEDFFilePath string, in characters
*/
static void main_GenerateFinalEDFFilePath(struct tm StartDateTime, TCHAR * strTempEDFFilePath, TCHAR * strFinalEDFFilePath, unsigned int uintFinalEDFFilePathLen)
{
	size_t				sztLength;
	TCHAR				strBuffer[256];
	TCHAR				strTemp[MAX_PATH + 1];
	unsigned int		i;

	// parse long folder path where EDF+ file will be stored
	sztLength = _tcslen(strTempEDFFilePath) + 1;				// +1 for terminating NULL character
	_stprintf_s(strTemp, sztLength, TEXT("%s"), strTempEDFFilePath);
	PathRemoveFileSpec(strTemp);
	PathAddBackslash(strTemp);

	// parse EDF+ file name
	_stprintf_s(strBuffer,
				sizeof(strBuffer)/sizeof(WORD),
				TEXT("%4d.%02d.%02d %02d-%02d"),
				StartDateTime.tm_year+1900, StartDateTime.tm_mon+1, StartDateTime.tm_mday, StartDateTime.tm_hour, StartDateTime.tm_min);
	
	// Parse EDF+ file path; algorithm loops until the parsed filename is not found to be in use
	i = 0;
	_stprintf_s(strFinalEDFFilePath,
				uintFinalEDFFilePathLen,
				TEXT("%s%s.edf"),
				strTemp, strBuffer);
	while(PathFileExists(strFinalEDFFilePath))
	{
		_stprintf_s(strFinalEDFFilePath,
					uintFinalEDFFilePathLen,
					TEXT("%s%s (%02d).edf"),
					strTemp, strBuffer, ++i);
	};
}

/**
 * \brief Adds an annotation to the annotation buffer that will be stored with the data record being recorded at the time the annotation was made. 
 *
 * Function called either by the main thread or by the sample thread.
 *
 * \param[in]	atAnnotationType	member of the AnnotationType enum indicating the type of annotation to be stored
 * \param[in]	intAnnotationId		additional annotation-specific (depends on atAnnotationType)
 * \param[in]	gui					struct containing handles to the elements of the main window's GUI
 * \param[in]	hwndMainWindow		handle to the main window
 */
static void main_InsertAnnotation(AnnotationType atAnnotationType, int intAnnotationId, GUIElements gui, HWND hwndMainWindow)
{
	char		strCAnnotation[ANNOTATION_MAX_CHARS + 1];	///< buffer that stores char version of the annotation
	char		strTemp[ANNOTATION_MAX_CHARS + 1];			///< temporary buffer used for converting TCHAR annotations stored stored in the CONFIGURATION structure to char strings (+1 for terminating NULL character)
	size_t		sztNCharsConverted;							///< amount of characters converted by the wcstombs_s() function
	struct tm	tmCurrentDateTime;							///< stores current date and time

	// Variable initialization
	tmCurrentDateTime = util_GetCurrentDateTime();
	strCAnnotation[0] = '\0';

	//
	// parse annotation string
	//
	switch(atAnnotationType)
	{
		case CommunicationFailure:
			sprintf_s(strCAnnotation, _countof(strCAnnotation),
					  "%02d:%02d:%02d Communication failure%c",
					  tmCurrentDateTime.tm_hour, tmCurrentDateTime.tm_min, tmCurrentDateTime.tm_sec, (char) 20);
		break;
		
		case CommunicationResume:
			sprintf_s(strCAnnotation, _countof(strCAnnotation),
					  "%02d:%02d:%02d Communication restart%c",
					  tmCurrentDateTime.tm_hour, tmCurrentDateTime.tm_min, tmCurrentDateTime.tm_sec, (char) 20);
		break;

		case CoordinatorInserted:
			sprintf_s(strCAnnotation, _countof(strCAnnotation),
					  "%02d:%02d:%02d Coordinator plugged-in%c",
					  tmCurrentDateTime.tm_hour, tmCurrentDateTime.tm_min, tmCurrentDateTime.tm_sec, (char) 20);
		break;

		case CoordinatorRemoved:
			sprintf_s(strCAnnotation, _countof(strCAnnotation),
					  "%02d:%02d:%02d Coordinator removed%c",
					  tmCurrentDateTime.tm_hour, tmCurrentDateTime.tm_min, tmCurrentDateTime.tm_sec, (char) 20);
		break;

		case Now:
			sprintf_s(strCAnnotation, _countof(strCAnnotation),
					  "Now %d%c",
					  ++m_intNNowAnnotations, (char) 20);
		break;

		case Regular:
			wcstombs_s(&sztNCharsConverted, strTemp, sizeof(strTemp), m_cfgConfiguration.Annotations[intAnnotationId], sizeof(strTemp));
			sprintf_s(strCAnnotation, _countof(strCAnnotation),
					  "%s%c",
					  strTemp, (char) 20);
		break;
	}

	// Wait on annotation mutex
	WaitForSingleObject(m_hMutexAnnotation, INFINITE);
	
	// Save annotation string to the annotation buffer
	if(m_intNAnnotationsBuffered < ANNOTATION_MAX_NO)
	{
		strcat_s(m_strCurrentDRAnnotations, ANNOTATION_TOTAL_NCHARS, strCAnnotation);
		m_intNAnnotationsBuffered++;
	}
	
	ReleaseMutex(m_hMutexAnnotation);

	// display annotation in status bar
	SendMessage(hwndMainWindow, EEGEMMsg_StatusBar_SetAnnotation, FALSE, (LPARAM) strCAnnotation);
}

/**
 * \brief Initializes the annotations signal for the current data record.
 *
 * \param[in]	strAnnotation		char string where annotation signal is stored
 * \param[in]	uintAnnotationLen	size of strAnnotation, in characters
 * \param[in]	uintTAL				time-stamp of the current data record
 */
static void main_InitAnnotationSignal(char * strAnnotation, unsigned int uintAnnotationLen, unsigned int uintTAL)
{
#ifdef _DEBUG
	// needed because sprintf_s() fills buffer with garbage after the NULL character when in DEBUG mode
	char strBuffer[ANNOTATION_TOTAL_NCHARS];
	
	// initialize annotation buffer
	SecureZeroMemory(strAnnotation, uintAnnotationLen*sizeof(char));

	// initialize signal
	strAnnotation[0] = '\0';
	sprintf_s(strBuffer, _countof(strBuffer), "+%d%c%c", uintTAL, (char) 20, (char) 20);
	strcat_s(strAnnotation,
			 (min(strlen(strBuffer) + 1, uintAnnotationLen))*sizeof(char),
			 strBuffer);
#else
	// initialize signal
	SecureZeroMemory(strAnnotation, uintAnnotationLen*sizeof(char));
	sprintf_s(strAnnotation, uintAnnotationLen, "+%d%c%c", uintTAL, (char) 20, (char) 20);
#endif
}

/**
 * \brief Replaces the message processing function of an edit control with the ASCIIMaskedEditProc function.
 *
 * \param[in]	hwndEdit	handle to edit control whose processing function is to be replaced
 */
static void main_MakeEditControlASCIIMasked(HWND hwndEdit)
{
    WNDPROC oldwndproc;

	// don't make a new mask if there is already one available
    oldwndproc = (WNDPROC) GetWindowLongPtr (hwndEdit, GWLP_WNDPROC);

    //don't update the user data if it is already in place
    if(oldwndproc != ASCIIMaskedEditProc)
    {
		SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (ULONG_PTR) ASCIIMaskedEditProc);
		SetWindowLongPtr(hwndEdit, GWLP_USERDATA, (ULONG_PTR) oldwndproc);
    }
}

/**
 * \brief Parses all command line switches.
 *
 * First the the command line is tokenized. The resulting tokens are then compared to
 * a list of switches. If there is a match, the respective switch is set to 'TRUE'.
 * The list of switches can easily be extended by adding new case statements in the
 * switch statement.
 *
 * \param[in]	strCommandLine	pointer to the command line
 */
static void main_ProcessCommandLine(char *strCommandLine)
{
	char *chrContext = NULL;
	char *strSwitchesBuffer[10];
	char strSeparators[] = " -";
	int i, intSwitchesCounter = 0;
	
	// Parse command line
	strSwitchesBuffer[intSwitchesCounter++] = strtok_s(strCommandLine,strSeparators,&chrContext);
	if(strSwitchesBuffer[0] != NULL)
	{
		while(intSwitchesCounter < sizeof(strSwitchesBuffer))
		{
			if((strSwitchesBuffer[intSwitchesCounter] = strtok_s(NULL,strSeparators,&chrContext)) == NULL)
				break;
			else
				intSwitchesCounter++;
		}

		// Process switches
		for(i=0; i<intSwitchesCounter; i++)
		{
			switch(*strSwitchesBuffer[i])
			{
				case 'a':
					m_blnDrawAccelerometerTraces =  TRUE;
				break;
			}
		}
	}
}

/**
 * \brief Stores the current contents of the configuration structure in the software's configuration file.
 *
 * \param[in]	gui					struct containing handles to the elemtents of the main window's GUI
 */
static void main_SaveCurrentSettings(GUIElements gui)
{
	m_cfgConfiguration.ScaleIndex = (int) SendMessage((HWND) gui.hwndCMBSensitivity, CB_GETCURSEL, 0, 0);
	m_cfgConfiguration.TimeBaseIndex = (int) SendMessage((HWND) gui.hwndCMBTimebase, CB_GETCURSEL, 0, 0);
	m_cfgConfiguration.LPFilterIndex = (int) SendMessage((HWND) gui.hwndCMBLPFilters, CB_GETCURSEL, 0, 0);
	config_store(m_cfgConfiguration);
}

/**
 * \brief Abnormal program termination.
 */
static void main_AbnormalProgramTermination()
{
	MsgPrintf(NULL, MB_ICONERROR, TEXT("Shit hit the fan!"));

	// close application log
	applog_logevent(SoftwareError, TEXT("Main"), TEXT("Unhandled error triggered software termination."), 0, TRUE);
	applog_close();

	// important to close serial port if it is open, otherwise will have to restart computer to free it
	serial_ClosePort();

	exit(1);
}

static void Dialog_RecordingInfo_SetNCharsLeft1(HWND hwndDlg)
{
	TCHAR strBuffer[EDFRECORDINGSPACE + 1], strNCharsRemaining[25];
	HWND hwndControl;
	int intNRemainigChars = EDFRECORDINGSPACE, intNChars;

	// compute # of chars remaining
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_ADMINISTRATIONCODE, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_TECHNICIAN, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_DEVICE, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_COMMENTS, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	_stprintf_s(strNCharsRemaining, sizeof(strNCharsRemaining)/sizeof(TCHAR), TEXT("%d characters remaining"), intNRemainigChars);
	
	// update the max # of chars that can be typed into the text boxes
	intNChars = GetDlgItemText (hwndDlg, IDC_ADMINISTRATIONCODE, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNChars += intNRemainigChars;
	hwndControl = GetDlgItem(hwndDlg, IDC_ADMINISTRATIONCODE);
	if(intNChars == 0)
		EnableWindow(hwndControl, FALSE);
	else
	{
		EnableWindow(hwndControl, TRUE);
		SendMessage(hwndControl, EM_SETLIMITTEXT, intNChars, 0);
	}

	intNChars = GetDlgItemText (hwndDlg, IDC_TECHNICIAN, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNChars += intNRemainigChars;
	hwndControl = GetDlgItem(hwndDlg, IDC_TECHNICIAN);
	if(intNChars == 0)
		EnableWindow(hwndControl, FALSE);
	else
	{
		EnableWindow(hwndControl, TRUE);
		SendMessage(hwndControl, EM_SETLIMITTEXT, intNChars, 0);
	}

	intNChars = GetDlgItemText (hwndDlg, IDC_DEVICE, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNChars += intNRemainigChars;
	hwndControl = GetDlgItem(hwndDlg, IDC_DEVICE);
	if(intNChars == 0)
		EnableWindow(hwndControl, FALSE);
	else
	{
		EnableWindow(hwndControl, TRUE);
		SendMessage(hwndControl, EM_SETLIMITTEXT, intNChars, 0);
	}

	intNChars = GetDlgItemText (hwndDlg, IDC_COMMENTS, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNChars += intNRemainigChars;
	hwndControl = GetDlgItem(hwndDlg, IDC_COMMENTS);
	if(intNChars == 0)
		EnableWindow(hwndControl, FALSE);
	else
	{
		EnableWindow(hwndControl, TRUE);
		SendMessage(hwndControl, EM_SETLIMITTEXT, intNChars, 0);
	}

	// display number of remaining chars
	SetDlgItemText (hwndDlg, IDC_CHARSREMAINING1, strNCharsRemaining);
}

static void Dialog_RecordingInfo_SetNCharsLeft2(HWND hwndDlg)
{
	TCHAR strBuffer[EDFRESERVEDSPACE + 1], strNCharsRemaining[25];
	int intNRemainigChars = EDFRESERVEDSPACE;

	// compute # of chars remaining
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_ADDITIONALCOMMENTS, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	_stprintf_s(strNCharsRemaining, sizeof(strNCharsRemaining)/sizeof(TCHAR), TEXT("%d characters remaining"), intNRemainigChars);

	// display number of remaining chars
	SetDlgItemText (hwndDlg, IDC_CHARSREMAINING2, strNCharsRemaining);
}

static void Dialog_RecordingInfo_SetNCharsLeft3(HWND hwndDlg)
{
	TCHAR strBuffer[EDFTRANSDUCERTYPELENGTH + 1], strNCharsRemaining[25];
	int intNRemainigChars = EDFTRANSDUCERTYPELENGTH;

	// compute # of chars remaining
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_ELECTRODES, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	_stprintf_s(strNCharsRemaining, sizeof(strNCharsRemaining)/sizeof(TCHAR), TEXT("%d characters remaining"), intNRemainigChars);

	// display number of remaining chars
	SetDlgItemText (hwndDlg, IDC_CHARSREMAINING3, strNCharsRemaining);
}

//---------------------------------------------------------------------------------------------------------------------------------
//   								Main Functions
//---------------------------------------------------------------------------------------------------------------------------------
/**
 * \brief Program entry point.
 *
 * This is the inital program entry point. 
 *
 * \param[in]	hInstance		handle to the current instance of the application
 * \param[in]	hPrevInstance	always NULL
 * \param[in]	lpCmdLine		pointer to a null-terminated string that specifies the command line for the application, excluding the program name
 * \param[in]	nCmdShow		specifies how the window is to be shown
 * \return Depends on program termination conditions.
 */
int APIENTRY WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Variable declaration
	HANDLE					hLock;
	HACCEL					hAcceleratorTable;
	HWND					hwndPreviousWindow, hwndMainWnd;
	INITCOMMONCONTROLSEX	icex;
	MSG						msg;
	size_t					sztLockFilePathLength;
	TCHAR					strBuffer[max(512, MAX_PATH + 1)], * pstrLockFile;
	WNDCLASSEX				WndClsEx;
	
	// Variable initialization
	m_blnDrawAccelerometerTraces = FALSE;
	m_blnPortraitOrientation = FALSE;
	m_blnIsFullScreen = FALSE;
	m_hinMain = hInstance;
	m_wsccCheckCode = WEEGSystem_INVALID;

	// install own abnormal termination routine (to catch unhandled errors)
	set_terminate(main_AbnormalProgramTermination);

	// set process priority class
	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	// prevent system from entering sleep or turning off the display while application is running
	SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);

	// save screen saver configuration before disabling it
	SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &m_blnScreenSaverActive, 0);
	SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, 0, 0);

	// check OS version
	if(!util_IsWinXorLater(WinXP_SP2))
	{
		MsgPrintf(NULL, MB_ICONERROR, TEXT("This application can only run on Windows XP SP2 or later."));
		return 0;
	}

	// Get command line switches & process them
	main_ProcessCommandLine(lpCmdLine);
	
	// check if program is already open; if so, set focus to its window and close this instance
	GetTempPath(sizeof(strBuffer)/sizeof(TCHAR), strBuffer);
	sztLockFilePathLength = _tcslen(LONGFILENAME_PRFX) + _tcslen(strBuffer) + _tcslen(SOFTWARE_LOCK) + 1;
	pstrLockFile = (TCHAR *) calloc(sztLockFilePathLength, sizeof(TCHAR));
	if(pstrLockFile == NULL)
	{
		MsgPrintf(NULL, MB_ICONERROR, TEXT("Failed to allocate memory for variable storage."));
		return -1;
	}

	_stprintf_s(pstrLockFile,
				sztLockFilePathLength,
				TEXT("%s%s%s"),
				LONGFILENAME_PRFX, strBuffer, SOFTWARE_LOCK);
	hLock = CreateFile(pstrLockFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
	if (hLock == INVALID_HANDLE_VALUE)
	{
		if(GetLastError() == ERROR_SHARING_VIOLATION)
		{
			hwndPreviousWindow = FindWindow (WINDOW_CLASSID_MAIN, SOFTWARE_TITLE);
			SetForegroundWindow (hwndPreviousWindow);
		}
		
		return (0);
	}

	// Create & register main window class
	WndClsEx.cbSize			= sizeof(WNDCLASSEX);
	WndClsEx.style			= CS_HREDRAW | CS_VREDRAW;	
	WndClsEx.lpfnWndProc	= (WNDPROC) MainWndProc;
	WndClsEx.cbClsExtra		= 0;
	WndClsEx.cbWndExtra		= 0;
	WndClsEx.hInstance		= hInstance;
	WndClsEx.hIcon			= LoadIcon (hInstance, MAKEINTRESOURCE (IDI_ICON));
	WndClsEx.hCursor		= LoadCursor (NULL, IDC_ARROW);
	WndClsEx.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
	WndClsEx.lpszMenuName	= MAKEINTRESOURCE (IDR_MENU);
	WndClsEx.lpszClassName	= WINDOW_CLASSID_MAIN;
	WndClsEx.hIconSm		= NULL;
	if (!RegisterClassEx(&WndClsEx))
	{
		MsgPrintf(NULL, MB_ICONERROR, TEXT("Failed to register main window class."));
		return -1;
	}

	// Create & register annotations display window class
	WndClsEx.cbSize			= sizeof(WNDCLASSEX);
	WndClsEx.style			= CS_HREDRAW | CS_VREDRAW;
	WndClsEx.lpfnWndProc	= AnnotationsWndProc;
	WndClsEx.cbClsExtra		= 0;
	WndClsEx.cbWndExtra		= 0;
	WndClsEx.hIcon			= NULL;
	WndClsEx.hCursor		= LoadCursor(NULL, IDC_ARROW);
	WndClsEx.hbrBackground	= (HBRUSH)GetStockObject(WHITE_BRUSH);
	WndClsEx.lpszMenuName	= NULL;
	WndClsEx.lpszClassName	= WINDOW_CLASSID_ANNOT;
	WndClsEx.hInstance		= m_hinMain;
	WndClsEx.hIconSm		= NULL;
	if (!RegisterClassEx(&WndClsEx))
	{
		MsgPrintf(NULL, MB_ICONERROR, TEXT("Failed to register annotations window class."));
		return -1;
	}

	// check display orientation and adjust the main window size accordingly
	if(GetSystemMetrics(SM_CYMAXIMIZED) > GetSystemMetrics(SM_CXMAXIMIZED))
		m_blnPortraitOrientation = TRUE;
	
	//
	// create main window
	//
	// parse main window title
	_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("%s (v%s)"), SOFTWARE_TITLE, SOFTWARE_VERSION);

	// create main window
#ifdef _DEBUG
	hwndMainWnd = CreateWindowEx(0,
#else
	hwndMainWnd = CreateWindowEx(WS_EX_TOPMOST,
#endif
								   WINDOW_CLASSID_MAIN,
								   strBuffer,
								   (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME)),
								   CW_USEDEFAULT,
								   CW_USEDEFAULT,
								   CW_USEDEFAULT,
								   CW_USEDEFAULT,
								   NULL,
								   NULL,
								   hInstance,
								   NULL);
	if (hwndMainWnd == NULL)
	{
		MsgPrintf(NULL, MB_ICONERROR, TEXT("Failed to create main window."));
		return (-2);
	}

	// Initialize common controls.
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

	// Display main window
	ShowWindow (hwndMainWnd, SW_SHOWMAXIMIZED);

	// Load accelerator table
	hAcceleratorTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATORS));

	while (GetMessage (&msg, NULL, 0, 0))
	{
       if (!TranslateAccelerator(hwndMainWnd, hAcceleratorTable, &msg))         // message data 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return ((int) msg.wParam);
}

/**
 * \brief Callback function that processes messages sent to the main window.
 *
 * \param[in]	hWnd		handle to the window
 * \param[in]	message		the message
 * \param[in]	wParam		additional message information (contents depend on value of message)
 * \param[in]	lParam		additional message information (contents depend on value of message)
 *
 * \return Result of the message processing and depends on the message sent.
 */
static LRESULT APIENTRY MainWndProc (HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
	BOOL					blnErrorOccured = FALSE;
	DWORD					ThreadId;
	DWORD					dwReturnValue;
	EDFFileHandle *			phEDFFile;
	HDC						hDC;
	HMODULE					hlibRichEditV2;
	float					f;
	HANDLE					hEDFPlusFile;						///< handle for the final EDF+ file
	int						i;
	PAINTSTRUCT				PS;
	PDEV_BROADCAST_PORT		pdbhPortBroadcast;
	PROCESS_INFORMATION		pi;
	RECT					rc;
	STARTUPINFO				si;
	size_t					sztLength;
	static BOOL				blnRecordingStarted = FALSE;		///< flag that is set to TRUE at the beginning of a recording and to FALSE when it is stopped
	static BOOL				blnMainWndShown4FirstTime = TRUE;	///< flag that is set to TRUE once the main window has been shown once
	static GUIElements		gui;
	static BYTE				epExitPermission = ExitPermission_Allowed;
	static TCHAR			strFinalEDFFilePath[MAX_PATH + 1];	///< path where final EDF+ file will be stored
	static TCHAR			strTempEDFFilePath[MAX_PATH + 1];	///< path where temporary EDF+ file will be stored
	static TCHAR			strStatusBarStatus[256];			///< stores string to be displayed in the status bar's status part
	static TCHAR			strStatusBarAnnotation[256];		///< stores string to be displayed in the status bar's annotation part
	static unsigned short	ushrEDFPlusHeaderBufferLenByt = 0;	///< size of the \a pEDFPlusHeaderBuffer, in bytes
	static void *			pEDFPlusHeaderBuffer;				///< pointer to buffer where complete EDF+ header record is stored
	static struct tm		tmRecordingStartDateTime;			///< date & time at which the recording of the temporary EDF+ file was started (used when generating the file name of the final EDF+ file)
	TCHAR					strBuffer[256], strRecordedEDFFilePath[MAX_PATH_UNICODE + 1];
	TCHAR *					strTemp, * strTemp2;
	unsigned int			j, k, l, uintNNewSamples;
	UploadThreadData		utd;
	WINDOWINFO				wi;
	
	// sample thread variables
	static HANDLE						hSampleThread;				///< handle to Sampling thread
	static SampleThreadData				std;

	// storage thread variables
	static HANDLE						hStorageThread;				///< handle to Storage thread
	static StorageThreadData			sttd;

	// streaming thread variables
	static HANDLE						hStreamingThread;			///< handle to Streaming thread
	static StreamingClientThreadData	sctd;
	static SimulationModeData			smd;

	// upload thread variables
	static HANDLE						hUploadThread;				///< handle to Upload thread

	// WiFi thread variales
	static HANDLE						hWiFiThread;				///< handle to WiFi thread

	static int LastNOfPackets = 0;

	// Buffers for samples to be used when measuring and displaying simultenously
	//static short shrSampleBuffer [EEGCHANNELS + ACCCHANNELS][SAMPLE_BUFFER_LENGTH];
	static short ** pshrSampleBuffer;
	
	// Graphics variables
	unsigned int uintEEGNewSamplesStartID;
	unsigned int uintAEEGNewSamplesStartID;

	// GUI Variables
	OPENFILENAME ofn;
	static int intSelectedTimebaseIndex = -1;
	static int intSelectedSensitivityIndex = -1;

	switch (message)
	{
		case WM_CREATE:
			// Create mutex objects for: annotations buffer, sample buffer and data records linked list
			m_hMutexAnnotation = CreateMutex(NULL, FALSE, NULL);
			m_hMutexSampleBuffer = CreateMutex(NULL, FALSE, NULL);

			//
			// Read settings from configuration file
			//
			hDC = GetDC(hWnd);
			config_init(GetSystemMetrics(SM_CXSCREEN),
						GetSystemMetrics(SM_CYSCREEN),
						GetDeviceCaps(hDC, HORZSIZE),
						GetDeviceCaps(hDC, VERTSIZE));
			ReleaseDC(hWnd, hDC);
			config_load(&m_cfgConfiguration);

			//
			// initialize application log and log version of libraries that are in use
			//
			applog_init(NULL, m_cfgConfiguration.ApplicationDataPath, NULL, 0);
			applog_logevent(Version, TEXT("EEGEM"), SOFTWARE_VERSION, 0, FALSE);
			
			//
			// create sample thread and associated synchronization events
			//
			// initialize data structure that will be passed to the thread
			std.pgui = &gui;
			std.pSamplingFrequency = &m_cfgConfiguration.SamplingFrequency;
			std.hevSampleThread_Start = CreateEvent(NULL, FALSE, FALSE, NULL);
			std.hevSampleThread_Idle = CreateEvent(NULL, FALSE, FALSE, NULL);
			std.hevSampleThread_Init = CreateEvent(NULL, FALSE, FALSE, NULL);
			
			// Create sample thread and set its priority
			hSampleThread = CreateThread (NULL,										// handle cannot be inherited by child processes
										  4096,										// initial size of the stack, in bytes
										  (LPTHREAD_START_ROUTINE) Sample_Thread,	// pointer to the function to be executed by the thread
										  &std,										// pointer to a variable to be passed to the thread
										  0,										// thread runs immediately after creation
										  &ThreadId);								// variable where thread identifier is stored
			if (hSampleThread == NULL)
			{
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: Failed to create sample thread. (GetLastError #)"), GetLastError(), TRUE);
				MsgPrintf(NULL, MB_ICONERROR, TEXT("WM_CREATE: Failed to create sample thread. (GetLastError #: %d)\nPlease restart the software!"), GetLastError());
			}
			else
			{
				// set sample thread priority
				SetThreadPriority (hSampleThread, THREAD_PRIORITY_TIME_CRITICAL);
			
				// ensure sample is in Idle state
				WaitForSingleObject(std.hevSampleThread_Idle, INFINITE);
			}

			//
			// create storage thread and associated synchronization events
			//
			// create Storage thread events
			sttd.hevStorageThread_Idling = CreateEvent(NULL, FALSE, FALSE, NULL);
			sttd.hevStorageThread_Init_End = CreateEvent(NULL, FALSE, FALSE, NULL);
			sttd.hevStorageThread_Init_Start = CreateEvent(NULL, FALSE, FALSE, NULL);
			sttd.hevStorageThread_Write = std.hevStorageThread_Write = CreateEvent(NULL, FALSE, FALSE, NULL);
			sttd.StopStorage = FALSE;
			sttd.pcfg = &m_cfgConfiguration;

			// Create storage thread and set its priority
			hStorageThread = CreateThread (NULL,									// handle cannot be inherited by child processes
										   4096,									// initial size of the stack, in bytes
										   (LPTHREAD_START_ROUTINE) Storage_Thread,	// pointer to the function to be executed by the thread
										   &sttd,									// pointer to a variable to be passed to the thread
										   0,										// thread runs immediately after creation
										   &ThreadId);								// variable where thread identifier is stored
			if (hStorageThread == NULL)
			{
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: Failed to create storage thread. (GetLastError #)"), GetLastError(), TRUE);
				MsgPrintf(NULL, MB_ICONERROR, TEXT("WM_CREATE: Failed to create storage thread. (GetLastError #: %d)\nPlease restart the software!"), GetLastError());
			}
			else
			{
				// set storage thread priority
				SetThreadPriority (hStorageThread, THREAD_PRIORITY_NORMAL);

				// ensure storage thread is in Idle state
				WaitForSingleObject(sttd.hevStorageThread_Idling, INFINITE);
			}

			//
			// create streaming thread and associated synchronization events
			//
			sctd.ExitThread = FALSE;
			sctd.EndTransmission = FALSE;
			sctd.hevThreadInit_Complete = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_Connect_Start = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_Connecting_Start = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_Connecting_End = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_Exiting = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_Transmit = std.hevVortexClient_Transmit = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_WaitingToConnect = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.hevVortexClient_WaitingToTransmit = CreateEvent(NULL, FALSE, FALSE, NULL);
			sctd.pMaxNSendMsgFailures = &m_cfgConfiguration.Streaming_MaxNSendMsgFailures;
			sctd.pMaxNWait4ReplyFailures = &m_cfgConfiguration.Streaming_MaxNWait4ReplyFailures;
			sctd.pServerIPv4Address_Field0 = &m_cfgConfiguration.Streaming_Server_IPv4_Field0;
			sctd.pServerIPv4Address_Field1 = &m_cfgConfiguration.Streaming_Server_IPv4_Field1;
			sctd.pServerIPv4Address_Field2 = &m_cfgConfiguration.Streaming_Server_IPv4_Field2;
			sctd.pServerIPv4Address_Field3 = &m_cfgConfiguration.Streaming_Server_IPv4_Field3;
			sctd.pServerPort = &m_cfgConfiguration.Streaming_Server_Port;

			// create streaming thread
			hStreamingThread = CreateThread (NULL,											// pointer to a SECURITY_ATTRIBUTES structure that determines whether the returned handle can be inherited by child processes
											 4096,											// initial size of the stack, in bytes
											 (LPTHREAD_START_ROUTINE) Streaming_Thread,		// pointer to the application-defined function to be executed by the thread
											 &sctd,											// pointer to a variable to be passed to the thread
											 0,												// flags that control the creation of the thread
											 NULL);											// (optional) pointer to a variable that receives the thread identifier
			if (hStreamingThread == NULL)
			{
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: Failed to create streaming thread. (GetLastError #)"), GetLastError(), TRUE);
				MsgPrintf(NULL, MB_ICONERROR, TEXT("WM_CREATE: Failed to create streaming thread. (GetLastError #: %d)\nPlease restart the software!"), GetLastError());
			}
			else
			{
				// set storage thread priority
				SetThreadPriority (hStreamingThread, THREAD_PRIORITY_TIME_CRITICAL);

				// ensure streaming thread is in Waiting2Connect state
				WaitForSingleObject(sctd.hevThreadInit_Complete, INFINITE);
				if(WaitForSingleObject(hStreamingThread, 250) == WAIT_OBJECT_0)
				{
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: An error occured while creating the streaming thread."), 0, TRUE);
					MsgPrintf(NULL, MB_ICONERROR, TEXT("WM_CREATE: An error occured while creating the streaming thread.\nPlease restart the software!"), GetLastError());
				}
				else
				{
					WaitForSingleObject(sctd.hevVortexClient_WaitingToConnect, INFINITE);
				}
			}

			//
			// create WiFi thread and associated synchronization events
			//
			// create Storage thread events
			/*m_hevWiFiThread_ScanComplete = CreateEvent(NULL, FALSE, FALSE, NULL);

			// Create storage thread and set its priority
			hWiFiThread = CreateThread (NULL,									// handle cannot be inherited by child processes
										4096,									// initial size of the stack, in bytes
										(LPTHREAD_START_ROUTINE) WiFi_Thread,	// pointer to the function to be executed by the thread
										0,										// pointer to a variable to be passed to the thread
										0,										// thread runs immediately after creation
										&ThreadId);								// return the thread identifier
			if (hWiFiThread == NULL)
			{
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: Failed to create WiFi thread. (GetLastError #)"), GetLastError(), TRUE);
				MsgPrintf(NULL, MB_ICONERROR, TEXT("WM_CREATE: Failed to create WiFi thread. (GetLastError #: %d)\nPlease restart the software!"), GetLastError());
			}
			else
			{
				// set WiFi thread priority
				SetThreadPriority (hWiFiThread, THREAD_PRIORITY_NORMAL);
			}*/

			// initialize GUI elements
			dwReturnValue = GUI_Init(hWnd, &gui);
			if(dwReturnValue != 0)
			{
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: Could not create GUI. (GetLastError #)"), dwReturnValue, FALSE);
				MsgPrintf(hWnd, MB_ICONERROR, TEXT("The GUI could not be properly initialized!\nPlease restart the software!"));
			}
		break;

		case WM_ERASEBKGND:
			//if(m_blnSampleStart)
			//	return 1;
			//else
				DefWindowProc(hWnd, message, wParam, lParam);
		break;
		
		// sent to a window after its size has changed
		case WM_SIZE:
			// resize rebar
			SendMessage(gui.hwndRebar, WM_SIZE, 0, 0);

			//
			// resize status bar
			//
			GUI_SetStatusBarPartSize(gui.hwndStatusBar, LOWORD(lParam));
			SendMessage(gui.hwndStatusBar, WM_SIZE, 0, 0);
						
			if(blnRecordingStarted)
			{
				// re-calculate drawing areas
				GraphicsEngine_CalculateDrawingAreas(m_blnIsFullScreen);

				// re-calculate scaling values
				GraphicsEngine_CalculateScales();
			}
		break;
		
		// sent when the system or another application makes a request to paint a portion of an application's window
		case WM_PAINT:
			hDC = BeginPaint (hWnd, &PS);
			
			if(blnRecordingStarted)
			{
				switch(m_smCurrentSignalMode)
				{
					case SM_EEG:
						GraphicsEngine_EEG_DrawStatic(hDC);
						GraphicsEngine_EEG_DrawDynamicOld(hDC, m_pdblEEGDisplayBuffer, m_uintEEGDisplayBufferID, m_dblEEGYScale);
					break;

					case SM_aEEG:
						GraphicsEngine_AEEG_DrawStatic(hDC);
						GraphicsEngine_AEEG_DrawDynamicOld(hDC, m_pdblAEEGDisplayBuffer, m_uintAEEGDisplayBufferID);
					break;

					default:
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - WM_PAINT: Invalid SignalMode code detected."), 0, TRUE);
						MsgPrintf(hWnd, MB_ICONERROR, TEXT("%s"), TEXT("MainWndProc() - WM_PAINT: Invalid SignalMode code detected."));
				}
			}

			EndPaint (hWnd, &PS);

			// if this is the first time window is being displayed and software is not in simulation mode
			// check status of WEEG system
			if(blnMainWndShown4FirstTime)
			{
				blnMainWndShown4FirstTime = FALSE;

				if(!m_cfgConfiguration.SimulationMode)
				{
					main_CheckWEEGSystem(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), &std, gui);
				}
			}
		break;

		case WM_TIMER:
			switch(wParam)
			{
				case IDT_REDRAW_TIMER:
					//
					// redraw signal that is already displayed
					//
					// check if Sensitivity value has changed, and if so compute new EEG y-scale
					i = (int) SendMessage(gui.hwndCMBSensitivity, CB_GETCURSEL, 0, 0);
					if(intSelectedSensitivityIndex != i)
					{
						intSelectedSensitivityIndex = i;
						i = mc_intSensitivityFactors[intSelectedSensitivityIndex];
						
						// calculate new Y scale
						m_dblEEGYScale = ((double)(ADC_RESOLUTION * CONVERSION_FACTOR * m_cfgConfiguration.VerticalDPC))/((double)(i * SIGNAL_GAIN));
					}
					
					// check if Timebase value has changed; if so, compute new EEG x-scale and perform redraw
					i = (int) SendMessage(gui.hwndCMBTimebase, CB_GETCURSEL, 0, 0);
					if(intSelectedTimebaseIndex != i)
					{
						f = mc_fltTimebaseFactors[i];

						// compute compute new display buffer length
						m_uintEEGDisplayBufferLength = (int) (f*m_cfgConfiguration.SamplingFrequency);

						// check if moving from higher time base to a lower one
						if(i < intSelectedTimebaseIndex)
						{
							// calculate "75% of buffer length" sample index
							j = (unsigned int) (0.75*m_uintEEGDisplayBufferLength);

							// check if display id is above the "75% of buffer length" sample index
							if(m_uintEEGDisplayBufferID > j)
							{
								// if yes: take 75%*[new display buffer length] worth of new samples from "old" buffer
								//		   and copy into "new" buffer
								
								// calculate index of the last sample to be copied
								k = m_uintEEGDisplayBufferID - j;

								for(l=0; l < (EEGCHANNELS + ACCCHANNELS); l++)
								{
									memcpy_s(m_pdblDisplayBufferTemp,
											 m_uintNMaxSamples*sizeof(double),
											 &m_pdblEEGDisplayBuffer[l][k],
											 j*sizeof(double));
									memcpy_s(m_pdblEEGDisplayBuffer[l],
											 m_uintEEGDisplayBufferLength*sizeof(double),
											 m_pdblDisplayBufferTemp,
											 j*sizeof(double));
								}

								// update the Display Buffer Index to reflect where the next new sample should be 
								// inserted
								m_uintEEGDisplayBufferID = j;
							}
						}
						intSelectedTimebaseIndex = i;

						// re-configure drawing engine
						GraphicsEngine_SetDisplayBufferLength(m_uintEEGDisplayBufferLength);
						GraphicsEngine_CalculateScales();
				
						// erase window (triggers a WM_PAINT messaes i.e. redrawing of the old signal)
						// NOTE: RedrawWindow function does not take into account right and bottom border of rectangle => compensated with ++
						rc = GraphicsEngine_GetDrawingRect();
						rc.right++;
						rc.bottom++;
						RedrawWindow(hWnd, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
					}
					
					//
					// display new signal samples
					//
					if(m_uintNNewSamples > 0)
					{
						// reserve m_shrSampleBuffer buffer so that data is not added by the sampling thread
						WaitForSingleObject(m_hMutexSampleBuffer, INFINITE);

						// copy m_uintSampleBufferLength * sizeof (short) bytes from m_shrSampleBuffer[i]
						// to shrSampleBuffer [i] that is SAMPLE_BUFFER_LENGTH*sizeof(short) bytes large
						for (i=0; i<(EEGCHANNELS + ACCCHANNELS); i++)
						{
							memcpy_s(pshrSampleBuffer [i], m_uintSampleBufferLength*sizeof(short),
									 m_pshrSampleBuffer[i], m_uintNNewSamples*sizeof(short));
						}
						
						uintNNewSamples = m_uintNNewSamples;
						m_uintNNewSamples = 0;
				
						// release m_shrSampleBuffer buffer so that it can be used by the sampling thread
						ReleaseMutex(m_hMutexSampleBuffer);

						uintEEGNewSamplesStartID = m_uintEEGDisplayBufferID;
						uintAEEGNewSamplesStartID = m_uintAEEGDisplayBufferID;

						// Copy gyro samples to display buffer
						if(m_blnDrawAccelerometerTraces)
						{
							for (i=EEGCHANNELS; i<(EEGCHANNELS + ACCCHANNELS); i++)
							{
								k = m_uintEEGDisplayBufferID;
								for(j = 0; j < uintNNewSamples; j++)
								{
									m_pdblEEGDisplayBuffer [i][k] = (double) pshrSampleBuffer [i][j];
									k = (++k)%m_uintEEGDisplayBufferLength;
								}
							}
						}
						
						// get selected LP filter
						// NOTE: -1 offset needed in order to compensate for the "Off" item
						m_cfgConfiguration.LPFilterIndex = ((int) SendMessage(gui.hwndCMBLPFilters, CB_GETCURSEL, 0, 0)) - 1; 
						
						// Filter EEG samples
						//sp_FilterEEGSignal(m_pshrSampleBuffer, m_pdblEEGDisplayBuffer, m_uintEEGDisplayBufferLength, &m_uintEEGDisplayBufferID, uintNNewSamples, m_cfgConfiguration.LPFilterIndex);
						//sp_FilterAEEGSignal(m_pshrSampleBuffer, m_pdblAEEGDisplayBuffer, m_uintAEEGDisplayBufferLength, &m_uintAEEGDisplayBufferID, uintNNewSamples);
						sp_FilterAllPass(m_pshrSampleBuffer, m_pdblEEGDisplayBuffer, m_uintEEGDisplayBufferLength, &m_uintEEGDisplayBufferID, uintNNewSamples);

						// Plot curves
						hDC = GetDC (hWnd);
						switch(m_smCurrentSignalMode)
						{
							case SM_EEG:
								GraphicsEngine_EEG_DrawDynamicNew(hDC, m_pdblEEGDisplayBuffer, uintEEGNewSamplesStartID, uintNNewSamples, m_dblEEGYScale);
							break;
							
							case SM_aEEG:
								if(uintAEEGNewSamplesStartID != m_uintAEEGDisplayBufferID)
									GraphicsEngine_AEEG_DrawDynamicNew(hDC, m_pdblAEEGDisplayBuffer, uintAEEGNewSamplesStartID, m_uintAEEGDisplayBufferID);
								//GraphicsEngine_DrawDynamicNewAEEG(hDC, m_dblAEEGDisplayBuffer, uintAEEGNewSamplesStartID, uintNNewSamples);
							break;

							default:
								applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDT_REDRAW_TIMER: Invalid m_smCurrentSignalMode value."), 0, TRUE);
						}
						ReleaseDC (hWnd, hDC);

						// Update status text
						if (LastNOfPackets != m_lngNPacketsReceived)
						{
							// display recording status in the status bar
							_stprintf_s (strBuffer,
											sizeof(strBuffer)/sizeof(TCHAR),
											TEXT("%d pkts., %d errs., %dHz"),
											m_lngNPacketsReceived,
											m_lngNPacketChecksumErrors + m_intNPacketsLost,
											m_cfgConfiguration.SamplingFrequency);
							SendMessage (hWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) strBuffer);

							LastNOfPackets = m_lngNPacketsReceived;
						}
					}
				break;

				case IDT_BATSTATUS_TIMER:
					// set battery status icon
					if(m_blnBatteryLow)
					{
						if(m_blnBatteryLowBlink)
							PostMessage (gui.hwndStatusBar, SB_SETICON, SB_BATTERYICON_PART, (LPARAM) gui.hIconsBatteryStates[1]);
						else
							PostMessage (gui.hwndStatusBar, SB_SETICON, SB_BATTERYICON_PART, (LPARAM) gui.hIconsBatteryStates[2]);

						m_blnBatteryLowBlink = !m_blnBatteryLowBlink;
					}
					else
						PostMessage (gui.hwndStatusBar, SB_SETICON, SB_BATTERYICON_PART, (LPARAM) gui.hIconsBatteryStates[0]);
				break;

				case IDT_RTC_TIMER:
					// increase variable
					if(++m_tmRecordingTime.tm_sec > 59)
					{
						m_tmRecordingTime.tm_sec = 0;
						if(++m_tmRecordingTime.tm_min > 59)
						{
							m_tmRecordingTime.tm_min = 0;
							if(++m_tmRecordingTime.tm_hour == INT_MAX)
							{
								m_tmRecordingTime.tm_hour = 0;
							}
						}
					}
					
					// display elapsed recording time
					_stprintf_s (strBuffer,
								 sizeof(strBuffer)/sizeof(TCHAR),
								 TEXT("%02d:%02d:%02d"),
								 m_tmRecordingTime.tm_hour,
								 m_tmRecordingTime.tm_min,
								 m_tmRecordingTime.tm_sec);
					SendMessage (gui.hwndStatusBar, SB_SETTEXT, SB_RECTIME_PART, (LPARAM) strBuffer);
				break;
			}
		break;

		case WM_LBUTTONDOWN:
			// set keyboard focus in the main window
			SetFocus(hWnd);
			
			if(blnRecordingStarted)
			{
				// get mouse coordinates (relative to the upper left screen coordinates)
				i = LOWORD(lParam); 
				j = HIWORD(lParam);

				// adjust coordinates based on the client are coordinates
				wi.cbSize = sizeof(WINDOWINFO);
				GetWindowInfo(hWnd, &wi);
				i += wi.rcClient.left;
				j += wi.rcClient.top;

				GUI_DisplayAnnotationsContextMenu(hWnd, i, j, gui, hWnd);
			}
		break;

		case WM_RBUTTONDOWN:
			// set keyboard focus in the main window
			SetFocus(hWnd);
		break;

		case WM_SYSCOMMAND:
			switch(wParam)
			{
				case IDM_FULLSCREEN:
					GUI_SetFullScreenMode(hWnd, gui);
				break;

				case IDM_SAMPLE_START:
					SendMessage (hWnd, WM_COMMAND, IDM_SAMPLE_START, 0);
				break;

				case IDM_SAMPLE_STOP:
					SendMessage (hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Normal);
				break;

				default:
					return DefWindowProc(hWnd, message, wParam, lParam);
			}
		break;

		case WM_COMMAND:

			switch (LOWORD (wParam))
            {
				//----------------------------------------------------------------------------------------
				// Accelerators
				//----------------------------------------------------------------------------------------
				case IDA_CTRLS:
					if(blnRecordingStarted)
						SendMessage (hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Normal);
					else
						SendMessage (hWnd, WM_COMMAND, IDM_SAMPLE_START, 0);
				break;

				case IDA_CTRLE:
					PostMessage (hWnd, WM_COMMAND, IDM_UTILITIES_EDFFILEEDITOR, 0);
				break;

				case IDA_CTRLP:
					if(blnRecordingStarted)
						PostMessage (hWnd, WM_COMMAND, IDM_PATIENTINFO, 0);
					else
						PostMessage (hWnd, WM_COMMAND, IDM_PARAMETERS, 0);
				break;
				
				case IDA_CTRLR:
					if(blnRecordingStarted)
						PostMessage (hWnd, WM_COMMAND, IDM_RECORDINGINFORMATION, 0);
				break;

				//----------------------------------------------------------------------------------------
				// Controls
				//----------------------------------------------------------------------------------------
				case IDC_SENSITIVITY:
					if(HIWORD(wParam) == CBN_SELCHANGE)
						main_SaveCurrentSettings(gui);
				break;

				case IDC_LP:
					if(HIWORD(wParam) == CBN_SELCHANGE)
						main_SaveCurrentSettings(gui);
				break;

				case IDC_TIMEBASE:
					if(HIWORD(wParam) == CBN_SELCHANGE)
						main_SaveCurrentSettings(gui);
				break;
				
				//----------------------------------------------------------------------------------------
				// Toolbar
				//----------------------------------------------------------------------------------------
				case IDM_SYSTEMCHECK:
					// check WEEG system
					i = IDRETRY;
					while(i == IDRETRY && !main_CheckWEEGSystem(strBuffer,sizeof(strBuffer)/sizeof(TCHAR), &std, gui))
					{
						i = MessageBox(hWnd,
									   strBuffer,
									   SOFTWARE_TITLE,
									   MB_ICONERROR | MB_RETRYCANCEL | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
					}
				break;
				
				case IDM_ANNOTATIONS_DISPLAY:
					// if annotations window was closed using the window's close button,
					// unchecheck the annotations button in the main window's toolbar
					if(lParam == TRUE)
					{
						m_hwndAnnotationsDisplay = NULL;
						SendMessage(gui.hwndToolbar, TB_CHECKBUTTON, IDM_ANNOTATIONS_DISPLAY, FALSE);
					}
					else
					{
						if(!m_hwndAnnotationsDisplay)
						{
							// Create the window object
							m_hwndAnnotationsDisplay = CreateWindowEx(WS_EX_TOOLWINDOW,
																	  WINDOW_CLASSID_ANNOT,
																	  TEXT("Annotations"),
																	  WS_VISIBLE,
																	  ADW_STARTPOS_X,
																	  ADW_STARTPOS_Y,
																	  200, 320,
																	  hWnd,
																	  NULL,
																	  m_hinMain,
																	  NULL);
							
							// send WM_PAINT message immediately to the window procedure
							UpdateWindow(m_hwndAnnotationsDisplay);
						}
						else
						{
							DestroyWindow(m_hwndAnnotationsDisplay);
							m_hwndAnnotationsDisplay = NULL;
						}
					}
				break;

				case IDM_SIGNAL_DISPLAY_MODE:
					switch(m_smCurrentSignalMode)
					{
						case SM_EEG:
							// change to aEEG signal mode
							m_smCurrentSignalMode = SM_aEEG;

							// disable timebase and filter selection combo boxes
							ComboBox_Enable(gui.hwndCMBLPFilters, FALSE);
							ComboBox_Enable(gui.hwndCMBTimebase, FALSE);

							// erase window (triggers a WM_PAINT message i.e. drawing of the static components and redrawing of old signal)
							// NOTE: RedrawWindow function does not take into account right and bottom border of rectangle => compensated with ++
							rc = GraphicsEngine_GetDrawingRect();
							rc.right++;
							rc.bottom++;
							RedrawWindow(hWnd, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
						break;

						case SM_aEEG:
							// change to EEG signal mode
							m_smCurrentSignalMode = SM_EEG;

							// enable timebase and filter selection combo boxes
							ComboBox_Enable(gui.hwndCMBLPFilters, TRUE);
							ComboBox_Enable(gui.hwndCMBTimebase, TRUE);

							// reset display buffer index
							m_uintEEGDisplayBufferID = 0;

							// erase window (triggers a WM_PAINT message i.e. drawing of the static components and redrawing of old signal)
							// NOTE: RedrawWindow function does not take into account right and bottom border of rectangle => compensated with ++
							rc = GraphicsEngine_GetDrawingRect();
							rc.right++;
							rc.bottom++;
							RedrawWindow(hWnd, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
						break;

						default:
							MessageBox(hWnd,
									   TEXT("ERROR"),
									   SOFTWARE_TITLE,
									   MB_ICONSTOP | MB_ABORTRETRYIGNORE | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);
					}
				break;

				//----------------------------------------------------------------------------------------
				// Menus
				//----------------------------------------------------------------------------------------
				// load pre-recorded EDF+ files
				case IDM_LOADEDFFILE:
					// Initialize OPENFILENAME
					SecureZeroMemory (&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = strRecordedEDFFilePath;
					ofn.lpstrFile[0] = TEXT('\0');												// Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the contents of szFile to initialize itself.
					ofn.nMaxFile = sizeof(strRecordedEDFFilePath)/sizeof(TCHAR);
					ofn.lpstrFilter = TEXT("EDF/EDF+ File (*.EDF)\0*.EDF\0All Files (*.*)\0*.*\0");
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
					
					// display open file dialog
					if(GetOpenFileName(&ofn) == TRUE)
					{
						// convert path from TCHAR to char
						wcstombs_s(&sztLength, smd.strSimulationEDFFile, sizeof(smd.strSimulationEDFFile), ofn.lpstrFile, sizeof(smd.strSimulationEDFFile));
					}
				break;

				case IDM_TESTCONNSCRIPT:
					utd.cfg = m_cfgConfiguration;
					utd.pstrFinalEDFFilePath = NULL;

					hUploadThread = CreateThread (NULL,										// pointer to a SECURITY_ATTRIBUTES structure that determines whether the returned handle can be inherited by child processes
												  4096,										// initial size of the stack, in bytes
												  (LPTHREAD_START_ROUTINE) Upload_Thread,	// pointer to the application-defined function to be executed by the thread
												  &utd,										// pointer to a variable to be passed to the thread
												  0,										// flags that control the creation of the thread
												  NULL);									// (optional) pointer to a variable that receives the thread identifier
					if (hUploadThread == NULL)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("WM_CREATE: Failed to create upload thread. (GetLastError #)"), GetLastError(), TRUE);
						MsgPrintf(NULL, MB_ICONERROR, TEXT("WM_CREATE: Failed to create upload thread. (GetLastError #: %d)"), GetLastError());
					}
					// TODO: clean-up code
				break;

				// Show error counters
				case IDM_ERRORS:
					MsgPrintf (hWnd, 0,
							   TEXT("Checksum errors\t: %d\nLost packets\t: %d"),
							   m_lngNPacketChecksumErrors, m_intNPacketsLost);
				break;

				// Close program; WM_DESTROY cleans things up
				case IDM_QUIT:
					SendMessage (hWnd, WM_CLOSE, 0, 0);
				break;

				case IDM_SAMPLE_START:
					// variable initialization required for each sampling run
					m_blnIsAnnotationsMenuDisplayed = m_blnCommunicationBlackout = m_blnBatteryLowBlink = m_blnBatteryLow = FALSE;
					gui.hmnuAnnotations = NULL;
					m_uintEEGDisplayBufferID = m_uintAEEGDisplayBufferID = 0;
					m_intNSamplesDatarecord = m_intNDataRecords = 0; // Set the counter of data records in EDF+ file to zero
					m_smCurrentSignalMode = SM_EEG;
					m_uintNNewSamples = m_uintEEGDisplayBufferLength = 0;
					LastNOfPackets = 0;

					// initialize annotations-related variables
					m_uintTimeKeepingTAL = 0;
					m_intNNowAnnotations = m_intNAnnotationsBuffered = 0;
					main_InitAnnotationSignal(m_strCurrentDRAnnotations, _countof(m_strCurrentDRAnnotations), m_uintTimeKeepingTAL);

					// initialize variable that will keep track of recording time
					m_tmRecordingTime.tm_hour = 0;
					m_tmRecordingTime.tm_min = 0;
					m_tmRecordingTime.tm_sec = 0;

					// check status of WEEG system (if not in Simulation mode)
					if(m_cfgConfiguration.SimulationMode)
					{
						// check if simulation file has been loaded
						if(strlen(smd.strSimulationEDFFile) > 0)
						{
							phEDFFile = libEDF_openFile(smd.strSimulationEDFFile);
							if(phEDFFile != NULL)
							{
								// when in Simulation mode, sampling frequency used depends on the EDF+ file that is loaded
								m_cfgConfiguration.SamplingFrequency = phEDFFile->SignalHeaders[0].NSamplesPerDataRecord;

								// close EDF file
								libEDF_closeFile(phEDFFile);
							}
							else
							{
								applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Could not load EDF+ file."), 0, TRUE);
								MsgPrintf(hWnd, MB_ICONSTOP, TEXT("MainWndProc() - IDM_SAMPLE_START: Could not load EDF+ file."), 0);
								PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
								break;
							}
						}
						else
						{
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: No simulation file loaded."), 0, TRUE);
							MsgPrintf(hWnd, MB_ICONERROR, TEXT("MainWndProc() - IDM_SAMPLE_START: No simulation file loaded."));
							PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
							break;
						}
					}
					else
					{
						// check WEEG system
						i = IDRETRY; blnErrorOccured = FALSE;
						while(i == IDRETRY && !main_CheckWEEGSystem(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), &std, gui))
						{
							i = MessageBox(hWnd,
										   strBuffer,
										   SOFTWARE_TITLE,
										   MB_ICONSTOP | MB_RETRYCANCEL | MB_APPLMODAL | MB_TOPMOST | MB_SETFOREGROUND);

							// if user clicked on the 'Cancel' button, abort recording start up
							if(i == IDCANCEL)
							{
								blnErrorOccured = TRUE;
								break;
							}
						}
						if(blnErrorOccured)
							break;
					}

					// set exit status
					PostMessage(hWnd, EEGEMMsg_ExitPermission_Set, ExitPermission_Denied_Recording, 0);
					
					// initialize signal processing module
					if(!sp_init())
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to initialize signal processing module."), 0, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						blnErrorOccured = TRUE;
						break;
					}

					// mark start of recording in application log & log start of recording
					applog_startgrouping(TEXT("Recording"), TRUE);
					applog_logevent(General, TEXT("Main"), TEXT("Recording Started"), 0, TRUE);

					// compute maximum number of data samples that can be stored in the display buffers at any one time if the longest
					// timebase is selected
					m_uintNMaxSamples = (unsigned int) ceil(m_cfgConfiguration.SamplingFrequency*mc_fltTimebaseFactors[(sizeof(mc_fltTimebaseFactors)/sizeof(float)) - 1]);
					
					// compute actual number of samples stored in the display buffers based on the selected timebase
					i = SendMessage(gui.hwndCMBTimebase, CB_GETCURSEL, 0, 0);
					m_uintEEGDisplayBufferLength = (int) (mc_fltTimebaseFactors[i]*m_cfgConfiguration.SamplingFrequency);

					// allocate memory for the display and sample buffers
					blnErrorOccured = FALSE;
					m_pdblEEGDisplayBuffer = (double **) malloc(sizeof(double *)*(EEGCHANNELS + ACCCHANNELS));
					if(m_pdblEEGDisplayBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							m_pdblEEGDisplayBuffer[i] = (double *) malloc(sizeof(double)*m_uintNMaxSamples);
							if(m_pdblEEGDisplayBuffer[i] == NULL)
							{
								blnErrorOccured = TRUE;
								break;
							}
						}
					}
					else
					{
						blnErrorOccured = TRUE;
					}
					if(blnErrorOccured)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to allocate memory for m_pdblEEGDisplayBuffer. (errno #)"), errno, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}

					m_uintAEEGDisplayBufferLength = (unsigned int) ceil((((double) m_cfgConfiguration.SamplingFrequency*24*3600)/AEEG_TIME_INTERVAL));
					m_pdblAEEGDisplayBuffer = (double **) malloc(sizeof(double *)*(EEGCHANNELS + ACCCHANNELS));
					if(m_pdblAEEGDisplayBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							m_pdblAEEGDisplayBuffer[i] = (double *) malloc(sizeof(double)*m_uintAEEGDisplayBufferLength);
							if(m_pdblAEEGDisplayBuffer[i] == NULL)
							{
								blnErrorOccured = TRUE;
								break;
							}
						}
					}
					else
					{
						blnErrorOccured = TRUE;
					}
					if(blnErrorOccured)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to allocate memory for m_pdblAEEGDisplayBuffer. (errno #)"), errno, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}

					m_pdblDisplayBufferTemp = (double *) malloc(sizeof(double)*(m_uintNMaxSamples));
					if(m_pdblDisplayBufferTemp == NULL)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDM_SAMPLE_START: Failed to allocate memory for m_pdblDisplayBufferTemp. (errno #)"), errno, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}

					// although the sample buffer shouldn't be bigger than about 100-150 samples
					// the number of samples being stored temporarily spikes sometimes when the system
					// is busy with other high-priority tasks (was 400 for 200 Hz, i.e., should be about 2*sampling frequency)
					m_uintSampleBufferLength = m_cfgConfiguration.SamplingFrequency*3;
					m_pshrSampleBuffer = (short **) malloc(sizeof(short *)*(EEGCHANNELS + ACCCHANNELS));
					blnErrorOccured = FALSE;
					if(m_pshrSampleBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							m_pshrSampleBuffer[i] = (short *) malloc(sizeof(short)*m_uintSampleBufferLength);
							if(m_pshrSampleBuffer[i] == NULL)
							{
								blnErrorOccured = TRUE;
								break;
							}
						}
					}
					else
					{
						blnErrorOccured = TRUE;
					}
					if(blnErrorOccured)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to allocate memory for m_pshrSampleBuffer. (errno #)"), errno, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
					for (i = 0; i < (EEGCHANNELS + ACCCHANNELS); i++)
					{
						for (j = 0; j < m_uintSampleBufferLength; j++)
							m_pshrSampleBuffer [i][j] = 0;
					}

					pshrSampleBuffer = (short **) malloc(sizeof(short *)*(EEGCHANNELS + ACCCHANNELS));
					if(pshrSampleBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							pshrSampleBuffer[i] = (short *) malloc(sizeof(short)*m_uintSampleBufferLength);
							if(pshrSampleBuffer[i] == NULL)
							{
								blnErrorOccured = TRUE;
								break;
							}
						}
					}
					else
					{
						blnErrorOccured = TRUE;
					}
					if(blnErrorOccured)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to allocate memory for pshrSampleBuffer. (errno #)"), errno, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
					for (i = 0; i < (EEGCHANNELS + ACCCHANNELS); i++)
					{
						for (j = 0; j < m_uintSampleBufferLength; j++)
							pshrSampleBuffer [i][j] = 0;
					}

					//
					// signal storage thread to move to writing state
					//
					sttd.StopStorage = FALSE;
					SignalObjectAndWait(sttd.hevStorageThread_Init_Start, sttd.hevStorageThread_Init_End, INFINITE, FALSE);
					if(Storage_GetMainFSMState() != STS_Write)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: An error occured while initializing the Storage thread."), 0, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}

					// initialize EDF+ record storage structures
					if(edf_InitHeaderStructures(&m_piPatientInfo, &m_riRecordingInfo) != 0)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to initialize EDF+ header structures. (errno #)"), i, TRUE);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}

					//
					// store EDF+ header in temporary file and send to streaming server (if enabled)
					//
					// generate EDF+ header record
					ushrEDFPlusHeaderBufferLenByt = edf_CalculateEDFplusHeaderRecord(EEGCHANNELS + ACCCHANNELS + 1);			// +1 for annotations signal
					pEDFPlusHeaderBuffer = malloc(ushrEDFPlusHeaderBufferLenByt + 1);											// +1 for terminating null character
					if(pEDFPlusHeaderBuffer != NULL)
					{
						tmRecordingStartDateTime = util_GetCurrentDateTime();

						if(edf_GenerateEDFplusHeaderRecord(TRUE, m_piPatientInfo, m_riRecordingInfo, m_cfgConfiguration.SamplingFrequency,
															m_intNDataRecords, EEGCHANNELS, ACCCHANNELS, m_cfgConfiguration.ElectrodeType,
															(char *) pEDFPlusHeaderBuffer, ushrEDFPlusHeaderBufferLenByt + 1))	// +1 for terminating null character
						{
							// send to storage thread
							if(!Storage_AddToQueue((BYTE *) pEDFPlusHeaderBuffer, ushrEDFPlusHeaderBufferLenByt, TRUE, sttd.hevStorageThread_Write))
							{
								applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDM_SAMPLE_START: Unable to add EDF+ header record to the storage thread's write queue."), 0, TRUE);
								MsgPrintf(hWnd, MB_ICONSTOP, TEXT("IDM_SAMPLE_START: Unable to add EDF+ header record to the storage thread's write queue."));
								PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
								break;
							}
						}
						else
						{
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Unable to generate EDF+ header record."), 0, TRUE);
							MsgPrintf(hWnd, MB_ICONSTOP, TEXT("MainWndProc() - IDM_SAMPLE_START: Unable to generate EDF+ header record."));
							PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
							break;
						}
					}
					else
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to allocate memory for Buffer. (errno #)"), errno, TRUE);
						MsgPrintf(hWnd, MB_ICONSTOP, TEXT("MainWndProc() - IDM_SAMPLE_START: Failed to allocate memory for Buffer. (errno #%d)."), errno);
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
						
					// enable/disable appropriate toolbar and menu commands
					GUI_SetEnabledCommands(hWnd, TRUE, gui);

					// clear annotations status bar
					SendMessage(hWnd, EEGEMMsg_StatusBar_SetAnnotation, TRUE, (LPARAM) TEXT(""));

					// Create scope update timer, battery status update, and recoring time timer
					if(SetTimer (hWnd, IDT_REDRAW_TIMER, REDRAW_TIMER_PERIOD, (TIMERPROC) NULL) == 0)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDM_SAMPLE_START: Unable to set time-out value of redraw timer. (GetLastError #)"), GetLastError(), TRUE);
						MsgPrintf(hWnd, MB_ICONSTOP, TEXT("Unable to set time-out value of redraw timer."));
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
					if(SetTimer (hWnd, IDT_BATSTATUS_TIMER, BATLOW_TIMER_PERIOD, (TIMERPROC) NULL) == 0)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDM_SAMPLE_START: Unable to set time-out value of battery-low icon timer. (GetLastError #)"), GetLastError(), TRUE);
						MsgPrintf(hWnd, MB_ICONSTOP, TEXT("Unable to set time-out value of battery-low icon timer."));
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
					if(SetTimer (hWnd, IDT_RTC_TIMER, 1000, (TIMERPROC) NULL) == 0)
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDM_SAMPLE_START: Unable to set time-out value of recording time timer. (GetLastError #)"), GetLastError(), TRUE);
						MsgPrintf(hWnd, MB_ICONSTOP, TEXT("Unable to set time-out value of recording time timer."));
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
						
					//
					// generate final EDF+ file name and path
					//
					// get full path of temporary EDF+ file from storage thread
					if(Storage_GetTemporaryEDFFilePath(strTempEDFFilePath, _countof(strTempEDFFilePath)))
					{
						main_GenerateFinalEDFFilePath(tmRecordingStartDateTime, strTempEDFFilePath, strFinalEDFFilePath, _countof(strFinalEDFFilePath));
					}
					else
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: An error occured while retrieving the full path of the temporary EDF+ file from Storage thread."), 0, TRUE);
						MsgPrintf(hWnd, MB_ICONSTOP, TEXT("MainWndProc() - IDM_SAMPLE_START: An error occured while retrieving the full path of the temporary EDF+ file from Storage thread."));
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
						
					//
					// signal streaming thread to start work (if streaming is enabled)
					//
					if(m_cfgConfiguration.Streaming_Enabled)
					{
						// check if thread encountered an error and exited prematurely
						if(WaitForSingleObject(sctd.hevVortexClient_Exiting, 0) != WAIT_OBJECT_0)
						{
							// initialize StreamingClientThreadData structure
							sctd.EndTransmission = FALSE;
							sctd.ExitThread = FALSE;

							// set EDF+ file name
							wcstombs_s(&sztLength,
										sctd.EDFFileName, sizeof(sctd.EDFFileName),
										PathFindFileName(strFinalEDFFilePath), (_countof(sctd.EDFFileName) - 1)*sizeof(char));	// -1 because there has to be room for terminating NULL character

							// signal streaming thread to start the connection process and wait until end of the process
							SignalObjectAndWait(sctd.hevVortexClient_Connect_Start, sctd.hevVortexClient_Connecting_Start, INFINITE, FALSE);
							
							// send EDF+ header record
							Streaming_SendPacket(EEGEMPacketType_EDFhdr, (BYTE *) pEDFPlusHeaderBuffer, ushrEDFPlusHeaderBufferLenByt, sctd.hevVortexClient_Transmit);
						}
						else
						{
							// log error
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Streaming thread has exited prematurely."), 0, TRUE);

							// since connection was unsucessfull, disable streaming
							m_cfgConfiguration.Streaming_Enabled = FALSE;
						}
					}

					//
					// initialize graphics engine
					//
					if(!GraphicsEngine_Init(hWnd, gui.hwndRebar, gui.hwndStatusBar, util_GetNOfSelectedChannels(m_cfgConfiguration.DisplayChannelMask), m_blnDrawAccelerometerTraces, m_cfgConfiguration, m_blnIsFullScreen, m_uintEEGDisplayBufferLength, m_uintAEEGDisplayBufferLength, NULL))
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_START: Unable to initialize graphics engine."), 0, TRUE);
						MsgPrintf(hWnd, MB_ICONERROR, TEXT("MainWndProc() - IDM_SAMPLE_START: Unable to initialize graphics engine."));
						PostMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}

					// set recording flag to true
					// NOTE: this has to be set here since WM_PAINT message draws static GUI items only if this flag is TRUE
					blnRecordingStarted = TRUE;

					// erase window (triggers a WM_PAINT message i.e. drawing of the static components and redrawing of old signal)
					// NOTE: RedrawWindow function does not take into account right and bottom border of rectangle => compensated with ++
					rc = GraphicsEngine_GetDrawingRect();
					rc.right++;
					rc.bottom++;
					RedrawWindow(hWnd, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
					
					//
					// enable sampling
					//
					std.EndActivity = FALSE;
					if(m_cfgConfiguration.SimulationMode)
					{
						std.Mode = SampleThreadMode_Simulation;
						std.pModeData = &smd;
					}
					else
					{
						std.Mode = SampleThreadMode_Recording;
						std.pModeData = NULL;
					}
					
					SetEvent(std.hevSampleThread_Start);
				break;

				case IDM_SAMPLE_STOP_NORMAL:
					SendMessage(hWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Normal);
				break;

				case IDM_SAMPLE_STOP:
					blnErrorOccured = FALSE;
					blnRecordingStarted = FALSE;

					//
					// wait for storage and streaming threads to reach idle states
					//
					// sample thread
					if(Sample_GetMainFSMState() != SampleThreadState_Idle)
					{
						// signal Sampling thread to stop and wait until it does so
						std.EndActivity = TRUE;
						WaitForSingleObject(std.hevSampleThread_Idle, INFINITE);
					}

					// storage thread
					if(Storage_GetMainFSMState() != STS_Idle)
					{
						// signal Storage thread to stop and wait until it does so
						sttd.StopStorage = TRUE;
						SignalObjectAndWait(sttd.hevStorageThread_Write, sttd.hevStorageThread_Idling, INFINITE, FALSE);
					}
					
					//
					// perform graphics engine clean-up
					//
					if(GraphicsEngine_IsInit())
						GraphicsEngine_CleanUp();

					//
					// clean up signal processing module
					//
					sp_cleanup();

					//
					// generate header record for the final EDF+ file
					//
					if(!edf_GenerateEDFplusHeaderRecord(FALSE, m_piPatientInfo, m_riRecordingInfo, m_cfgConfiguration.SamplingFrequency,
														m_intNDataRecords, EEGCHANNELS, ACCCHANNELS, m_cfgConfiguration.ElectrodeType,
														(char *) pEDFPlusHeaderBuffer, ushrEDFPlusHeaderBufferLenByt + 1))		// +1 for terminating null character
					{
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_STOP: Unable to generate EDF+ header record for the final EDF+ file."), 0, TRUE);
						free(pEDFPlusHeaderBuffer);
						pEDFPlusHeaderBuffer = NULL;
						ushrEDFPlusHeaderBufferLenByt = 0;
					}

					//
					// transfer data from temporary EDF+ file to the final EDF+ file and update header record
					//
					blnErrorOccured = TRUE;
					if(((StopCode) lParam == Stop_Normal) || (StopCode) lParam == Stop_Cancel)
					{
						// copy EDF+ from temporary file to its parsed filename and correct header record
						if(CopyFile(strTempEDFFilePath, strFinalEDFFilePath, TRUE))
						{
							if(pEDFPlusHeaderBuffer != NULL)
							{
								// Open EDF+ file and correct header information
								hEDFPlusFile = CreateFile(strFinalEDFFilePath,
															GENERIC_WRITE | GENERIC_READ,
															0,
															NULL,
															OPEN_EXISTING,
															FILE_ATTRIBUTE_NORMAL,
															NULL);
								if(hEDFPlusFile != INVALID_HANDLE_VALUE)
								{
									// write header record to file
									if(WriteFile(hEDFPlusFile, pEDFPlusHeaderBuffer, ushrEDFPlusHeaderBufferLenByt, &dwReturnValue, NULL))
									{
										if(dwReturnValue == ushrEDFPlusHeaderBufferLenByt)
										{
											blnErrorOccured = FALSE;
										}
										else
										{
											applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_STOP: Erroneous amount of bytes written while attemtping to store EDF+ header record."), 0, TRUE);
										}
									}
									else
									{
										applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_STOP: Unable to store EDF+ header record in final EDF+ file (GetLastError() #)."), GetLastError(), TRUE);
									}
												
									// close final EDF+ file
									CloseHandle (hEDFPlusFile);
								}
								else
								{
									applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_STOP: Unable to open final EDF+ file (GetLastError() #)."), GetLastError(), TRUE);
								}
							}
						}
						else
						{
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_STOP: Unable to copy temporary EDF+ file into final EDF+ file (GetLastError() #)."), GetLastError(), TRUE);
						}
					}
					
					//
					// send final EDF+ header record to streaming server (if enabled)
					//
					if(m_cfgConfiguration.Streaming_Enabled && pEDFPlusHeaderBuffer != NULL)
					{
						// signal that the following packet will be the last one
						sctd.EndTransmission = TRUE;

						// send header record
						Streaming_SendPacket(EEGEMPacketType_EDFhdr,
											 (BYTE *) pEDFPlusHeaderBuffer,
											 ushrEDFPlusHeaderBufferLenByt,
											 sctd.hevVortexClient_Transmit);

						// wait for streaming thread to transition to an idle state
						WaitForSingleObject(sctd.hevVortexClient_WaitingToConnect, INFINITE);
					}

					//
					// Upload EDF+ file to SSH server (if enabled)
					//
					if(!blnErrorOccured && m_cfgConfiguration.SSH_AutomaticUpload)
					{
						// initialize members of UploadThreadData structure
						utd.cfg = m_cfgConfiguration;
						utd.pstrFinalEDFFilePath = strFinalEDFFilePath;

						// create streaming thread
						hUploadThread = CreateThread (NULL,											// pointer to a SECURITY_ATTRIBUTES structure that determines whether the returned handle can be inherited by child processes
														4096,											// initial size of the stack, in bytes
														(LPTHREAD_START_ROUTINE) Upload_Thread,		// pointer to the application-defined function to be executed by the thread
														&utd,											// pointer to a variable to be passed to the thread
														0,											// flags that control the creation of the thread
														NULL);										// (optional) pointer to a variable that receives the thread identifier
						if (hUploadThread == NULL)
						{
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("MainWndProc() - IDM_SAMPLE_STOP: Failed to create upload thread. (GetLastError #)"), GetLastError(), TRUE);
							MsgPrintf(NULL, MB_ICONERROR, TEXT("MainWndProc() - IDM_SAMPLE_STOP: Failed to create upload thread. (GetLastError #: %d)"), GetLastError());
						}

						// set storage thread priority
						SetThreadPriority (hUploadThread, THREAD_PRIORITY_TIME_CRITICAL);

						// TODO: add clean-up code
					}
					
					//
					// memory allocation clean-up
					//
					// free EDF+ header
					if(pEDFPlusHeaderBuffer != NULL)
						free(pEDFPlusHeaderBuffer);

					// free EDF+ record storage structures
					if(m_riRecordingInfo.Equipment != NULL)
					{
						free(m_riRecordingInfo.Equipment);
						m_riRecordingInfo.Equipment = NULL;
					}

					// buffers
					if(m_pdblEEGDisplayBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							free(m_pdblEEGDisplayBuffer[i]);
						}

						free(m_pdblEEGDisplayBuffer);
					}

					if(m_pdblAEEGDisplayBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							free(m_pdblAEEGDisplayBuffer[i]);
						}

						free(m_pdblAEEGDisplayBuffer);
					}

					if(m_pdblDisplayBufferTemp == NULL)
					{
						free(m_pdblDisplayBufferTemp);
					}

					if(m_pshrSampleBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							free(m_pshrSampleBuffer[i]);
						}

						free(m_pshrSampleBuffer);
					}
					
					if(pshrSampleBuffer != NULL)
					{
						for(i=0; i < (EEGCHANNELS + ACCCHANNELS); i++)
						{
							free(pshrSampleBuffer[i]);
						}

						free(pshrSampleBuffer);
					}

					//
					// misc. clean-up 
					//
					// Destroy update timers
					KillTimer (hWnd, IDT_REDRAW_TIMER);
					KillTimer (hWnd, IDT_BATSTATUS_TIMER);
					KillTimer (hWnd, IDT_RTC_TIMER);

					//
					// GUI clean-up
					//
					// enable & disable relevant commands								
					GUI_SetEnabledCommands(hWnd, FALSE, gui);

					// clear status bar
					SendMessage (gui.hwndStatusBar, SB_SETTEXT, SB_STATUS_PART, (LPARAM) mc_strStateStrs [0]);
					SendMessage (gui.hwndStatusBar, SB_SETTEXT, SB_ANNOTATIONS_PART, (LPARAM) TEXT(""));
					SendMessage (gui.hwndStatusBar, SB_SETICON, SB_BATTERYICON_PART, (LPARAM) NULL);
					SendMessage (gui.hwndStatusBar, SB_SETTEXT, SB_RECTIME_PART, (LPARAM) TEXT(""));

					// log amount of lost packets and checksum errors
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("Number of packet checksum errors"), m_lngNPacketChecksumErrors, TRUE);
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("Number of lost packets"), m_intNPacketsLost, TRUE);

					// log start of recording & mark end of recording in application log
					applog_logevent(General, TEXT("Main"), TEXT("Recording Ended"), 0, TRUE);
					applog_endgrouping();

					// set exit status
					PostMessage(hWnd, EEGEMMsg_ExitPermission_Clear, ExitPermission_Denied_Recording, 0);
				break;
				
				case IDM_PATIENTINFO:
					DialogBox (m_hinMain, MAKEINTRESOURCE (IDD_PATIENTINFO), hWnd, (DLGPROC) Dialog_PatientInfo);
				break;
				
				case IDM_RECORDINGINFORMATION:
					DialogBox (m_hinMain, MAKEINTRESOURCE (IDD_RECORDINGINFORMATION), hWnd, (DLGPROC) Dialog_RecordingInfo);
				break;

				// Parameters dialog
				case IDM_PARAMETERS:
					GUI_DisplayPropertySheet(hWnd, gui);
				break;
				
				// EDF File Editor
				case IDM_UTILITIES_EDFFILEEDITOR:
					SecureZeroMemory (&pi, sizeof(PROCESS_INFORMATION));
					SecureZeroMemory (&si, sizeof(STARTUPINFO));
					si.cb = sizeof(STARTUPINFO);
					
					i = _tcslen(EDFFILEEDITOREXE) + _tcslen(m_cfgConfiguration.ApplicationPath) + 1; // +1 for \0
					strTemp = (TCHAR *) malloc(i*sizeof(TCHAR));
					_stprintf_s(strTemp, i, TEXT("%s%s"), m_cfgConfiguration.ApplicationPath, EDFFILEEDITOREXE);

					i = _tcslen(EDFFILEEDITOREXE) + 1;
					strTemp2 = (TCHAR *) malloc(i*sizeof(TCHAR));
					_stprintf_s(strTemp2, i, TEXT("%s"), EDFFILEEDITOREXE);

					if(CreateProcess(strTemp,
										strTemp2,
										NULL,
										NULL,
										FALSE,
										0,
										NULL,
										NULL,
										&si,
										&pi))
					{
						CloseHandle(si.hStdError);
						CloseHandle(si.hStdInput);
						CloseHandle(si.hStdOutput);
						CloseHandle(pi.hProcess);
						CloseHandle(pi.hThread);
					}
					else
						applog_logevent(SoftwareError, TEXT("Main"), TEXT("IDM_UTILITIES_EDFFILEEDITOR: Unable to start the EDFFileEditor application. (GetLastError #)"), GetLastError(), TRUE);

					// free allocated memory
					free(strTemp);
					free(strTemp2);
				break;

				case IDM_ABOUT:
					// SECURITY: full path for DLL not specified since software is assumed to run under
					//			 Win XP SP2 (and greater), which searches the system folders for DLLs first
					hlibRichEditV2 = LoadLibrary(TEXT("riched20.dll"));
					DialogBox (m_hinMain, MAKEINTRESOURCE (IDD_ABOUTBOX), hWnd, (DLGPROC) Dialog_About);
					FreeLibrary(hlibRichEditV2);
				break;

				case IDM_FULLSCREEN:
					GUI_SetFullScreenMode(hWnd, gui);
				break;
			}
		break;
	
		case WM_KEYUP:
			switch(wParam)
			{
				case VK_F2:
				case VK_F3:
				case VK_F4:
				case VK_F5:
				case VK_F6:
				case VK_F7:
				case VK_F8:
					if(blnRecordingStarted)
						main_InsertAnnotation(Regular, wParam - VK_F2, gui, hWnd);
				break;

				case VK_F11:
					GUI_SetFullScreenMode(hWnd, gui);
				break;

				case VK_SPACE:
					if(blnRecordingStarted)
						main_InsertAnnotation(Now, -1, gui, hWnd);
				break;
		
				default:
					return (DefWindowProc (hWnd, message, wParam, lParam));
			}
		break;
		
		// software is being closed
		case WM_CLOSE:
			// if recording is in progress, do not close window
			if(epExitPermission == ExitPermission_Allowed)
			{
					main_SaveCurrentSettings(gui);

					DestroyWindow(hWnd);
			}
			else
			{
				MsgPrintf(hWnd, MB_ICONERROR, TEXT("%s"), TEXT("Operations are still in progress. Please wait for the recording/streaming/uploading to finish before closing the application."));
			}
		break;

		// operating system is querying application whether shutdown/restart can proceed
		case WM_QUERYENDSESSION:
			// if recording is in progress, stop shutdown
			if(blnRecordingStarted)
				MsgPrintf(hWnd, MB_ICONERROR, TEXT("%s"), TEXT("EEG recording is currently taking place.\nThe recording must be first stopped before proceeding with the shutdown."));
			else
				return TRUE;
		break;

		// display resolution has changed
		case WM_DISPLAYCHANGE:
			// maximize the window, whatever the orientation or resolution
			ShowWindow(hWnd, SW_MAXIMIZE);
			
			// detect if orientation has changed
			if((HIWORD(lParam) > LOWORD(lParam)) !=	m_blnPortraitOrientation)
				m_blnPortraitOrientation = !m_blnPortraitOrientation;
		break;

		case WM_DEVICECHANGE:
			if(wParam != 0 && lParam != 0L)
			{
				// check if a port has changed
				pdbhPortBroadcast = (PDEV_BROADCAST_PORT) lParam;
				if(pdbhPortBroadcast->dbcp_devicetype == DBT_DEVTYP_PORT)
				{
					// check if a serial port has changed
					if(CompareString(LOCALE_INVARIANT,
									 NORM_IGNORECASE,
									 pdbhPortBroadcast->dbcp_name,
									 3,
									 TEXT("COM"),
									 3) == CSTR_EQUAL)
					{
						// check if WEEG serial port has changed
						sztLength = _tcslen(pdbhPortBroadcast->dbcp_name) - 3;
						strTemp = (TCHAR *) calloc (sztLength + 1, sizeof(TCHAR));
						_tcsncpy_s(strTemp, sztLength + 1, pdbhPortBroadcast->dbcp_name + 3, sztLength);
						if(_wtoi(strTemp) == m_cfgConfiguration.COMPortIndex + 1)
						{
							switch(wParam)
							{
								// WEEG coordinator has been inserted
								case DBT_DEVICEARRIVAL:
									if(blnRecordingStarted)
									{
										// annotate event in EDF file
										main_InsertAnnotation(CoordinatorInserted, 0, gui, hWnd);
										
										// log event
										applog_logevent(HardwareError, TEXT("Main"), TEXT("WEEG coordinator re-inserted during recording."), 0, TRUE);

										// restart sampling
										WaitForSingleObject(std.hevSampleThread_Idle, INFINITE);
										std.Mode = SampleThreadMode_Recording;
										std.pModeData = NULL;
										SetEvent(std.hevSampleThread_Start);
									}
									else
									{
										SendMessage(gui.hwndStatusBar, SB_SETTEXT, SB_ANNOTATIONS_PART, (LPARAM) TEXT("Activity detected on WEEG COM port. WEEG system check in progress..."));
										main_CheckWEEGSystem(NULL, 0, &std, gui);
									}
								break;
								
								// WEEG coordinator has been removed
								case DBT_DEVICEREMOVECOMPLETE:
									if(blnRecordingStarted)
									{
										// annotate event in EDF file
										main_InsertAnnotation(CoordinatorRemoved, 0, gui, hWnd);

										// log event
										applog_logevent(HardwareError, TEXT("Main"), TEXT("WEEG coordinator removed during recording."), 0, TRUE);
									}
									else
									{
										main_CheckWEEGSystem(NULL, 0, &std, gui);
									}
								break;
							}
						}
						
						free(strTemp);
					}
				}
			}
		break;

		case WM_DESTROY:
			// streaming thread
			if(WaitForSingleObject(sctd.hevVortexClient_Exiting, 0) != WAIT_OBJECT_0)
			{
				sctd.ExitThread = TRUE;											// set flag that thread that should exist
				SetEvent(sctd.hevVortexClient_Connect_Start);					// make thread exit the Waiting2Connect state
				WaitForSingleObject(sctd.hevVortexClient_Exiting, INFINITE);	// wait for thread to exit
			}
			CloseHandle (hStreamingThread);

			//
			// stop threads
			//
			CloseHandle (hStorageThread);
			CloseHandle (hSampleThread);
			CloseHandle (hWiFiThread);

			// Delete icons
			DestroyIcon(gui.hIconsBatteryStates[0]);
			DestroyIcon(gui.hIconsBatteryStates[1]);
			
			// Release Mutex objects
			CloseHandle(m_hMutexAnnotation);
			CloseHandle(m_hMutexSampleBuffer);
			
			// release created events for sample thread
			CloseHandle(std.hevSampleThread_Start);
			CloseHandle(std.hevSampleThread_Idle);
			CloseHandle(std.hevSampleThread_Init);

			// release created events for storage thread
			CloseHandle(sttd.hevStorageThread_Idling);
			CloseHandle(sttd.hevStorageThread_Init_End);
			CloseHandle(sttd.hevStorageThread_Init_Start);
			CloseHandle(sttd.hevStorageThread_Write);
			
			// release created events for streaming thread
			CloseHandle(sctd.hevThreadInit_Complete);
			CloseHandle(sctd.hevVortexClient_Connecting_Start);
			CloseHandle(sctd.hevVortexClient_Connecting_End);
			CloseHandle(sctd.hevVortexClient_Connect_Start);
			CloseHandle(sctd.hevVortexClient_Exiting);
			CloseHandle(sctd.hevVortexClient_Transmit);
			CloseHandle(sctd.hevVortexClient_WaitingToConnect);
			CloseHandle(sctd.hevVortexClient_WaitingToTransmit);
			
			// reset screen saver configuration to its initial state
			SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, m_blnScreenSaverActive, 0, 0);
		
			// Close COM library on user interface thread
			CoUninitialize();

			// unregister window classes
			UnregisterClass(WINDOW_CLASSID_MAIN, m_hinMain);
			UnregisterClass(WINDOW_CLASSID_ANNOT, m_hinMain);

			// close log file
			applog_close();

			PostQuitMessage (0);					// ... and quit.
		break;

		//
		// EEGEM-defined messages
		//
		// set exit permission
		case EEGEMMsg_ExitPermission_Set:
			epExitPermission |= wParam;
		break;

		// clear exit permission
		case EEGEMMsg_ExitPermission_Clear:
			epExitPermission &= ~wParam;
		break;

		// update status displayed in status bar
		case EEGEMMsg_StatusBar_SetAnnotation:
			// if wParam is FALSE, lParam points to a char string, which must be converted to a TCHAR
			if(wParam)
				_tcscpy_s(strStatusBarAnnotation, _countof(strStatusBarAnnotation), (TCHAR *) lParam);
			else
			{
				mbstowcs_s(&sztLength, strBuffer, sizeof(strBuffer)/sizeof(WORD), (char *) lParam, _TRUNCATE);
				_stprintf_s(strStatusBarAnnotation, _countof(strStatusBarAnnotation), TEXT("+%d: %s"), m_uintTimeKeepingTAL, strBuffer);
			}
			
			PostMessage(gui.hwndStatusBar, SB_SETTEXT, SB_ANNOTATIONS_PART, (LPARAM) strStatusBarAnnotation);
		break;

		// update status displayed in status bar
		case EEGEMMsg_StatusBar_SetStatus:
			_tcscpy_s(strStatusBarStatus, _countof(strStatusBarStatus), (TCHAR *) lParam);
			PostMessage(gui.hwndStatusBar, SB_SETTEXT, SB_STATUS_PART, (LPARAM) strStatusBarStatus);
		break;

		// update streaming thread status displayed on the second line of the rebar
		case EEGEMMsg_Streaming_DisplayStatus:
			switch((StreamingClientState) wParam)
			{
				case StreamingClientState_None:
				case StreamingClientState_Exit:
					PostMessage(gui.hwndSTAStreamingStatus, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) gui.hStreamingStatusImages[0]);
				break;

				case StreamingClientState_Init:
				case StreamingClientState_Waiting2Connect:
					PostMessage(gui.hwndSTAStreamingStatus, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) gui.hStreamingStatusImages[1]);
				break;
				
				case StreamingClientState_Connecting:
					PostMessage(gui.hwndSTAStreamingStatus, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) gui.hStreamingStatusImages[2]);
				break;
				
				case StreamingClientState_DataStreaming:
					PostMessage(gui.hwndSTAStreamingStatus, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) gui.hStreamingStatusImages[3]);
				break;
				
				case StreamingClientState_Cleanup:
					PostMessage(gui.hwndSTAStreamingStatus, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) gui.hStreamingStatusImages[4]);
				break;
				
				default:
					PostMessage(gui.hwndSTAStreamingStatus, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM) gui.hStreamingStatusImages[0]);
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("EEGEMMsg_Streaming_DisplayStatus: Unknown status message was received. (wParam value)"), wParam, TRUE);
			}
		break;
		
		default:
			return (DefWindowProc (hWnd, message, wParam, lParam));
	}

	return (0);
}

static BOOL CALLBACK Dialog_Settings (HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL				blnHandled = FALSE, blnAllowDeactivation = FALSE;
	BROWSEINFO			bi;
	DWORD				d;
	HWND				hwndComboBox, hwndEdit, hwndOwner, hwndPropertySheet;
	int					i;
	PIDLIST_ABSOLUTE	pidFilePath;
	PSHNOTIFY *			phdr;
	RECT				rc, rcDlg, rcOwner;
	TCHAR				strBuffer [256];
	UINT				State;
	unsigned int		uintAvailableRecordingTime[2], uintTemp;

	int CheckBoxIDs [EEGCHANNELS] = {IDC_CH0, IDC_CH1, IDC_CH2, IDC_CH3, IDC_CH4, IDC_CH5};

	static TCHAR		FilePath[MAX_PATH + 1] = TEXT("");

	switch (uMsg)
	{
		case WM_INITDIALOG:
			//
			// center property sheet
			//
			// Get handle to property sheet
			hwndPropertySheet = GetParent(hwndDlg);

			// Get the owner window and dialog box rectangles. 
			if ((hwndOwner = GetParent(hwndPropertySheet)) == NULL) 
				hwndOwner = GetDesktopWindow(); 

			GetWindowRect(hwndOwner, &rcOwner); 
			GetWindowRect(hwndPropertySheet, &rcDlg); 
			CopyRect(&rc, &rcOwner); 

			// Offset the owner and dialog box rectangles so that right and bottom 
			// values represent the width and height, and then offset the owner again 
			// to discard space taken up by the dialog box. 
			OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
			OffsetRect(&rc, -rc.left, -rc.top); 
			OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

			// The new position is the sum of half the remaining space and the owner's 
			// original position. 
			SetWindowPos(hwndPropertySheet, 
						 HWND_TOP, 
						 rcOwner.left + (rc.right / 2), 
						 rcOwner.top + (rc.bottom / 2), 
						 0, 0,          // Ignores size arguments. 
						 SWP_NOSIZE | SWP_NOZORDER );

			//
			// initialize dialog
			//
			// Init channel buttons
			for (i = 0; i < _countof(CheckBoxIDs); i++)
			{
				if ((0x01 << i) & m_cfgConfiguration.DisplayChannelMask)
					CheckDlgButton (hwndDlg, CheckBoxIDs [i], BST_CHECKED);	
			}

			// Init serial port list
			hwndComboBox = GetDlgItem (hwndDlg, IDC_SERPORT);		
			for (i = 0; i < NSERPORTS; i++)
			{
				_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("COM%03d"), i + 1);
				SendMessage (hwndComboBox, CB_ADDSTRING, 0, (LPARAM) strBuffer);
			}
			SendMessage (hwndComboBox, CB_SETCURSEL, m_cfgConfiguration.COMPortIndex, 0);
			//SelectedSerial = m_cfgConfiguration.COMPortIndex;

			// Init sampling frequency
			hwndEdit = GetDlgItem (hwndDlg, IDC_FSAMPLE);
			SetDlgItemInt (hwndDlg, IDC_FSAMPLE, m_cfgConfiguration.SamplingFrequency, FALSE);

			// Init filename
			hwndEdit = GetDlgItem (hwndDlg, IDC_FNAME);
			SendMessage (hwndEdit, EM_SETLIMITTEXT, MAX_PATH_DEST_FOLDER, 0);
			SetDlgItemText (hwndDlg, IDC_FNAME, m_cfgConfiguration.DestinationFolder);
			_tcscpy_s (FilePath, sizeof(FilePath)/sizeof(TCHAR), m_cfgConfiguration.DestinationFolder);
		break;
 		
		case WM_NOTIFY:
			phdr = (LPPSHNOTIFY) lParam;
			switch (phdr->hdr.code)
			{
				// the page is about to loose its focus (user clicked OK or selected another page)
				// validate user input
				case PSN_KILLACTIVE:
					blnAllowDeactivation = TRUE;

					// Test COM port (if not in Simulation mode
					if(!m_cfgConfiguration.SimulationMode)
					{
						if (serial_OpenPort (m_cfgConfiguration.COMPortIndex + 1) != ERROR_SUCCESS)
						{
							MsgPrintf (hwndDlg, MB_ICONSTOP, TEXT("The selected COM port is invalid. Please choose another port."));
						}
						else
							serial_ClosePort();
					}
					
					// check that the destination folder is valid
					if (blnAllowDeactivation)
					{
						d = GetFileAttributes(FilePath);
						if((d != INVALID_FILE_ATTRIBUTES ) && (d & FILE_ATTRIBUTE_DIRECTORY))
						{
							PathRemoveBackslash(FilePath);
						}
						else
						{
							MsgPrintf (hwndDlg, MB_ICONSTOP, TEXT("The given destination folder is invalid. Please choose another folder."));
							blnAllowDeactivation = FALSE;
						}
					}

					// check that the sampling frequency is valid, and if so, check that the channel limit is not exceeded
					if (blnAllowDeactivation)
					{
						hwndEdit = GetDlgItem (hwndDlg, IDC_FSAMPLE);
						uintTemp = GetDlgItemInt (hwndDlg, IDC_FSAMPLE, NULL, FALSE);
						if((uintTemp >= MIN_SAMPLERATE) && (uintTemp <= MAX_SAMPLERATE))
						{
							if(util_IsMaxNChannelsExceeded(uintTemp, m_cfgConfiguration.DisplayChannelMask, &uintTemp))
							{
								_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Maximum number of measurement channels exceeded! At the current sampling frequency, at most %u channels can be selected."), uintTemp);
								MsgPrintf (hwndDlg, MB_ICONSTOP, strBuffer);
								blnAllowDeactivation = FALSE;
							}
						}
						else
						{
							_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Invalid sampling frequency! The sampling frequency must be in the %u-%u Hz range."), MIN_SAMPLERATE, MAX_SAMPLERATE);							
							MsgPrintf (hwndDlg, MB_ICONSTOP, strBuffer);
							blnAllowDeactivation = FALSE;
						}
					}
					
					// inform property sheet manager whether or not the settings sheet can be deactivated
					if(blnAllowDeactivation)
					{
						// settings sheet can be deactivated
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, FALSE);
					}
					else
					{
						// settings sheet should not be deactivated
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, TRUE);
					}
					
					// PSN_KILLACTIVE notification has been handled
					return TRUE;
				break;

				case PSN_APPLY:
					// user clicked on the OK button
					if(phdr->lParam == TRUE)
					{
						// Store configuration data
						_tcscpy_s (m_cfgConfiguration.DestinationFolder, sizeof(m_cfgConfiguration.DestinationFolder)/sizeof(TCHAR), FilePath);
	
						// notify property sheet manager that the changes made to this page are valid and have been applied
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
						return TRUE;
					}
				break;
			}
		break;

		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				case IDC_CH0:									// Channel toggled, store change
				case IDC_CH1:
				case IDC_CH2:
				case IDC_CH3:
				case IDC_CH4:
				case IDC_CH5:
					for (i = 0; i < EEGCHANNELS && CheckBoxIDs [i] != LOWORD (wParam); i++);
					
					State = IsDlgButtonChecked (hwndDlg, LOWORD (wParam));
					
					switch(State)
					{
						case BST_CHECKED:
							m_cfgConfiguration.DisplayChannelMask |= (0x01 << i);
						break;
						
						case BST_UNCHECKED:
							m_cfgConfiguration.DisplayChannelMask &= ~(0x01 << i);
						break;
					}
				break;
				
				// Serial port selected, find port number
				case IDC_SERPORT:
					hwndComboBox = GetDlgItem (hwndDlg, IDC_SERPORT);
					m_cfgConfiguration.COMPortIndex = (int) SendMessage (hwndComboBox, CB_GETCURSEL, 0, 0);
				break;
				
				// Read entered filename
				case IDC_FNAME:
					GetDlgItemText (hwndDlg, IDC_FNAME, FilePath, _countof(FilePath));
					if(main_GetMaxAvailableRecordingTime(FilePath, uintAvailableRecordingTime) == 0)
					{
						_stprintf_s(strBuffer,
									sizeof(strBuffer)/sizeof(TCHAR),
									TEXT("Maximum possible recording length: %d:%02d (hours:minutes)."),
								    uintAvailableRecordingTime[0], uintAvailableRecordingTime[1]);
						SetDlgItemText (hwndDlg, IDC_DISKSPACE, strBuffer);
					}
					else
						SetDlgItemText (hwndDlg, IDC_DISKSPACE, TEXT("Could not determine available disk space!"));
				break;

				// Sampling frequency changed
				case IDC_FSAMPLE:
					hwndEdit = GetDlgItem (hwndDlg, IDC_FSAMPLE);
					m_cfgConfiguration.SamplingFrequency = GetDlgItemInt (hwndDlg, IDC_FSAMPLE, NULL, FALSE);
				break;
				
				// Pressed folder selection button
				case IDC_FNAME_BUTTON:
					// initialize BROWSEINFO structures
					SecureZeroMemory (&bi, sizeof(bi));
					bi.hwndOwner = hwndDlg;
					bi.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;
					bi.lpszTitle = TEXT("Select the folder where the EEG measurements should be saved:");
					bi.lpfn = Dialog_Settings_SHBrowseForFolder;

					// display browse for folder dialog
					pidFilePath = SHBrowseForFolder(&bi);
					if(pidFilePath != NULL)
					{
						if(SHGetPathFromIDList(pidFilePath, FilePath))
							SetDlgItemText (hwndDlg, IDC_FNAME, FilePath);
					}
				break;
			}
		break;
	}

	return blnHandled;
}

static int CALLBACK Dialog_Settings_SHBrowseForFolder(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	char chrPathCheckCode = -1;
	HANDLE hTestFile;
	TCHAR strBuffer[MAX_PATH + 1], strTestFile[MAX_PATH + 1], strTempFilePrefix[TMPFILE_PRFX_LEN + 1];

	switch(uMsg)
	{
		case BFFM_SELCHANGED:
			SHGetPathFromIDList((PCIDLIST_ABSOLUTE) lParam, strBuffer);
			if(_tcslen(strBuffer) > MAX_PATH_DEST_FOLDER)
				chrPathCheckCode = 0;
			else
			{
				// get random prefix
				if(util_GetRandomPrefix(strTempFilePrefix, TMPFILE_PRFX_LEN + 1) == 0)
				{
					// create temporary file
					if(GetTempFileName(strBuffer, strTempFilePrefix, 0, strTestFile) != 0)
					{
						// create temporary file with exactly the same rights as the actual temporary file
						// that will be used during recording
						hTestFile = CreateFile(strTestFile,
											   GENERIC_READ | GENERIC_WRITE,					// R/W rights (no execute)
											   0,												// don't share file
											   NULL,											// default security descriptor
											   CREATE_ALWAYS,									// create new file, always
											   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
											   NULL);
					   if(hTestFile == INVALID_HANDLE_VALUE)
						   chrPathCheckCode = 1;
					   else
						   CloseHandle(hTestFile);
					   
					   DeleteFile(strTestFile);
					}
					else
						chrPathCheckCode = 1;
				}
			}

			switch(chrPathCheckCode)
			{
				case 0:
					SendMessage(hwnd, BFFM_ENABLEOK, 0, FALSE);
					SendMessage(hwnd, BFFM_SETOKTEXT, 0, (LPARAM) TEXT("Too Long"));
				break;

				case 1:
					SendMessage(hwnd, BFFM_ENABLEOK, 0, FALSE);
					SendMessage(hwnd, BFFM_SETOKTEXT, 0, (LPARAM) TEXT("Not Allowed"));
				break;
				
				default:
					SendMessage(hwnd, BFFM_ENABLEOK, 0, TRUE);
					SendMessage(hwnd, BFFM_SETOKTEXT, 0, (LPARAM) TEXT("OK"));
			}
		break;
	}

	return 0;
}

static BOOL CALLBACK Dialog_Annotation (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndControl;
	int intAnnotationsEditHandles[] = {IDC_AN1, IDC_AN2, IDC_AN3, IDC_AN4, IDC_AN5, IDC_AN6, IDC_AN7};
	PSHNOTIFY * phdr;
	TCHAR strTemp[ANNOTATION_MAX_CHARS + 1];
	unsigned int i = 0;

	switch (uintMsg)
	{
		case WM_INITDIALOG:
			for(i = 0; i < ANNOTATION_MAX_TYPES; i++)
			{
				// set maximum number of characters that can be entered in the annotation text boxes
				// and make text boxes only accept ASCII characters in order to be compatible
				// with EDF+ standard
				hwndControl = GetDlgItem(hwndDlg, intAnnotationsEditHandles[i]);
				SendMessage(hwndControl, EM_SETLIMITTEXT, ANNOTATION_MAX_CHARS, 0);
				main_MakeEditControlASCIIMasked(hwndControl);
				
				SetDlgItemText (hwndDlg, intAnnotationsEditHandles[i], m_cfgConfiguration.Annotations[i]);
			}
		break;

		case WM_NOTIFY:
			phdr = (LPPSHNOTIFY) lParam;
			switch (phdr->hdr.code)
			{
				case PSN_APPLY:
					// user clicked on the OK button
					if(phdr->lParam == TRUE)
					{
						// Save Changes
						for(i=0; i<ANNOTATION_MAX_TYPES; i++)
						{
							GetDlgItemText (hwndDlg, intAnnotationsEditHandles[i], strTemp, sizeof(strTemp)/sizeof(TCHAR));
							edf_CorrectSpecialCharacters(strTemp, TRUE);
							_tcsncpy_s(m_cfgConfiguration.Annotations[i],
								       sizeof(m_cfgConfiguration.Annotations[0])/sizeof(TCHAR),
									   strTemp,
									   ANNOTATION_MAX_CHARS);
						}

						// if annotation display window is open, send repaint command
						if(m_hwndAnnotationsDisplay != NULL)
							RedrawWindow(m_hwndAnnotationsDisplay, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);
							
						// notify property sheet manager that the changes made to this page are valid and have been applied
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
 						return TRUE;
					}
				break;
			}
		break;
	}

	return (0);
}

static BOOL CALLBACK Dialog_PatientInfo (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	HWND hwndComboBox, hwndEdit;
	int intTemp, i;
	LRESULT lResult;
	TCHAR strMonthNames[12][4] = {TEXT("JAN"), TEXT("FEB"), TEXT("MAR"), TEXT("APR"), TEXT("MAY"), TEXT("JUN"),
								 TEXT("JUL"), TEXT("AUG"), TEXT("SEP"), TEXT("OCT"), TEXT("NOV"), TEXT("DEC") };
	TCHAR strTemp[128];
	struct tm date;
	
	switch (uintMsg)
	{
		case WM_INITDIALOG:
			SetDlgItemText (hwndDlg, IDC_PIC, m_piPatientInfo.PIC);
			hwndEdit = GetDlgItem (hwndDlg, IDC_PIC);
			SendMessage (hwndEdit, EM_SETLIMITTEXT, PIC_SIZE, 0);
			main_MakeEditControlASCIIMasked(hwndEdit);

			SetDlgItemText (hwndDlg, IDC_NAME, m_piPatientInfo.Name);
			hwndEdit = GetDlgItem (hwndDlg, IDC_NAME);
			SendMessage (hwndEdit, EM_SETLIMITTEXT, NAME_SIZE, 0);
			main_MakeEditControlASCIIMasked(hwndEdit);
			
			if(m_piPatientInfo.BirthDay > 0)
			{
				_stprintf_s(strTemp, sizeof(strTemp)/sizeof(TCHAR), TEXT("%02d"), m_piPatientInfo.BirthDay);
				SetDlgItemText (hwndDlg, IDC_BDATEDAY, strTemp);
			}
			else
				SetDlgItemText (hwndDlg, IDC_BDATEDAY, TEXT(""));
			hwndEdit = GetDlgItem (hwndDlg, IDC_BDATEDAY);
			SendMessage (hwndEdit, EM_SETLIMITTEXT, 2, 0);
			
			hwndComboBox = GetDlgItem (hwndDlg, IDC_BDATEMONTH);
			for (i = 0; i < 12; i++)
				SendMessage (hwndComboBox, CB_ADDSTRING, 0, (LPARAM) strMonthNames[i]);
			if(m_piPatientInfo.BirthMonth > 0)
				SendMessage (hwndComboBox, CB_SETCURSEL, m_piPatientInfo.BirthMonth - 1, 0);
			else
				SendMessage (hwndComboBox, CB_SETCURSEL, 0, 0);
			
			if(m_piPatientInfo.BirthYear > 0)
			{
				_stprintf_s(strTemp, sizeof(strTemp)/sizeof(TCHAR), TEXT("%02d"), m_piPatientInfo.BirthYear);
				SetDlgItemText (hwndDlg, IDC_BDATEYEAR, strTemp);
			}
			else
				SetDlgItemText (hwndDlg, IDC_BDATEYEAR, TEXT(""));
			hwndEdit = GetDlgItem (hwndDlg, IDC_BDATEYEAR);
			SendMessage (hwndEdit, EM_SETLIMITTEXT, 4, 0);

			switch(m_piPatientInfo.Sex)
			{
				case TEXT('M'):
					CheckDlgButton (hwndDlg, IDC_SEXM, BST_CHECKED);
					CheckDlgButton (hwndDlg, IDC_SEXF, BST_UNCHECKED);
					CheckDlgButton (hwndDlg, IDC_SEXU, BST_UNCHECKED);
				break;

				case TEXT('F'):
					CheckDlgButton (hwndDlg, IDC_SEXF, BST_CHECKED);
					CheckDlgButton (hwndDlg, IDC_SEXM, BST_UNCHECKED);
					CheckDlgButton (hwndDlg, IDC_SEXU, BST_UNCHECKED);
				break;

				default:
					CheckDlgButton (hwndDlg, IDC_SEXU, BST_CHECKED);
					CheckDlgButton (hwndDlg, IDC_SEXM, BST_UNCHECKED);
					CheckDlgButton (hwndDlg, IDC_SEXF, BST_UNCHECKED);
				break;
			}
			
			Dialog_PatientInfo_SetNCharsLeft(hwndDlg);

			// set focus on the name text box
			hwndEdit = GetDlgItem (hwndDlg, IDC_NAME);
			SetFocus(hwndEdit);
		break;
    
		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				case IDC_NAME:
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_PatientInfo_SetNCharsLeft(hwndDlg);
				break;
				
				case IDC_PIC:
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_PatientInfo_SetNCharsLeft(hwndDlg);
				break;

				case IDOK:
					// Save PIC & name
					intTemp = GetDlgItemText(hwndDlg, IDC_NAME, strTemp, sizeof(strTemp)/sizeof(TCHAR));
					if(intTemp > 0)
					{
						// free previously allocated memory
						free(m_piPatientInfo.Name);
						m_piPatientInfo.Name = NULL;
						
						// allocate new memory and store data
						m_piPatientInfo.Name = (TCHAR *) malloc((intTemp + 1)*sizeof(TCHAR));
						_stprintf_s(m_piPatientInfo.Name, intTemp + 1, TEXT("%s"), strTemp);
					}
					intTemp = GetDlgItemText(hwndDlg, IDC_PIC, strTemp, sizeof(strTemp)/sizeof(TCHAR));
					if(intTemp > 0)
					{
						// free previously allocated memory
						free(m_piPatientInfo.PIC);
						m_piPatientInfo.PIC = NULL;
						
						// allocate new memory and store data
						m_piPatientInfo.PIC = (TCHAR *) malloc((intTemp + 1)*sizeof(TCHAR));
						if(m_piPatientInfo.PIC != NULL)
							_stprintf_s(m_piPatientInfo.PIC, intTemp + 1, TEXT("%s"), strTemp);
						else
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_PatientInfo() - WM_COMMAND - IDOK: Failed to allocate memory for m_piPatientInfo.PIC. (errno #)"), errno, TRUE);
					}
					
					// save birtday date
					GetDlgItemText (hwndDlg, IDC_BDATEDAY, strTemp, sizeof(strTemp)/sizeof(TCHAR));
					intTemp = _tstoi(strTemp);
					if(intTemp == 0 && _tcslen(strTemp) > 0)
					{
						MsgPrintf(hwndDlg, MB_ICONERROR, TEXT("%s"), TEXT("The day of birth is invalid!"));
						return FALSE;
					}
					m_piPatientInfo.BirthDay = intTemp;
					
					hwndComboBox = GetDlgItem (hwndDlg, IDC_BDATEMONTH);
					lResult = SendMessage(hwndComboBox, CB_GETCURSEL, 0, 0);
					if(lResult != CB_ERR)
						m_piPatientInfo.BirthMonth = ((int) lResult) + 1;
					
					date = util_GetCurrentDateTime();
					GetDlgItemText (hwndDlg, IDC_BDATEYEAR, strTemp, sizeof(strTemp)/sizeof(TCHAR));
					intTemp = _tstoi(strTemp);
					if((intTemp < 1900 || intTemp > (date.tm_year + 1900)) && _tcslen(strTemp) > 0)
					{
						MsgPrintf(hwndDlg, MB_ICONERROR, TEXT("%s"), TEXT("The year of birth is invalid!"));
						return FALSE;
					}
					m_piPatientInfo.BirthYear = intTemp;
					
					// save patient gender
					if(IsDlgButtonChecked (hwndDlg, IDC_SEXM) == BST_CHECKED)
						m_piPatientInfo.Sex = 'M';
					if(IsDlgButtonChecked (hwndDlg, IDC_SEXF) == BST_CHECKED)
						m_piPatientInfo.Sex = 'F';

					// set flag indicating that valid data is stored in PatientInfo structure
					m_piPatientInfo.ContainsValidData = TRUE;

					EndDialog (hwndDlg, 0);
				break;

				case IDCANCEL:
					// Close, discard changes
					EndDialog (hwndDlg, 0);
				break;
			}
		break;

		case WM_CLOSE:
			// Close, discard changes
			EndDialog (hwndDlg, 0);
		break;
	}

	return (0);
}

static BOOL CALLBACK Dialog_RecordingInfo (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR strBuffer[EDFTRANSDUCERTYPELENGTH + 1];						// make the buffer as big as the largest field it can contain + 1
	HWND hwndControl;
	int intNCharsRemaining, intTextLength;

	switch (uintMsg)
	{
		case WM_INITDIALOG:
			// set maximum number of characters that can be entered in the annotation text boxes
			// and make text boxes only accept ASCII characters in order to be compatible
			// with EDF+ standard
			hwndControl = GetDlgItem(hwndDlg, IDC_ADMINISTRATIONCODE);
			main_MakeEditControlASCIIMasked(hwndControl);

			hwndControl = GetDlgItem(hwndDlg, IDC_TECHNICIAN);
			main_MakeEditControlASCIIMasked(hwndControl);

			hwndControl = GetDlgItem(hwndDlg, IDC_DEVICE);
			main_MakeEditControlASCIIMasked(hwndControl);

			hwndControl = GetDlgItem(hwndDlg, IDC_COMMENTS);
			//SendMessage(hwndControl, EM_SETLIMITTEXT, EDFRECORDINGSPACE, 0);
			main_MakeEditControlASCIIMasked(hwndControl);

			hwndControl = GetDlgItem(hwndDlg, IDC_ELECTRODES);
			SendMessage(hwndControl, EM_SETLIMITTEXT, EDFTRANSDUCERTYPELENGTH, 0);
			main_MakeEditControlASCIIMasked(hwndControl);

			hwndControl = GetDlgItem(hwndDlg, IDC_ADDITIONALCOMMENTS);
			SendMessage(hwndControl, EM_SETLIMITTEXT, EDFRESERVEDSPACE, 0);
			main_MakeEditControlASCIIMasked(hwndControl);

			// displayed stored recording information
			if(m_riRecordingInfo.HospitalCode != NULL)
				SetDlgItemText(hwndDlg, IDC_ADMINISTRATIONCODE, m_riRecordingInfo.HospitalCode);
			if(m_riRecordingInfo.Technician != NULL)
				SetDlgItemText(hwndDlg, IDC_TECHNICIAN, m_riRecordingInfo.Technician);
			if(m_riRecordingInfo.Equipment != NULL)
				SetDlgItemText(hwndDlg, IDC_DEVICE, m_riRecordingInfo.Equipment);
			if(m_riRecordingInfo.Technician != NULL)
				SetDlgItemText(hwndDlg, IDC_TECHNICIAN, m_riRecordingInfo.Technician);
			if(m_riRecordingInfo.Comments != NULL)
				SetDlgItemText(hwndDlg, IDC_COMMENTS, m_riRecordingInfo.Comments);
			if(m_cfgConfiguration.ElectrodeType != NULL)
				SetDlgItemText(hwndDlg, IDC_ELECTRODES, m_cfgConfiguration.ElectrodeType);
			if(m_riRecordingInfo.AdditionalComments != NULL)
				SetDlgItemText(hwndDlg, IDC_ADDITIONALCOMMENTS, m_riRecordingInfo.AdditionalComments);

			// display amount of characters remaining
			Dialog_RecordingInfo_SetNCharsLeft1(hwndDlg);
			Dialog_RecordingInfo_SetNCharsLeft2(hwndDlg);
			Dialog_RecordingInfo_SetNCharsLeft3(hwndDlg);

			// set focus on the EEG # text box
			hwndControl = GetDlgItem (hwndDlg, IDC_ADMINISTRATIONCODE);
			SetFocus(hwndControl);
		break;
    
		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				// controls
				case IDC_ADMINISTRATIONCODE:
					// control is about to redraw itself (before text is displayed)
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_RecordingInfo_SetNCharsLeft1(hwndDlg);
				break;

				case IDC_TECHNICIAN:
					// control is about to redraw itself (before text is displayed)
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_RecordingInfo_SetNCharsLeft1(hwndDlg);
				break;

				case IDC_DEVICE:
					// control is about to redraw itself (before text is displayed)
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_RecordingInfo_SetNCharsLeft1(hwndDlg);
				break;

				case IDC_COMMENTS:
					// control is about to redraw itself (before text is displayed)
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_RecordingInfo_SetNCharsLeft1(hwndDlg);
				break;

				case IDC_ELECTRODES:
					// control is about to redraw itself (before text is displayed)
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_RecordingInfo_SetNCharsLeft3(hwndDlg);
				break;

				case IDC_ADDITIONALCOMMENTS:
					// control is about to redraw itself (before text is displayed)
					if(HIWORD(wParam) == EN_UPDATE)
						Dialog_RecordingInfo_SetNCharsLeft2(hwndDlg);
				break;

				// buttons
				case IDOK:
					// save equipment
					intNCharsRemaining = EDFRECORDINGSPACE;
					intTextLength = GetDlgItemText(hwndDlg, IDC_DEVICE, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
					if((intTextLength > 0) && (intNCharsRemaining > 0))
					{
						// ensure that data does not overflow EDF+ header
						if((intNCharsRemaining - intTextLength) < 0)
							intTextLength = intNCharsRemaining;
						
						// free previously allocated memory
						free(m_riRecordingInfo.Equipment);
						m_riRecordingInfo.Equipment = NULL;
						
						// allocate new memory and store data
						m_riRecordingInfo.Equipment = (TCHAR *) calloc(intTextLength + 1, sizeof(TCHAR));
						memcpy_s(m_riRecordingInfo.Equipment, (intTextLength + 1)*sizeof(TCHAR), strBuffer, intTextLength*sizeof(TCHAR));
						m_riRecordingInfo.Equipment[intTextLength] = TEXT('\0');
						
						// update amount of room left in EDF+ header
						intNCharsRemaining -= intTextLength;
					}

					// save hospital code
					intTextLength = GetDlgItemText(hwndDlg, IDC_ADMINISTRATIONCODE, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
					if((intTextLength > 0) && (intNCharsRemaining > 0))
					{
						// ensure that data does not overflow EDF+ header
						if((intNCharsRemaining - intTextLength) < 0)
							intTextLength = intNCharsRemaining;
						
						// free previously allocated memory
						free(m_riRecordingInfo.HospitalCode);
						m_riRecordingInfo.HospitalCode = NULL;
						
						// allocate new memory and store data
						m_riRecordingInfo.HospitalCode = (TCHAR *) calloc(intTextLength + 1, sizeof(TCHAR));
						if(m_riRecordingInfo.HospitalCode != NULL)
						{
							memcpy_s(m_riRecordingInfo.HospitalCode, (intTextLength + 1)*sizeof(TCHAR), strBuffer, intTextLength*sizeof(TCHAR));
							m_riRecordingInfo.HospitalCode[intTextLength] = TEXT('\0');
				
							// update amount of room left in EDF+ header
							intNCharsRemaining -= intTextLength;
						}
						else
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_RecordingInfo() - WM_COMMAND - IDOK: Failed to allocate memory for m_riRecordingInfo.HospitalCode. (errno #)"), errno, TRUE);
					}
					
					// save technician
					intTextLength = GetDlgItemText(hwndDlg, IDC_TECHNICIAN, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
					if((intTextLength > 0) && (intNCharsRemaining > 0))
					{
						// ensure that data does not overflow EDF+ header
						if((intNCharsRemaining - intTextLength) < 0)
							intTextLength = intNCharsRemaining;
						
						// free previously allocated memory
						free(m_riRecordingInfo.Technician);
						m_riRecordingInfo.Technician = NULL;
						
						// allocate new memory and store data
						m_riRecordingInfo.Technician = (TCHAR *) calloc(intTextLength + 1, sizeof(TCHAR));
						if(m_riRecordingInfo.Technician != NULL)
						{
							memcpy_s(m_riRecordingInfo.Technician, (intTextLength + 1)*sizeof(TCHAR), strBuffer, intTextLength*sizeof(TCHAR));
							m_riRecordingInfo.Technician[intTextLength] = TEXT('\0');
						
							// update amount of room left in EDF+ header
							intNCharsRemaining -= intTextLength;
						}
						else
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_RecordingInfo() - WM_COMMAND - IDOK: Failed to allocate memory for m_riRecordingInfo.Technician. (errno #)"), errno, TRUE);
					}
					
					// save comments
					intTextLength = GetDlgItemText(hwndDlg, IDC_COMMENTS, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
					if((intTextLength > 0) && (intNCharsRemaining > 0))
					{
						// ensure that data does not overflow EDF+ header
						if((intNCharsRemaining - intTextLength) < 0)
							intTextLength = intNCharsRemaining;
						
						// free previously allocated memory
						free(m_riRecordingInfo.Comments);
						m_riRecordingInfo.Comments = NULL;
						
						// allocate new memory and store data
						m_riRecordingInfo.Comments = (TCHAR *) calloc(intTextLength + 1, sizeof(TCHAR));
						if(m_riRecordingInfo.Comments != NULL)
						{
							memcpy_s(m_riRecordingInfo.Comments, (intTextLength + 1)*sizeof(TCHAR), strBuffer, intTextLength*sizeof(TCHAR));
							m_riRecordingInfo.Comments[intTextLength] = TEXT('\0');
						}
						else
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_RecordingInfo() - WM_COMMAND - IDOK: Failed to allocate memory for m_riRecordingInfo.Comments. (errno #)"), errno, TRUE);
					}

					// save electrode type
					intTextLength = GetDlgItemText(hwndDlg, IDC_ELECTRODES, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
					if(intTextLength > 0)
					{
						// allocate new memory and store data
						memcpy_s(m_cfgConfiguration.ElectrodeType, sizeof(m_cfgConfiguration.ElectrodeType), strBuffer, intTextLength*sizeof(TCHAR));
						m_cfgConfiguration.ElectrodeType[intTextLength] = TEXT('\0');
					}

					// save additional comments
					intTextLength = GetDlgItemText(hwndDlg, IDC_ADDITIONALCOMMENTS, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
					if(intTextLength > 0)
					{
						// free previously allocated memory
						free(m_riRecordingInfo.AdditionalComments);
						m_riRecordingInfo.AdditionalComments = NULL;
						
						// allocate new memory and store data
						m_riRecordingInfo.AdditionalComments = (TCHAR *) calloc(intTextLength + 1, sizeof(TCHAR));
						if(m_riRecordingInfo.AdditionalComments != NULL)
							_stprintf_s(m_riRecordingInfo.AdditionalComments, intTextLength + 1, TEXT("%s"), strBuffer);
						else
							applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_RecordingInfo() - WM_COMMAND - IDOK: Failed to allocate memory for m_riRecordingInfo.AdditionalComments. (errno #)"), errno, TRUE);
					}

					// set flag indicating that RecordingInformation structure contains valid data
					m_riRecordingInfo.ContainsValidData = TRUE;

					EndDialog (hwndDlg, 0);
				break;

				case IDCANCEL:
					// Close, discard changes
					EndDialog (hwndDlg, 0);
				break;
			}
		break;

		case WM_CLOSE:
			// Close, discard changes
			EndDialog (hwndDlg, 0);
		break;
	}

	return (0);
}

static void GUI_SetEnabledCommands(HWND hwndOwner, BOOL blnIsRecording, GUIElements gui)
{
	HMENU hmnuMenu;

	if(blnIsRecording)
	{	
		// set menu options
		hmnuMenu = GetMenu (hwndOwner);
		EnableMenuItem (hmnuMenu, IDM_LOADEDFFILE, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_TESTCONNSCRIPT, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_SAMPLE_START, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_SAMPLE_STOP, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_PARAMETERS, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_PATIENTINFO, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_RECORDINGINFORMATION, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_UTILITIES_EDFFILEEDITOR, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_ABOUT, MF_GRAYED);

		// set system menu options
		hmnuMenu = GetSystemMenu (hwndOwner, FALSE);
		ModifyMenu (hmnuMenu, 2, MF_BYPOSITION | MF_STRING | MF_DISABLED | MF_GRAYED, IDM_SAMPLE_START, TEXT("Start Recording"));
		ModifyMenu (hmnuMenu, 3, MF_BYPOSITION | MF_STRING | MF_ENABLED, IDM_SAMPLE_STOP, TEXT("Stop Recording"));

		// set toolbar buttons
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SAMPLE_START, FALSE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SAMPLE_STOP_NORMAL, TRUE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_PARAMETERS, FALSE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SYSTEMCHECK, FALSE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_PATIENTINFO, TRUE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_RECORDINGINFORMATION, TRUE);
		//SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SIGNAL_DISPLAY_MODE, TRUE); // NOTE: aEEG mode is disabled since it doesn't work.
	}
	else
	{
		// set menu options
		hmnuMenu = GetMenu (hwndOwner);
		EnableMenuItem (hmnuMenu, IDM_LOADEDFFILE, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_TESTCONNSCRIPT, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_SAMPLE_START, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_SAMPLE_STOP, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_PARAMETERS, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_PATIENTINFO, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_RECORDINGINFORMATION, MF_GRAYED);
		EnableMenuItem (hmnuMenu, IDM_UTILITIES_EDFFILEEDITOR, MF_ENABLED);
		EnableMenuItem (hmnuMenu, IDM_ABOUT, MF_ENABLED);

		// set system menu options
		hmnuMenu = GetSystemMenu (hwndOwner, FALSE);
		ModifyMenu (hmnuMenu, 2, MF_BYPOSITION | MF_STRING | MF_ENABLED, IDM_SAMPLE_START, TEXT("Start Recording"));
		ModifyMenu (hmnuMenu, 3, MF_BYPOSITION | MF_STRING | MF_DISABLED | MF_GRAYED, IDM_SAMPLE_STOP, TEXT("Stop Recording"));

		// set toolbar buttons
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SAMPLE_START, TRUE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SAMPLE_STOP_NORMAL, FALSE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_PARAMETERS, TRUE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SYSTEMCHECK, TRUE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_PATIENTINFO, FALSE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_RECORDINGINFORMATION, FALSE);
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SIGNAL_DISPLAY_MODE, FALSE);
		//SendMessage(gui.hwndToolbar, TB_CHECKBUTTON, IDM_SIGNAL_DISPLAY_MODE, FALSE);  // NOTE: aEEG mode is disabled since it doesn't work.
	}

	// enable Timebase and LP combo boxes (needed in case user stops the recording while in aEEG mode)
	ComboBox_Enable(gui.hwndCMBLPFilters, TRUE);
	ComboBox_Enable(gui.hwndCMBTimebase, TRUE);

	if(m_cfgConfiguration.SimulationMode)
		SendMessage(gui.hwndToolbar, TB_ENABLEBUTTON, IDM_SYSTEMCHECK, FALSE);
	
	// refresh menu bar
	DrawMenuBar (hwndOwner);
}

static LRESULT APIENTRY AnnotationsWndProc (HWND hWnd, UINT message, UINT wParam, LONG lParam)
{
	TCHAR strAnnotationLine[ANNOTATION_MAX_CHARS + 6 + 1];
	COLORREF clrForeColor;
	HDC hdcScreen, hdcWindow;
	HWND hwndParent;
	int intFontHeight, intNTextLines, intMaxLineLength, intTitleBarHeight, intBorderWidth, i;
	RECT rectClient;
	PAINTSTRUCT	ps;
	SIZE szTextSize;
	unsigned int uintCBData;
	WINDOWINFO wi;
	
	static TCHAR strBuffer[ANNOTATION_MAX_TYPES*(ANNOTATION_MAX_CHARS + 7)];
	static HFONT hfntAnnotations, hfntOld;
	static OUTLINETEXTMETRIC * potmTextMetric;

	// initialize variables
	clrForeColor = RGB(255,0,255);
	intNTextLines = 0;
	intMaxLineLength = 0;

	switch (message)
	{
		case WM_CREATE:
			//get font height from the screen's DC
			hdcScreen = GetDC(NULL);
			intFontHeight = -MulDiv(12, GetDeviceCaps(hdcScreen, LOGPIXELSY), 72);
			ReleaseDC(NULL, hdcScreen);
			
			// Create font for annotations legend
			hfntAnnotations = CreateFont (intFontHeight,
										  0, 0, 0,
										  FW_NORMAL,
										  0, 0, 0,
										  DEFAULT_CHARSET,
										  OUT_DEFAULT_PRECIS,
										  CLIP_DEFAULT_PRECIS,
										  DEFAULT_QUALITY,
										  VARIABLE_PITCH | FF_ROMAN,
										  TEXT("Book Antiqua"));

			// load annotations font into DC, save old font
			hdcWindow = GetDC(hWnd);
			SetMapMode(hdcWindow, MM_TEXT);
			hfntOld = (HFONT) SelectObject(hdcWindow, hfntAnnotations);
			
			// establish required size for OUTLINETEXTMETRIC structure and allocate the space
			uintCBData = GetOutlineTextMetrics(hdcWindow, sizeof(NULL), NULL);
			potmTextMetric = (OUTLINETEXTMETRIC *) malloc(uintCBData);
			if(potmTextMetric != NULL)
			{
				potmTextMetric->otmSize = uintCBData;

				// retrieve font metrics
				GetOutlineTextMetrics(hdcWindow, uintCBData, potmTextMetric);
			}
			else
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("AnnotationsWndProc() - WM_CREATE: Failed to allocate memory for potmTextMetric. (errno #)"), errno, TRUE);

			// restore and release DC
			SelectObject(hdcWindow, hfntOld);
			ReleaseDC(hWnd, hdcWindow);
		break;

		case WM_PAINT:
            hdcWindow = BeginPaint(hWnd, &ps); 
			
			// sets the mapping mode of the specified device context i.e. the unit of measure used to
			// transform page-space units (logical unites) into device-space units
			SetMapMode(hdcWindow, MM_TEXT);
			
			// set the text color
			SetTextColor(hdcWindow, clrForeColor);

			// load annotations font into DC and save old font
			hfntOld = (HFONT) SelectObject(hdcWindow, hfntAnnotations);
			
			// create string with all the annotations that are to be displayed
			strBuffer[0] = TEXT('\0');
			for(i=0; i<ANNOTATION_MAX_TYPES; i++)
			{
				if(_tcslen(m_cfgConfiguration.Annotations[i]) > 0)
				{
					// parse shortcut key and annotation
					_stprintf_s(strAnnotationLine, sizeof(strAnnotationLine)/sizeof(TCHAR), TEXT("F%d - "), i + 2);
					_tcscat_s(strAnnotationLine, sizeof(strAnnotationLine)/sizeof(TCHAR), m_cfgConfiguration.Annotations[i]);
					_tcscat_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), strAnnotationLine);
					_tcscat_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("\n"));
					
					GetTextExtentPoint32(hdcWindow, strAnnotationLine, (int) _tcslen(strAnnotationLine), &szTextSize);

					// count # of lines
					intNTextLines++;
					
					// save the length of the longest annotation line
					if(szTextSize.cx > intMaxLineLength)
						intMaxLineLength = szTextSize.cx;
				}
			}

			// get window client rectangle
			GetClientRect(hWnd, &rectClient);

			DrawTextEx(hdcWindow, strBuffer, -1, &rectClient, 0, NULL);
						
			// restore DC to its initial state
			SelectObject(hdcWindow, hfntOld);

            EndPaint(hWnd, &ps);

			// get window titlebar height & border width
			wi.cbSize = sizeof(WINDOWINFO);
			GetWindowInfo(hWnd, &wi);
			intTitleBarHeight = wi.rcClient.top - wi.rcWindow.top + wi.cyWindowBorders;
			intBorderWidth = wi.cxWindowBorders*2;

			// set minimal window width
			if(intMaxLineLength == 0)
				intMaxLineLength = 100;
			
			// resize window so that it takes a minimal amount of space and show it
			if(potmTextMetric != NULL)
			{
				SetWindowPos(hWnd,
							 HWND_TOP,
							 0, 0,
							 intBorderWidth + intMaxLineLength,
							 intTitleBarHeight + (potmTextMetric->otmTextMetrics.tmHeight + potmTextMetric->otmLineGap) * intNTextLines,
							 SWP_NOZORDER | SWP_NOMOVE);
			}
		break;

		case WM_ACTIVATE:
			hwndParent = GetParent(hWnd);
			SetActiveWindow(hwndParent);
		break;

		case WM_DESTROY:
			// notify main window that annotations window has been closed
			hwndParent = GetParent(hWnd);
			PostMessage(hwndParent, WM_COMMAND, IDM_ANNOTATIONS_DISPLAY,TRUE);

			// delete GDI objects
			if(hfntOld)
				DeleteObject(hfntOld);
			if (hfntAnnotations)
				DeleteObject (hfntAnnotations);

			// free allocated memory
			if(potmTextMetric != NULL)
				free(potmTextMetric);
		break;

		default:
			return (DefWindowProc (hWnd, message, wParam, lParam));
	}

	return 0;
}

static void GUI_SetStatusBarPartSize(HWND hwndStatusBar, int intNewClientWidth)
{
	HDC hdcStatusBar;
	int intHorizontalDPI, intStatusBarParts [SB_NPARTS] = {0, 0, 0, 0};

	// get screen DPI
	hdcStatusBar = GetDC(hwndStatusBar);
	intHorizontalDPI = GetDeviceCaps(hdcStatusBar, LOGPIXELSX);
	ReleaseDC(hwndStatusBar, hdcStatusBar);

	// compute status bar part sizes
	// compute individual part widths
	intStatusBarParts[0] = (int) (SB_STATUS_WIDTH*intHorizontalDPI);
	intStatusBarParts[2] = SB_BATTERYICON_WIDTH;
	intStatusBarParts[3] = (int) (SB_RECTIME_WIDTH*intHorizontalDPI);
	
	// compute bounaries in client coordinates
	intStatusBarParts[1] = intNewClientWidth - intStatusBarParts[3] - intStatusBarParts[2];
	intStatusBarParts[2] += intStatusBarParts[1];
	intStatusBarParts[3] += intStatusBarParts[2];

	SendMessage(hwndStatusBar, SB_SETPARTS, SB_NPARTS, (LPARAM) intStatusBarParts);
}

static void Dialog_PatientInfo_SetNCharsLeft(HWND hwndDlg)
{
	TCHAR strBuffer[EDFPATIENTSPACE + 1], strNCharsRemaining[25];
	HWND hwndControl;
	int intNRemainigChars = EDFPATIENTSPACE, intNChars;

	// compute # of chars remaining
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_NAME, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNRemainigChars -= GetDlgItemText (hwndDlg, IDC_PIC, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	_stprintf_s(strNCharsRemaining, sizeof(strNCharsRemaining)/sizeof(TCHAR), TEXT("%d characters remaining"), intNRemainigChars);
	
	// update the max # of chars that can be typed into the text boxes
	intNChars = GetDlgItemText (hwndDlg, IDC_NAME, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNChars += intNRemainigChars;
	hwndControl = GetDlgItem(hwndDlg, IDC_NAME);
	if(intNChars == 0)
		EnableWindow(hwndControl, FALSE);
	else
	{
		EnableWindow(hwndControl, TRUE);
		SendMessage(hwndControl, EM_SETLIMITTEXT, intNChars, 0);
	}

	intNChars = GetDlgItemText (hwndDlg, IDC_PIC, strBuffer, sizeof(strBuffer)/sizeof(TCHAR));
	intNChars += intNRemainigChars;
	hwndControl = GetDlgItem(hwndDlg, IDC_PIC);
	if(intNChars == 0)
		EnableWindow(hwndControl, FALSE);
	else
	{
		EnableWindow(hwndControl, TRUE);
		SendMessage(hwndControl, EM_SETLIMITTEXT, intNChars, 0);
	}

	// display number of remaining chars
	SetDlgItemText (hwndDlg, IDC_CHARSREMAINING, strNCharsRemaining);
}

static LRESULT CALLBACK ASCIIMaskedEditProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL blnCharFound;
	TCHAR strMask[] = TEXT(" !\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\b");
	TCHAR * t, c;
    WNDPROC oldwndproc;

    oldwndproc = (WNDPROC) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch(uMsg)
    {
		case WM_CHAR:
			t = strMask;
			c = (TCHAR) wParam;
			blnCharFound = FALSE;
			
			switch(c)
			{
				//
				// Finnish-character translation
				//
				case TEXT('Å'):
				case TEXT('Ä'):
					c = TEXT('A');
					blnCharFound = TRUE;
				break;
				
				case TEXT('å'):
				case TEXT('ä'):
					c = TEXT('a');
					blnCharFound = TRUE;
				break;

				case TEXT('Ö'):
					c = TEXT('O');
					blnCharFound = TRUE;
				break;

				case TEXT('ö'):
					c = TEXT('o');
					blnCharFound = TRUE;
				break;
								
				//
				// keyboard shortcuts
				//
				// ^C
				case 0x03:
					blnCharFound = TRUE;
				break;

				// ^V
				case 0x16:
					blnCharFound = TRUE;
				break;
				
				// ^X
				case 0x18:
					blnCharFound = TRUE;
				break;

				// ^Z
				case 0x1A:
					blnCharFound = TRUE;
				break;

				default:
					// algorithm that checks if the currently typed character
					// is present int the strMask string
					for(;;)
					{
						if(*t == c)
						{
							blnCharFound = TRUE;
							break;
						}
				        
						if (*t == TEXT('\0'))
							break;

						t++;
					}
			}
			
			// if character is found in mask string, process it normally;
			// if not, don't process it (return 0)
			if(blnCharFound)
				return CallWindowProc(oldwndproc, hwnd, uMsg, (WPARAM) c, lParam);
			else
				return 0;
		
		default:
			return CallWindowProc(oldwndproc, hwnd, uMsg, wParam, lParam);
    }
}

static void GUI_DisplayPropertySheet(HWND hwndOwner, GUIElements gui)
{
	PROPSHEETHEADER pshProprtySheetFrame;
	PROPSHEETPAGE pspPropertySheets[6];
 
	// clear memory structures
	SecureZeroMemory(pspPropertySheets, sizeof(pspPropertySheets));
	SecureZeroMemory(&pshProprtySheetFrame, sizeof(pshProprtySheetFrame));

	// initialize property sheet pages
	pspPropertySheets[0].dwSize = sizeof(PROPSHEETPAGE);
	pspPropertySheets[0].dwFlags = PSP_DEFAULT;
	pspPropertySheets[0].hInstance = m_hinMain;
	pspPropertySheets[0].pszTemplate = (TCHAR *) IDD_SETTINGS;
	pspPropertySheets[0].hIcon = NULL;
	pspPropertySheets[0].pszIcon = NULL;
	pspPropertySheets[0].pfnDlgProc = (DLGPROC) Dialog_Settings;
	pspPropertySheets[0].lParam = 0;

	pspPropertySheets[1].dwSize = sizeof(PROPSHEETPAGE);
	pspPropertySheets[1].dwFlags = PSP_DEFAULT;
	pspPropertySheets[1].hInstance = m_hinMain;
	pspPropertySheets[1].pszTemplate = (TCHAR *) IDD_SCREENSIZE;
	pspPropertySheets[1].hIcon = NULL;
	pspPropertySheets[1].pszIcon = NULL;
	pspPropertySheets[1].pfnDlgProc = (DLGPROC) Dialog_ScreenSize;
	pspPropertySheets[1].lParam = 0;

	pspPropertySheets[2].dwSize = sizeof(PROPSHEETPAGE);
	pspPropertySheets[2].dwFlags = PSP_DEFAULT;
	pspPropertySheets[2].hInstance = m_hinMain;
	pspPropertySheets[2].pszTemplate = (TCHAR *) IDD_ANNOTATIONS;
	pspPropertySheets[2].hIcon = NULL;
	pspPropertySheets[2].pszIcon = NULL;
	pspPropertySheets[2].pfnDlgProc = (DLGPROC) Dialog_Annotation;
	pspPropertySheets[2].lParam = 0;

	pspPropertySheets[3].dwSize = sizeof(PROPSHEETPAGE);
	pspPropertySheets[3].dwFlags = PSP_DEFAULT;
	pspPropertySheets[3].hInstance = m_hinMain;
	pspPropertySheets[3].pszTemplate = (TCHAR *) IDD_INTERNETCONFIG;
	pspPropertySheets[3].hIcon = NULL;
	pspPropertySheets[3].pszIcon = NULL;
	pspPropertySheets[3].pfnDlgProc = (DLGPROC) Dialog_InternetConnectionConfig;
	pspPropertySheets[3].lParam = 0;

	pspPropertySheets[4].dwSize = sizeof(PROPSHEETPAGE);
	pspPropertySheets[4].dwFlags = PSP_DEFAULT;
	pspPropertySheets[4].hInstance = m_hinMain;
	pspPropertySheets[4].pszTemplate = (TCHAR *) IDD_STREAMINGCONFIG;
	pspPropertySheets[4].hIcon = NULL;
	pspPropertySheets[4].pszIcon = NULL;
	pspPropertySheets[4].pfnDlgProc = (DLGPROC) Dialog_StreamingConfig;
	pspPropertySheets[4].lParam = 0;

	pspPropertySheets[5].dwSize = sizeof(PROPSHEETPAGE);
	pspPropertySheets[5].dwFlags = PSP_DEFAULT;
	pspPropertySheets[5].hInstance = m_hinMain;
	pspPropertySheets[5].pszTemplate = (TCHAR *) IDD_SSHCONFIG;
	pspPropertySheets[5].hIcon = NULL;
	pspPropertySheets[5].pszIcon = NULL;
	pspPropertySheets[5].pfnDlgProc = (DLGPROC) Dialog_SSHConfig;
	pspPropertySheets[5].lParam = 0;

	// create property sheets
	pshProprtySheetFrame.dwSize = sizeof(PROPSHEETHEADER);
	pshProprtySheetFrame.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	pshProprtySheetFrame.hInstance = m_hinMain;
	pshProprtySheetFrame.hwndParent = hwndOwner;
	pshProprtySheetFrame.pszCaption = TEXT("Parameters");
	pshProprtySheetFrame.nPages = 6;
	pshProprtySheetFrame.nStartPage = 0;
	pshProprtySheetFrame.ppsp = (LPCPROPSHEETPAGE) pspPropertySheets;
	
	// display property sheet
	PropertySheet(&pshProprtySheetFrame);

	// on exit, save settings
	main_SaveCurrentSettings(gui);
}

static void GUI_DisplayAnnotationsContextMenu(HWND hwndOwner, int intXPos, int intYPos, GUIElements gui, HWND hwndMainWindow)
{
	int i, intNDisplayedAnnotations, intSelectedItem;
	
	if(!m_blnIsAnnotationsMenuDisplayed)
	{
		// creates an initially empty shortcut menu
		gui.hmnuAnnotations = CreatePopupMenu();
		
		// create the menu items
		intNDisplayedAnnotations = 0;
		for(i=0; i<ANNOTATION_MAX_TYPES; i++)
		{
			if(_tcslen(m_cfgConfiguration.Annotations[i]) > 0)
			{	
				AppendMenu(gui.hmnuAnnotations, MF_ENABLED | MF_STRING, i + 1, m_cfgConfiguration.Annotations[i]);
				intNDisplayedAnnotations++;
			}
		}
		
		// Append 'NOW' annotation
		if(intNDisplayedAnnotations > 0)
			AppendMenu(gui.hmnuAnnotations, MF_SEPARATOR, 0, 0);
		AppendMenu(gui.hmnuAnnotations, MF_ENABLED | MF_STRING, ANNOTATION_MAX_TYPES, TEXT("Now"));

		// display context menu
		intSelectedItem = TrackPopupMenuEx(gui.hmnuAnnotations,
										   TPM_VERTICAL | TPM_RETURNCMD,
										   intXPos, intYPos,
										   hwndOwner,
								  		   NULL);

		m_blnIsAnnotationsMenuDisplayed = TRUE;
		
		if(intSelectedItem > 0)
		{
			if(intSelectedItem < ANNOTATION_MAX_TYPES)
				main_InsertAnnotation(Regular, intSelectedItem - 1, gui, hwndMainWindow);
			else
				main_InsertAnnotation(Now, -1, gui, hwndMainWindow);

			m_blnIsAnnotationsMenuDisplayed = FALSE;
		}
	}
	else
	{
		// free menu resources
		DestroyMenu(gui.hmnuAnnotations);

		m_blnIsAnnotationsMenuDisplayed = FALSE;
	}
}

static BOOL CALLBACK Dialog_ScreenSize (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL blnError;
	DRAWITEMSTRUCT * pDrawItem;
	HDC hdcDialog;
	HWND hwndControl, hwndScreenWidth, hwndScreenHeight;
	int intMinScreenHeight, intMinScreenWidth, intMaxScreenHeight, intMaxScreenWidth;
	PSHNOTIFY * phdr;

	static int intHorizontalResolution, intVerticalResolution, intScreenHeight, intScreenWidth;
	
	// initialize variable
	blnError = FALSE;
	intMinScreenHeight = intMinScreenWidth = 50;
	intMaxScreenHeight = intMaxScreenWidth = 2000;
	hwndScreenWidth = GetDlgItem(hwndDlg, IDC_SCREENWIDTHSPIN);
	hwndScreenHeight = GetDlgItem(hwndDlg, IDC_SCREENHEIGHTSPIN);

	switch (uintMsg)
	{
		case WM_INITDIALOG:
			// get approximate screen height and width
			hdcDialog = GetDC(hwndDlg);
			intScreenWidth = GetDeviceCaps(hdcDialog, HORZSIZE);
			intScreenHeight = GetDeviceCaps(hdcDialog, VERTSIZE);
			intHorizontalResolution = GetDeviceCaps(hdcDialog, HORZRES);
			intVerticalResolution = GetDeviceCaps(hdcDialog, VERTRES);
			ReleaseDC(hwndDlg, hdcDialog);
			
			//
			// configure up-down controls
			//
			// set max number of chars for the text boxes
			hwndControl = GetDlgItem(hwndDlg, IDC_SCREENWIDTH);
			SendMessage(hwndControl, EM_SETLIMITTEXT, 4, 0);
			hwndControl = GetDlgItem(hwndDlg, IDC_SCREENHEIGHT);
			SendMessage(hwndControl, EM_SETLIMITTEXT, 4, 0);

			// set range of up-down controls
			SendMessage(hwndScreenWidth, UDM_SETRANGE32, intMinScreenWidth, intMaxScreenWidth);
			SendMessage(hwndScreenHeight, UDM_SETRANGE32, intMinScreenHeight, intMaxScreenHeight);

			// set initial values for the up-down controls
			if(m_cfgConfiguration.ScreenWidth > 0)
				intScreenWidth = m_cfgConfiguration.ScreenWidth;
			if(m_cfgConfiguration.ScreenHeight > 0)
				intScreenHeight = m_cfgConfiguration.ScreenHeight;
			
			SendMessage(hwndScreenWidth, UDM_SETPOS32, 0, intScreenWidth);
			SendMessage(hwndScreenHeight, UDM_SETPOS32, 0, intScreenHeight);

			return TRUE;
		break;

		case WM_DISPLAYCHANGE:
			hdcDialog = GetDC(hwndDlg);
			intHorizontalResolution = GetDeviceCaps(hdcDialog, HORZRES);
			intVerticalResolution = GetDeviceCaps(hdcDialog, VERTRES);
			ReleaseDC(hwndDlg, hdcDialog);
		break;

		case WM_COMMAND:
			if(HIWORD(wParam) == EN_CHANGE)
				RedrawWindow(hwndDlg, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		break;
		
		case WM_DRAWITEM:
			pDrawItem = (LPDRAWITEMSTRUCT) lParam;
			switch(wParam)
			{
				case IDC_VERTICALRULER:
					intScreenHeight = (int) SendMessage(hwndScreenHeight, UDM_GETPOS32, 0, (LPARAM)(LPBOOL) &blnError);
					if(!blnError)
						Dialog_ScreenSize_DrawRuler(pDrawItem->hDC, pDrawItem->rcItem, intHorizontalResolution, intVerticalResolution, intScreenWidth, intScreenHeight, TRUE);
				break;

				case IDC_HORIZONTALRULER:
					intScreenWidth = (int) SendMessage(hwndScreenWidth, UDM_GETPOS32, 0, (LPARAM)(LPBOOL) &blnError);
					if(!blnError)
						Dialog_ScreenSize_DrawRuler(pDrawItem->hDC, pDrawItem->rcItem, intHorizontalResolution, intVerticalResolution, intScreenWidth, intScreenHeight, FALSE);
				break;
			}

			return TRUE;
		break;

		case WM_NOTIFY:
			phdr = (LPPSHNOTIFY) lParam;
			switch (phdr->hdr.code)
			{
				case PSN_APPLY:
					// user clicked on the OK button
					if(phdr->lParam == TRUE)
					{
						intScreenWidth = (int) SendMessage(hwndScreenWidth, UDM_GETPOS32, 0, (LPARAM)(LPBOOL) &blnError);
						if(!blnError)
						{
							m_cfgConfiguration.ScreenWidth = intScreenWidth;
							m_cfgConfiguration.HorizontalDPC = (int) ((((double)intHorizontalResolution)/((double)intScreenWidth))*10.0f);
						}
		
						intScreenHeight = (int) SendMessage(hwndScreenHeight, UDM_GETPOS32, 0, (LPARAM)(LPBOOL) &blnError);
						if(!blnError)
						{
							m_cfgConfiguration.ScreenHeight = intScreenHeight;
							m_cfgConfiguration.VerticalDPC = (int) ((((double)intVerticalResolution)/((double)intScreenHeight))*10.0f);
						}

						// notify property sheet manager that the changes made to this page are valid and have been applied
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
 						return TRUE;
					}
				break;
			}
		break;
	}

	return (0);
}

static void Dialog_ScreenSize_DrawRuler(HDC hdcRuler, RECT rcRuler, int intHorizontalResolution, int intVerticalResolution, int intHorizontalSize, int intVerticalSize, BOOL blnVertical)
{
	HBITMAP hbmMemory;
	HDC hdcMemory;
	HGDIOBJ hgdioPen;
	int intDrawingAreaWidth, intDrawingAreaHeight, intHPixelsPerPhysicalCM, intVPixelsPerPhysicalCM, i;
	
	intHPixelsPerPhysicalCM = (int) ((((double)intHorizontalResolution)/((double)intHorizontalSize))*10);
	intVPixelsPerPhysicalCM = (int) ((((double)intVerticalResolution)/((double)intVerticalSize))*10);
	
	// calculate coordinates and size of shadow window
	intDrawingAreaWidth = rcRuler.right - rcRuler.left;
	intDrawingAreaHeight = rcRuler.bottom - rcRuler.top;

	// Create memory-based DC (to prevent flickering)
	hdcMemory = CreateCompatibleDC (hdcRuler);					
	hbmMemory = CreateCompatibleBitmap(hdcRuler, intDrawingAreaWidth, intDrawingAreaHeight);
	SelectObject(hdcMemory, hbmMemory);
	
	// select stock black pen
	hgdioPen = GetStockObject (WHITE_PEN);
	SelectObject (hdcMemory, hgdioPen);

	// draw ruler
	if(blnVertical)
	{
		i = 2;
		while(i<(intDrawingAreaHeight - 4))
		{
			MoveToEx (hdcMemory, 2, i, NULL);
			LineTo (hdcMemory, intDrawingAreaWidth - 2, i);
			i+= intVPixelsPerPhysicalCM;
		}

		MoveToEx (hdcMemory, 2, 2, NULL);
		LineTo (hdcMemory, 2, i - intVPixelsPerPhysicalCM);
	}
	else
	{
		i = 2;
		while(i<(intDrawingAreaWidth - 4))
		{
			MoveToEx (hdcMemory, i, 2, NULL);
			LineTo (hdcMemory, i, intDrawingAreaHeight - 2);
			i+= intHPixelsPerPhysicalCM;
		}

		MoveToEx (hdcMemory, 2, intDrawingAreaHeight - 2, NULL);
		LineTo (hdcMemory, i - intHPixelsPerPhysicalCM, intDrawingAreaHeight - 2);
	}

	// Bit-block transfer of the color data corresponding to a rectangle of pixels from memory to the client window
	BitBlt (hdcRuler, 0, 0, intDrawingAreaWidth, intDrawingAreaHeight, hdcMemory, 0, 0, SRCCOPY);
	
	// release resourves
	DeleteObject (hgdioPen);
	DeleteObject (hbmMemory);
	DeleteDC (hdcMemory);
}

static void GUI_SetFullScreenMode(HWND hWnd, GUIElements gui)
{
	HMENU hmnuSystem;

	static HMENU hmnuMainWindow;

	hmnuSystem = GetSystemMenu(hWnd, FALSE);

	if(m_blnIsFullScreen)
	{
		m_blnIsFullScreen = FALSE;
		
		// show rebar
		ShowWindow(gui.hwndRebar, SW_SHOW);

		// show window menu
		SetMenu(hWnd, hmnuMainWindow);
		
		// set system menu check state
		ModifyMenu(hmnuSystem, 0, MF_BYPOSITION | MF_STRING, IDM_FULLSCREEN, TEXT("Full-Screen Mode"));
	}
	else
	{
		m_blnIsFullScreen = TRUE;
		
		// hide rebar
		ShowWindow(gui.hwndRebar, SW_HIDE);

		// hide window menu
		hmnuMainWindow = GetMenu(hWnd);
		SetMenu(hWnd, NULL);

		// maximize the main window
		ShowWindow(hWnd, SW_MAXIMIZE);

		// set system menu check state
		ModifyMenu(hmnuSystem, 0, MF_BYPOSITION | MF_CHECKED | MF_STRING, IDM_FULLSCREEN, TEXT("Full-Screen Mode"));
	}
}

/**
 * \brief Initializes the GUI elements of the main window (i.e., rebar, statusbar, and system menu)
 *
 * \param[in]	pgui	pointer to GUI struct
 *
 * \return ERROR_SUCCESS or a system error code generated by GetLastError().
 */
static DWORD GUI_Init(HWND hwndMainWindow, GUIElements * pgui)
{
	DWORD				dwBtnSize;
	float				fltScale;
	float *				pfltLPCutOffFrequenciesBuffer;
	HDC					hDC;
	HICON				hIcon;
	HIMAGELIST			hImageList;
	HMENU				hmnuMenu;
	int					intHorizontalDPI, intImageListID, intTextHeight, intToolbarImageSize, i;
	NONCLIENTMETRICS	ncm;
	REBARBANDINFO		rbBand;
	RECT				rc;
	TBBUTTON			tbButtons[TB_NBUTTONS];
	TCHAR				strBuffer[100];

	// variable init
	intImageListID = 0;

	// get screen DPI
	hDC = GetDC(hwndMainWindow);
	intHorizontalDPI = GetDeviceCaps(hDC, LOGPIXELSX);
	ReleaseDC(hwndMainWindow, hDC);

	// Initializes the COM library for use by user interface thread
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	// calculate toolbar image size
	fltScale = intHorizontalDPI / 96.0f;
	//intToolbarImageSize = (16*fltScale+0.5f) >= 24 ? 24 : ((16*fltScale+0.5f) >= 20 ? 20 : 16);
	intToolbarImageSize = (16*fltScale+0.5f) >= 24 ? 24 : 16;

	//
	// load image files
	//
	// Load battery state icons
	pgui->hIconsBatteryStates[0] = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_BATFULL), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	pgui->hIconsBatteryStates[1] = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_BATLOW1), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	pgui->hIconsBatteryStates[2] = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_BATLOW2), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
	
	// create imagelist and load toolbar icons
	hImageList = ImageList_Create(intToolbarImageSize,
									intToolbarImageSize,
									ILC_COLOR32,   // Ensures transparent background.
									TB_NBUTTONS,
									0);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_PATIENT), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_RECORDING), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_PARAMETERS), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_ANNOTATIONS), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_RECORD), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_STOP), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_EXIT), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_ALERT), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);
	hIcon = (HICON) LoadImage(m_hinMain, MAKEINTRESOURCE(IDI_CHECK), IMAGE_ICON, intToolbarImageSize, intToolbarImageSize, LR_DEFAULTCOLOR);
	ImageList_AddIcon(hImageList, hIcon);

	// load streaming status images
	pgui->hStreamingStatusImages[0] = LoadImage(m_hinMain, MAKEINTRESOURCE(IDB_TRAFFIC_OFF), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	pgui->hStreamingStatusImages[1] = LoadImage(m_hinMain, MAKEINTRESOURCE(IDB_TRAFFIC_RED), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	pgui->hStreamingStatusImages[2] = LoadImage(m_hinMain, MAKEINTRESOURCE(IDB_TRAFFIC_REDYEL), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	pgui->hStreamingStatusImages[3] = LoadImage(m_hinMain, MAKEINTRESOURCE(IDB_TRAFFIC_GRN), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	pgui->hStreamingStatusImages[4] = LoadImage(m_hinMain, MAKEINTRESOURCE(IDB_TRAFFIC_YEL), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
		
	//
	// create the application toolbar
	//
	pgui->hwndToolbar = CreateWindowEx(0,
 									   TOOLBARCLASSNAME,
									   NULL,
									   WS_CHILD | WS_VISIBLE | CCS_NOPARENTALIGN | CCS_NORESIZE | CCS_NODIVIDER | TBSTYLE_LIST, //    // must use CCS_NODIVIDER here too to get rid of double 3D line at top; must also specify CCS_NOPARENTALIGN and CCS_NORESIZE to prevent toolbar from aligning to the top
									   0, 0, 0, 0,
									   hwndMainWindow,
									   NULL,
									   m_hinMain,
									   NULL);
	if(pgui->hwndToolbar != NULL)
	{
		// Set the image list.
		SendMessage(pgui->hwndToolbar, TB_SETIMAGELIST, (WPARAM) intImageListID, (LPARAM) hImageList);

		// Initialize button info.
		tbButtons[0].iBitmap = MAKELONG(0, intImageListID);
		tbButtons[0].idCommand = IDM_PATIENTINFO;
		tbButtons[0].fsState = TBSTATE_ENABLED;
		tbButtons[0].fsStyle = BTNS_AUTOSIZE;
		tbButtons[0].iString = (INT_PTR) TEXT("Patient Info");
		tbButtons[0].dwData = 0;

		tbButtons[1].iBitmap = MAKELONG(1, intImageListID);
		tbButtons[1].idCommand = IDM_RECORDINGINFORMATION;
		tbButtons[1].fsState = TBSTATE_ENABLED;
		tbButtons[1].fsStyle = BTNS_AUTOSIZE;
		tbButtons[1].iString = (INT_PTR) TEXT("Recording Info"); 
		tbButtons[1].dwData = 0;

		//tbButtons[2].iBitmap = MAKELONG(2, intImageListID);
		tbButtons[2].idCommand = IDM_SIGNAL_DISPLAY_MODE;
		tbButtons[2].fsState = TBSTATE_ENABLED;
		tbButtons[2].fsStyle = BTNS_AUTOSIZE | BTNS_CHECK;
		tbButtons[2].iString = (INT_PTR) TEXT("aEEG Mode"); 
		tbButtons[2].dwData = 0;

		tbButtons[3].iBitmap = MAKELONG(2, intImageListID);
		tbButtons[3].idCommand = IDM_PARAMETERS;
		tbButtons[3].fsState = TBSTATE_ENABLED;
		tbButtons[3].fsStyle = BTNS_AUTOSIZE;
		tbButtons[3].iString = (INT_PTR) TEXT("Parameters"); 
		tbButtons[3].dwData = 0;

		tbButtons[4].iBitmap = MAKELONG(7, intImageListID);
		tbButtons[4].idCommand = IDM_SYSTEMCHECK;
		tbButtons[4].fsState = TBSTATE_ENABLED;
		tbButtons[4].fsStyle = BTNS_AUTOSIZE;
		tbButtons[4].iString = (INT_PTR) TEXT("System Check"); 
		tbButtons[4].dwData = 0;

		tbButtons[5].iBitmap = MAKELONG(3, intImageListID);
		tbButtons[5].idCommand = IDM_ANNOTATIONS_DISPLAY;
		tbButtons[5].fsState = TBSTATE_ENABLED;
		tbButtons[5].fsStyle = BTNS_AUTOSIZE | BTNS_CHECK;
		tbButtons[5].iString = (INT_PTR) TEXT("Annotations"); 
		tbButtons[5].dwData = 0;

		tbButtons[6].iBitmap = 0;
		tbButtons[6].idCommand = 0;
		tbButtons[6].fsState = 0;
		tbButtons[6].fsStyle = BTNS_SEP;
		tbButtons[6].iString = 0; 
		tbButtons[6].dwData = 0;

		tbButtons[7].iBitmap = MAKELONG(4, intImageListID);
		tbButtons[7].idCommand = IDM_SAMPLE_START;
		tbButtons[7].fsState = TBSTATE_ENABLED;
		tbButtons[7].fsStyle = BTNS_AUTOSIZE;
		tbButtons[7].iString = (INT_PTR) TEXT("Record"); 
		tbButtons[7].dwData = 0;

		tbButtons[8].iBitmap = MAKELONG(5, intImageListID);
		tbButtons[8].idCommand = IDM_SAMPLE_STOP_NORMAL;
		tbButtons[8].fsState = TBSTATE_ENABLED;
		tbButtons[8].fsStyle = BTNS_AUTOSIZE;
		tbButtons[8].iString = (INT_PTR) TEXT("Stop"); 
		tbButtons[8].dwData = 0;

		tbButtons[9].iBitmap = 0;
		tbButtons[9].idCommand = 0;
		tbButtons[9].fsState = 0;
		tbButtons[9].fsStyle = BTNS_SEP;
		tbButtons[9].iString = 0; 
		tbButtons[9].dwData = 0;

		tbButtons[10].iBitmap = MAKELONG(6, intImageListID);
		tbButtons[10].idCommand = IDM_QUIT;
		tbButtons[10].fsState = TBSTATE_ENABLED;
		tbButtons[10].fsStyle = BTNS_AUTOSIZE;
		tbButtons[10].iString = (INT_PTR) TEXT("Quit"); 
		tbButtons[10].dwData = 0;

		// Add buttons.
		SendMessage(pgui->hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
		SendMessage(pgui->hwndToolbar, TB_ADDBUTTONS, (WPARAM) TB_NBUTTONS, (LPARAM) &tbButtons);

		// Tell the toolbar to resize itself, and show it.
		SendMessage(pgui->hwndToolbar, TB_AUTOSIZE, 0, 0);
	}
	else
		return GetLastError();
	
	//
	// create combo boxes
	//
	// Create drop-down list for the sensitivity scale and populate it
	pgui->hwndCMBSensitivity = CreateWindow(TEXT("COMBOBOX"),
											TEXT(""),
											WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
											900, 550,
											110, 100,
											hwndMainWindow,
											(HMENU) IDC_SENSITIVITY,
											m_hinMain,
											0);
	if(pgui->hwndCMBSensitivity != NULL)
	{
		for (i=0; i < sizeof(mc_intSensitivityFactors)/sizeof(int); i++)
		{
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("%d"), mc_intSensitivityFactors[i]);
			SendMessage (pgui->hwndCMBSensitivity, CB_ADDSTRING, 0, (LPARAM) strBuffer);
		}
		SendMessage((HWND) pgui->hwndCMBSensitivity, CB_SETCURSEL, m_cfgConfiguration.ScaleIndex, 0); 
	}
	else
		return GetLastError();

	// Create drop-down list for LP filter slection and populate it
	pgui->hwndCMBLPFilters = CreateWindow(TEXT("COMBOBOX"),
										  TEXT(""),
										  WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
										  900, 650,
										  110, 100,
										  hwndMainWindow,
										  (HMENU) IDC_LP,
										  m_hinMain,
										  0);
	if(pgui->hwndCMBLPFilters != NULL)
	{
		// get cut-off frequencies from signal processing module
		pfltLPCutOffFrequenciesBuffer = (float *) malloc(sizeof(float)*NLPFILTERS);
		if(pfltLPCutOffFrequenciesBuffer != NULL)
		{
			sp_GetLPFiltersFc(pfltLPCutOffFrequenciesBuffer, NLPFILTERS);

			// update combobox
			SendMessage (pgui->hwndCMBLPFilters, CB_ADDSTRING, 0, (LPARAM) TEXT("Off"));
			for (i=0; i < NLPFILTERS; i++)
			{
				_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("%.2f"), pfltLPCutOffFrequenciesBuffer[i]);
				SendMessage (pgui->hwndCMBLPFilters, CB_ADDSTRING, 0, (LPARAM) strBuffer);
			}
			SendMessage((HWND) pgui->hwndCMBLPFilters, CB_SETCURSEL, m_cfgConfiguration.LPFilterIndex, 0);

			// free memory
			free(pfltLPCutOffFrequenciesBuffer);
		}
		else
		{
			applog_logevent(SoftwareError, TEXT("Main"), TEXT("GUI_Init: Could not allocate memory for pfltLPCutOffFrequenciesBuffer."), 0, TRUE);
			return ULONG_MAX - 1;
		}
	}
	else
		return GetLastError();

	// Create time-base combo-box and populate it
	pgui->hwndCMBTimebase = CreateWindow(TEXT("COMBOBOX"),
										 TEXT(""),
										 WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST,
										 900, 550,
										 110, 100,
										 hwndMainWindow,
										 (HMENU) IDC_TIMEBASE,
										 m_hinMain,
										 0);
	if(pgui->hwndCMBTimebase != NULL)
	{
		// add items to combo box
		for (i=0; i < sizeof(mc_fltTimebaseFactors)/sizeof(float); i++)
		{
			_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("%1.2f"), mc_fltTimebaseFactors[i]);
			SendMessage (pgui->hwndCMBTimebase, CB_ADDSTRING, 0, (LPARAM) strBuffer);
		}
		SendMessage((HWND) pgui->hwndCMBTimebase, CB_SETCURSEL, m_cfgConfiguration.TimeBaseIndex, 0);
	}
	else
		return GetLastError();

	//
	// create static controls
	//
	// Create time-base combo-box and populate it
	pgui->hwndSTAStreamingStatus = CreateWindow(TEXT("STATIC"),
								 				TEXT(""),
												WS_CHILD | WS_VISIBLE | SS_BITMAP,
												900, 550,
												110, 25,
												hwndMainWindow,
												NULL,
												NULL,
												0);
	if(pgui->hwndCMBTimebase == NULL)
		return GetLastError();

	//
	// create the application rebar
	//
    // create empty rebar.
	pgui->hwndRebar = CreateWindowEx(WS_EX_TOOLWINDOW,
									 REBARCLASSNAME,
									 NULL,
									 WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | RBS_VARHEIGHT | CCS_NODIVIDER | RBS_BANDBORDERS | RBS_AUTOSIZE,
									 0,0,0,0,
									 hwndMainWindow,
									 NULL,
									 m_hinMain,
									 NULL);
    if(pgui->hwndRebar != NULL)
	{
		//
		// Initialize band info used by all bands.
		//
		rbBand.cbSize = sizeof(REBARBANDINFO);
		rbBand.fMask  =   RBBIM_STYLE       // fStyle is valid.
					    | RBBIM_TEXT        // lpText is valid.
						| RBBIM_CHILD       // hwndMainWindowChild is valid.
						| RBBIM_CHILDSIZE   // child size members are valid.
						| RBBIM_SIZE;       // cx is valid

		rbBand.fStyle = RBBS_NOGRIPPER | RBBS_USECHEVRON | RBBS_CHILDEDGE;

		//
		// add toolbar on the first line of the rebar
		//
		// Get the height of the toolbar.
		dwBtnSize = (DWORD) SendMessage(pgui->hwndToolbar, TB_GETBUTTONSIZE, 0,0);

		// Set values unique to the band with the toolbar.
		rbBand.lpText = TEXT("");
		rbBand.hwndChild = pgui->hwndToolbar;
		rbBand.cyChild = HIWORD(dwBtnSize);
		rbBand.cxMinChild = 200;							// NOTE: must adjust whenever toolbar size increases
		rbBand.cyMinChild = HIWORD(dwBtnSize);
		// The default width is the width of the buttons.
		rbBand.cx = 0;
		// Add the band that has the toolbar.
		SendMessage(pgui->hwndRebar, RB_INSERTBAND, (WPARAM) 0, (LPARAM)&rbBand);

		rbBand.fStyle |= RBBS_BREAK;						// next rebar element should be on a new line
		
		//
		// add sensitivity combo box on the second line of the rebar
		//
		// Set values unique to the band with the combo box.
		GetWindowRect(pgui->hwndCMBSensitivity, &rc);
		rbBand.lpText = TEXT("Sensitivity (uV/cm)");
		rbBand.hwndChild = pgui->hwndCMBSensitivity;
		rbBand.cxMinChild = 0;
		rbBand.cyMinChild = rc.bottom - rc.top;
		// The default width should be set to some value wider than the text. The combo 
		// box itself will expand to fill the band.
		rbBand.cx = (int) (RBB_SENSITIVITY_WIDTH*intHorizontalDPI);

		// Add the band that has the combo box.
		SendMessage(pgui->hwndRebar, RB_INSERTBAND, (WPARAM) 1, (LPARAM)&rbBand);

		rbBand.fStyle &= ~RBBS_BREAK;						// remaining rebar element should be on the same line as the "Sensitivity" combo box

		//
		// add timebase combo box on the second line of the rebar
		//
		// Set values unique to the band with the combo box.
		GetWindowRect(pgui->hwndCMBTimebase, &rc);
		rbBand.lpText = TEXT("Timebase (sec/scr)");
		rbBand.hwndChild = pgui->hwndCMBTimebase;
		rbBand.cxMinChild = 0;
		rbBand.cyMinChild = rc.bottom - rc.top;
		// The default width should be set to some value wider than the text. The combo 
		// box itself will expand to fill the band.
		rbBand.cx = (int) (RBB_TIMEBASE_WIDTH*intHorizontalDPI);

		// Add the band that has the combo box.
		SendMessage(pgui->hwndRebar, RB_INSERTBAND, (WPARAM) 2, (LPARAM)&rbBand);
		
		//
		// add lowpass filter combo box on the second line of the rebar
		//
		// Set values unique to the band with the combo box.
		GetWindowRect(pgui->hwndCMBLPFilters, &rc);
		rbBand.lpText = TEXT("Lowpass Filter (Hz)");
		rbBand.hwndChild = pgui->hwndCMBLPFilters;
		rbBand.cxMinChild = 0;
		rbBand.cyMinChild = rc.bottom - rc.top;
		// The default width should be set to some value wider than the text. The combo 
		// box itself will expand to fill the band.
		rbBand.cx = (int) (RBB_LP_WIDTH*intHorizontalDPI);

		// Add the band that has the combo box.
		SendMessage(pgui->hwndRebar, RB_INSERTBAND, (WPARAM) 3, (LPARAM)&rbBand);

		//
		// add lowpass filter combo box on the second line of the rebar
		//
		// Set values unique to the band with the combo box.
		GetWindowRect(pgui->hwndSTAStreamingStatus, &rc);
		rbBand.lpText = TEXT("Streaming status:");
		rbBand.hwndChild = pgui->hwndSTAStreamingStatus;
		rbBand.cxMinChild = 0;
		rbBand.cyMinChild = rc.bottom - rc.top;
		// The default width should be set to some value wider than the text. The combo 
		// box itself will expand to fill the band.
		rbBand.cx = (int) (RBB_STRSTA_WIDTH*intHorizontalDPI);

		// Add the band that has the combo box.
		SendMessage(pgui->hwndRebar, RB_INSERTBAND, (WPARAM) 4, (LPARAM)&rbBand);

		//
		// add dummy band
		//
		rbBand.fMask  =   RBBIM_STYLE       // fStyle is valid.
						| RBBIM_SIZE;       // cx is valid

		// Set values unique to the band with the combo box.
		rbBand.cx = 110;

		// Add the band that has the combo box.
		SendMessage(pgui->hwndRebar, RB_INSERTBAND, (WPARAM) 5, (LPARAM)&rbBand);
	}
	else
		return GetLastError();

	//
	// create status bar and its font
	//
	// get the main window client area
	GetClientRect (hwndMainWindow, &rc);
	
	// retrieve Windows statusbar font
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);

	// create statusbar font either by using the Windows status bar font
	// or a default font
	pgui->hfntStatusBar = CreateFontIndirect(&ncm.lfStatusFont);
	if(pgui->hfntStatusBar != NULL)
		intTextHeight = abs(ncm.lfStatusFont.lfHeight);
	else
	{
		pgui->hfntStatusBar = CreateFont (DEFAULTTEXTHEIGHT,
										  0,
										  0,
										  0,
										  FW_NORMAL,
										  0,
										  0,
										  0,
										  DEFAULT_CHARSET, 
										  OUT_DEFAULT_PRECIS,
										  CLIP_DEFAULT_PRECIS,
										  DEFAULT_QUALITY,
										  VARIABLE_PITCH | FF_SWISS,
										  TEXT("MS Sans Serif"));
		intTextHeight = DEFAULTTEXTHEIGHT;
	}
	
	// create statusbar window
	pgui->hwndStatusBar = CreateWindowEx (0,
										  STATUSCLASSNAME,
										  mc_strStateStrs [0],
										  WS_CHILD | WS_VISIBLE,
										  0,
										  rc.bottom - intTextHeight - 2,
										  rc.right,
										  intTextHeight + 2, 
										  hwndMainWindow,
										  NULL,
										  m_hinMain,
										  0);
	
	if (pgui->hfntStatusBar && pgui->hwndStatusBar)
	{
		SendMessage(pgui->hwndStatusBar, WM_SETFONT, (WPARAM) pgui->hfntStatusBar, 0);
	}
	else
		return GetLastError();

	//
	// customize system menu
	//
	hmnuMenu = GetSystemMenu(hwndMainWindow, FALSE);
	InsertMenu(hmnuMenu, 0, MF_BYPOSITION | MF_STRING, IDM_FULLSCREEN, TEXT("Full-Screen Mode"));
	InsertMenu(hmnuMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, TEXT(""));
	InsertMenu(hmnuMenu, 2, MF_BYPOSITION | MF_STRING, IDM_SAMPLE_START, TEXT("Start Recording"));
	InsertMenu(hmnuMenu, 3, MF_BYPOSITION | MF_STRING, IDM_SAMPLE_STOP, TEXT("Stop Recording"));
	InsertMenu(hmnuMenu, 4, MF_BYPOSITION | MF_SEPARATOR, 0, TEXT(""));

	//
	// customize window menu
	//
	if(m_cfgConfiguration.SimulationMode)
	{
		hmnuMenu = GetMenu(hwndMainWindow);

		// in simulation mode => add "Load EDF+ File" menu item
		hmnuMenu = GetSubMenu(hmnuMenu, 0);
		InsertMenu(hmnuMenu, 0, MF_BYPOSITION | MF_STRING, IDM_LOADEDFFILE, TEXT("&Load EDF+ File"));
		InsertMenu(hmnuMenu, 0, MF_BYPOSITION | MF_STRING, IDM_TESTCONNSCRIPT, TEXT("&Test Connection Script"));
	}

	// enable/disable appropriate toolbar and menu commands
	GUI_SetEnabledCommands(hwndMainWindow, FALSE, *pgui);

	return ERROR_SUCCESS;
}

static BOOL CALLBACK Dialog_About (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	EDITSTREAM es = {0};
	HANDLE hFile;
	HWND hwndControl;
	LITEM item;
	PNMLINK pNMLink;
	TCHAR * pstrFilePath;
	SHELLEXECUTEINFO sei;
	size_t sztNChars;
	
	switch (uintMsg)
	{
		case WM_INITDIALOG:
			sztNChars = _tcslen(m_cfgConfiguration.ApplicationPath) + _tcslen(LIBRARIESRTF) + 1;
			pstrFilePath = (TCHAR *) malloc(sztNChars*sizeof(TCHAR));
			if(pstrFilePath != NULL)
			{
				_stprintf_s(pstrFilePath, sztNChars, TEXT("%s%s"), m_cfgConfiguration.ApplicationPath, LIBRARIESRTF);
			
				hFile = CreateFile(pstrFilePath,				// file to open
								   GENERIC_READ,				// open for reading
								   FILE_SHARE_READ,				// share for reading
								   NULL,						// default security
								   OPEN_EXISTING,				// existing file only
								   FILE_FLAG_SEQUENTIAL_SCAN,	// normal file
								   NULL);						// no attr. template
				if (hFile != INVALID_HANDLE_VALUE) 
				{
					// configure EDITSTREAM structure
					es.pfnCallback = Dialog_About_EditStreamCallback;
					es.dwCookie = (DWORD_PTR)hFile;
			
					hwndControl = GetDlgItem(hwndDlg, IDC_EXTLIBRARIES);
					SendMessage(hwndControl, EM_STREAMIN, SF_RTF, (LPARAM) &es);

					CloseHandle(hFile);
				}
				else
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_About - WM_INITDIALOG: Could not display copyrights. (GetLastError #)"), GetLastError(), TRUE);
			
				free(pstrFilePath);
			}
			else
				applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_About - WM_INITDIALOG: Could not allocate memory for pstrFilePath."), 0, TRUE);			
		break;
    
		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				case IDOK:
					EndDialog (hwndDlg, 0);
				break;

				case IDC_VIEWLICENSES:
					// show license file in default text editor
					SecureZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
					sei.cbSize = sizeof(SHELLEXECUTEINFO);
					sei.fMask = SEE_MASK_ASYNCOK;
					sei.hwnd = hwndDlg;
					sei.lpVerb = TEXT("open");
					sei.lpFile = LICENSESTXT;
					sei.lpDirectory = m_cfgConfiguration.ApplicationPath;
					sei.nShow = SW_SHOW;
					ShellExecuteEx(&sei);
				break;
			}
		break;

		case WM_NOTIFY:
			switch (((LPNMHDR)lParam)->code)
			{
				case NM_CLICK:
					// open URLs in web-browser
					pNMLink = (PNMLINK)lParam;
					item = pNMLink->item;
					
					SecureZeroMemory(&sei, sizeof(SHELLEXECUTEINFO));
					sei.cbSize = sizeof(SHELLEXECUTEINFO);
					sei.fMask = SEE_MASK_ASYNCOK | SEE_MASK_CLASSNAME;
					sei.hwnd = hwndDlg;
					sei.lpVerb = TEXT("open");
					sei.lpFile = item.szUrl;
					sei.lpClass = TEXT("http");
					sei.nShow = SW_SHOW;
					ShellExecuteEx(&sei);
				break;
			}
		break;

		case WM_CLOSE:
			EndDialog (hwndDlg, 0);
		break;
	}

	return (0);
}

static DWORD CALLBACK Dialog_About_EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	HANDLE hFile = (HANDLE)dwCookie;
	
	if (ReadFile(hFile, pbBuff, cb, (DWORD *)pcb, NULL)) 
		return 0;
	
	return -1;
}

static BOOL CALLBACK Dialog_StreamingConfig (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL			blnSave;
	CREDUI_INFO		cui;
	DWORD			dwrdTemp;
	PSHNOTIFY *		phdr;
	TCHAR			strUserName[CREDUI_MAX_USERNAME_LENGTH + 1];
	TCHAR			strPassword[CREDUI_MAX_PASSWORD_LENGTH + 1];

	/*
	TCHAR			strConnectionScriptFilePath[MAX_PATH_UNICODE + 1];*/

	switch (uintMsg)
	{
		case WM_INITDIALOG:
			if(m_cfgConfiguration.Streaming_Enabled)
				CheckDlgButton (hwndDlg, IDC_STREAMINGENABLED, BST_CHECKED);
			else
				CheckDlgButton (hwndDlg, IDC_STREAMINGENABLED, BST_UNCHECKED);
			
			// set IP address
			SendMessage(GetDlgItem(hwndDlg, IDC_STREAMINGSERVERIPv4),
						IPM_SETADDRESS,
						0,
						MAKEIPADDRESS(m_cfgConfiguration.Streaming_Server_IPv4_Field0,
									  m_cfgConfiguration.Streaming_Server_IPv4_Field1,
									  m_cfgConfiguration.Streaming_Server_IPv4_Field2,
									  m_cfgConfiguration.Streaming_Server_IPv4_Field3));

			// set initial control values
			SetDlgItemInt (hwndDlg, IDC_STREAMINGSERVERPORT, m_cfgConfiguration.Streaming_Server_Port, FALSE);
			SetDlgItemInt (hwndDlg, IDC_MAXNSENDMSGFAILURES, m_cfgConfiguration.Streaming_MaxNSendMsgFailures, FALSE);
			SetDlgItemInt (hwndDlg, IDC_MAXNWAIT4REPLYFAILURES, m_cfgConfiguration.Streaming_MaxNWait4ReplyFailures, FALSE);

			// enable/disable dialog items
			EnableWindow(GetDlgItem(hwndDlg, IDC_STREAMINGSERVERIPv4), m_cfgConfiguration.Streaming_Enabled);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_STREAMINGSERVERPORT), m_cfgConfiguration.Streaming_Enabled);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_MAXNSENDMSGFAILURES), m_cfgConfiguration.Streaming_Enabled);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_MAXNWAIT4REPLYFAILURES), m_cfgConfiguration.Streaming_Enabled);

			// set tool tips
			util_CreateToolTip(IDC_MAXNSENDMSGFAILURES, hwndDlg, TEXT ("Has to be be > 0."));
			util_CreateToolTip(IDC_MAXNWAIT4REPLYFAILURES, hwndDlg, TEXT ("Has to be be > 0."));
		break;

		case WM_NOTIFY:
			phdr = (LPPSHNOTIFY) lParam;
			switch (phdr->hdr.code)
			{
				case PSN_APPLY:
					// user clicked on the OK button
					if(phdr->lParam == TRUE)
					{
						// save IP address
						SendMessage(GetDlgItem(hwndDlg, IDC_STREAMINGSERVERIPv4), IPM_GETADDRESS, 0, (LPARAM) &dwrdTemp);
						m_cfgConfiguration.Streaming_Server_IPv4_Field0 = FIRST_IPADDRESS(dwrdTemp);
						m_cfgConfiguration.Streaming_Server_IPv4_Field1 = SECOND_IPADDRESS(dwrdTemp);
						m_cfgConfiguration.Streaming_Server_IPv4_Field2 = THIRD_IPADDRESS(dwrdTemp);
						m_cfgConfiguration.Streaming_Server_IPv4_Field3 = FOURTH_IPADDRESS(dwrdTemp);

						// save other values
						m_cfgConfiguration.Streaming_Server_Port = GetDlgItemInt (hwndDlg, IDC_STREAMINGSERVERPORT, NULL, FALSE);
						m_cfgConfiguration.Streaming_MaxNSendMsgFailures = GetDlgItemInt (hwndDlg, IDC_MAXNSENDMSGFAILURES, NULL, FALSE);
						m_cfgConfiguration.Streaming_MaxNWait4ReplyFailures = GetDlgItemInt (hwndDlg, IDC_MAXNWAIT4REPLYFAILURES, NULL, FALSE);
													
						// notify property sheet manager that the changes made to this page are valid and have been applied
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
 						return TRUE;
					}
				break;
			}
		break;

		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				// streaming enabled/disabled
				case IDC_STREAMINGENABLED:
					m_cfgConfiguration.Streaming_Enabled = !m_cfgConfiguration.Streaming_Enabled;

					// enable/disable dialog items
					EnableWindow(GetDlgItem(hwndDlg, IDC_STREAMINGSERVERIPv4), m_cfgConfiguration.Streaming_Enabled);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_STREAMINGSERVERPORT), m_cfgConfiguration.Streaming_Enabled);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_MAXNSENDMSGFAILURES), m_cfgConfiguration.Streaming_Enabled);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_MAXNWAIT4REPLYFAILURES), m_cfgConfiguration.Streaming_Enabled);
				break;

				// get SSH credentials
				case IDC_STREAMINGUSERPWD:
					cui.cbSize = sizeof(CREDUI_INFO);
					cui.hwndParent = hwndDlg;
					cui.pszMessageText = TEXT("Enter user account information");
					cui.pszCaptionText = TEXT("EEG Streaming Server Credentials");
					cui.hbmBanner = NULL;

					blnSave = TRUE;
					SecureZeroMemory(strUserName, sizeof(strUserName));
					SecureZeroMemory(strPassword, sizeof(strPassword));
					dwrdTemp = CredUIPromptForCredentials(&cui,									// CREDUI_INFO structure
														  CREDS_STREAMINGSERVER,				// Target for credentials (usually a server)
														  NULL,									// Reserved
														  0,									// Reason
														  strUserName,							// User name
														  CREDUI_MAX_USERNAME_LENGTH+1,			// Max number of char for user name
														  strPassword,							// Password
														  CREDUI_MAX_PASSWORD_LENGTH+1,			// Max number of char for password
														  &blnSave,								// State of save check box
														  CREDUI_FLAGS_GENERIC_CREDENTIALS |	// flags
														  CREDUI_FLAGS_ALWAYS_SHOW_UI);  

					//  erase credentials from memory
					SecureZeroMemory(strUserName, sizeof(strUserName));
					SecureZeroMemory(strPassword, sizeof(strPassword));
				break;
			}
		break;
	}

	return (0);
}

static BOOL CALLBACK Dialog_InternetConnectionConfig (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL					blnTranslated;
	DWORD					dwrdCb;					///< on input contains the size, in bytes, of the buffer specified by lpRasEntryName; on output,  contains the size, in bytes, of the array of RASENTRYNAME structures required for the phone-book entries
	DWORD					dwrdEntries;			///< number of phone-book entries written by RasEnumEntries() to the buffer specified by lpRasEntryName
	DWORD					dwrdRet;				///< function return code
	HWND					hwndControl;
	int						intPosition;			///< stores zero-based index of an item when it is added in the list box
	OPENFILENAME			ofn;
	PSHNOTIFY *				phdr;
	static LPRASENTRYNAME	lpRasEntryName = NULL;	///< buffer that, on output of RasEnumEntries(), receives an array of RASENTRYNAME structures, one for each phone-book entry
	TCHAR					strConnectionScriptFilePath[MAX_PATH_UNICODE + 1];
	unsigned int			i;

	// variable initialization
	dwrdCb = 0;
    dwrdRet = ERROR_SUCCESS;
    dwrdEntries = 0;

	switch (uintMsg)
	{
		case WM_INITDIALOG:
			//
			// internet connection
			//
			// enable/disable controls
			if(m_cfgConfiguration.DialConnectionScript)
				CheckDlgButton (hwndDlg, IDC_ICC_DIALCONN, BST_CHECKED);
			else
				CheckDlgButton (hwndDlg, IDC_ICC_DIALCONN, BST_UNCHECKED);
			//ListBox_Enable(GetDlgItem(hwndDlg, IDC_ICC_CONNECTIONSLISTBOX), m_cfgConfiguration.DialConnectionScript);

			// get required size of lpRasEntryName
			dwrdRet = RasEnumEntries(NULL, NULL, lpRasEntryName, &dwrdCb, &dwrdEntries);
			if (dwrdRet == ERROR_BUFFER_TOO_SMALL)
			{
				// Allocate the memory needed for the array of RAS entry names.
				lpRasEntryName = (LPRASENTRYNAME) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwrdCb);
				if (lpRasEntryName == NULL)
				{
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_InternetConnectionConfig(): HeapAlloc failed."), 0, TRUE);
				}
				else
				{
					// The first RASENTRYNAME structure in the array must contain the structure size
					lpRasEntryName[0].dwSize = sizeof(RASENTRYNAME);
        
					// Call RasEnumEntries to enumerate all RAS entry names
					dwrdRet = RasEnumEntries(NULL, NULL, lpRasEntryName, &dwrdCb, &dwrdEntries);

					// if successful, add RAS entry names to listbox
					if (dwrdRet == ERROR_SUCCESS)
					{
						hwndControl = GetDlgItem(hwndDlg, IDC_ICC_CONNECTIONSLISTBOX);  
						for (i = 0; i < dwrdEntries; i++) 
						{ 
							intPosition = (int)SendMessage(hwndControl, LB_ADDSTRING, 0, (LPARAM) lpRasEntryName[i].szEntryName); 
							
							// Set the array index of the player as item data.
							// This enables us to retrieve the item from the array
							// even after the items are sorted by the list box.
							SendMessage(hwndControl, LB_SETITEMDATA, intPosition, (LPARAM) i); 
						} 
					}
				
					//Deallocate memory for the connection buffer
					HeapFree(GetProcessHeap(), 0, lpRasEntryName);
					lpRasEntryName = NULL;
				}
			}
			else
			{
				// there was either a problem with RAS or there are no RAS entry names to enumerate    
				if(dwrdEntries >= 1)
				{
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_InternetConnectionConfig(): The operation failed to acquire the buffer size."), 0, TRUE);
				}
				else
				{
					applog_logevent(SoftwareError, TEXT("Main"), TEXT("Dialog_InternetConnectionConfig(): There were no RAS entry names found."), 0, TRUE);
				}
			}

			// TODO: highlight currently set connection in the listbox
			//listbox.count > 1 && config file connection name is not emprty

			//
			// script execution
			//
			// enable/disable controls
			if(m_cfgConfiguration.DialConnectionScript)
				CheckDlgButton (hwndDlg, IDC_ICC_EXECCONNSCRIPT, BST_CHECKED);
			else
				CheckDlgButton (hwndDlg, IDC_ICC_EXECCONNSCRIPT, BST_UNCHECKED);

			Edit_Enable(GetDlgItem(hwndDlg, IDC_ICC_CONNSCRIPT_FILE), m_cfgConfiguration.DialConnectionScript);

			// populate script textbox
			SetDlgItemText (hwndDlg, IDC_ICC_CONNSCRIPT_FILE, m_cfgConfiguration.ConnectionScriptPath);
		break;

		case WM_NOTIFY:
			phdr = (LPPSHNOTIFY) lParam;
			switch (phdr->hdr.code)
			{
				case PSN_APPLY:
					// user clicked on the OK button
					if(phdr->lParam == TRUE)
					{
						// Save Changes
						GetDlgItemText (hwndDlg, IDC_SSHHOSTNAME, m_cfgConfiguration.SSH_HostName, sizeof(m_cfgConfiguration.SSH_HostName)/sizeof(TCHAR));
						m_cfgConfiguration.SSH_Port = GetDlgItemInt (hwndDlg, IDC_SSHPORT, &blnTranslated, FALSE);
						GetDlgItemText (hwndDlg, IDC_SSHREMOTEPATH, m_cfgConfiguration.SSH_RemotePath, sizeof(m_cfgConfiguration.SSH_RemotePath)/sizeof(TCHAR));
						GetDlgItemText (hwndDlg, IDC_CONNSCRIPT_FILE, m_cfgConfiguration.ConnectionScriptPath, sizeof(m_cfgConfiguration.ConnectionScriptPath)/sizeof(TCHAR));
						m_cfgConfiguration.SSH_ConnectionTimeout = GetDlgItemInt (hwndDlg, IDC_SSHCONNTIMEOUT, &blnTranslated, FALSE);
						m_cfgConfiguration.SSH_LowSpeedLimit = GetDlgItemInt (hwndDlg, IDC_SSHLOWSPEEDLIMIT, &blnTranslated, FALSE);
						m_cfgConfiguration.SSH_LowSpeedTime = GetDlgItemInt (hwndDlg, IDC_SSHLOWSPEEDTIME, &blnTranslated, FALSE);

						// notify property sheet manager that the changes made to this page are valid and have been applied
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
 						return TRUE;
					}
				break;
			}
		break;

		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				/*case IDC_ICC_CONNECTIONSLISTBOX:
				{
					switch (HIWORD(wParam)) 
					{ 
						case LBN_SELCHANGE:
						{
							hwndList = GetDlgItem(hDlg, IDC_LISTBOX_EXAMPLE); 

							// Get selected index.
							int lbItem = (int)SendMessage(hwndList, LB_GETCURSEL, 0, 0); 

							// Get item data.
							i = (int) SendMessage(hwndList, LB_GETITEMDATA, lbItem, 0);

							// Do something with the data from Roster[i]
							/*TCHAR buff[MAX_PATH];
							StringCbPrintf (buff, ARRAYSIZE(buff),  
								TEXT("Position: %s\nGames played: %d\nGoals: %d"), 
								Roster[i].achPosition, Roster[i].nGamesPlayed, 
								Roster[i].nGoalsScored);

							SetDlgItemText(hDlg, IDC_STATISTICS, buff);
							return TRUE; 
						} 
					}
				}
				return TRUE;*/

				// selection toggled, store change
				case IDC_ICC_DIALCONN:
					m_cfgConfiguration.SSH_AutomaticUpload = !m_cfgConfiguration.SSH_AutomaticUpload;
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHHOSTNAME), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHPORT), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHREMOTEPATH), m_cfgConfiguration.SSH_AutomaticUpload);
					Button_Enable(GetDlgItem(hwndDlg, IDC_SSHUSERPWD), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHCONNTIMEOUT), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHLOWSPEEDLIMIT), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHLOWSPEEDTIME), m_cfgConfiguration.SSH_AutomaticUpload);
				break;

				// selection toggled, store change
				case IDC_ICC_EXECCONNSCRIPT:
					m_cfgConfiguration.DialConnectionScript = !m_cfgConfiguration.DialConnectionScript;

					// enable text box with path to connection script
					Edit_Enable(GetDlgItem(hwndDlg, IDC_CONNSCRIPT_FILE), m_cfgConfiguration.DialConnectionScript);
					
					if(m_cfgConfiguration.DialConnectionScript)
					{
						if(_tcslen(m_cfgConfiguration.ConnectionScriptPath) > 0)
							_tcscpy_s(strConnectionScriptFilePath, sizeof(strConnectionScriptFilePath)/sizeof(TCHAR), m_cfgConfiguration.ConnectionScriptPath);

						// Initialize OPENFILENAME
						SecureZeroMemory (&ofn, sizeof(ofn));
						ofn.lStructSize = sizeof(ofn);
						ofn.hwndOwner = hwndDlg;
						ofn.lpstrFile = strConnectionScriptFilePath;
						ofn.lpstrFile[0] = TEXT('\0');												// Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the contents of szFile to initialize itself.
						ofn.nMaxFile = sizeof(strConnectionScriptFilePath)/sizeof(TCHAR);
						ofn.lpstrFilter = TEXT("Windows Script Host Files (*.js;*.jse;*.vbs;*.vbe;*.wsf)\0*.js;*.jse;*.vbs;*.vbe;*.wsf\0All Files (*.*)\0*.*\0");
						ofn.nFilterIndex = 1;
						ofn.lpstrFileTitle = NULL;
						ofn.nMaxFileTitle = 0;
						ofn.lpstrInitialDir = NULL;
						ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
					
						// display open file dialog
						if(GetOpenFileName(&ofn) == TRUE)
						{
							SetDlgItemText (hwndDlg, IDC_CONNSCRIPT_FILE, strConnectionScriptFilePath);
						}
					}
				break;
			}
		break;
	}

	return (0);
}

static BOOL CALLBACK Dialog_SSHConfig (HWND hwndDlg, UINT uintMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL			blnSave, blnTranslated;
	CREDUI_INFO		cui;
	DWORD			dwrdErr;
	HWND			hwndControl;
	PSHNOTIFY *		phdr;
	TCHAR			strUserName[CREDUI_MAX_USERNAME_LENGTH + 1];
	TCHAR			strPassword[CREDUI_MAX_PASSWORD_LENGTH + 1];

	switch (uintMsg)
	{
		case WM_INITDIALOG:
			// set maximum number of characters that can be entered in the annotation text boxes
			hwndControl = GetDlgItem(hwndDlg, IDC_SSHHOSTNAME);
			SendMessage(hwndControl, EM_SETLIMITTEXT, MAX_HOSTNAME_LEN, 0);

			// set initial control values
			SetDlgItemText (hwndDlg, IDC_SSHHOSTNAME, m_cfgConfiguration.SSH_HostName);
			SetDlgItemInt  (hwndDlg, IDC_SSHPORT, m_cfgConfiguration.SSH_Port, TRUE);
			SetDlgItemText (hwndDlg, IDC_SSHREMOTEPATH, m_cfgConfiguration.SSH_RemotePath);
			SetDlgItemInt  (hwndDlg, IDC_SSHCONNTIMEOUT, m_cfgConfiguration.SSH_ConnectionTimeout, TRUE);
			SetDlgItemInt  (hwndDlg, IDC_SSHLOWSPEEDLIMIT, m_cfgConfiguration.SSH_LowSpeedLimit, TRUE);
			SetDlgItemInt  (hwndDlg, IDC_SSHLOWSPEEDTIME, m_cfgConfiguration.SSH_LowSpeedTime, TRUE);

			// enable/disable GUI items
			if(m_cfgConfiguration.SSH_AutomaticUpload)
				CheckDlgButton (hwndDlg, IDC_AUTOMATICUPLOAD, BST_CHECKED);
			else
				CheckDlgButton (hwndDlg, IDC_AUTOMATICUPLOAD, BST_UNCHECKED);
			
			Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHHOSTNAME), m_cfgConfiguration.SSH_AutomaticUpload);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHPORT), m_cfgConfiguration.SSH_AutomaticUpload);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHREMOTEPATH), m_cfgConfiguration.SSH_AutomaticUpload);
			Button_Enable(GetDlgItem(hwndDlg, IDC_SSHUSERPWD), m_cfgConfiguration.SSH_AutomaticUpload);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHCONNTIMEOUT), m_cfgConfiguration.SSH_AutomaticUpload);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHLOWSPEEDLIMIT), m_cfgConfiguration.SSH_AutomaticUpload);
			Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHLOWSPEEDTIME), m_cfgConfiguration.SSH_AutomaticUpload);

			// set tool tips
			util_CreateToolTip(IDC_SSHHOSTNAME, hwndDlg, TEXT ("e.g., ssh.server.fi"));
			util_CreateToolTip(IDC_SSHREMOTEPATH, hwndDlg, TEXT ("e.g., /home/user"));
			util_CreateToolTip(IDC_SSHCONNTIMEOUT, hwndDlg, TEXT ("maximum time that connection to the SSH server is allowed to take (in seconds)"));
			util_CreateToolTip(IDC_SSHLOWSPEEDLIMIT, hwndDlg, TEXT ("transfer speed that the transfer should be below during the 'Low Speed Time' for the SSH transfer to be consider as too slow and aborted (in bytes per second)"));
			util_CreateToolTip(IDC_SSHLOWSPEEDTIME, hwndDlg, TEXT ("time that the transfer should be below the SSH_LowSpeedLimit for the SSH transfer to be considered as too slow and aborted (in seconds)"));
		break;

		case WM_NOTIFY:
			phdr = (LPPSHNOTIFY) lParam;
			switch (phdr->hdr.code)
			{
				case PSN_APPLY:
					// user clicked on the OK button
					if(phdr->lParam == TRUE)
					{
						// Save Changes
						GetDlgItemText (hwndDlg, IDC_SSHHOSTNAME, m_cfgConfiguration.SSH_HostName, sizeof(m_cfgConfiguration.SSH_HostName)/sizeof(TCHAR));
						m_cfgConfiguration.SSH_Port = GetDlgItemInt (hwndDlg, IDC_SSHPORT, &blnTranslated, FALSE);
						GetDlgItemText (hwndDlg, IDC_SSHREMOTEPATH, m_cfgConfiguration.SSH_RemotePath, sizeof(m_cfgConfiguration.SSH_RemotePath)/sizeof(TCHAR));
						m_cfgConfiguration.SSH_ConnectionTimeout = GetDlgItemInt (hwndDlg, IDC_SSHCONNTIMEOUT, &blnTranslated, FALSE);
						m_cfgConfiguration.SSH_LowSpeedLimit = GetDlgItemInt (hwndDlg, IDC_SSHLOWSPEEDLIMIT, &blnTranslated, FALSE);
						m_cfgConfiguration.SSH_LowSpeedTime = GetDlgItemInt (hwndDlg, IDC_SSHLOWSPEEDTIME, &blnTranslated, FALSE);

						// notify property sheet manager that the changes made to this page are valid and have been applied
						SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
 						return TRUE;
					}
				break;
			}
		break;

		case WM_COMMAND:
			switch (LOWORD (wParam))
 			{ 
				// get SSH credentials
				case IDC_SSHUSERPWD:
					cui.cbSize = sizeof(CREDUI_INFO);
					cui.hwndParent = hwndDlg;
					cui.pszMessageText = TEXT("Enter user account information");
					cui.pszCaptionText = TEXT("EEG SSH Server Credentials");
					cui.hbmBanner = NULL;

					blnSave = TRUE;
					SecureZeroMemory(strUserName, sizeof(strUserName));
					SecureZeroMemory(strPassword, sizeof(strPassword));
					dwrdErr = CredUIPromptForCredentials(&cui,								// CREDUI_INFO structure
														 CREDS_SSHSERVER,					// Target for credentials (usually a server)
														 NULL,								// Reserved
														 0,									// Reason
														 strUserName,						// User name
														 CREDUI_MAX_USERNAME_LENGTH+1,		// Max number of char for user name
														 strPassword,						// Password
														 CREDUI_MAX_PASSWORD_LENGTH+1,		// Max number of char for password
														 &blnSave,							// State of save check box
														 CREDUI_FLAGS_GENERIC_CREDENTIALS | // flags
														 CREDUI_FLAGS_ALWAYS_SHOW_UI);  

					//  erase credentials from memory
					SecureZeroMemory(strUserName, sizeof(strUserName));
					SecureZeroMemory(strPassword, sizeof(strPassword));
				break;

				// selection toggled, store change
				case IDC_AUTOMATICUPLOAD:
					m_cfgConfiguration.SSH_AutomaticUpload = !m_cfgConfiguration.SSH_AutomaticUpload;
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHHOSTNAME), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHPORT), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHREMOTEPATH), m_cfgConfiguration.SSH_AutomaticUpload);
					Button_Enable(GetDlgItem(hwndDlg, IDC_SSHUSERPWD), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHCONNTIMEOUT), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHLOWSPEEDLIMIT), m_cfgConfiguration.SSH_AutomaticUpload);
					Edit_Enable(GetDlgItem(hwndDlg, IDC_SSHLOWSPEEDTIME), m_cfgConfiguration.SSH_AutomaticUpload);
				break;
			}
		break;
	}

	return (0);
}

//---------------------------------------------------------------------------------------------------------------------------------
//   								Sample thread functions
//---------------------------------------------------------------------------------------------------------------------------------
/**
 * \brief Adds the 'Communication failure' annotation to the data record when a communication failure occurs.
 *
 * When a communication failure occurs, this function is called. The first time it is called, a 'Communication failure' annotation
 * containing a time stamp is added to the data record. The remainder of the the data record signal samples (both EEG and Gyro) are
 * filled with a default value. Once this is done, the data record is immediately saved to the EDF+ file.
 * On subsequent calls during a communication failure, this function returns without doing anything.
 *
 * \param[in]	pdrCurrentDataRecord	pointer to the SampleDataRecord structure where the measurement data of the current data record is being stored
 * \param[in]	pstd					pointer to the SampleThreadData structure that was passed to the thread upon its creation by the CreateThread function
 * \param[in]	hwndMainWindow			handle to the application's main window
 *
 * \return TRUE if succesfull, FALSE otherwise.
 */
static BOOL Sample_DetectedCommunicationFailure(SampleDataRecord * pdrCurrentDataRecord, SampleThreadData * pstd, HWND hwndMainWindow)
{
	BOOL blnResult = TRUE;
	GUIElements * pgui = pstd->pgui;

	// Check if communication blackout is already in progress
	if(!m_blnCommunicationBlackout)
	{
		m_blnCommunicationBlackout = TRUE;
		
		// add annotation indicating that a communication failer has occured
		main_InsertAnnotation(CommunicationFailure, -1, *pgui, hwndMainWindow);
		
		// fill the data record with INVALID_DATA_SAMPLE samples
		while(m_intNSamplesDatarecord < m_cfgConfiguration.SamplingFrequency)
		{
			pdrCurrentDataRecord->MeasurementData[0][m_intNSamplesDatarecord] = INVALID_ACC_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[1][m_intNSamplesDatarecord] = INVALID_ACC_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[2][m_intNSamplesDatarecord] = INVALID_ACC_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[3][m_intNSamplesDatarecord] = INVALID_EEG_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[4][m_intNSamplesDatarecord] = INVALID_EEG_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[5][m_intNSamplesDatarecord] = INVALID_EEG_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[6][m_intNSamplesDatarecord] = INVALID_EEG_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[7][m_intNSamplesDatarecord] = INVALID_EEG_SAMPLE;
			pdrCurrentDataRecord->MeasurementData[8][m_intNSamplesDatarecord] = INVALID_EEG_SAMPLE;
			m_intNSamplesDatarecord++;
		}

		// store and transmit data record
		m_intNSamplesDatarecord = 0;
		blnResult = Sample_StoreAndTransmitDataRecord(pdrCurrentDataRecord, pstd);
	}
	
	return blnResult;
}

/**
 * \brief Returns the current state of the sample thread's main FSM.
 *
 * \return	A member of the SampleThreadState enumeration.
 */
 static SampleThreadState Sample_GetMainFSMState(void)
 {
	 return m_stsCurrentSampleThreadState;
 }

/**
 * \brief 
 *
 * Handle received data: construct display buffer for the scope and store data to the file.
 * If bInit is true, packet's number is stored and it is used to detect first skipped packets.
 *
 * \param[in]	Data
 * \param[in]	DataLength
 * \param[in]	ClusterSize
 * \param[in]	bInit
 * \return ... 
 */
static BOOL Sample_ProcessDataPacket (tPacket_DATA * ptpMeasurementData, SampleDataRecord * pdrCurrentDataRecord, SampleThreadData * pstd, HWND hwndMainWindow)
{
	BOOL			blnResult;
	GUIElements *	pgui;
	int				j;

	// variable initialization
	blnResult = TRUE;
	pgui = (GUIElements *) pstd->pgui;
		
	// if a communication failure was in progress, insert anotation that
	// comunication has now resumed
	if(m_blnCommunicationBlackout)
	{
		m_blnCommunicationBlackout = FALSE;
		main_InsertAnnotation(CommunicationResume, -1, *pgui, hwndMainWindow);
	}

	// check battery voltage
	if(ptpMeasurementData->BatteryLevel <= 3600)
		m_blnBatteryLow = TRUE;
	else
		m_blnBatteryLow = FALSE;

	//
	// add data to display buffer
	//
	WaitForSingleObject(m_hMutexSampleBuffer, INFINITE);
			
	// Iterate through all samples from each channel
	for (j = 0; j < mc_intSampleLengths [EEGCHANNELS]; j++) // SampleLength = number of measurements per channel
	{ 
		// store EEG channels in display buffer
		m_pshrSampleBuffer [0][m_uintNNewSamples] = CAST_16b_US2S(ptpMeasurementData->Measurements [(EEGCHANNELS) * j]) + ((short) m_cfgConfiguration.ChannelDCOffset[0]);
		m_pshrSampleBuffer [1][m_uintNNewSamples] = CAST_16b_US2S(ptpMeasurementData->Measurements [(EEGCHANNELS) * j + 1]) + ((short) m_cfgConfiguration.ChannelDCOffset[1]);
		m_pshrSampleBuffer [2][m_uintNNewSamples] = CAST_16b_US2S(ptpMeasurementData->Measurements [(EEGCHANNELS) * j + 2]) + ((short) m_cfgConfiguration.ChannelDCOffset[2]);
		m_pshrSampleBuffer [3][m_uintNNewSamples] = CAST_16b_US2S(ptpMeasurementData->Measurements [(EEGCHANNELS) * j + 3]) + ((short) m_cfgConfiguration.ChannelDCOffset[3]);
		m_pshrSampleBuffer [4][m_uintNNewSamples] = CAST_16b_US2S(ptpMeasurementData->Measurements [(EEGCHANNELS) * j + 4]) + ((short) m_cfgConfiguration.ChannelDCOffset[4]);
		m_pshrSampleBuffer [5][m_uintNNewSamples] = CAST_16b_US2S(ptpMeasurementData->Measurements [(EEGCHANNELS) * j + 5]) + ((short) m_cfgConfiguration.ChannelDCOffset[5]);

		// store accelerometer channels in display buffer
		m_pshrSampleBuffer [6][m_uintNNewSamples] = CAST_10b_US2S(ptpMeasurementData->Accelerometers[0]);
		m_pshrSampleBuffer [7][m_uintNNewSamples] = CAST_10b_US2S(ptpMeasurementData->Accelerometers[1]);
		m_pshrSampleBuffer [8][m_uintNNewSamples] = CAST_10b_US2S(ptpMeasurementData->Accelerometers[2]);

		// NOTE: reset to 0 -> should not be needed but is kept here just as an indication
		// of a data bottleneck
		if(++m_uintNNewSamples == m_uintSampleBufferLength)
		{
			applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_ProcessDataPacket: m_pshrSampleBuffer overflowed."), 0, TRUE);
			m_uintNNewSamples = 0;
		}
	}

	//
	ReleaseMutex(m_hMutexSampleBuffer);

	//
	// add data to storage and streaming buffer
	//
	// Iterate through all samples from each channel again
	for (j = 0; j < mc_intSampleLengths [EEGCHANNELS]; j++) // SampleLength = number of measurements per channel
	{ 
		//////// EDF+ ////////////////
		// Fill the EDF+ buffer
		pdrCurrentDataRecord->MeasurementData[0][m_intNSamplesDatarecord] = CAST_10b_US2S(ptpMeasurementData->Accelerometers[0]);							// Acc - X
		pdrCurrentDataRecord->MeasurementData[1][m_intNSamplesDatarecord] = CAST_10b_US2S(ptpMeasurementData->Accelerometers[1]);							// Acc - Y
		pdrCurrentDataRecord->MeasurementData[2][m_intNSamplesDatarecord] = CAST_10b_US2S(ptpMeasurementData->Accelerometers[2]);							// Acc - Z
		pdrCurrentDataRecord->MeasurementData[3][m_intNSamplesDatarecord] = CAST_16b_US2S(ptpMeasurementData->Measurements [EEGCHANNELS * j]);				// EEG - P10
		pdrCurrentDataRecord->MeasurementData[4][m_intNSamplesDatarecord] = CAST_16b_US2S(ptpMeasurementData->Measurements [EEGCHANNELS * j + 1]);			// EEG - F8
		pdrCurrentDataRecord->MeasurementData[5][m_intNSamplesDatarecord] = CAST_16b_US2S(ptpMeasurementData->Measurements [EEGCHANNELS * j + 2]);			// EEG - FP2
		pdrCurrentDataRecord->MeasurementData[6][m_intNSamplesDatarecord] = CAST_16b_US2S(ptpMeasurementData->Measurements [EEGCHANNELS * j + 3]);			// EEG - FP1
		pdrCurrentDataRecord->MeasurementData[7][m_intNSamplesDatarecord] = CAST_16b_US2S(ptpMeasurementData->Measurements [EEGCHANNELS * j + 4]);			// EEG - F7
		pdrCurrentDataRecord->MeasurementData[8][m_intNSamplesDatarecord] = CAST_16b_US2S(ptpMeasurementData->Measurements [EEGCHANNELS * j + 5]);			// EEG - P9
		
		if (++m_intNSamplesDatarecord == m_cfgConfiguration.SamplingFrequency)
		{
			blnResult = Sample_StoreAndTransmitDataRecord(pdrCurrentDataRecord, pstd);
			m_intNSamplesDatarecord = 0;
		}
	}
	
	return blnResult;
} 

/**
 * \brief Adds current data record to the write queue of the storage thread and the transmission queue of the streaming thread.
 *
 * \param[in]	pdrCurrentDataRecord	
 * \param[in]	pstd					
 *
 * \return TRUE if successsfull, FALSE otherwise. 
 */
static BOOL Sample_StoreAndTransmitDataRecord(SampleDataRecord * pdrCurrentDataRecord, SampleThreadData * pstd)
{
	BOOL			blnResult = FALSE;
	unsigned int	uintSignalLength;	///< length of acceleration/EEG signal per data record, in bytes
	unsigned int	k;
	
	//
	// copy data to from MeasurementData to WriteBuffer
	//
	// copy measurement data
	uintSignalLength = m_cfgConfiguration.SamplingFrequency*sizeof(short);
	for(k = 0; k < (EEGCHANNELS + ACCCHANNELS); k++)
	{
		memcpy_s(pdrCurrentDataRecord->WriteBuffer + k*uintSignalLength, uintSignalLength,
				 pdrCurrentDataRecord->MeasurementData[k], uintSignalLength);
	}

	// copy annotations
	WaitForSingleObject(m_hMutexAnnotation, INFINITE);
	
	memcpy_s(&pdrCurrentDataRecord->WriteBuffer[(EEGCHANNELS + ACCCHANNELS) * m_cfgConfiguration.SamplingFrequency * sizeof(short)], ANNOTATION_TOTAL_NCHARS*sizeof(char),
			 m_strCurrentDRAnnotations, ANNOTATION_TOTAL_NCHARS*sizeof(char));
	
	m_uintTimeKeepingTAL += EDFDURATIONOFRECORD;
	m_intNAnnotationsBuffered = 0;
	main_InitAnnotationSignal(m_strCurrentDRAnnotations, _countof(m_strCurrentDRAnnotations), m_uintTimeKeepingTAL);
	
	ReleaseMutex(m_hMutexAnnotation);

	//
	// send data record to storage thread
	//
	blnResult = Storage_AddToQueue(pdrCurrentDataRecord->WriteBuffer, pdrCurrentDataRecord->WriteBufferLen, FALSE, pstd->hevStorageThread_Write);

	//
	// send data record to streaming thread (if enabled)
	//
	if(m_cfgConfiguration.Streaming_Enabled)
	{
		// send data record
		if(!Streaming_SendPacket(EEGEMPacketType_EDFdr,
							     pdrCurrentDataRecord->WriteBuffer,
							     (unsigned short) pdrCurrentDataRecord->WriteBufferLen,
							     pstd->hevVortexClient_Transmit))
		{
			// log error
			applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Sample_StoreAndTransmitDataRecord(): Unable to send packet to streaming server."), 0, TRUE);

			// since connection was unsucessfull, disable streaming
			m_cfgConfiguration.Streaming_Enabled = FALSE;
		}
	}

	// Reset value of current samples in data record and increase amount of total data records
	++m_intNDataRecords;

	return blnResult;
}

/**
 * \brief Function executed when sampling thread is created using the CreateThread function.
 *
 * \param[in]	lParam		thread data passed to the function using the lpParameter parameter of the CreateThread function
 *
 * \return Indicates success or failure of the function.
 */
static long WINAPI Sample_Thread (LPARAM lParam)
{
	HWND						hwndMainWnd;
	GUIElements	*				pgui;
	SampleThreadData *			pstd;

	// local variable init
	pstd = (SampleThreadData *) lParam;
	pgui = pstd->pgui;
	hwndMainWnd = FindWindow (WINDOW_CLASSID_MAIN, NULL);

	while (1)
	{
		switch (m_stsCurrentSampleThreadState)
		{
			case SampleThreadState_Idle:
				// signal that state machine is in Idle state and wait for Start event
				SignalObjectAndWait(pstd->hevSampleThread_Idle, pstd->hevSampleThread_Start, INFINITE, FALSE);

				//
				// state transition
				//
				m_stsCurrentSampleThreadState = SampleThreadState_Working;
			break;

			case SampleThreadState_Working:
				// check requested mode and pass control to the appropriate FSM
				switch(pstd->Mode)
				{
					case SampleThreadMode_Recording:
						Sample_RecordingFSM(pstd, hwndMainWnd);
					break;

					case SampleThreadMode_WEEGSystemCheck:
						Sample_WEEGSystemCheckFSM(pstd);
					break;

					case SampleThreadMode_Simulation:
						Sample_SimulationFSM(pstd, hwndMainWnd);
					break;

					case SampleThreadMode_WEEGDCCalibration:
						/// TODO
					break;

					default:
						applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_Thread(): Unknown sample thread mode detected."), 0, TRUE);
				}

				//
				// state transition
				//
				m_stsCurrentSampleThreadState = SampleThreadState_Idle;
			break;
		}
	}
	
	return (0);
}

static void Sample_SimulationFSM(SampleThreadData * pstd, HWND	hwndMainWnd)
{
	// code for simulating the reception of data
	BOOL					blnStayInFSM;
	BOOL					blnStateErrorOccured;
	EDFFileHandle *			hEDFFile;				///< handle to the EDF+ file used as input for the simulation mode
	int						intDRIndex;				///< one-based index of the data record currently being read from the EDF+ file (i.e., index of first DR is 0)
	int						i;
	int						intSamplingFrequency;
	ledf_RetCode			rc;
	tPacket_DATA			tpdMeasurementData;
	SampleDataRecord		drCurrentDataRecord;
	SimulationModeData *	psmd;
	SimulationModeState		smsState;
	TCHAR					strBuffer[80];
	unsigned int			uintDRSampleCounter;	///< zero-based index indiciating sample being currently read from the data record

	// variable initialization
	blnStayInFSM = TRUE;
	drCurrentDataRecord = mc_sdrEmpty;
	smsState = SimulationModeState_Initialize;
	psmd = (SimulationModeData *) pstd->pModeData;
	intSamplingFrequency = *(pstd->pSamplingFrequency);

	while (blnStayInFSM)
	{
		switch (smsState)
		{
			case SimulationModeState_Initialize:
				blnStateErrorOccured = FALSE;

				// set event to signaled state
				SetEvent(pstd->hevSampleThread_Init);

				// check if simulation file has been loaded
				if(strlen(psmd->strSimulationEDFFile) > 0)
				{
					hEDFFile = libEDF_openFile(psmd->strSimulationEDFFile);
					if(hEDFFile == NULL)
					{
						applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_SimulationFSM() - SimulationModeState_Initialize: Could not load EDF+ file."), 0, TRUE);
						MsgPrintf(hwndMainWnd, MB_ICONSTOP, TEXT("Sample_SimulationFSM() - SimulationModeState_Initialize: Could not load EDF+ file."), 0);
						PostMessage(hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						break;
					}
				}
				else
				{
					applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_SimulationFSM() - SimulationModeState_Initialize: No simulation file loaded."), 0, TRUE);
					MsgPrintf(hwndMainWnd, MB_ICONERROR, TEXT("Sample_SimulationFSM() - SimulationModeState_Initialize: No simulation file loaded."));
					PostMessage(hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
					break;
				}

				// initialize data record linked list
				if(!Sample_InitDataRecord(&drCurrentDataRecord, intSamplingFrequency))
				{
					applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_SimulationFSM() - SimulationModeState_Initialize: Failed to initialize data record structure."), 0, TRUE);
					PostMessage (hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);	// Force stop
					blnStateErrorOccured = TRUE;
				}
				
				if(!blnStateErrorOccured)
				{
					if(hEDFFile->FileHeader.NSignalsPerDataRecord <= 0)
					{
						// get size of a data record, in bytes
						applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_SimulationFSM() - SampleThreadState_Initialize: EDF+ file contains no signals."), 0, TRUE);
						PostMessage (hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);	// Force stop
						blnStateErrorOccured = TRUE;
					}
				}

				//
				// state transition
				//
				if(blnStateErrorOccured)
					smsState = SimulationModeState_CleanUp;
				else
					smsState = SimulationModeState_Acquire;
			break;
			
			// Sampling state
			case SimulationModeState_Acquire:
				// simulate good battery level so that 'low battery' alarm doesn't get triggered
				tpdMeasurementData.BatteryLevel = 4100;

				// initialize DR sample coutner to the sampling frequency in order to trigger a read
				uintDRSampleCounter = intSamplingFrequency;

				// initilize data record index
				intDRIndex = 0;

				while(!pstd->EndActivity)
				{
					//
					// fill tPacket_DATA structure with data samples from EDFFileHandle structure
					//
					for(i = 0; i < mc_intSampleLengths [EEGCHANNELS]; i++)
					{
						//
						// if end of EDF+ data record has been reached, read new data record from EDF+ file
						//
						if(uintDRSampleCounter == intSamplingFrequency)
						{
							// if yes: reset reading indexes to beginning of next data record, read the next record
							//		   and update status bar
							uintDRSampleCounter = 0;

							// get data record from file
							rc = libEDF_getDataRecord(hEDFFile, intDRIndex + 1);
							if(rc == LEDF_OK)
							{
								_stprintf_s (strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Reading data record: %d/%d"), intDRIndex + 1, hEDFFile->FileHeader.NDataRecords);
								SendMessage(hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) strBuffer);
							}

							// increase data record index
							intDRIndex = (intDRIndex++)%hEDFFile->FileHeader.NDataRecords;
						}
						
						// transfer accelerometer and EEG signals but not the annotations signal
						// (no easy way to transfer annotations from EDF+ being read to the one being stored)
						tpdMeasurementData.Accelerometers[0] = CAST_10b_S2US(hEDFFile->DataRecord.Data[0][uintDRSampleCounter]);
						tpdMeasurementData.Accelerometers[1] = CAST_10b_S2US(hEDFFile->DataRecord.Data[1][uintDRSampleCounter]);
						tpdMeasurementData.Accelerometers[2] = CAST_10b_S2US(hEDFFile->DataRecord.Data[2][uintDRSampleCounter]);
						tpdMeasurementData.Measurements [(EEGCHANNELS) * i] = CAST_16b_S2US(hEDFFile->DataRecord.Data[3][uintDRSampleCounter]);
						tpdMeasurementData.Measurements [(EEGCHANNELS) * i + 1] = CAST_16b_S2US(hEDFFile->DataRecord.Data[4][uintDRSampleCounter]);
						tpdMeasurementData.Measurements [(EEGCHANNELS) * i + 2] = CAST_16b_S2US(hEDFFile->DataRecord.Data[5][uintDRSampleCounter]);
						tpdMeasurementData.Measurements [(EEGCHANNELS) * i + 3] = CAST_16b_S2US(hEDFFile->DataRecord.Data[6][uintDRSampleCounter]);
						tpdMeasurementData.Measurements [(EEGCHANNELS) * i + 4] = CAST_16b_S2US(hEDFFile->DataRecord.Data[7][uintDRSampleCounter]);
						tpdMeasurementData.Measurements [(EEGCHANNELS) * i + 5] = CAST_16b_S2US(hEDFFile->DataRecord.Data[8][uintDRSampleCounter]);

						uintDRSampleCounter++;
					}
					
					//
					// send data to be processed
					//
					Sleep(20);
					Sample_ProcessDataPacket(&tpdMeasurementData, &drCurrentDataRecord, pstd, hwndMainWnd);
				}
				
				//
				// state transition
				//
				smsState = SimulationModeState_CleanUp;
			break;

			case SimulationModeState_CleanUp:
				if(hEDFFile != NULL)
					libEDF_closeFile(hEDFFile);

				Sample_FreeDataRecord(&drCurrentDataRecord);

				blnStayInFSM = FALSE;
			break;

			default:
				applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_SimulationFSM(): Unknown state reached."), 0, TRUE);
				smsState = SimulationModeState_CleanUp;
		}
	}

	return;
}

static void Sample_WEEGSystemCheckFSM(SampleThreadData * pstd)
{
	BOOL						blnErrorOccured;
	BOOL						blnStayInFSM;
	DWORD						dwReturnCode;
	int							intNRetries;
	int							intTimeout;
	int							intSamplingFrequency;
	tReceivedData				trdReceivedData;
	unsigned char				uchrNWEEGDevicesFound, uchrWEEGCOMPort;
	unsigned int				uintNTestPackets;
	WEEGSystemCheckModeState	wscmsState;

	// variable initialization
	m_wsccCheckCode = WEEGSystem_OK;
	blnStayInFSM = TRUE;
	blnErrorOccured = FALSE;
	intSamplingFrequency = *(pstd->pSamplingFrequency);
	wscmsState = WEEGSystemCheckModeState_Stage1;

	while(blnStayInFSM)
	{
		switch(wscmsState)
		{
			// Configuration of measurement device for testing state
			case WEEGSystemCheckModeState_Stage1:
				// variable init
				intNRetries = 0;
								
				//
				// detect the WEEG coordinator's COM port
				//
				uchrNWEEGDevicesFound = serial_DetectWEEGPort(&uchrWEEGCOMPort, 1);
				if(uchrNWEEGDevicesFound == 0)
				{
					m_wsccCheckCode = WEEGSystem_COORD_DC;
					blnErrorOccured = TRUE;
				}
				else
				{
					m_cfgConfiguration.COMPortIndex = uchrWEEGCOMPort - 1;
					
					if(uchrNWEEGDevicesFound > 1)
					{
						m_wsccCheckCode = WEEGSystem_COORD_NCOM;
						blnErrorOccured = TRUE;
					}
				}
				
				//
				// open serial communication port
				//
				if(!blnErrorOccured)
				{
					dwReturnCode = serial_OpenPort (m_cfgConfiguration.COMPortIndex + 1);
					if(dwReturnCode != ERROR_SUCCESS)
					{
						m_wsccCheckCode = WEEGSystem_COORD_COM;
						blnErrorOccured = TRUE;
					}
				}

				//
				// configure WEEG system for measurement device test
				// NOTE: even if this check succeeds, it doesn't mean that the WEEG measurment unit is on and recording
				//
				if(!blnErrorOccured)
				{
					dwReturnCode = serial_StartSampling(WEEG_DEVICENR, WEEG_NETNR, 0x00000000, MCHANNELMASK, intSamplingFrequency);
					if(dwReturnCode != ERROR_SUCCESS)
					{
						m_wsccCheckCode = WEEGSystem_MEASDEV_CONFIG;
						blnErrorOccured = TRUE;
					}
				}

				//
				// state transition
				//
				if(blnErrorOccured)
					wscmsState = WEEGSystemCheckModeState_CleanUp;
				else
					wscmsState = WEEGSystemCheckModeState_Stage2;
			break;

			// Testing of measurement device state
			case WEEGSystemCheckModeState_Stage2:
				// variable initialization
				uintNTestPackets = 0;
				intTimeout = 0; intNRetries = 0;
				
				SecureZeroMemory(&trdReceivedData, sizeof(trdReceivedData));
				while((uintNTestPackets < mc_uintNTestPacketsRequired) && m_wsccCheckCode == WEEGSystem_OK)
				{
					// Without this system will choke as this thread runs on high priority
					Sleep(TRANSFER_IDLE);

					switch (serial_ReceivedDataStateMachine (&trdReceivedData))
					{
						case ERR_NOERROR:
							intTimeout = 0; intNRetries = 0;		
							
							// check if packet is valid
							if (trdReceivedData.PacketType == SER_DATA)
								uintNTestPackets++;
						break;
													    

						case ERR_NODATA:
							intTimeout++;		// No packet received?
							
							// check if timeout has occured
							if(intTimeout > TRANSFER_PACKWAIT/TRANSFER_IDLE)
							{
								// yes: set appropriate WEEGSystemCheckCode and quit testing
								m_wsccCheckCode = WEEGSystem_MEASDEV_TIMEOUT;
								blnErrorOccured = TRUE;
								break;
							}
						break;

						case ERR_CHECKSUM:
							intNRetries++;
							
							// check if max. number of checksum errors has been reached
							if (intNRetries >= MAX_RETRIES)
							{
								// yes: set appropriate WEEGSystemCheckCode and quit testing
								m_wsccCheckCode = WEEGSystem_MEASDEV_CHECKSUM;
								blnErrorOccured = TRUE;
								break;													   
							}
						break;
					}
				}

				//
				// state transition
				//
				wscmsState = WEEGSystemCheckModeState_CleanUp;
			break;

			case WEEGSystemCheckModeState_CleanUp:
				// stop the WEEG measurement device
				serial_StopSampling();

				// close serial prot
				serial_ClosePort();
				
				// exit the WEEG system check FSM
				blnStayInFSM = FALSE;
			break;
			
			default:
				applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_WEEGSystemCheckFSM(): Unknown state reached."), 0, TRUE);
				wscmsState = WEEGSystemCheckModeState_CleanUp;
		}
	}
}

static void Sample_RecordingFSM(SampleThreadData * pstd, HWND hwndMainWnd)
{
	BOOL						blnStayInFSM;
	BOOL						blnStateErrorOccured;
	DWORD						dwrdLastTimeStamp;
	DWORD						dwrdReturnCode;
	int							intNRetries;
	int							intSamplingFrequency;
	int							intTimeout;
	RecordingModeState			rmsState;
	SampleDataRecord			drCurrentDataRecord;
	SerialCommunicationResult	scrResult;
	TCHAR						strBuffer[256];
	tPacket_DATA *				ptpMeasurementData;
	tReceivedData				trdReceivedData;
	
	// variable initialization
	blnStayInFSM = TRUE;
	drCurrentDataRecord = mc_sdrEmpty;
	intSamplingFrequency = *(pstd->pSamplingFrequency);
	rmsState = RecordingModeState_Initialize;

	while(blnStayInFSM)
	{
		switch(rmsState)
		{
			case RecordingModeState_Initialize:
				blnStateErrorOccured = FALSE;

				// set event to signaled state
				SetEvent(pstd->hevSampleThread_Init);

				// initialize data record structure
				if(!Sample_InitDataRecord(&drCurrentDataRecord, intSamplingFrequency))
				{
					applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Initialize: Failed to initialize data record structure."), 0, TRUE);
					MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Sample_RecordingFSM() - RecordingModeState_Initialize: Failed to initialize data record structure."));
					PostMessage (hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);	// Force stop
					blnStateErrorOccured = TRUE;
				}
				
				// open serial communication port
				if(!blnStateErrorOccured)
				{
					dwrdReturnCode = serial_OpenPort (m_cfgConfiguration.COMPortIndex + 1);
					if(dwrdReturnCode != ERROR_SUCCESS)
					{
						applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Initialize: Could not open/configure WEEG COM port. (GetLastError #)"), dwrdReturnCode, TRUE);
						MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Sample_RecordingFSM() - RecordingModeState_Initialize: Could not open/configure WEEG COM port. (GetLastError #%d)"), dwrdReturnCode);
						PostMessage (hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);	// Force stop
						blnStateErrorOccured = TRUE;
					}
				}

				// check if configuration was successfull
				if(!blnStateErrorOccured)
				{
					dwrdReturnCode = serial_StartSampling(WEEG_DEVICENR, WEEG_NETNR, 0x00000000, MCHANNELMASK, intSamplingFrequency);
					if(dwrdReturnCode != ERROR_SUCCESS)
					{
						applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Initialize: Unable to configure WEEG device and start recording."), 0, TRUE);
						MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Sample_RecordingFSM() - RecordingModeState_Initialize: Unable to configure WEEG device and start recording."));
						PostMessage (hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Abort);
						blnStateErrorOccured = TRUE;
					}
				}

				//
				// state transition
				//
				if(blnStateErrorOccured)
					rmsState = RecordingModeState_CleanUp;
				else
					rmsState = RecordingModeState_Acquire;
			break;
			
			// Sampling state
			case RecordingModeState_Acquire:
				blnStateErrorOccured = FALSE;

				// init variables for data aquisition state
				SecureZeroMemory(&trdReceivedData, sizeof(trdReceivedData));
				dwrdLastTimeStamp = 0x0000;
				intTimeout = 0; intNRetries = 0;
					
				// initialize statistics variables
				m_lngNPacketsReceived = 0;
				m_lngNPacketChecksumErrors = 0;
				m_intNPacketsLost = 0;

				while(!(pstd->EndActivity))
				{
					// Without this system will choke as this thread runs on high priority
					Sleep (TRANSFER_IDLE);

					// check received packet buffer
					while ((scrResult = serial_ReceivedDataStateMachine (&trdReceivedData)) != ERR_NODATA &&
						   !blnStateErrorOccured)
					{
						//reset intTimeout variable
						intTimeout = 0;

						switch(scrResult)
						{
							case ERR_NOERROR:
								intTimeout = 0; intNRetries = 0;		// Received valid packet?

								if (trdReceivedData.PacketType == SER_DATA)
								{
									ptpMeasurementData = (tPacket_DATA *) trdReceivedData.PacketData;

									// check the packet's time stamp
									if(m_lngNPacketsReceived > 0)
									{
										if (ptpMeasurementData->TimeStamp <= dwrdLastTimeStamp)
											break;
										else if(ptpMeasurementData->TimeStamp > (dwrdLastTimeStamp + mc_intSampleLengths [EEGCHANNELS]))
										{
											_stprintf_s(strBuffer,
														sizeof(strBuffer)/sizeof(TCHAR),
														TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire - ERR_NOERROR: Packet Timestamp Error: was expecting %u, received %u."),
														dwrdLastTimeStamp + mc_intSampleLengths [EEGCHANNELS],
														ptpMeasurementData->TimeStamp);
											applog_logevent(SoftwareError, TEXT("SampleThread"), strBuffer, 0, TRUE);
											m_intNPacketsLost += (ptpMeasurementData->TimeStamp - dwrdLastTimeStamp)/mc_intSampleLengths [EEGCHANNELS];
										}
									}
									dwrdLastTimeStamp = ptpMeasurementData->TimeStamp;
								
									m_lngNPacketsReceived++;
								
									// Ok, handle data
									if (!Sample_ProcessDataPacket (ptpMeasurementData, &drCurrentDataRecord, pstd, hwndMainWnd))
									{
										applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire - ERR_NOERROR: Unable to process and store data record."), 0, TRUE);
										MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire - ERR_NOERROR: Unable to process and store data record."));
										blnStateErrorOccured = TRUE;
									}
								}
							break;

							case ERR_CHECKSUM:
								intNRetries++;
								m_lngNPacketChecksumErrors++;

								if (intNRetries >= MAX_RETRIES)
								{
									applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire - ERR_CHECKSUM: Measurement aborted due to high number of checksum errors."), 0, TRUE);
									MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire - ERR_CHECKSUM: Measurement aborted due to high number of checksum errors."));
									blnStateErrorOccured = TRUE;
								}
							break;

							default:
								applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire: serial_ReceivedDataStateMachine() returned unhandled SerialCommunicationResult."), 0, TRUE);
						}
					}
					
					// check if canceling of measurement is requested
					if(blnStateErrorOccured)
					{
						// post cancel message to main thread
						PostMessage (hwndMainWnd, WM_COMMAND, IDM_SAMPLE_STOP, (LPARAM) Stop_Cancel);
						break;
					}
					
					// check if communication has timed out
					if(++intTimeout > TRANSFER_PACKWAIT/TRANSFER_IDLE)
					{
						if (!Sample_DetectedCommunicationFailure(&drCurrentDataRecord, pstd, hwndMainWnd))
						{
							applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire: Unable to process and store communication blackout data record. (errno #)"), 0, TRUE);
							MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Sample_RecordingFSM() - RecordingModeState_Acquire: Unable to process and store communication blackout data record. (errno #%d)"), 0);
						}
					}
				}

				//
				// state transition
				//
				rmsState = RecordingModeState_CleanUp;
			break;

			case RecordingModeState_CleanUp:
				serial_StopSampling();

				serial_ClosePort();

				Sample_FreeDataRecord(&drCurrentDataRecord);

				blnStayInFSM = FALSE;
			break;

			default:
				m_stsCurrentSampleThreadState = SampleThreadState_Idle;
		}
	}
}