/**
 * 
 * \file util.cpp
 * \brief The implementation file for the general utils module.
 *
 * 
 * $Id: util.cpp 76 2013-02-14 14:26:17Z jakab $
 */

// CRT libraries
# include <math.h>

# include "util.h"

/**
 * \brief Creates a tooltip for an item in a dialog box.
 *
 * \param <toolID>	identifier of an dialog box item
 * \param <hDlg>	window handle of the dialog box
 * \param <pszText>	string to use as the tooltip text
 *
 * \return The handle to the tooltip.
 */
HWND util_CreateToolTip(int toolID, HWND hDlg, PTSTR pszText)
{
	HWND hwndTip, hwndTool;
	TOOLINFO toolInfo = { 0 };

    if (!toolID || !hDlg || !pszText)
    {
        return FALSE;
    }
    // Get the window of the tool.
    hwndTool = GetDlgItem(hDlg, toolID);
    
    // Create the tooltip. g_hInst is the global instance handle.
    hwndTip = CreateWindowEx(0,
							 TOOLTIPS_CLASS,
							 NULL,
							 WS_POPUP | TTS_ALWAYSTIP | TTS_BALLOON,
							 CW_USEDEFAULT, CW_USEDEFAULT,
							 CW_USEDEFAULT, CW_USEDEFAULT,
							 hDlg,
							 NULL,
							 NULL,
							 NULL);
    
   if (!hwndTool || !hwndTip)
   {
       return (HWND)NULL;
   }                              
                              
    // Associate the tooltip with the tool.
    toolInfo.cbSize = sizeof(toolInfo);
    toolInfo.hwnd = hDlg;
    toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    toolInfo.uId = (UINT_PTR)hwndTool;
    toolInfo.lpszText = pszText;
    SendMessage(hwndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);

    return hwndTip;
}

/**
 * \brief Gets the current date and time.
 *
 * \return The current date and time in the form of a 'struct tm'.
 */
struct tm util_GetCurrentDateTime()
{
	struct tm tmCurrentDateTime;
	time_t tmtNow;

	tmtNow = time(NULL);
	localtime_s(&tmCurrentDateTime, &tmtNow);
	
	return tmCurrentDateTime;
}

/**
 * \brief Count number of bits set to 1 in given channel mask.
 *
 * Count number of bits set to 1 in given byte.
 *
 * \param <bytChannelMask> Byte where the number of ones is checked
 * \return Number of bytes set to 1 in given byte
 */
unsigned int util_GetNOfSelectedChannels (BYTE bytChannelMask)
{
	int i, j;

	for (i = j = 0; i < NCHANNELSINMASK; i++)
	{
		if (bytChannelMask & (0x01 << i))
			j++;
	}
	
	return (j);
}

DWORD util_GetRandomPrefix(TCHAR * strPrefixBuffer, unsigned int uintPrefixBufferLen)
{
	HCRYPTPROV hProv = 0;
	DWORD dwErr = 0, dwTemp;
	TCHAR * szValues = TEXT("abcdefghijklmnopqrstuvwxyz0123456789");
	size_t cbValues = lstrlen(szValues);
	unsigned char i;

#ifdef _DEBUG
	// make sure buffer is actually as long as advertised
	memset(strPrefixBuffer, 0xAE, uintPrefixBufferLen*sizeof(TCHAR));
#endif

	if(CryptAcquireContext(&hProv,
						   NULL, NULL,
						   PROV_RSA_FULL,
						   CRYPT_VERIFYCONTEXT) == FALSE)
		return GetLastError();

	for(i=0; i<uintPrefixBufferLen - 1; i++)
	{
		CryptGenRandom(hProv, sizeof(DWORD), (LPBYTE) &dwTemp);
		strPrefixBuffer[i] = szValues[dwTemp%cbValues];
	}
	strPrefixBuffer[uintPrefixBufferLen - 1] = TEXT('\0');

	if(hProv)
		CryptReleaseContext(hProv, 0);

	return dwErr;
}

