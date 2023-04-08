/**
 * \file		thread_sample.cpp
 * \since		07.02.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 * \version		1.0.0
 *
 * \brief		???
 *
 * $Id: thread_sample.cpp 78 2013-02-21 17:23:21Z jakab $
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
//							Libraries
//---------------------------------------------------------------------------
// Windows libraries


//---------------------------------------------------------------------------
//   						Includes
//------------------------------------------------------------ ---------------
// Windows libaries
#include <windows.h>

// CRT libraries
#include <stdlib.h>

// custom libraries
#include <eegem_beep.h>

// program headers
#include "globals.h"
#include "applog.h"
#include "serialV4.h"
#include "thread_storage.h"
#include "thread_stream.h"
#include "util.h"
#include "thread_sample.h"

//---------------------------------------------------------------------------
//   						Definitions
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//   						Structs/Enums
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//							Global variables
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//							Internally-accessible functions
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//							Globally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief Free all of the memory resources allocated to the members of a SampleDataRecord structure.
 *
 * \param[in]	pdrDataRecord	pointer to the SampleDataRecord the memory resources of which are to be released
 */
void Sample_FreeDataRecord(SampleDataRecord * pdrDataRecord)
{	
	if(pdrDataRecord->MeasurementData != NULL)
	{
		if(pdrDataRecord->MeasurementData[0] != NULL)
			free(pdrDataRecord->MeasurementData[0]);
	
		free(pdrDataRecord->MeasurementData);
	}

	// allocate memory for write buffer
	if(pdrDataRecord->WriteBuffer != NULL)
		free(pdrDataRecord->WriteBuffer);
}

/**
 * \brief Allocates memory for the MeasurementData and WriteBuffer members of a SampleDataRecord structure.
 *
 * \param[in]	pdrDataRecord			pointer to the SampleDataRecord structure to be initialized
 * \param[in]	intSamplingFrequency	frequency at which the EEG and acceleration signals are sampled
 *
 * \return TRUE if succesfull, FALSE otherwise.
 */
BOOL Sample_InitDataRecord(SampleDataRecord * pdrDataRecord, int intSamplingFrequency)
{
	BOOL			blnResult = TRUE;
	unsigned int	i;
	
	//
	// allocate memory for measurement data buffer
	//
	pdrDataRecord->MeasurementData = (short **) malloc((EEGCHANNELS + ACCCHANNELS) * sizeof(short *));
	if(pdrDataRecord->MeasurementData != NULL)
	{
		pdrDataRecord->MeasurementData[0] = (short *) malloc((EEGCHANNELS + ACCCHANNELS) * intSamplingFrequency * sizeof(short));
		if(pdrDataRecord->MeasurementData[0] != NULL)
		{
			SecureZeroMemory (pdrDataRecord->MeasurementData[0] , (EEGCHANNELS + ACCCHANNELS) * intSamplingFrequency * sizeof(short));

			for(i = 1; i < (EEGCHANNELS + ACCCHANNELS); i++)
				pdrDataRecord->MeasurementData[i] = pdrDataRecord->MeasurementData[0] + i * intSamplingFrequency;
		}
		else
		{
			free(pdrDataRecord->MeasurementData);
			applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_InitDataRecord(): Failed to allocate memory for the contents of the MeasurementData member of the EEGEMDataRecord structure. (errno #)"), errno, TRUE);
			blnResult = FALSE;
		}
	}
	else
	{
		applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_InitDataRecord(): Failed to allocate memory for the pointers of the MeasurementData member of the EEGEMDataRecord structure. (errno #)"), errno, TRUE);
		blnResult = FALSE;
	}
	
	//
	// allocate memory for write buffer
	//
	if(blnResult)
	{
		pdrDataRecord->WriteBufferLen = ((EEGCHANNELS + ACCCHANNELS) * intSamplingFrequency * sizeof(short)) + ANNOTATION_TOTAL_NCHARS*sizeof(char);
		pdrDataRecord->WriteBuffer = (BYTE *) malloc(pdrDataRecord->WriteBufferLen);
		if(pdrDataRecord->WriteBuffer != NULL)
			SecureZeroMemory (pdrDataRecord->WriteBuffer, ((EEGCHANNELS + ACCCHANNELS) * intSamplingFrequency * sizeof(short)) + ANNOTATION_TOTAL_NCHARS*sizeof(char));
		else
		{
			applog_logevent(SoftwareError, TEXT("SampleThread"), TEXT("Sample_InitDataRecord(): Failed to allocate memory for the WriteBuffer member of the EEGEMDataRecord structure. (errno #)"), errno, TRUE);
			blnResult = FALSE;
		}
	}

	return blnResult;
}