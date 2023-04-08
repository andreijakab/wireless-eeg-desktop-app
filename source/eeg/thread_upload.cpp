/**
 * \file		thread_WiFi.cpp
 * \since		21.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 * \version		1.0.0
 *
 * \brief		???
 *
 * $Id: thread_upload.cpp 76 2013-02-14 14:26:17Z jakab $
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
//   							Includes
//------------------------------------------------------------ ---------------
// Windows libaries
#include <windows.h>
#include <wincred.h>
#include <shlobj.h>
#include <shlwapi.h>

// CRT libraries
#include <tchar.h>

// custom libraries
#include <curl/curl.h>
#include <libmath.h>

// program headers
#include "globals.h"
#include "applog.h"
#include "util.h"
#include "thread_upload.h"

//---------------------------------------------------------------------------
// 							Structs/Enums
//---------------------------------------------------------------------------
/**
 * 
 */
typedef struct {HWND			hwndMainWnd;					///< handle to main window status bar
} LibcurlDebugCallbackData;

/**
 * 
 */
typedef struct {HWND			hwndMainWnd;					///< handle to main window status bar
} LibcurlProgressCallbackData;

//---------------------------------------------------------------------------
//							Internally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief This function gets called by libcurl as soon as it needs to read data in order to send it to the peer.
 *
 * \param[in]	pBuffer		
 * \param[in]	size		
 * \param[in]	nmemb		
 * \param[in]	hFile		
 *
 * \return
 */
size_t libcurl_read_callback(void * pBuffer, size_t size, size_t nmemb, void * hFile)
{
	DWORD dwNumberOfBytesRead = 0;

	BOOL bResult = ReadFile((HANDLE) hFile, pBuffer, size * nmemb, &dwNumberOfBytesRead, NULL);

	return dwNumberOfBytesRead;
}

/**
 * \brief This callback receives debug information, as specified with the curl_infotype argument.
 *
 * \param[in]	objCurl		
 * \param[in]	objT		
 * \param[in]	lpszText		
 * \param[in]	uTextSize		
 * \param[in]	pPointer		pointer to client data passed to the callback
 *
 * \return Always 0.
 */
int libcurl_debug_callback (CURL * objCurl, curl_infotype objT, char * lpszText, size_t uTextSize, void * pPointer)
{
	char *						lpszDebugMessage;
	LibcurlDebugCallbackData *	pldcd = (LibcurlDebugCallbackData *) pPointer;
	size_t						sztNCharsConverted;
	TCHAR						strDebugMessage[256];
	    
	if (objT == CURLINFO_TEXT)
    {
		lpszDebugMessage = (char *) malloc(uTextSize + 2);
		if(lpszDebugMessage != NULL)
		{
			sprintf_s(lpszDebugMessage, uTextSize + 2, "%s\0", lpszText);
			mbstowcs_s(&sztNCharsConverted, strDebugMessage, 255, lpszDebugMessage, _TRUNCATE);
			SendMessage(pldcd->hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) strDebugMessage);
			free(lpszDebugMessage);
		}
    }
	    
	return 0;
}

/**
 * \brief This function gets called by libcurl instead of its internal equivalent with a frequent interval during operation
 *        (roughly once per second or sooner) no matter if data is being transferred or not. 
 *
 * \param		clientp		pointer to client data passed to the callback
 * \param[in]	dltotal		
 * \param[in]	dlnow		
 * \param[in]	ultotal		
 * \param[in]	ulnow		
 *
 * \return 
 */
int libcurl_progress_callback (void * clientp, double dltotal, double dlnow, double ultotal, double ulnow)
{
	double							dblProgress = 0.0;
	LibcurlProgressCallbackData *	plpcd = (LibcurlProgressCallbackData *) clientp;
	TCHAR strBuffer[128];
	
	if(ultotal > 0 && ulnow >= 0)
		dblProgress = round((ulnow/ultotal)*100);

	_stprintf_s(strBuffer, 128, TEXT("Uploaded: %d%% (%d / %d)\n"), (int) dblProgress, (int) ulnow, (int) ultotal);
	SendMessage(plpcd->hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) strBuffer);

	return 0;
}