BOOL util_IsMaxNChannelsExceeded(unsigned int uintSamplingFrequency, BYTE bytChannelMask, unsigned int * puintMaxNChannelPossible)
{
	BOOL			blnReturn = TRUE;
	unsigned int	uintNSelectedChannels;

	// compute max. number of channel possible at the entered sampling frequency
	*puintMaxNChannelPossible = (unsigned int) floor((double) MAXNMEASUREMENTS/uintSamplingFrequency);

	// calculate number of seleceted channels
	uintNSelectedChannels = util_GetNOfSelectedChannels(bytChannelMask);
	
	// perform check
	if(uintNSelectedChannels <= *puintMaxNChannelPossible)
		blnReturn = FALSE;

	return blnReturn;
}

BOOL util_IsWinXorLater(WindowsVersions wvWindowsVersion)
{
	OSVERSIONINFOEX osvi;
	DWORDLONG dwlConditionMask = 0;
	int op = VER_GREATER_EQUAL;

	// Initialize the OSVERSIONINFOEX structure.
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	switch(wvWindowsVersion)
	{
		case WinXP_SP2:
			osvi.dwMajorVersion = 5;
			osvi.dwMinorVersion = 1;
			osvi.wServicePackMajor = 2;
			osvi.wServicePackMinor = 0;
		break;

		case WinVista:
			osvi.dwMajorVersion = 6;
			osvi.dwMinorVersion = 0;
			osvi.wServicePackMajor = 0;
			osvi.wServicePackMinor = 0;
		break;

		case Win7:
			osvi.dwMajorVersion = 6;
			osvi.dwMinorVersion = 1;
			osvi.wServicePackMajor = 0;
			osvi.wServicePackMinor = 0;
		break;
		
		case Win8:
			osvi.dwMajorVersion = 6;
			osvi.dwMinorVersion = 2;
			osvi.wServicePackMajor = 0;
			osvi.wServicePackMinor = 0;
		break;
	}

	// Initialize the condition mask.
	VER_SET_CONDITION( dwlConditionMask, VER_MAJORVERSION, op );
	VER_SET_CONDITION( dwlConditionMask, VER_MINORVERSION, op );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMAJOR, op );
	VER_SET_CONDITION( dwlConditionMask, VER_SERVICEPACKMINOR, op );

	// Perform the test.

	return VerifyVersionInfo(&osvi, 
							VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
							dwlConditionMask);
}

/**
 * \brief		Re-maps a number from one range to another.
 *
 * \details		A value \a x of  \a in_min would get mapped to \a out_min, a value of in_max to out_max, values in-between to values in-between, etc. 
 *				Does not constrain values to within the range, because out-of-range values are sometimes intended and useful.
 *				Note that the "lower bounds" of either range may be larger or smaller than the "upper bounds" so the map()
 *				function may be used to reverse a range of numbers. The function also handles negative numbers well.\n
 *				The map() function uses integer math so it will not generate fractions, when the math might indicate that it should do so.
 *				Fractional remainders are truncated, and are not rounded or averaged. 
 *
 * \returns		The mapped value.
 *
 * \param[in]	x			number to map
 * \param[in]	in_min		lower bound of the value's current range
 * \param[in]	in_max		upper bound of the value's current range
 * \param[in]	out_min		lower bound of the value's target range
 * \param[in]	out_max		upper bound of the value's target range
 */
/*int util_map(int x, int in_min, int in_max, int out_min, int out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}*/

int util_map(double x, int in_min, int in_max, int out_min, int out_max)
{
	//return (int) ((x - in_min) * ((double) out_max - out_min) / ((double) in_max - in_min) + out_min);
	return (out_max - (int) ((x - in_min) * ((double) out_max - out_min) / ((double) in_max - in_min)));
}


/**
 * \brief Prints message to messagebox.
 *
 * Prints the given message to the screen in a messagebox of a given style.
 *
 * \param	hwndOwner	handle to the window that will act as the messagebox's parent
 * \param	intStyle	value indicating the style of message box to be generated
 * \param	Format		string specifying the message, maximum length of text is 255 chars
 * \return Nothing.
 */
void MsgPrintf (HWND hwndOwner, int intStyle, TCHAR * Format, ...)		
{														
	va_list v;
	TCHAR strBuffer [256];

	va_start (v, Format);  
	_vstprintf_s (strBuffer, sizeof(strBuffer)/sizeof(TCHAR), Format, v);
	va_end (v);

	MessageBox (hwndOwner, strBuffer, SOFTWARE_TITLE, MB_OK | MB_APPLMODAL | intStyle);
}