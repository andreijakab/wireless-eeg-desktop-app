/**
 * 
 * \file config.cpp
 * \brief Module handling the configuration.
 *
 * 
 * $Id: config.cpp 76 2013-02-14 14:26:17Z jakab $ 
 */
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER			0x0502									// application requires at least Windows XP SP2
#define _WIN32_WINNT	0x0502									// application requires at least Windows XP SP2
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//---------------------------------------------------------------------------
//   								Includes
//---------------------------------------------------------------------------
# include <tchar.h>
# include <windows.h>
# include <shlobj.h>
# include <shlwapi.h>

# include <libmath.h>

# include "annotations.h"
# include "globals.h"
# include "edfPlus.h"
# include "iniFile.h"
# include "util.h"
# include "config.h"

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define CONFIG_FILE								TEXT("\\config.ini")
# define SECTION_CONFIG								TEXT("Configuration")
# define KEY_SIMULATIONMODE							TEXT("UseSimulationMode")
# define KEY_SCREENWIDTH							TEXT("ScreenWidth")
# define KEY_SCREENHEIGHT							TEXT("ScreenHeight")
# define KEY_HORIZONTALDPC							TEXT("HorizontalDPC")
# define KEY_VERTICALDPC							TEXT("VerticalDPC")
# define KEY_SERPORT								TEXT("PortNumber")					// Serial port number
# define KEY_DISPLAYCHMASK							TEXT("DisplayChannelMask")			// Displayed channel mask value
# define KEY_SAMPLINGFREQUENCY						TEXT("SamplingFrequency")
# define KEY_ELECTRODETYPE							TEXT("ElectrodeType")
# define KEY_SCALE									TEXT("ScaleIndex")
# define KEY_TIMEBASE								TEXT("TimeBaseIndex")
# define KEY_FILEPATH								TEXT("FilePath")					// Store file name
# define KEY_LPFILTER								TEXT("LPFilterIndex")					
# define KEY_CONNSCRIPT								TEXT("ConnectionScript")
# define KEY_DIALCONNSCRIPT							TEXT("DialConnectionScript")
# define DEFAULT_SIMULATIONMODE						0
# define DEFAULT_SERPORT							4									// Default serial port
# define DEFAULT_DISPLAYCHMASK						0x3F								// Default channel mask value
# define DEFAULT_SAMPLINGFREQUENCY					500									// Default sampling frequency (Hz)
# define DEFAULT_ELECTRODETYPE						TEXT("Zipprep AgAgCl electrode")
# define DEFAULT_SCALE								5									// Default scale that is used to display EEG signals
# define DEFAULT_TIMEBASE							6									// Default time base that is used to display EEG signals
# define DEFAULT_LPFILTER							0
# define DEFAULT_DIALCONNSCRIPT						0

# define SECTION_CHANNELDCOFFSET					TEXT("Channel DC Offset")
# define DEFAULT_CHANNELDCOFFSET					0

# define SECTION_ANNOTATION							TEXT("Annotations")

# define SECTION_SSHCONFIG							TEXT("SSH Configuration")
# define KEY_SSH_AUTOUPLOAD							TEXT("AutomaticUpload")
# define KEY_SSH_HOSTNAME							TEXT("HostName")
# define KEY_SSH_PORT								TEXT("Port")
# define KEY_SSH_REMOTEPATH							TEXT("RemotePath")
# define KEY_SSH_CONNTIMEOUT						TEXT("Connection Timeout")
# define KEY_SSH_LOWSPEEDLIMIT						TEXT("Low Speed Limit")
# define KEY_SSH_LOWSPEEDTIME						TEXT("Low Speed Time")
# define DEFAULT_SSH_AUTOUPLOAD						0
# define DEFAULT_SSH_PORT							22
# define DEFAULT_SSH_CONNTIMEOUT					300									///< default maximum time, in seconds, that connection to the SSH server is allowed to take
# define DEFAULT_SSH_LOWSPEEDLIMIT					2000								///< default transfer speed, in bytes per second, that the transfer should be below during LowSpeedTime seconds for the SSH transfer to be consider as too slow and aborted
# define DEFAULT_SSH_LOWSPEEDTIME					10									///< default time, in seconds, that the transfer should be below the LowSpeedLimit for the SSH transfer to be considered as too slow and aborted

