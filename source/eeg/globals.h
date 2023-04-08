/**
 * 
 * \file globals.h
 * \brief The global header file for program wide constants.
 *
 * 
 * $Id: globals.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __GLOBALS_H__
# define __GLOBALS_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//---------------------------------------------------------------------------
//   								Includes
//---------------------------------------------------------------------------
# include <windows.h>

# include "annotations.h"

//---------------------------------------------------------------------------
//   								Macros
//---------------------------------------------------------------------------
#define CAST_16b_S2US(a)            ((unsigned short)(((int) a) + 32768))	///< safely converts a signed 16 bit value to an unsigned 16 bit value
#define CAST_16b_US2S(a)            ((short)(((int) a) - 32768))			///< safely converts an unsigned 16 bit value to a signed 16 bit value
#define CAST_10b_S2US(a)            ((unsigned short)(((int) a) + 512))		///< safely converts a signed 10 bit value to an unsigned 10 bit value
#define CAST_10b_US2S(a)            ((short)(((int) a) - 512))				///< safely converts an unsigned 10 bit value to a signed 10 bit value

//---------------------------------------------------------------------------
//   								Libraries
//---------------------------------------------------------------------------
// Custom
#pragma comment(lib, "verttextlib.lib")

// Windows
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "credui.lib")
#pragma comment(lib, "setupapi.lib")

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define SOFTWARE_MANUFACTURER	TEXT("Tampere University of Technology")
# define SOFTWARE_NAME			TEXT("EEGEM")
# define SOFTWARE_TITLE			TEXT("EEGEM Measurement Software")
# define SOFTWARE_LOCK			TEXT("EEGEM.tmp")
# define SOFTWARE_VERSION		TEXT("4.0.0")

# define WINDOW_CLASSID_MAIN	TEXT("Zigbeeclass")
# define WINDOW_CLASSID_ANNOT	TEXT("AnnotationsDisplayClass")

# define NSERPORTS				256					// List serial ports COM1 ... COM<NSERPORTS>
# define NSENSFACTORS			14					// # of sensitivity factors available (see main.h, mc_intSensitivityFactors[])
# define NTBFACTORS				8					// # of timebase factors available (see main.h, mc_fltTimebaseFactors[])

# define NLPFILTERS				11					// # of LP filters in filter.h

# define MAX_PATH_DEST_FOLDER	MAX_PATH - 25		// MAX_PATH - 14 - 11
													// Reasons:
													//  - GetTempFileName() requires a path that is MAX_PATH - 14
													//  - a folder with format '\2008.12.01' (11 chars) is created in the recording folder

// Serial communication constants
# define PACKET_BLOCKSIZE		512					// Block size used in receive state machine
# define PACKET_MAXLEN			(4096 + 128)		// Maximum packet size; longer packets are discarded
# define MAXNMEASUREMENTS		4000				// Maximum number of 16b measurements per second

// Hardware Constants
# define NGYROSAMPLES			1024				// # of gyro levels (2^(gyro word size))

# define MIN_SAMPLERATE			200					// Minimum sample rate (Hz)
//# define MAX_SAMPLERATE			1000				// Maximum sample rate (Hz)
# define MAX_SAMPLERATE			666					// NOTE: the actual max. sampling rate of the system is 1000 Hz but the
													// code that interfaces with the WEEG device is not set up to accept this yet.

# define EEGCHANNELS			6					// Maximum number of EEG channels
# define ACCCHANNELS			3					// three 10b accelrometer measurements per packet

# define WEEG_NETNR				0x0000
# define WEEG_DEVICENR			0x01

# define ADC_RESOLUTION			(3.0/65536)			// resolution of the data samples (V/sample)
# define SIGNAL_GAIN			1900				// amplification of the EEG signal
# define WEEG_LSB_UV			(ADC_RESOLUTION/SIGNAL_GAIN)*1000000

# define NCHANNELSINMASK		8
# define MCHANNELMASK			0x3F				// measurement channels to be sent from the device (1 byte)
													// b0 : Channel 0
													// b1 : Channel 1
													// b2 : Channel 2
													// b3 : Channel 3
													// b4 : Channel 4
													// b5 : Channel 5
													// b6 : Channel 6
													// b7 : Channel 7

# define RCHANNELMASK			0x00618000			// radio channels to be used: allow channels 15, 16, 21 and 22 (safest channel mask when WiFi environment is unknown)

// GUI constants
# define DEFAULTTEXTHEIGHT		14

// Statusbar constants
# define SB_NPARTS				4
# define SB_STATUS_PART			0
# define SB_ANNOTATIONS_PART	1
# define SB_BATTERYICON_PART	2
# define SB_RECTIME_PART		3
# define SB_STATUS_PROP			0.29
# define SB_ANNOTATIONS_PROP	0.57
# define SB_BATTERYICON_PROP	0.05
# define SB_STATUS_WIDTH		3.0							// width of the status part (in logical in)
# define SB_BATTERYICON_WIDTH	52							// width of the battery icon part (in pixels) + 8
# define SB_RECTIME_WIDTH		0.75						// width of the battery status part (in logical in)

// aEEG constants
# define AEEG_TIME_INTERVAL		200

// OS Constants
# define TMPFILE_PRFX_LEN		3
# define MAX_PATH_UNICODE		31000
# define LONGFILENAME_PRFX		TEXT("\\\\?\\")
# define LONGFILENAME_PRFX_LEN	4

// Credential informaiton
# define CREDS_SSHSERVER		TEXT("EEGEMSSHServer")
# define CREDS_STREAMINGSERVER	TEXT("EEGEMStreamingServer")

//
// SSH-related constants
//
# define MAX_HOSTNAME_LEN		255					// maximum hostname length (RFC 1123)

//
// EEGEM-defined messages
//


//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
/**
 * EEGEM-specific message-identifier values (have to be in the  0x0400 - 0x7FFF range)
 */
