/**
 * \file		thread_WiFi.cpp
 * \since		21.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 * \version		1.0.0
 *
 * \brief		???
 *
 * $Id: thread_WiFi.cpp 76 2013-02-14 14:26:17Z jakab $
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
//									Libraries
//---------------------------------------------------------------------------
// custom

//---------------------------------------------------------------------------
//   							Includes
//------------------------------------------------------------ ---------------
// Windows libaries
#include <windows.h>
#include <Wlanapi.h>

// CRT libraries

// custom libraries

// program headers
#include "globals.h"
#include "applog.h"
#include "util.h"
#include "thread_WiFi.h"

//---------------------------------------------------------------------------
//   						Definitions
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// 							Structs/Enums
//---------------------------------------------------------------------------
/**
 * 
 */
typedef struct { BOOL	WasScanSuccessfull;				///< 
				 HANDLE	hevWiFiThread_ScanComplete;		///< 
} WiFiCallbackData;

//---------------------------------------------------------------------------
//							Global variables
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//							Internally-accessible functions
//---------------------------------------------------------------------------
static void	WiFi_CallBack(PWLAN_NOTIFICATION_DATA pNotifData, PVOID pContext)
{
	WiFiCallbackData * pwfcd = (WiFiCallbackData *) pContext;

	if (pNotifData->NotificationSource == WLAN_NOTIFICATION_SOURCE_ACM) 
	{
		switch(pNotifData->NotificationCode)
		{
			case wlan_notification_acm_scan_complete:
				pwfcd->WasScanSuccessfull = TRUE;
				SetEvent(pwfcd->hevWiFiThread_ScanComplete);
			break;

			case wlan_notification_acm_scan_fail:
				pwfcd->WasScanSuccessfull = FALSE;
				SetEvent(pwfcd->hevWiFiThread_ScanComplete);
			break;
		}
	}
}

