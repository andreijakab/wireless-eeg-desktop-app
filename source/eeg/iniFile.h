/**
 * 
 * \file iniFile.h
 * \brief The header file for module handling the configuration file.
 *
 * 
 * $Id: iniFile.h 74 2013-01-22 16:29:42Z jakab $
 */

#ifndef _INIFILE_H_
#define _INIFILE_H_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER			0x0502									// application requires at least Windows XP SP2
#define _WIN32_WINNT	0x0502									// application requires at least Windows XP SP2
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//---------------------------------------------------------------------------
//   								Includes
//---------------------------------------------------------------------------
# include <stdio.h>
# include <string.h>
# include <tchar.h>
# include <windows.h>

# include "globals.h"

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define BUFFER_SIZE	200

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
BOOL iniFile_GetValueH(TCHAR *strSectionName, TCHAR *strKeyName, int intDefaultValue, int *intValue);
BOOL iniFile_GetValueI(TCHAR *strSectionName, TCHAR *strKeyName, int intDefaultValue, int *intValue);
BOOL iniFile_GetValueS(TCHAR *strSectionName, TCHAR *strKeyName, TCHAR *strDefaultString, TCHAR *strBuffer, int intBufferLen);
void iniFile_init(TCHAR *strINIFilePath);
BOOL iniFile_SetValue(TCHAR *strSectionName, TCHAR *strKeyName, TCHAR *strValue, BOOL blnCreate);
BOOL iniFile_SetValueH(TCHAR *strSectionName, TCHAR *strKeyName, unsigned int uintValue, BOOL blnCreate);
BOOL iniFile_SetValueI(TCHAR *strSectionName, TCHAR *strKeyName, int intValue, BOOL blnCreate);
BOOL iniFile_SetValueF(TCHAR *, TCHAR *, double, BOOL);
BOOL iniFile_SetValueV(TCHAR *, TCHAR *, BOOL, TCHAR *, ...);

#endif