# define SECTION_STREAMING							TEXT("Streaming")
# define KEY_STREAMING_ENABLED						TEXT("Enabled")
# define KEY_STREAMING_SERVERIPV4_FIELD0			TEXT("ServerIPv4_Field0")
# define KEY_STREAMING_SERVERIPV4_FIELD1			TEXT("ServerIPv4_Field1")
# define KEY_STREAMING_SERVERIPV4_FIELD2			TEXT("ServerIPv4_Field2")
# define KEY_STREAMING_SERVERIPV4_FIELD3			TEXT("ServerIPv4_Field3")
# define KEY_STREAMING_SERVERPORT					TEXT("Port")
# define KEY_STREAMING_MAXNSENDMSGFAILURES			TEXT("MaxNSendMsgFailures")
# define KEY_STREAMING_MAXNWAIT4REPLYFAILURES		TEXT("MaxNWait4ReplyFailures")
# define DEFAULT_STREAMING_ENABLED					0
# define DEFAULT_STREAMING_SERVERIPV4_FIELD0		0
# define DEFAULT_STREAMING_SERVERIPV4_FIELD1		0
# define DEFAULT_STREAMING_SERVERIPV4_FIELD2		0
# define DEFAULT_STREAMING_SERVERIPV4_FIELD3		0
# define DEFAULT_STREAMING_SERVERPORT				0
# define DEFAULT_STREAMING_MAXNSENDMSGFAILURES		3
# define DEFAULT_STREAMING_MAXNWAIT4REPLYFAILURES	3

//---------------------------------------------------------------------------
//   								Global variables
//---------------------------------------------------------------------------
BOOL	m_blnHaveUserAppDataPath, m_blnCanUseConfigFile;
int		m_intDefaultHorizontalDPC, m_intMaxHorizontalDPC;
int		m_intDefaultVerticalDPC, m_intMaxVerticalDPC;
int		m_intDefaultScreenWidth;
int		m_intDefaultScreenHeight;
TCHAR	m_strConfigFilePath[MAX_PATH_UNICODE + 1];
TCHAR	m_strAppStartUpPath[MAX_PATH_UNICODE + 1];
TCHAR	m_strAppDataFolder[MAX_PATH_UNICODE + 1];
TCHAR	m_strUserAppDataPath[MAX_PATH + 1];
TCHAR	m_strUserMyDocsPath[MAX_PATH + 1];

//---------------------------------------------------------------------------
//							Globally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief Initializes configuration and the iniFile modules.
 *
 * The function first parses the path of the configuration file by using the program's start-up path. It then
 * uses the path in order to initialize the iniFile module.
 * This function needs to be called before the LoadSetup() and StoreSetup() functions are used.
 *
 * \return Nothing.
 */
