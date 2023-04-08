/**
 * \file		thread_upload.h
 * \since		16.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 *
 * \brief		???
 *
 * $Id: thread_upload.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __THREAD_UPLOAD_H__
# define __THREAD_UPLOAD_H__

//---------------------------------------------------------------------------
//   								Libraries
//---------------------------------------------------------------------------
// Custom
#pragma warning (disable : 4099)		// disables "PDB 'filename' was not found"-error caused by libcurl
#pragma comment(lib, "libcurl.lib")
#pragma comment(lib, "libmath.lib")

// Windows
#pragma comment(lib, "ws2_32.lib")

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
/**
 * 
 */
typedef struct {TCHAR *			pstrFinalEDFFilePath;			///< pointer to NULL-terminated string containing the full path of the final EDF+ file
				CONFIGURATION	cfg;							///< values contained in CONFIGURATION struct used in the main module are copied to this struct when the upload thread is created
} UploadThreadData;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
long WINAPI		Upload_Thread(LPARAM lParam);

#endif