BOOL Upload_DialConnectionScript(UploadThreadData *	putd, HWND hwndMainWnd)
{
	BOOL					blnSuccess = FALSE;
	DWORD					d;
	PROCESS_INFORMATION		pi;
	STARTUPINFO				si;
	TCHAR					strSystem32Path[MAX_PATH + 1];
	TCHAR *					pstrCScriptPath;
	TCHAR *					pstrCScriptScriptFilePath;
	unsigned long			ulngTemp;
	
	SendMessage(hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) TEXT("Running connection script..."));

	// construct path to CScript executable
	strSystem32Path[0] = '\0';
	SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, strSystem32Path);
	ulngTemp = _tcslen(strSystem32Path) + 12 + 1; // +12 for "\cscript.exe", +1 for \0
	pstrCScriptPath = (TCHAR *) malloc(ulngTemp*sizeof(TCHAR));
	if(pstrCScriptPath != NULL)
	{
		_stprintf_s(pstrCScriptPath, ulngTemp, TEXT("%s\\%s"), strSystem32Path, TEXT("wscript.exe"));
					
		ulngTemp = 12 + _tcslen(putd->cfg.ConnectionScriptPath) + 1; // +12 for "cscript.exe ", +1 for \0
		pstrCScriptScriptFilePath = (TCHAR *) malloc(ulngTemp*sizeof(TCHAR));
		if(pstrCScriptScriptFilePath != NULL)
		{
			_stprintf_s(pstrCScriptScriptFilePath, ulngTemp, TEXT("%s %s"), TEXT("cscript.exe"), putd->cfg.ConnectionScriptPath);

			SecureZeroMemory (&pi, sizeof(PROCESS_INFORMATION));
			SecureZeroMemory (&si, sizeof(STARTUPINFO));
			si.cb = sizeof(STARTUPINFO);

			if(CreateProcess(pstrCScriptPath,
 								pstrCScriptScriptFilePath,
								NULL,
								NULL,
								FALSE,
								0,
								NULL,
								NULL,
								&si,
								&pi))
			{
				// wait for process to finish
				do
				{
					GetExitCodeProcess(pi.hProcess, &d);
					Sleep (250);
				}while(d == STILL_ACTIVE);

				// process finished => close unneeded handles
				CloseHandle(si.hStdError);
				CloseHandle(si.hStdInput);
				CloseHandle(si.hStdOutput);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);

				SendMessage(hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) TEXT("Connection script executed."));

				blnSuccess = TRUE;
			}
			else
				applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("UploadThread: Execution of connection script failed. (GetLastError #)"), GetLastError(), TRUE);
		}

		// free allocated memory
		if(pstrCScriptPath != NULL)
			free(pstrCScriptPath);
		if(pstrCScriptScriptFilePath != NULL)
			free(pstrCScriptScriptFilePath);
	}

	return blnSuccess;
}

//---------------------------------------------------------------------------
//							Globally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief Function executed when Upload thread is created using the CreateThread function.
 *
 * \param[in]	lParam		thread data passed to the function using the lpParameter parameter of the CreateThread function
 *
 * \return Indicates success or failure of the function.
 */
