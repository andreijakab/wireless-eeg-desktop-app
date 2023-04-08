/**
 * 
 * \file edfPlus.h
 * \brief This file has all the constants that are required when making EDF+ header.
 *
 * 
 * $Id: edfPlus.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __EDFPLUS_H__
# define __EDFPLUS_H__

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////
// General constants

// Format for dd.mm.yy date
# define DATEFORMAT			"%02d.%02d.%02d"	

////////////////////////////////////////////////////////////////////
// Constants for EDF+ header
#define EDFVERSION						"0       "
#define EDFUNKNOWNPATIENT				"X X X X"

// Data for 'local recording identification' field
# define EDFRECORDINGIDENTFORMAT		"%s %02d-%s-%4d %s %s %s"
# define EDFRECORDINGSTARTDATE			"Startdate"
# define EDFRECORDINGSTARTDATENCHARS	21
# define EDFRECORDINGEQUIPMENT			"WEEG4"

// Number of data records in EDF+ is -1 during the writing operation
#define EDFLENGTHDURINGRECORD			-1

#define EDFSIGNALTYPE					"EDF+C"

// Duration of data record, in seconds
#define EDFDURATIONOFRECORD				1

#define ACCTRANSDUCERTYPE				"Accelerometer"

// Unit in which the signal is measured
#define EDFPHYSICALDIMSIGNAL			"uV"

// Minimum and maximum physical voltage values that signal can have
#define EDFPHYSMINVALUE					"-790"
#define EDFPHYSMAXVALUE					"790"

// Minimum and maximum digital values (in 16bit signal)
#define EDFDIGITALMAXVALUE				"32767"
#define EDFDIGITALMINVALUE				"-32768"
#define EDFDIGITALMAXVALUEANNOTATION	"32767"
#define EDFDIGITALMINVALUEANNOTATION	"-32768"

// Prefiltering settings of all signals
#define EDFPREFILTERSETTINGS			"BP:0.2-120Hz"

// Length of EDF+ file header fields
#define EDFFILEHEADERLENGTH		256				// # of bytes in the EDF(+) file header
#define EDFVERSIONLENGTH		8				// # of bytes in the 'version of this data format' field
#define EDFPATIENTLENGTH		80				// # of bytes in the 'local patient identification' field
#define EDFRECORDINGLENGTH		80				// # of bytes in the 'local recording identification' field
#define EDFSTARTDATELENGTH		8				// # of bytes in the 'startdate of recording' field
#define EDFSTARTTIMELENGTH		8				// # of bytes in the 'starttime of recording' field
#define EDFNUMBEROFBYTESLENGTH	8				// # of bytes in the 'number of bytes in header record' field
#define EDFRESERVEDLENGTH		44				// # of bytes in the 'reserved' field
#define EDFNOFDATARECORDSLENGTH	8				// # of bytes in the 'number of data records' field
#define EDFDURATIONOFDRLENGTH	8				// # of bytes in the 'duration of a data record' field
#define EDFNOFSIGNALSINDRLENGTH 4				// # of bytes in the 'number of signals in data record' field

// Space that is available in the EDF+ field that can be used by the application
#define EDFPATIENTSPACE			63				// # of bytes in the 'local patient identification' field that can be used by the application (i.e. by removing # of chars for 'X M 15-JAN-2001 X')
#define EDFRECORDINGSPACE		52				// # of bytes in the 'local recording identification' field that can be used by the application (i.e. by removing # of chars for 'Startdate 15-APR-2008 X X X ')
#define EDFRESERVEDSPACE		38				// # of bytes in the 'reserved' field that can be used by the application (i.e. by removing # of chars for 'EDF+C ')

// Length of EDF+ signal header fields
#define EDFSIGNALHEADERLENGTH	256				// # of bytes in the EDF(+) signal header
#define EDFLABELFORSIGNALLENGTH	16				// # of bytes in the 'label' field
#define EDFTRANSDUCERTYPELENGTH	80				// # of bytes in the 'transducer type' field
#define EDFDIMONOFSIGNALLENGTH	8				// # of bytes in the 'physical dimension' field
#define EDFPHYSMINOFSIGNLENGTH	8				// # of bytes in the 'physical minimum' field
#define EDFPHYSMAXOFSIGNLENGTH	8				// # of bytes in the 'physical maximum' field
#define EDFDIGMINOFSIGNLENGTH	8				// # of bytes in the 'digital minimum' field
#define EDFDIGMAXOFSIGNLENGTH	8				// # of bytes in the 'digital maximum' field
#define EDFPREFILTLENGTH		80				// # of bytes in the 'prefiltering' field
#define EDFNSAMPLESINSIGNLENGTH	8				// # of bytes in the 'nr of samples in each data record' fielD (Number of samples in data record for each signal separately)
#define EDFRESERVERSIGNALLENGTH	32				// # of bytes in the 'reserved' field

// Patient Identification fields
# define PIC_SIZE			11
# define BDATE_SIZE			11
# define NAME_SIZE			53

//---------------------------------------------------------------------------
//   								Enums/Structs
//---------------------------------------------------------------------------
// For storing patient information
typedef struct
{
	BOOL	ContainsValidData;
	TCHAR * PIC;
	TCHAR	Sex;
	short	BirthDay;
	short	BirthMonth;
	short	BirthYear;
	TCHAR *	Name;
}
PatientIdentification;

// For storing recording information
typedef struct
{
	BOOL	ContainsValidData;
	TCHAR *	HospitalCode;
	TCHAR *	Technician;
	TCHAR * Equipment;
	TCHAR * Comments;
	TCHAR * AdditionalComments;
}RecordingIdentification;

typedef struct												// EDF File Header
{
	char Version[EDFVERSIONLENGTH + 1];						// version of this data format
	char PatientIdentification[EDFPATIENTLENGTH + 1];		// local patient identification
	char RecordingInformation[EDFRECORDINGLENGTH + 1];		// local recording identification
	struct tm StartDateTime;								// startdate and starttime of recording
	int NBytesHeader;										// number of bytes in header record
	char Reserved[EDFRESERVEDLENGTH + 1];					// reserved
	int NDataRecords;										// number of data records (-1 if unknown)
	int NSecsPerDataRecord;									// duration of a data record, in seconds
	int NSignalsPerDataRecord;								// number of signals (ns) in data record
}EDFFileHeader;

typedef struct												// EDF Signal Header
{
	char Label[EDFLABELFORSIGNALLENGTH + 1];				// signal label
	char TransducerType[EDFTRANSDUCERTYPELENGTH + 1];		// transducer type
	char MeasurementUnit[EDFDIMONOFSIGNALLENGTH + 1];		// physical dimension
	int PhysicalMinimum;									// physical minimum
	int PhysicalMaximum;									// physical maximum
	int DigitalMinimum;										// digital minimum
	int DigitalMaximum;										// digital maximum
	char Prefiltering[EDFPREFILTLENGTH + 1];				// prefiltering
	int NSamplesPerDataRecord;								// nr. of samples in each data record
	char Reserved[EDFRESERVERSIGNALLENGTH + 1];				// reserved
}EDFSignalHeader;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
/**
* \brief Calculate the size of the EDF+ file header record.
*
* \param	uintNSignals	number of signals in one data record
*
* \return The size of the EDF+ file header record, in bytes, if it is smaller than the maimum unsigned short value and 0 otherwise.
*/
unsigned short edf_CalculateEDFplusHeaderRecord(unsigned int uintNSignals);