void config_init(int intScreenXResolution, int intScreenYResolution, int intScreenWidth, int intScreenHeight)
{
	TCHAR * chrContext = NULL, strSeparators[] = TEXT("\""), *strTemp;

	int i;

	//
	// get start-up path
	//
	strTemp = _tcstok_s(GetCommandLine(), strSeparators, &chrContext);

	// Remove file name
	for(i= (int)_tcslen(strTemp)-1; i>=0; i--)
	{
		if(strTemp[i] == TEXT('\\'))
			break;
	}

	// add long filename prefix
	_stprintf_s(m_strAppStartUpPath, sizeof(m_strAppStartUpPath)/sizeof(TCHAR), LONGFILENAME_PRFX);
	
	// append application start-up path
	_tcsncpy_s(m_strAppStartUpPath + LONGFILENAME_PRFX_LEN, sizeof(m_strAppStartUpPath)/sizeof(TCHAR) - LONGFILENAME_PRFX_LEN, strTemp, i+1);
	
	//
	// get and configure software's application data directory
	//
	// get application data directory (of current user)
	m_strUserAppDataPath[0] = '\0';
	m_blnHaveUserAppDataPath = m_blnCanUseConfigFile = FALSE;
	if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, m_strUserAppDataPath)))
	{
		m_blnHaveUserAppDataPath = TRUE;

		// check if appropriate folder structure is already in place
		_stprintf_s(m_strAppDataFolder,
					sizeof(m_strConfigFilePath)/sizeof(TCHAR),
					TEXT("%s%s\\%s\\%s"),
					LONGFILENAME_PRFX, m_strUserAppDataPath, SOFTWARE_MANUFACTURER, SOFTWARE_NAME);
		if(!PathFileExists(m_strAppDataFolder))
		{
			// if no: attempt to create appropriate folder structure
			if(config_CreateAppdataDirStructure())
			{
				m_blnCanUseConfigFile = TRUE;
			}
		}
		else
			m_blnCanUseConfigFile = TRUE;
		
		if(m_blnCanUseConfigFile)
		{
			_stprintf_s(m_strConfigFilePath,
						sizeof(m_strConfigFilePath)/sizeof(TCHAR),
						TEXT("%s%s"),
						m_strAppDataFolder, CONFIG_FILE);

			iniFile_init(m_strConfigFilePath);
		}
	}

	// get documents directory (of current user)
	SHGetFolderPath(NULL, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, 0, m_strUserMyDocsPath);

	//
	// calculate default dotes-per-centimeter values (i.e. amount of pixels for 1 cm of vertical or horizontal screen real estate)
	//
	m_intDefaultScreenWidth = intScreenWidth;
	m_intDefaultScreenHeight = intScreenHeight;
	m_intDefaultHorizontalDPC = (int) (((double) intScreenXResolution) / ((double) intScreenWidth) * 10.0f);
	m_intDefaultVerticalDPC = (int) (((double) intScreenYResolution) / ((double) intScreenHeight) * 10.0f);
	m_intMaxHorizontalDPC = (int) (round((double) intScreenXResolution) / ((double) (intScreenWidth - 150)/10.0f));
	m_intMaxVerticalDPC = (int) (round((double) intScreenYResolution) / ((double) (intScreenHeight - 150)/10.0f));
}

/**
 * \brief Loads configuration data from the configuration file.
 *
 * Reads the program parameters from the configuration file, stores them in a 'CONFIGURATION' struct, which
 * is then returned to the calling function.
 *
 * \return The configuration data as a 'CONFIGURATION' struct.
 */
