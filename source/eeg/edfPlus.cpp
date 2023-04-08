/**
 * 
 * \file edfPlus.cpp
 * \brief This file has all the functions needed when handling a EDF+ file.
 *
 * 
 * $Id: edfPlus.cpp 76 2013-02-14 14:26:17Z jakab $
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
# include <io.h>
# include <math.h>
# include <stdio.h>
# include <string.h>
# include <tchar.h>
# include <time.h>

# include "globals.h"
# include "annotations.h"
# include "util.h"
# include "edfPlus.h"

//----------------------------------------------------------------------------------------------------------------------------
//   								Constants
//----------------------------------------------------------------------------------------------------------------------------
// Label for each channel
static char m_strChannelLabels[11][EDFLABELFORSIGNALLENGTH + 1] = {"X-axis", "Y-axis", "Z-axis", "EEG P10-Ref", "EEG F8-Ref", "EEG Fp2-Ref", "EEG Fp1-Ref", "EEG F7-Ref", "EEG P9-Ref", "EDF Annotations"};

// Month labels used for recording info in dd-MMM-yyyy format
static char m_strMonthNames[12][4] = {	"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
										"JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

//----------------------------------------------------------------------------------------------------------------------------
//   								Locally-accessible functions
//----------------------------------------------------------------------------------------------------------------------------
/**
* \brief Fills string to given length with spaces if it is shorter
*
* Since EDF+ header needs ASCII-fields of certain length, this function takes in shorter
* char arrays and fills them with spaces (ASCII code 32) to given length.
* Last character of original array is presumed to be 0. It also is filled with space.
* Empty arrays are also filled with space.
*
* \param <string>			The original char array
* \param <stringLength>		Size of original char array which ends with 0
* \param <length>			To which length result will be extented
* \param <result>			Char array for resulting string
*/
void edf_PadHeaderString(char* string, unsigned uintStringLength, unsigned uintResultLength, char* result)
{
	unsigned u = 0;

	for ( u = 0; u < uintResultLength; ++u )
	{	
		if(string != NULL)
		{
			// each character from original string is transfered as-is to result sting
			if ( u < uintStringLength )
			{
				*result = *string;
				++result;
				++string;
			}
			// terminal NULL character in original string is replaced by code 32
			else if ( *(string) == 0 )
			{
				*result = 32;			
				++result;
				++string;
			}
			// fill remaining space in result string with character 32
			else
			{
				*result = 32;
				++result;
			}
		}
		else
		{
			*result = 32;
			result++;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------------
//   								Globally-accessible functions
//----------------------------------------------------------------------------------------------------------------------------
/**
* \brief Calculate the size of the EDF+ file header record.
*
* \param	uintNSignals	number of signals in one data record
*
* \return The size of the EDF+ file header record, in bytes, if it is smaller than the maimum unsigned short value and 0 otherwise.
*/
unsigned short edf_CalculateEDFplusHeaderRecord(unsigned int uintNSignals)
{
	unsigned short ushrReturnValue;
	
	if((EDFFILEHEADERLENGTH + uintNSignals*EDFSIGNALHEADERLENGTH) > USHRT_MAX)
		ushrReturnValue = 0;
	else
		ushrReturnValue = (short) (EDFFILEHEADERLENGTH + uintNSignals*EDFSIGNALHEADERLENGTH);

	return ushrReturnValue;
}

/**
* \brief Convert scandinavian characters into ASCII characters. If string is not an annotation, spaces are converte to '_' for EDF+ compliance.
*
* \param strString			string with characters to be converted
* \param blnIsAnnotation	indicates whether or not the provided string is an annotation
*/
void edf_CorrectSpecialCharacters(TCHAR * strString, BOOL blnIsAnnotation)
{
	BOOL blnCharFound;
	int i;
	TCHAR strMask[] = TEXT("AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz ^~@!#$%&()[]{}*+,-|./0123456789:;<>=?@\b\'\"\\");
	TCHAR *t;

	for(i=0; i < (int) _tcslen(strString); i++)
	{
		switch(strString[i])
		{
			case TEXT('ä'):
			case TEXT('å'):
				strString[i] = TEXT('a');
			break;

			case TEXT('ö'):
				strString[i] = TEXT('o');
			break;
			
			case TEXT('Ä'):
			case TEXT('Å'):
				strString[i] = TEXT('A');
			break;

			case TEXT('Ö'):
				strString[i] = TEXT('O');
			break;

			case TEXT(' '):
				if(!blnIsAnnotation)
					strString[i] = TEXT('_');
			break;

			default:
				t = strMask;
				blnCharFound = FALSE;
				
				// algorithm that checks if the current character is present in the strMask string
				for(;;)
				{
					if(*t == strString[i])
					{
						blnCharFound = TRUE;
						break;
					}
			        
					if (*t == TEXT('\0'))
						break;

					t++;
				}

				if(!blnCharFound)
					strString[i] = TEXT('_');
		}
	}
}

/**
* \brief Generates a complete EDF+ header record from the arguments passed to it and stores it in a buffer passed to it.
*
* \param	blnUpdateStartDateTime	specifies whether the start date & time should be set to the current date & time
* \param	piPatientInfo			PatientIdentification structure containing the the information to be stored in the 'local patient identification' field
* \param	riRecordingInfo			RecordingIdentification structure containing the the information to be stored in the 'local recording identification' field
* \param	intSamplingFrequency	frequency at which the EEG and acceleration signals are sampled, in Hz
* \param	intNDataRecords			amount of data records in the EDF+ file
* \param	uintNEEGChannels		amount of EEG signals
* \param	uintNAccChannels		amount of acceleration signals
* \param	pstrElectrodeType		pointer to null-terminated string containing the type of electrode used to measure the EEG signals
* \param	HeaderBuffer			pointer to char buffer where header record is to be stored
* \param	uintHeaderBufferLen		size of the Buffer, in bytes
*
* \return TRUE if header record was succesfully generated, FALSE otherwise.
*/
BOOL edf_GenerateEDFplusHeaderRecord(BOOL blnSetStartDateTime, PatientIdentification piPatientInfo, RecordingIdentification riRecordingInfo,
									 int intSamplingFrequency, int intNDataRecords, unsigned int uintNEEGChannels, unsigned int uintNAccChannels,
									 TCHAR * pstrElectrodeType, char * HeaderBuffer, unsigned int uintHeaderBufferLen)
{
	char *				strBuffer1;
	char				strBuffer2[256];
	char *				strEmpty;
	size_t				sztBuffer1Byt;
	size_t				stSize;
	static struct tm	tmCurrentDateTime;
	unsigned int		i;
	unsigned int		uintHeaderSizeByt;
	unsigned int		uintNSignals;

	// variable init
	strEmpty = "X ";
	uintNSignals = uintNEEGChannels + uintNAccChannels + 1;				// +1 for EDF+ Annotations channel
	if(blnSetStartDateTime)
		tmCurrentDateTime = tmCurrentDateTime = util_GetCurrentDateTime();

	// check if buffer is large enough
	uintHeaderSizeByt = edf_CalculateEDFplusHeaderRecord(uintNSignals);
	if(uintHeaderBufferLen < uintHeaderSizeByt)
		return FALSE;

	// allocate buffer memory
	sztBuffer1Byt = uintNSignals * EDFTRANSDUCERTYPELENGTH * sizeof(char);
	strBuffer1 = (char *) malloc(sztBuffer1Byt);
	if(strBuffer1 == NULL)
		return FALSE;

	// fill buffer with zeros
	SecureZeroMemory(HeaderBuffer, uintHeaderBufferLen);
	
	//
	// Parse File Header Field
	//
	// 'version'
	sprintf_s(HeaderBuffer, uintHeaderBufferLen, "%s", EDFVERSION);

	// 'local patient identification'
	if(piPatientInfo.ContainsValidData)
	{	
		// PIC
		if(piPatientInfo.PIC != NULL && _tcslen(piPatientInfo.PIC) > 0)
		{	
			edf_CorrectSpecialCharacters(piPatientInfo.PIC, FALSE);
			wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, piPatientInfo.PIC, sztBuffer1Byt);
			strcpy_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
			strcat_s(strBuffer2, sizeof(strBuffer2), " ");
		}
		else
			strcpy_s(strBuffer2, sizeof(strBuffer2), strEmpty);
	
		// gender
		switch(piPatientInfo.Sex)
		{
			case TEXT('M'):
				strcat_s(strBuffer2, sizeof(strBuffer2), "M ");
			break;

			case TEXT('F'):
				strcat_s(strBuffer2, sizeof(strBuffer2), "F ");
			break;

			default:
				strcat_s(strBuffer2, sizeof(strBuffer2), strEmpty);
			break;
		}
	
		// birthday
		if(piPatientInfo.BirthDay > 0 && piPatientInfo.BirthYear > 0)
		{
			sprintf_s(strBuffer1, sztBuffer1Byt, "%02d-%s-%4d ", piPatientInfo.BirthDay, m_strMonthNames[piPatientInfo.BirthMonth - 1], piPatientInfo.BirthYear);
			strcat_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
		}
		else
			strcat_s(strBuffer2, sizeof(strBuffer2), strEmpty);
		
		// Name
		if(piPatientInfo.Name != NULL && _tcslen(piPatientInfo.Name) > 0)
		{
			edf_CorrectSpecialCharacters(piPatientInfo.Name, FALSE);
			wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, piPatientInfo.Name, sztBuffer1Byt);
			strcat_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
			strcat_s(strBuffer2, sizeof(strBuffer2), " ");
		}
		else
			strcat_s(strBuffer2, sizeof(strBuffer2), strEmpty);

		// pad 'local patient identification'-string so that its length matches that defined in the EDF standard
		edf_PadHeaderString(strBuffer2, (int) strlen(strBuffer2), EDFPATIENTLENGTH, strBuffer2);
	}
	else
	{
		edf_PadHeaderString(EDFUNKNOWNPATIENT, (int) strlen(EDFUNKNOWNPATIENT), EDFPATIENTLENGTH, strBuffer2);
	}
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer2, EDFPATIENTLENGTH);

	// 'local recording identification'
	sprintf_s(strBuffer2, sizeof(strBuffer2), EDFRECORDINGIDENTFORMAT, EDFRECORDINGSTARTDATE, tmCurrentDateTime.tm_mday, m_strMonthNames[tmCurrentDateTime.tm_mon], tmCurrentDateTime.tm_year + 1900, "X", "X", EDFRECORDINGEQUIPMENT);
	if(riRecordingInfo.ContainsValidData)
	{
		// hospital administration code
		if(riRecordingInfo.HospitalCode != NULL)
		{
			edf_CorrectSpecialCharacters(riRecordingInfo.HospitalCode, FALSE);
			wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, riRecordingInfo.HospitalCode, sztBuffer1Byt);
			strcat_s(strBuffer2, sizeof(strBuffer2), " ");
			strcat_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
		}
		else
			strcat_s(strBuffer2, sizeof(strBuffer2), " X");

		// technician
		if(riRecordingInfo.Technician != NULL)
		{
			edf_CorrectSpecialCharacters(riRecordingInfo.Technician, FALSE);
			wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, riRecordingInfo.Technician, sztBuffer1Byt);
			strcat_s(strBuffer2, sizeof(strBuffer2), " ");
			strcat_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
		}
		else
			strcat_s(strBuffer2, sizeof(strBuffer2), " X");

		// equipment
		if(riRecordingInfo.Equipment != NULL)
		{
			edf_CorrectSpecialCharacters(riRecordingInfo.Equipment, FALSE);
			wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, riRecordingInfo.Equipment, sztBuffer1Byt);
			strcat_s(strBuffer2, sizeof(strBuffer2), " ");
			strcat_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
		}
		else
			strcat_s(strBuffer2, sizeof(strBuffer2), " X");

		// comments
		if(riRecordingInfo.Comments != NULL)
		{
			edf_CorrectSpecialCharacters(riRecordingInfo.Comments, FALSE);
			wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, riRecordingInfo.Comments, sztBuffer1Byt);
			strcat_s(strBuffer2, sizeof(strBuffer2), " ");
			strcat_s(strBuffer2, sizeof(strBuffer2), strBuffer1);
		}
	}
	edf_PadHeaderString(strBuffer2, (int) strlen(strBuffer2), EDFRECORDINGLENGTH, strBuffer2);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer2, EDFRECORDINGLENGTH);
	
	// 'startdate of recording'
	sprintf_s(strBuffer1, sztBuffer1Byt, DATEFORMAT, tmCurrentDateTime.tm_mday, tmCurrentDateTime.tm_mon + 1, tmCurrentDateTime.tm_year - 100);
	edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFSTARTDATELENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFSTARTDATELENGTH);

	// 'starttime of recording'
	sprintf_s(strBuffer1, sztBuffer1Byt, DATEFORMAT, tmCurrentDateTime.tm_hour, tmCurrentDateTime.tm_min, tmCurrentDateTime.tm_sec);
	edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFSTARTTIMELENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFSTARTTIMELENGTH);

	// 'number of bytes in header record'
	sprintf_s(strBuffer1, sztBuffer1Byt, "%d", uintHeaderSizeByt);
	edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFNUMBEROFBYTESLENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFNUMBEROFBYTESLENGTH);

	// 'reserved'
	if(riRecordingInfo.ContainsValidData && riRecordingInfo.AdditionalComments != NULL)
	{
		edf_CorrectSpecialCharacters(riRecordingInfo.AdditionalComments, FALSE);
		wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, riRecordingInfo.AdditionalComments, EDFRESERVEDLENGTH - strlen(EDFSIGNALTYPE) + 1);
		
		sprintf_s(strBuffer2, sizeof(strBuffer2), "%s %s", EDFSIGNALTYPE, strBuffer1);
	}
	else
	{
		sprintf_s(strBuffer2, sizeof(strBuffer2), "%s", EDFSIGNALTYPE);
	}	
	edf_PadHeaderString(strBuffer2, (int)strlen(strBuffer2), EDFRESERVEDLENGTH, strBuffer2);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer2, EDFRESERVEDLENGTH);

	// 'number of data records'
	if(intNDataRecords > 0)
	{
		sprintf_s(strBuffer1, sztBuffer1Byt, "%d", intNDataRecords);
	}
	else
	{
		sprintf_s(strBuffer1, sztBuffer1Byt, "%d", EDFLENGTHDURINGRECORD);
	}
	edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFNOFDATARECORDSLENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFNOFDATARECORDSLENGTH);

	// 'duration of a data record'
	sprintf_s(strBuffer1, sztBuffer1Byt, "%d", EDFDURATIONOFRECORD);
	edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFDURATIONOFDRLENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFDURATIONOFDRLENGTH);

	// 'number of signals in data record'
	sprintf_s(strBuffer1, sztBuffer1Byt, "%d", uintNSignals);
	edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFNOFSIGNALSINDRLENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFNOFSIGNALSINDRLENGTH);

	//
	// Parse Signal Header Fields
	//
	// 'label'
	for ( i = 0; i < uintNSignals; ++i )
	{
		edf_PadHeaderString(m_strChannelLabels[i], (int) strlen(m_strChannelLabels[i]), EDFLABELFORSIGNALLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFLABELFORSIGNALLENGTH);
	}

	// 'transducer type'
	for ( i = 0; i < uintNSignals; ++i )
	{
		if(i < uintNAccChannels)
		{
			edf_PadHeaderString(ACCTRANSDUCERTYPE, (int) strlen(ACCTRANSDUCERTYPE), EDFTRANSDUCERTYPELENGTH, strBuffer1);
		}
		else
		{
			if(i < (uintNEEGChannels + uintNAccChannels))
			{
				wcstombs_s(&stSize, strBuffer1, sztBuffer1Byt, pstrElectrodeType, EDFTRANSDUCERTYPELENGTH);
				edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFTRANSDUCERTYPELENGTH, strBuffer1);
			}
			else
			{
				edf_PadHeaderString(NULL, 0, EDFTRANSDUCERTYPELENGTH, strBuffer1);
			}
		}
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFTRANSDUCERTYPELENGTH);
	}

	// 'physical dimension'
	for ( i = 0; i < uintNSignals; ++i )
	{
		if(i < (uintNSignals - 1))
			edf_PadHeaderString(EDFPHYSICALDIMSIGNAL, (int) strlen(EDFPHYSICALDIMSIGNAL), EDFDIMONOFSIGNALLENGTH, strBuffer1);
		else
			edf_PadHeaderString(NULL, 0, EDFDIMONOFSIGNALLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFDIMONOFSIGNALLENGTH);
	}

	// 'physical minimum'
	for ( i = 0; i < uintNSignals; ++i )
	{
		edf_PadHeaderString(EDFPHYSMINVALUE, (int) strlen(EDFPHYSMINVALUE), EDFPHYSMINOFSIGNLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFPHYSMINOFSIGNLENGTH);
	}
	
	// 'physical maximum'
	for ( i = 0; i < uintNSignals; ++i )
	{
		edf_PadHeaderString(EDFPHYSMAXVALUE, (int) strlen(EDFPHYSMAXVALUE), EDFPHYSMAXOFSIGNLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFPHYSMAXOFSIGNLENGTH);
	}

	// 'digital minimum'
	for ( i = 0; i < uintNSignals; ++i )
	{
		if(i < (uintNSignals - 1))
			edf_PadHeaderString(EDFDIGITALMINVALUE, (int) strlen(EDFDIGITALMINVALUE), EDFDIGMINOFSIGNLENGTH, strBuffer1);
		else
			edf_PadHeaderString(EDFDIGITALMINVALUEANNOTATION, (int) strlen(EDFDIGITALMINVALUEANNOTATION), EDFDIGMINOFSIGNLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFDIGMINOFSIGNLENGTH);
	}

	// 'digital maximum'
	for ( i = 0; i < uintNSignals; ++i )
	{
		if(i < (uintNSignals - 1))
			edf_PadHeaderString(EDFDIGITALMAXVALUE, (int) strlen(EDFDIGITALMAXVALUE), EDFDIGMAXOFSIGNLENGTH, strBuffer1);
		else
			edf_PadHeaderString(EDFDIGITALMAXVALUEANNOTATION, (int) strlen(EDFDIGITALMAXVALUEANNOTATION), EDFDIGMAXOFSIGNLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFDIGMAXOFSIGNLENGTH);
	}
	
	// 'prefiltering'
	for ( i = 0; i < uintNSignals; ++i )
	{
		if(i < (uintNSignals - 1))
			edf_PadHeaderString(EDFPREFILTERSETTINGS, (int) strlen(EDFPREFILTERSETTINGS), EDFPREFILTLENGTH, strBuffer1);
		else
			edf_PadHeaderString(NULL, 0, EDFPREFILTLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFPREFILTLENGTH);
	}
	
	// 'number of samples in each data record'
	for ( i = 0; i < uintNSignals; ++i )
	{
		if(i < (uintNSignals - 1))
			sprintf_s(strBuffer1, sztBuffer1Byt, "%d", intSamplingFrequency);
		else
			sprintf_s(strBuffer1, sztBuffer1Byt, "%d", ANNOTATION_TOTAL_NCHARS/2);
		
		edf_PadHeaderString(strBuffer1, (int) strlen(strBuffer1), EDFNSAMPLESINSIGNLENGTH, strBuffer1);
		strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, EDFNSAMPLESINSIGNLENGTH);
	}
	
	// 'reserved'
	edf_PadHeaderString(NULL, 0, uintNSignals*EDFRESERVERSIGNALLENGTH, strBuffer1);
	strncat_s(HeaderBuffer, uintHeaderBufferLen, strBuffer1, uintNSignals*EDFRESERVERSIGNALLENGTH);

	//
	// free allcoated memory
	//
	free(strBuffer1);

	return TRUE;
}

