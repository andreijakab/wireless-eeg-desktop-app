/**
 * 
 * \file util.h
 * \brief The header file for the general utils module.
 *
 * 
 * $Id: util.h 74 2013-01-22 16:29:42Z jakab $
 */

# ifndef __UTIL_H__
# define __UTIL_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER			0x0502									// application requires at least Windows XP SP2
#define _WIN32_WINNT	0x0502									// application requires at least Windows XP SP2
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//--------------------------------------------------------------------------------------------------------------
//   								Includes
//--------------------------------------------------------------------------------------------------------------
# include <windows.h>
# include <commctrl.h>
# include <stdio.h>
# include <tchar.h>
# include <time.h>


# include "globals.h"

//--------------------------------------------------------------------------------------------------------------
//   								Definitions
//--------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------
//   								Enums/Structs
//---------------------------------------------------------------------------
// 
typedef enum {WinXP_SP2,				// Windows XP w/ SP2
			  WinVista,					// Windows Vista
			  Win7,						// Windows 7
			  Win8						// Windows 8
			 } WindowsVersions;

//--------------------------------------------------------------------------------------------------------------
//   								Prototypes
//--------------------------------------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
	HWND			util_CreateToolTip(int toolID, HWND hDlg, PTSTR pszText);
	struct tm		util_GetCurrentDateTime();
	unsigned int	util_GetNOfSelectedChannels (BYTE bytChannelMask);
	DWORD			util_GetRandomPrefix(TCHAR * strPrefixBuffer, unsigned int uintPrefixBufferLen);
	BOOL			util_IsMaxNChannelsExceeded(unsigned int uintSamplingFrequency, BYTE bytChannelMask, unsigned int * puintMaxNChannelPossible);
	BOOL			util_IsWinXorLater(WindowsVersions wvWindowsVersion);
	//int				util_map(int x, int in_min, int in_max, int out_min, int out_max);
	int				util_map(double x, int in_min, int in_max, int out_min, int out_max);
	void			MsgPrintf (HWND hwndOwner, int intStyle, TCHAR * Format, ...);

#ifdef __cplusplus
}
#endif

# endif