typedef enum
{
	EEGEMMsg_ExitPermission_Set			= 0x0400,	///< set an exit permission (wParam specifies the member of the ExitPermission enum to be set)
	EEGEMMsg_ExitPermission_Clear		= 0x0401,	///< clear an exit permission (wParam specifies the member of the ExitPermission enum to be set)
	EEGEMMsg_StatusBar_SetStatus		= 0x0402,	///< display a string in the status part of the main window's status bar (lParam is a pointer to the NULL-terminated string to be displayed)
	EEGEMMsg_StatusBar_SetAnnotation	= 0x0403,	///< display a string in the annotations part main window's status bar (wParam is TRUE if lParam is a TCHAR string, FALSE otherwise; lParam is a pointer to the NULL-terminated string to be displayed)
	EEGEMMsg_Streaming_DisplayStatus	= 0x0404	///< update streaming thread status displayed on the second line of the main window's rebar (wParam specifies the member of the StreamingClientState enum representing the current state of the streaming thread)
} EEGEMMsg;

/**
 * Status codes that are used to signal whether or not the application can be closed
 */
typedef enum
{
	ExitPermission_Allowed = 0,				///< closing the application is allowed
	ExitPermission_Denied_SSHUpload = 1,	///< closing the application not allowed; EDF+ file is being uploaded
	ExitPermission_Denied_Recording = 2,	///< closing the application not allowed; recording in progress
	ExitPermission_Denied_Streaming = 4		///< closing the application not allowed; recording in progress
} ExitPermission;

/**
 * 
 */
typedef struct
{
	// General parameters
	BYTE	DisplayChannelMask;												///< Mask for selected channels
	int		COMPortIndex;
	int		SamplingFrequency;												// WEEG device sample rate (Hz) has to be in the range
																			// 200…1000 or zero. Please note that the maximum throughput is 4000
																			// measurements per second, meaning that at most 4 channels can be used at the
																			// maximum sample rate. Similarly, the maximum allowed sampling speed is
																			// 500Hz when all the eight measurement channels are in use.
	TCHAR	DestinationFolder[MAX_PATH_DEST_FOLDER + 1];					// Path where EEGEM measurements should be saved
	BOOL	DialConnectionScript;
	TCHAR	ConnectionScriptPath[MAX_PATH_UNICODE + 1];

	// Screen size parameters
	int		ScreenHeight;
	int		ScreenWidth;
	int		VerticalDPC;
	int		HorizontalDPC;

	// SSH configuration parameters
	TCHAR	SSH_HostName[MAX_HOSTNAME_LEN + 1];
	int		SSH_Port;
	TCHAR	SSH_RemotePath[MAX_PATH + 1];									///< used MAX_PATH because assuming that connecting to an old machine
	BOOL	SSH_AutomaticUpload;
	int		SSH_ConnectionTimeout;											///< maximum time, in seconds, that connection to the SSH server is allowed to take
	int		SSH_LowSpeedLimit;												///< transfer speed, in bytes per second, that the transfer should be below SSH_LowSpeedTime seconds for the SSH transfer to be consider as too slow and aborted
	int		SSH_LowSpeedTime;												///< time, in seconds, that the transfer should be below the SSH_LowSpeedLimit for the SSH transfer to be considered as too slow and aborted

	// Streaming configuration parameters
	BOOL	Streaming_Enabled;
	BYTE	Streaming_Server_IPv4_Field0;
	BYTE	Streaming_Server_IPv4_Field1;
	BYTE	Streaming_Server_IPv4_Field2;
	BYTE	Streaming_Server_IPv4_Field3;
	int		Streaming_Server_Port;
	int		Streaming_MaxNSendMsgFailures;
	int		Streaming_MaxNWait4ReplyFailures;
	
	// Annotations
	TCHAR	Annotations[ANNOTATION_MAX_TYPES][ANNOTATION_MAX_CHARS + 1];	///<
	
	// GUI parameters
	int		LPFilterIndex;	
	int		ScaleIndex;
	int		TimeBaseIndex;

	// Members that can only be changed directly from configuration file
	BOOL	SimulationMode;													///< Software used in Simulation mode when this member is TRUE 
    TCHAR	ElectrodeType[80 + 1];											///< type of transducer used to record the EEG (max length defined in the EDF standard)
	int		ChannelDCOffset[EEGCHANNELS];

	// Members used solely for configuration module
	TCHAR	ApplicationPath[MAX_PATH_UNICODE + 1];
	TCHAR	ApplicationDataPath[MAX_PATH_UNICODE + 1];
}CONFIGURATION;

/**
 * Contains handles of GUI elements in main window
 */
typedef struct {// fonts
				HFONT	hfntStatusBar;

				// image files
				HANDLE	hStreamingStatusImages[5];	///< handles to images that are used to indicate status of streaming thread in toolbar
				HICON	hIconsBatteryStates[3];
				
				// menus
				HMENU	hmnuAnnotations;

				// status bar and toobars
				HWND	hwndRebar;
				HWND	hwndStatusBar;
				HWND	hwndToolbar;

				// windows
				HWND	hwndCMBSensitivity;
				HWND	hwndCMBLPFilters;
				HWND	hwndCMBTimebase;
				HWND	hwndSTAStreamingStatus;
} GUIElements;

# endif