/**
* \brief Initializes the members of the provided PatientIdentification structure and/or RecordingIdentification structure.

* \param piPatientInfo		pointer to PatientIdentification structure (can be NULL)
* \param riRecordingInfo	pointer to RecordingIdentification structure (can be NULL)
*
* \return 0 if succesfull, errno (generated by _stprintf_s) otherwise.
*/
int edf_InitHeaderStructures(PatientIdentification * piPatientInfo, RecordingIdentification * riRecordingInfo)
{
	size_t sztNChars, sztStringLen;

	//
	// Initialize patient information structure
	//
	if(piPatientInfo != NULL)
	{
		piPatientInfo->ContainsValidData = FALSE;
		piPatientInfo->PIC = piPatientInfo->Name = NULL;
		piPatientInfo->BirthDay = piPatientInfo->BirthMonth = piPatientInfo->BirthYear = 0;
	}

	//
	// initialize recording information structure
	//
	if(riRecordingInfo != NULL)
	{
		riRecordingInfo->ContainsValidData = FALSE;
		riRecordingInfo->AdditionalComments = NULL;
		riRecordingInfo->Comments = NULL;
		riRecordingInfo->HospitalCode = NULL;
		riRecordingInfo->Technician = NULL;
		sztStringLen = strlen(EDFRECORDINGEQUIPMENT);
		riRecordingInfo->Equipment = (TCHAR *) calloc(sztStringLen + 1, sizeof(TCHAR));				// +1 for terminating NULL character
		if(riRecordingInfo->Equipment != NULL)
		{
			mbstowcs_s(&sztNChars,
					   riRecordingInfo->Equipment, ((sztStringLen + 1)*sizeof(TCHAR))/sizeof(WORD),	// +1 for terminating NULL character
					   EDFRECORDINGEQUIPMENT, sztStringLen);
		}
		else
			return errno;
	}

	return 0;
}