/**
* \brief Convert scandinavian characters into ASCII characters. If string is not an annotation, spaces are converte to '_' for EDF+ compliance.
*
* \param strString			string with characters to be converted
* \param blnIsAnnotation	indicates whether or not the provided string is an annotation
*/
void			edf_CorrectSpecialCharacters(TCHAR * strString, BOOL blnIsAnnotation);

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
									 TCHAR * pstrElectrodeType, char * HeaderBuffer, unsigned int uintHeaderBufferLen);

/**
* \brief Initializes the members of the provided PatientIdentification structure and/or RecordingIdentification structure.
*
* \param piPatientInfo		pointer to PatientIdentification structure (can be NULL)
* \param riRecordingInfo	pointer to RecordingIdentification structure (can be NULL)
*
* \return 0 if succesfull, errno (generated by _stprintf_s) otherwise.
*/
int				edf_InitHeaderStructures(PatientIdentification * piPatientInfo, RecordingIdentification * riRecordingInfo);

/*unsigned int	edf_GetDataRecord(HANDLE hEDFPlusFile, EDFFileHeader ehFileHeader, EDFSignalHeader * edfSignalHeaders, short ** shrpDataSamples, int intDataRecordIndex);
void			edf_ReadFileHeader(HANDLE hEDFPlusFile, EDFFileHeader *ehFileHeader);
void			edf_ReadSignalHeaders(HANDLE hEDFPlusFile, EDFFileHeader ehFileHeader, EDFSignalHeader *edfSignalHeaders);*/

#endif