void config_load(CONFIGURATION * pcfgConfiguration)
{
	TCHAR strAnnotationBuffer[ANNOTATION_MAX_CHARS + 1], strKeyName[2];
	DWORD d;
	int i;
	
	//
	// get general configuration data
	//
	iniFile_GetValueI(SECTION_CONFIG, KEY_SIMULATIONMODE, DEFAULT_SIMULATIONMODE, &pcfgConfiguration->SimulationMode);

	iniFile_GetValueI(SECTION_CONFIG, KEY_SERPORT, DEFAULT_SERPORT, &pcfgConfiguration->COMPortIndex);
	if(pcfgConfiguration->COMPortIndex < 0 || pcfgConfiguration->COMPortIndex > (NSERPORTS - 1))
		pcfgConfiguration->COMPortIndex = DEFAULT_SERPORT;

	iniFile_GetValueI(SECTION_CONFIG, KEY_SCREENWIDTH, m_intDefaultScreenWidth, &pcfgConfiguration->ScreenWidth);
	if(pcfgConfiguration->ScreenWidth < 0 || pcfgConfiguration->ScreenWidth > m_intDefaultScreenWidth)
		pcfgConfiguration->ScreenWidth = m_intDefaultScreenWidth;

	iniFile_GetValueI(SECTION_CONFIG, KEY_SCREENHEIGHT, m_intDefaultScreenHeight, &pcfgConfiguration->ScreenHeight);
	if(pcfgConfiguration->ScreenHeight < 0 || pcfgConfiguration->ScreenHeight > m_intDefaultScreenHeight)
		pcfgConfiguration->ScreenHeight = m_intDefaultScreenHeight;
	
	iniFile_GetValueI(SECTION_CONFIG, KEY_HORIZONTALDPC, m_intDefaultHorizontalDPC, &pcfgConfiguration->HorizontalDPC);
	if(pcfgConfiguration->HorizontalDPC < m_intDefaultHorizontalDPC || pcfgConfiguration->HorizontalDPC > m_intMaxHorizontalDPC)
		pcfgConfiguration->HorizontalDPC = m_intDefaultHorizontalDPC;

	iniFile_GetValueI(SECTION_CONFIG, KEY_VERTICALDPC, m_intDefaultVerticalDPC, &pcfgConfiguration->VerticalDPC);
	if(pcfgConfiguration->VerticalDPC < m_intDefaultVerticalDPC || pcfgConfiguration->VerticalDPC > m_intMaxVerticalDPC)
		pcfgConfiguration->VerticalDPC = m_intDefaultVerticalDPC;

	iniFile_GetValueH(SECTION_CONFIG, KEY_DISPLAYCHMASK, DEFAULT_DISPLAYCHMASK, &i);
	pcfgConfiguration->DisplayChannelMask = (BYTE) i;

	iniFile_GetValueS(SECTION_CONFIG, KEY_FILEPATH, m_strUserMyDocsPath, pcfgConfiguration->DestinationFolder, sizeof(pcfgConfiguration->DestinationFolder)/sizeof(TCHAR));

	// connection script
	iniFile_GetValueI(SECTION_SSHCONFIG, KEY_DIALCONNSCRIPT, DEFAULT_DIALCONNSCRIPT, &pcfgConfiguration->DialConnectionScript);

	iniFile_GetValueS(SECTION_SSHCONFIG, KEY_CONNSCRIPT, NULL, pcfgConfiguration->ConnectionScriptPath, sizeof(pcfgConfiguration->ConnectionScriptPath)/sizeof(TCHAR));
	

	// check if path leads to a folder; if yes, remove trailing backslash (if present); if no, use user's My Docs folder
	d = GetFileAttributes(pcfgConfiguration->DestinationFolder);
	if((d != INVALID_FILE_ATTRIBUTES ) && (d & FILE_ATTRIBUTE_DIRECTORY))
		PathRemoveBackslash(pcfgConfiguration->DestinationFolder);
	else
		_tcscpy_s(pcfgConfiguration->DestinationFolder, sizeof(pcfgConfiguration->DestinationFolder)/sizeof(TCHAR), m_strUserMyDocsPath);

	iniFile_GetValueS(SECTION_CONFIG, KEY_ELECTRODETYPE, DEFAULT_ELECTRODETYPE, pcfgConfiguration->ElectrodeType, sizeof(pcfgConfiguration->ElectrodeType)/sizeof(TCHAR));

	iniFile_GetValueI(SECTION_CONFIG, KEY_SCALE, DEFAULT_SCALE, &pcfgConfiguration->ScaleIndex);
	if(pcfgConfiguration->ScaleIndex < 0 || pcfgConfiguration->ScaleIndex > (NSENSFACTORS - 1))
		pcfgConfiguration->ScaleIndex = DEFAULT_SCALE;

	iniFile_GetValueI(SECTION_CONFIG, KEY_TIMEBASE, DEFAULT_TIMEBASE, &pcfgConfiguration->TimeBaseIndex);
	if(pcfgConfiguration->TimeBaseIndex < 0 || pcfgConfiguration->TimeBaseIndex > (NTBFACTORS - 1))
		pcfgConfiguration->TimeBaseIndex = DEFAULT_TIMEBASE;

	iniFile_GetValueI(SECTION_CONFIG, KEY_LPFILTER, DEFAULT_LPFILTER, &pcfgConfiguration->LPFilterIndex);
	if(pcfgConfiguration->LPFilterIndex < 0 || pcfgConfiguration->LPFilterIndex > (NLPFILTERS - 1))
		pcfgConfiguration->LPFilterIndex = DEFAULT_LPFILTER;

	iniFile_GetValueI(SECTION_CONFIG, KEY_SAMPLINGFREQUENCY, DEFAULT_SAMPLINGFREQUENCY, &pcfgConfiguration->SamplingFrequency);
	if(pcfgConfiguration->SamplingFrequency < MIN_SAMPLERATE || pcfgConfiguration->SamplingFrequency > MAX_SAMPLERATE)
		pcfgConfiguration->SamplingFrequency = DEFAULT_SAMPLINGFREQUENCY;

	//
	// get DC offsets
	//
	for(i=0; i<EEGCHANNELS; i++)
	{
		_stprintf_s(strKeyName, sizeof(strKeyName)/sizeof(TCHAR), TEXT("%d"), i);
		iniFile_GetValueI(SECTION_CHANNELDCOFFSET, strKeyName, DEFAULT_CHANNELDCOFFSET, &pcfgConfiguration->ChannelDCOffset[i]);
	}

	//
	// get annotations
	//
	for(i=0; i<ANNOTATION_MAX_TYPES; i++)
	{
		_stprintf_s(strKeyName, sizeof(strKeyName)/sizeof(TCHAR), TEXT("%d"), i);
		iniFile_GetValueS(SECTION_ANNOTATION, strKeyName, NULL, strAnnotationBuffer, sizeof(strAnnotationBuffer)/sizeof(TCHAR));
		_tcscpy_s(pcfgConfiguration->Annotations[i], sizeof(pcfgConfiguration->Annotations[i])/sizeof(TCHAR), strAnnotationBuffer);
		edf_CorrectSpecialCharacters(pcfgConfiguration->Annotations[i], TRUE);
	}
	
	//
	// get SSH configuration
	//
	iniFile_GetValueS(SECTION_SSHCONFIG, KEY_SSH_HOSTNAME, NULL, pcfgConfiguration->SSH_HostName, sizeof(pcfgConfiguration->SSH_HostName)/sizeof(TCHAR));
	
	iniFile_GetValueI(SECTION_SSHCONFIG, KEY_SSH_PORT, DEFAULT_SSH_PORT, &pcfgConfiguration->SSH_Port);
	if(pcfgConfiguration->SSH_Port < 0 || pcfgConfiguration->SSH_Port > 65535)
		pcfgConfiguration->SSH_Port = DEFAULT_SSH_PORT;
	
	iniFile_GetValueS(SECTION_SSHCONFIG, KEY_SSH_REMOTEPATH, NULL, pcfgConfiguration->SSH_RemotePath, sizeof(pcfgConfiguration->SSH_RemotePath)/sizeof(TCHAR));

	iniFile_GetValueI(SECTION_SSHCONFIG, KEY_SSH_AUTOUPLOAD, DEFAULT_SSH_AUTOUPLOAD, &pcfgConfiguration->SSH_AutomaticUpload);

	iniFile_GetValueI(SECTION_SSHCONFIG, KEY_SSH_CONNTIMEOUT, DEFAULT_SSH_CONNTIMEOUT, &pcfgConfiguration->SSH_ConnectionTimeout);
	
	iniFile_GetValueI(SECTION_SSHCONFIG, KEY_SSH_LOWSPEEDLIMIT, DEFAULT_SSH_LOWSPEEDLIMIT, &pcfgConfiguration->SSH_LowSpeedLimit);
	
	iniFile_GetValueI(SECTION_SSHCONFIG, KEY_SSH_LOWSPEEDTIME, DEFAULT_SSH_LOWSPEEDTIME, &pcfgConfiguration->SSH_LowSpeedTime);
	
	//
	// get streaming configuration
	//
	// streaming enabled/disabled
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_ENABLED, DEFAULT_STREAMING_ENABLED, &pcfgConfiguration->Streaming_Enabled);

	// server's IPv4 address
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD0, DEFAULT_STREAMING_SERVERIPV4_FIELD0, &i);
	if(i >= 0 && i <= 255)
		pcfgConfiguration->Streaming_Server_IPv4_Field0 = (BYTE) i;
	else
		pcfgConfiguration->Streaming_Server_IPv4_Field0 = (BYTE) DEFAULT_STREAMING_SERVERIPV4_FIELD0;
	
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD1, DEFAULT_STREAMING_SERVERIPV4_FIELD1, &i);
	if(i >= 0 && i <= 255)
		pcfgConfiguration->Streaming_Server_IPv4_Field1 = (BYTE) i;
	else
		pcfgConfiguration->Streaming_Server_IPv4_Field1 = (BYTE) DEFAULT_STREAMING_SERVERIPV4_FIELD1;

	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD2, DEFAULT_STREAMING_SERVERIPV4_FIELD2, &i);
	if(i >= 0 && i <= 255)
		pcfgConfiguration->Streaming_Server_IPv4_Field2 = (BYTE) i;
	else
		pcfgConfiguration->Streaming_Server_IPv4_Field2 = (BYTE) DEFAULT_STREAMING_SERVERIPV4_FIELD2;
	
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD3, DEFAULT_STREAMING_SERVERIPV4_FIELD3, &i);
	if(i >= 0 && i <= 255)
		pcfgConfiguration->Streaming_Server_IPv4_Field3 = (BYTE) i;
	else
		pcfgConfiguration->Streaming_Server_IPv4_Field3 = (BYTE) DEFAULT_STREAMING_SERVERIPV4_FIELD3;
	
	// server's port
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERPORT, DEFAULT_STREAMING_SERVERPORT, &pcfgConfiguration->Streaming_Server_Port);
	if(pcfgConfiguration->Streaming_Server_Port < 0 || pcfgConfiguration->Streaming_Server_Port > 65535)
		pcfgConfiguration->Streaming_Server_Port = DEFAULT_STREAMING_SERVERPORT;
	
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_MAXNSENDMSGFAILURES, DEFAULT_STREAMING_MAXNSENDMSGFAILURES, &pcfgConfiguration->Streaming_MaxNSendMsgFailures);
	if(pcfgConfiguration->Streaming_MaxNSendMsgFailures < 0)
		pcfgConfiguration->Streaming_MaxNSendMsgFailures = DEFAULT_STREAMING_MAXNSENDMSGFAILURES;
	
	iniFile_GetValueI(SECTION_STREAMING, KEY_STREAMING_MAXNWAIT4REPLYFAILURES, DEFAULT_STREAMING_MAXNWAIT4REPLYFAILURES, &pcfgConfiguration->Streaming_MaxNWait4ReplyFailures);
	if(pcfgConfiguration->Streaming_MaxNWait4ReplyFailures < 0)
		pcfgConfiguration->Streaming_MaxNWait4ReplyFailures = DEFAULT_STREAMING_MAXNWAIT4REPLYFAILURES;

	//
	// get misc. configuration
	//
	_tcsncpy_s(pcfgConfiguration->ApplicationPath, sizeof(pcfgConfiguration->ApplicationPath)/sizeof(TCHAR), m_strAppStartUpPath, sizeof(pcfgConfiguration->ApplicationPath)/sizeof(TCHAR));
	
	if(m_blnCanUseConfigFile)
		_tcsncpy_s(pcfgConfiguration->ApplicationDataPath,
				   sizeof(pcfgConfiguration->ApplicationDataPath)/sizeof(TCHAR),
				   m_strAppDataFolder,
				   sizeof(pcfgConfiguration->ApplicationDataPath)/sizeof(TCHAR));
}