//---------------------------------------------------------------------------
//							Globally-accessible functions
//---------------------------------------------------------------------------
static long WINAPI WiFi_Thread (LPARAM lParam)
{
	DWORD						dwrdNegotiatedVersion;
	DWORD						dwrdDataSize;
	GUID						guidWiFiInterface;
	HANDLE						hWiFiHandle;
	PWLAN_BSS_LIST				pBSSList;
	PWLAN_INTERFACE_INFO_LIST	pInterfaceList;
	PWLAN_RADIO_STATE			pRadioState;
	PVOID						pData;
	unsigned int				i;
	WiFiCallbackData			wfcd;
	WiFiThreadData *			pwftd;

	// init variables
	pwftd = (WiFiThreadData *) lParam;
	wfcd.hevWiFiThread_ScanComplete = pwftd->hevWiFiThread_ScanComplete;
	wfcd.WasScanSuccessfull = FALSE;
	//m_dwrdRadioChannelMask = RCHANNELMASK; // use default "safe" channel mask if thread fails to execute for some reason 
	pBSSList = NULL;
	pInterfaceList = NULL;

	if(util_IsWinXorLater(WinVista))
	{
		// WLAN Site Management Initialization
		if(WlanOpenHandle(WLAN_API_VERSION_2_0,		// Vista WiFi API version
						  NULL,						// reserved, must be NULL
						  &dwrdNegotiatedVersion,	// version of the WLAN service
						  &hWiFiHandle				// returned handle
						  ) == ERROR_SUCCESS) 
		{
			// enumerate WLAN interfaces
			if(WlanEnumInterfaces(hWiFiHandle,			// opened handle
								  NULL,					// reserved, must be NULL
								  &pInterfaceList		// returned interface info list
								  ) == ERROR_SUCCESS)
			{
				// check if there are any WiFi interfaces
				if(pInterfaceList->dwNumberOfItems > 0)
				{
					// check if first is ready
					if(pInterfaceList[0].InterfaceInfo->isState != wlan_interface_state_not_ready)
					{
						// choose first WiFi interace
						guidWiFiInterface = pInterfaceList[0].InterfaceInfo->InterfaceGuid;

						// check if WiFi radio is enabled
						if(WlanQueryInterface(hWiFiHandle,					// opened handle
											  &guidWiFiInterface,			// interface GUID
											  wlan_intf_opcode_radio_state,	// value that specifies the parameter to be queried
											  NULL,							// reserved, must be NULL
											  &dwrdDataSize,
											  &pData,
											  NULL) == ERROR_SUCCESS)
						{
							pRadioState = (PWLAN_RADIO_STATE) pData;

							// ensure that WiFi radio is turned on
							if(pRadioState[0].PhyRadioState->dot11HardwareRadioState == dot11_radio_state_on &&
							   pRadioState[0].PhyRadioState->dot11SoftwareRadioState == dot11_radio_state_on)
							{

								// register to receive WLAN notifications
								if(WlanRegisterNotification(hWiFiHandle,								// opened handle
															WLAN_NOTIFICATION_SOURCE_ACM,				// register for notifications generated by auto configuration module
															FALSE,										// don’t ignore duplicate 
															(WLAN_NOTIFICATION_CALLBACK) WiFi_CallBack,	// callback function
															&wfcd,										// pointer to data to be passed to callback function
															NULL,										// reserved, must be NULL
															NULL										// don’t return previous
														   ) == ERROR_SUCCESS)
								{
									
									
									while(1)
									{
										// perform scan for WiFi networks on chosen interface
										if(WlanScan(hWiFiHandle,		// opened handle
													&guidWiFiInterface,	// interface GUID
													NULL,				// don’t probe 
													NULL,				// don’t probe
													NULL				// reserved, must be NULL
												   ) == ERROR_SUCCESS)
										{
											// wait for WiFi network scan to complete
											WaitForSingleObject(pwftd->hevWiFiThread_ScanComplete, INFINITE);

											// query BSS list of all networks
											if(WlanGetNetworkBssList(hWiFiHandle,			// opened handle
																	 &guidWiFiInterface,	// interface GUID
																	 NULL,					// all networks
																	 dot11_BSS_type_any,	// any BSS network
																	 FALSE,					// security settings not applicable since no SSID specified
																	 NULL,					// reserved, must be NULL
																	 &pBSSList				// pointer to the returned BSS list    
																	) ==  ERROR_SUCCESS)
											{
												for(i=0; i < pBSSList->dwNumberOfItems; i++)
												{
				// Process BSS entries
				//PWLAN_BSS_ENTRY pBssEntry = &pNetworkList->wlanBssEntries[i];

												}
											}
											else
											{
												// log failure of WlanGetNetworkBssList()
												applog_logevent(SoftwareError, TEXT("WiFi"), TEXT("WiFi_Thread: WlanGetNetworkBssList() failed. (GetLastError #)"), GetLastError(), TRUE);
											}

											// free allocated ememory
											WlanFreeMemory(pBSSList);
										}
										else
										{
											// log failure of WlanScan()
											applog_logevent(SoftwareError, TEXT("WiFi"), TEXT("WiFi_Thread: WlanScan() failed. (GetLastError #)"), GetLastError(), TRUE);
										}

										// repeat algorithm every 15 minutes
										Sleep(900000);
									}
								}
								else
								{
									// log failure of WlanRegisterNotification()
									applog_logevent(SoftwareError, TEXT("WiFi"), TEXT("WiFi_Thread: WlanRegisterNotification() failed. (GetLastError #)"), GetLastError(), TRUE);
								}
							}
						}
						else
						{
							// log failure of WlanQueryInterface()
							applog_logevent(SoftwareError, TEXT("WiFi"), TEXT("WiFi_Thread: WlanQueryInterface() failed. (GetLastError #)"), GetLastError(), TRUE);
						}
					}
				}
			}
			else
			{
				// log failure of WlanEnumInterface()
				applog_logevent(SoftwareError, TEXT("WiFi"), TEXT("WiFi_Thread: WlanEnumInterfaces() failed. (GetLastError #)"), GetLastError(), TRUE);
			}
		}
		else
		{
			// log failure of WlanOpenHandle()
			applog_logevent(SoftwareError, TEXT("WiFi"), TEXT("WiFi_Thread: WlanOpenHandle() failed. (GetLastError #)"), GetLastError(), TRUE);
		}
	}

	// memory clean-up
	if(pInterfaceList != NULL)
	{
		WlanFreeMemory(pInterfaceList);
		pInterfaceList = NULL;
	}

	return 0;
}