long WINAPI Upload_Thread(LPARAM lParam)
{
	BOOL						blnSuccess, blnSave;
	char						strBuffer[1024], * pstrEDFPlusFileName = NULL;
	char						* pstrSSHHostName = NULL, strSSHUserName[CREDUI_MAX_USERNAME_LENGTH + 1], strSSHPassword[CREDUI_MAX_PASSWORD_LENGTH + 1], * pstrSSHRemotePath = NULL;
	CREDUI_INFOA				cui;
	CURL						* hCurl;
	CURLcode					ccCurlResult;
	curl_off_t					cotFileSize;
	DWORD						dwrdErr;
	HANDLE						hFile;
	HWND						hwndMainWnd;
	LARGE_INTEGER				liFileSize;
	LibcurlDebugCallbackData	ldcd;
	LibcurlProgressCallbackData	lpcd;
	long						lngReturnCode = -1;
	size_t						sztLength, sztNCharsConverted;
	TCHAR						* pwstrEDFPlusFileName = NULL;
	unsigned long				ulngTemp;
	UploadThreadData *			putd;

	// get handle to main window
	hwndMainWnd = FindWindow (WINDOW_CLASSID_MAIN, NULL);

	// variable init
	putd = (UploadThreadData *) lParam;
	blnSuccess = FALSE;
	ccCurlResult = CURL_LAST;
	ldcd.hwndMainWnd = hwndMainWnd;
	lpcd.hwndMainWnd = hwndMainWnd;
	
	applog_logevent(Version, TEXT("libcurl"), TEXT(LIBCURL_VERSION), 0, FALSE);
	PostMessage(hwndMainWnd, EEGEMMsg_ExitPermission_Set, ExitPermission_Denied_SSHUpload, 0);

	//
	// Run connection script
	//
	if(putd->cfg.DialConnectionScript)
	{
		if(Upload_DialConnectionScript(putd, hwndMainWnd))
			blnSuccess = TRUE;
		else
		{
			applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("Upload_Thread(): Could not execute connection script successfully."), 0, TRUE);
			MsgPrintf(hwndMainWnd, MB_ICONERROR, TEXT("Measurement upload failed! Could not execute connection script successfully!"));
		}
	}
	else
		blnSuccess = TRUE;

	//
	// SSH Upload
	//
	if(blnSuccess)
	{
		// get username and password
		cui.cbSize = sizeof(CREDUI_INFOA);
		cui.hwndParent = NULL;
		cui.pszMessageText = "Enter user account information";
		cui.pszCaptionText = "EEG Server Credentials";
		cui.hbmBanner = NULL;
		blnSave = FALSE;
		SecureZeroMemory(strSSHUserName, sizeof(strSSHUserName));
		SecureZeroMemory(strSSHPassword, sizeof(strSSHPassword));

		// convert curl-required strings from TCHAR to char (libcurl only accepts char * strings)
		sztLength = _tcslen(putd->cfg.SSH_HostName);
		pstrSSHHostName = (char *) malloc((sztLength + 1) * sizeof(char));
		if(pstrSSHHostName != NULL)
		{
			wcstombs_s(&sztNCharsConverted, pstrSSHHostName, sztLength + 1, putd->cfg.SSH_HostName, sztLength);

			sztLength = _tcslen(putd->cfg.SSH_RemotePath);
			pstrSSHRemotePath = (char *) malloc((sztLength + 1) * sizeof(char));
			if(pstrSSHRemotePath != NULL)
			{
				wcstombs_s(&sztNCharsConverted, pstrSSHRemotePath, sztLength + 1, putd->cfg.SSH_RemotePath, sztLength);

				pwstrEDFPlusFileName = (TCHAR *) calloc(_tcslen(putd->pstrFinalEDFFilePath) + 1, sizeof(TCHAR));
				if(pwstrEDFPlusFileName != NULL)
				{
					_tcscpy_s(pwstrEDFPlusFileName, _tcslen(putd->pstrFinalEDFFilePath) + 1, putd->pstrFinalEDFFilePath);
					PathStripPath(pwstrEDFPlusFileName);
	
					sztLength = _tcslen(pwstrEDFPlusFileName);
					pstrEDFPlusFileName = (char *) calloc((sztLength + 1), sizeof(char));
					if(pstrEDFPlusFileName != NULL)
					{
						wcstombs_s(&sztNCharsConverted, pstrEDFPlusFileName, sztLength + 1, pwstrEDFPlusFileName, sztLength);

						// create a handle to the file
						hFile = CreateFile(putd->pstrFinalEDFFilePath,	// file to open
										   GENERIC_READ,			// open for reading
										   FILE_SHARE_READ,			// share for reading
										   NULL,					// default security
										   OPEN_EXISTING,			// existing file only
										   FILE_ATTRIBUTE_NORMAL,	// normal file
										   NULL);					// no attr. template
						if(hFile != INVALID_HANDLE_VALUE)
						{
							// start libcurl easy session 
							hCurl = curl_easy_init();
							if(hCurl)
							{
								// enable verbose operation
								curl_easy_setopt(hCurl, CURLOPT_VERBOSE, TRUE);
								curl_easy_setopt(hCurl, CURLOPT_DEBUGFUNCTION, libcurl_debug_callback);
								curl_easy_setopt(hCurl, CURLOPT_DEBUGDATA, &ldcd);		// configure pointer to data to be passed to debug callback function

								// enable uploading and set file size
								curl_easy_setopt(hCurl, CURLOPT_UPLOAD, TRUE);
								GetFileSizeEx(hFile, &liFileSize);
								cotFileSize = liFileSize.QuadPart;
								curl_easy_setopt(hCurl, CURLOPT_INFILESIZE_LARGE, cotFileSize);

								// enable progress report
								curl_easy_setopt(hCurl, CURLOPT_NOPROGRESS, FALSE);
								curl_easy_setopt(hCurl, CURLOPT_PROGRESSFUNCTION, libcurl_progress_callback);
								curl_easy_setopt(hCurl, CURLOPT_PROGRESSDATA, &lpcd);		// configure pointer to data to be passed to progress callback function

								// use custom read function
								curl_easy_setopt(hCurl, CURLOPT_READFUNCTION, libcurl_read_callback);

								// specify which file to upload
								curl_easy_setopt(hCurl, CURLOPT_READDATA, hFile);

								// specify target
								sprintf_s(strBuffer, 1024, "%s://%s/%s/%s", "sftp", pstrSSHHostName, pstrSSHRemotePath, pstrEDFPlusFileName);
								curl_easy_setopt(hCurl, CURLOPT_URL, strBuffer);
								curl_easy_setopt(hCurl, CURLOPT_PORT, (long) putd->cfg.SSH_Port);

								// set maximum time in seconds that connection to the server can take (default is 300 seconds)
								curl_easy_setopt(hCurl, CURLOPT_CONNECTTIMEOUT, putd->cfg.SSH_ConnectionTimeout);

								// transfer speed in bytes per second that the transfer should be below during CURLOPT_LOW_SPEED_TIME seconds for the library to consider it too slow and abort
								curl_easy_setopt(hCurl, CURLOPT_LOW_SPEED_LIMIT, putd->cfg.SSH_LowSpeedLimit);

								// time in seconds that the transfer should be below the CURLOPT_LOW_SPEED_LIMIT for the library to consider it too slow and abort
								curl_easy_setopt(hCurl, CURLOPT_LOW_SPEED_TIME, putd->cfg.SSH_LowSpeedTime);

								wcstombs_s(&sztNCharsConverted, strBuffer, sizeof(strBuffer), CREDS_SSHSERVER, sizeof(strBuffer)/sizeof(char) - 1);
								dwrdErr = CredUIPromptForCredentialsA(&cui,								// CREDUI_INFO structure
																	  strBuffer,						// Target for credentials (usually a server)
																	  NULL,								// Reserved
																	  0,								// Reason
																	  strSSHUserName,					// User name
																	  CREDUI_MAX_USERNAME_LENGTH + 1,	// Max number of char for user name
																	  strSSHPassword,					// Password
																	  CREDUI_MAX_PASSWORD_LENGTH + 1,	// Max number of char for password
																	  &blnSave,							// State of save check box
																	  CREDUI_FLAGS_GENERIC_CREDENTIALS);  

								if(!dwrdErr)
								{
									// specify username and password and specify authentication as password only
									sprintf_s(strBuffer, 1024, "%s:%s", strSSHUserName, strSSHPassword);

									// erase credentials from memory
									SecureZeroMemory(strSSHUserName, sizeof(strSSHUserName));
									SecureZeroMemory(strSSHPassword, sizeof(strSSHPassword));
				
									// set SSH user name and password in libcurl
									curl_easy_setopt(hCurl, CURLOPT_USERPWD, strBuffer); // user : password
									SecureZeroMemory(strBuffer, sizeof(strBuffer));
				
									// set SSH authentication to user name and password
									ulngTemp = CURLSSH_AUTH_PASSWORD;
									curl_easy_setopt(hCurl, CURLOPT_SSH_AUTH_TYPES, ulngTemp);

									// execute command
									ccCurlResult = curl_easy_perform(hCurl);
								}
								else
								{
									applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("CredUIPromptForCredentialsA failed. (function return value)"), dwrdErr, TRUE);
									ccCurlResult = CURLE_COULDNT_CONNECT;
								}

								// end libcurl easy session 
								curl_easy_cleanup(hCurl);
							}
		
							// release file handle
							CloseHandle(hFile);
		
							// global libcurl cleanup
							curl_global_cleanup();

							if (ccCurlResult == CURLE_OK)
								SendMessage(hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) TEXT("Measurement data uploaded successfully."));
							else
							{
								SendMessage(hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) TEXT("Measurement data upload failed!"));
								MsgPrintf(hwndMainWnd, MB_ICONERROR, TEXT("Measurement upload failed! Curl error: %d"), ccCurlResult);
								applog_logevent(SoftwareError, TEXT("libcurl"), TEXT("SSH upload failed. (ccCurlResult #)"), ccCurlResult, TRUE);
							}
						}
						else
						{
							applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("Couldn't open EDF+ file. (GetLastError #)"), GetLastError(), TRUE);
							MsgPrintf(hwndMainWnd, MB_ICONERROR, TEXT("Measurement upload failed! Could not open EDF+ file."));
						}

						SendMessage(hwndMainWnd, EEGEMMsg_StatusBar_SetStatus, 0, (LPARAM) TEXT(""));
					}
					else
						applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("Upload_Thread(): Failed to allocate memory for pstrEDFPlusFileName. (errno #)"), errno, TRUE);
				}
				else
					applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("Upload_Thread(): Failed to allocate memory for pwstrEDFPlusFileName. (errno #)"), errno, TRUE);
			}
			else
				applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("Upload_Thread(): Failed to allocate memory for pstrSSHRemotePath. (errno #)"), errno, TRUE);
		}
		else
			applog_logevent(SoftwareError, TEXT("UploadThread"), TEXT("Upload_Thread(): Failed to allocate memory for pstrSSHHostName. (errno #)"), errno, TRUE);

		// free malloc-ed memory
		if(pstrSSHHostName != NULL)
			free(pstrSSHHostName);
		if(pstrSSHRemotePath != NULL)
			free(pstrSSHRemotePath);
		if(pstrEDFPlusFileName != NULL)
			free(pstrEDFPlusFileName);
		if(pwstrEDFPlusFileName != NULL)
			free(pwstrEDFPlusFileName);

		if(ccCurlResult == CURLE_OK)
			lngReturnCode = 0;
	}
	
	// set exit status and exit
	PostMessage(hwndMainWnd, EEGEMMsg_ExitPermission_Clear, ExitPermission_Denied_SSHUpload, 0);	
	
	return lngReturnCode;
}