/**
 * \brief Store parameters in configuration file.
 *
 * The progam's parameters, which are passed to the function as a 'CONFIGURATION' struct, are
 * saved in the configuration file.
 *
 * \param[in] cfgConfiguration structure containing the configuration data to be saved
 * \return Nothing.
 */
void config_store(CONFIGURATION cfgConfiguration)
{
	TCHAR strKeyName[2];
	int i;
	
	if(m_blnCanUseConfigFile)
	{
		// Store configuration data
		iniFile_SetValueI(SECTION_CONFIG, KEY_SIMULATIONMODE, cfgConfiguration.SimulationMode, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_SCREENWIDTH, cfgConfiguration.ScreenWidth, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_SCREENHEIGHT, cfgConfiguration.ScreenHeight, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_HORIZONTALDPC, cfgConfiguration.HorizontalDPC, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_VERTICALDPC, cfgConfiguration.VerticalDPC, TRUE);
		i = cfgConfiguration.DisplayChannelMask;
		iniFile_SetValueH(SECTION_CONFIG, KEY_DISPLAYCHMASK, i, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_SERPORT, cfgConfiguration.COMPortIndex, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_LPFILTER, cfgConfiguration.LPFilterIndex, TRUE);
		iniFile_SetValue(SECTION_CONFIG, KEY_FILEPATH, cfgConfiguration.DestinationFolder, TRUE);
		iniFile_SetValue(SECTION_CONFIG, KEY_ELECTRODETYPE, cfgConfiguration.ElectrodeType, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_SCALE, cfgConfiguration.ScaleIndex, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_TIMEBASE, cfgConfiguration.TimeBaseIndex, TRUE);
		iniFile_SetValueI(SECTION_CONFIG, KEY_SAMPLINGFREQUENCY, cfgConfiguration.SamplingFrequency, TRUE);
		iniFile_SetValueI(SECTION_SSHCONFIG, KEY_DIALCONNSCRIPT, cfgConfiguration.DialConnectionScript, TRUE);
		iniFile_SetValue(SECTION_SSHCONFIG, KEY_CONNSCRIPT, cfgConfiguration.ConnectionScriptPath, TRUE);

		// store DC offsets
		for(i=0; i<EEGCHANNELS; i++)
		{
			_stprintf_s(strKeyName, sizeof(strKeyName)/sizeof(TCHAR), TEXT("%d"), i);
			iniFile_SetValueI(SECTION_CHANNELDCOFFSET, strKeyName, cfgConfiguration.ChannelDCOffset[i], TRUE);
		}

		// store annotations
		for(i=0; i<ANNOTATION_MAX_TYPES; i++)
		{
			_stprintf_s(strKeyName, sizeof(strKeyName)/sizeof(TCHAR), TEXT("%d"), i);
			iniFile_SetValue(SECTION_ANNOTATION, strKeyName, cfgConfiguration.Annotations[i], TRUE);
		}

		// store SSH configuration
		iniFile_SetValue(SECTION_SSHCONFIG, KEY_SSH_HOSTNAME, cfgConfiguration.SSH_HostName, TRUE);
		iniFile_SetValueI(SECTION_SSHCONFIG, KEY_SSH_PORT, cfgConfiguration.SSH_Port, TRUE);
		iniFile_SetValue(SECTION_SSHCONFIG, KEY_SSH_REMOTEPATH, cfgConfiguration.SSH_RemotePath, TRUE);
		iniFile_SetValueI(SECTION_SSHCONFIG, KEY_SSH_AUTOUPLOAD, cfgConfiguration.SSH_AutomaticUpload, TRUE);
		iniFile_SetValueI(SECTION_SSHCONFIG, KEY_SSH_CONNTIMEOUT, cfgConfiguration.SSH_ConnectionTimeout, TRUE);
		iniFile_SetValueI(SECTION_SSHCONFIG, KEY_SSH_LOWSPEEDLIMIT, cfgConfiguration.SSH_LowSpeedLimit, TRUE);
		iniFile_SetValueI(SECTION_SSHCONFIG, KEY_SSH_LOWSPEEDTIME, cfgConfiguration.SSH_LowSpeedTime, TRUE);

		// store streaming configuration
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_ENABLED, (int) cfgConfiguration.Streaming_Enabled, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD0, (int) cfgConfiguration.Streaming_Server_IPv4_Field0, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD1, (int) cfgConfiguration.Streaming_Server_IPv4_Field1, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD2, (int) cfgConfiguration.Streaming_Server_IPv4_Field2, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERIPV4_FIELD3, (int) cfgConfiguration.Streaming_Server_IPv4_Field3, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_SERVERPORT, cfgConfiguration.Streaming_Server_Port, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_MAXNSENDMSGFAILURES, cfgConfiguration.Streaming_MaxNSendMsgFailures, TRUE);
		iniFile_SetValueI(SECTION_STREAMING, KEY_STREAMING_MAXNWAIT4REPLYFAILURES, cfgConfiguration.Streaming_MaxNWait4ReplyFailures, TRUE);
	}
}

BOOL config_CreateAppdataDirStructure(void)
{
	BOOL blnSuccess = FALSE, blnTemp = FALSE;
	TCHAR strBuffer[MAX_PATH_UNICODE + 1];
	
	if(m_blnHaveUserAppDataPath)
	{
		// parse & create first directory level
		_stprintf_s(strBuffer,
					sizeof(strBuffer)/sizeof(TCHAR),
					TEXT("%s%s\\%s\\"),
					LONGFILENAME_PRFX, m_strUserAppDataPath, SOFTWARE_MANUFACTURER);
		blnTemp = CreateDirectory(strBuffer, NULL);
		
		// check if first-level directory creation was successfull
		if(blnTemp)
		{
			// if yes: parse & create second directory level
			_tcscat_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), SOFTWARE_NAME);
			blnTemp = CreateDirectory(strBuffer, NULL);
			
			// check if second-level directory creation was successfull
			if(blnTemp)
				blnSuccess = TRUE;
		}
	}

	return blnSuccess;
}
