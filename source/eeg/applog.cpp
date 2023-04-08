/**
 * 
 * \file applog.cpp
 * \brief Module containing all the functions of the application logger module.
 *
 * 
 * $Id: applog.cpp 76 2013-02-14 14:26:17Z jakab $
 */
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER			0x0502									// application requires at least Windows XP SP2
#define _WIN32_WINNT	0x0502									// application requires at least Windows XP SP2
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//----------------------------------------------------------------------------------------------------------
//									Libraries
//----------------------------------------------------------------------------------------------------------
// custom
#pragma comment(lib, "libxml2_a.lib")

//----------------------------------------------------------------------------------------------------------
//   								Includes
//----------------------------------------------------------------------------------------------------------
// Windows libaries
# include <windows.h>

// CRT libraries
# include <stdio.h>
# include <tchar.h>
# include <time.h>

// custom libraries
# include <libxml/xmlwriter.h>

// program headers
# include "globals.h"
# include "util.h"
# include "applog.h"

//----------------------------------------------------------------------------------------------------------
//   								Definitions
//----------------------------------------------------------------------------------------------------------
// libxml2 XML file encoding (NULL = default)
# define APPLOG_FILE_ENCODING		NULL

// date/time format used for parsing the log file name
# define APPLOG_DATETIME_FORMAT		TEXT("%04d%02d%02d%02d%02d%02d")
# define APPLOG_DATETIME_FORMAT_LEN	14

// default filename
# define APPLOG_FILENAME_DEFAULT	TEXT("log")
# define APPLOG_FILENAME_EXT		TEXT(".xml")

//----------------------------------------------------------------------------------------------------------
//   								Constants
//----------------------------------------------------------------------------------------------------------
const char * mc_strEventElements[] = {"General", "Version", "Error-Software", "Error-Hardware"};
const char * mc_TimestampFormat = "%02d:%02d:%02d %02d-%02d-%04d";

//----------------------------------------------------------------------------------------------------------
//   								Global variables
//----------------------------------------------------------------------------------------------------------
static BOOL				m_blnLogInitialized = FALSE;
static HANDLE			m_hSerializationMutex;
static xmlTextWriterPtr	m_pXMLWriter;

//----------------------------------------------------------------------------------------------------------
//   								Globally-accessible functions
//----------------------------------------------------------------------------------------------------------
void applog_init(TCHAR * strPrefix, TCHAR * strLogFilePath, TCHAR * strLogFileNameBuf, unsigned int uintLogFileNameBufLen)
{
	BOOL blnReturnLogFileName = FALSE, blnUsePrefix = FALSE;
	char * pstrURI = NULL;
	DWORD dwrdReturnCode;
	int intReturnCode;
	size_t sztNCharsURI, sztNConverted;
	struct tm tmDateTime;
	TCHAR strRandomChars[4], strDateTime[APPLOG_DATETIME_FORMAT_LEN + 1];
	TCHAR * pwstrURI = NULL;

	// this initialize the library and check potential API mismatches
    // between the version it was compiled for and the actual shared
    // library used
	LIBXML_TEST_VERSION

	//
	// check inputs
	//
	if(strLogFilePath == NULL || _tcslen(strLogFilePath) == 0)
		return;

	if(strPrefix != NULL && _tcslen(strPrefix) > 0)
		blnUsePrefix = TRUE;
	
	if(strLogFileNameBuf != NULL && uintLogFileNameBufLen != MAX_PATH)
	{
		blnReturnLogFileName = TRUE;
#ifdef _DEBUG
		// make sure buffer is actually as long as advertised
		memset(strLogFileNameBuf, 0xAE, uintLogFileNameBufLen*sizeof(TCHAR));
#endif
	}

	//
	// parse URI
	//
	// get current date/time
	tmDateTime = util_GetCurrentDateTime();
	_stprintf_s(strDateTime,
				APPLOG_DATETIME_FORMAT_LEN + 1,
				APPLOG_DATETIME_FORMAT,
				tmDateTime.tm_year + 1900, tmDateTime.tm_mon + 1, tmDateTime.tm_mday, tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec);

	// get some random characters
	dwrdReturnCode = util_GetRandomPrefix(strRandomChars, sizeof(strRandomChars)/sizeof(TCHAR));

	// compute # of chars in URI
	sztNCharsURI = _tcslen(strLogFilePath);
	if(blnUsePrefix) sztNCharsURI += _tcslen(strPrefix);
	sztNCharsURI += _tcslen(APPLOG_FILENAME_DEFAULT) + APPLOG_DATETIME_FORMAT_LEN;
	if(dwrdReturnCode == 0) sztNCharsURI += _tcslen(strRandomChars) + 1;			// +1 for '-'
	sztNCharsURI += _tcslen(APPLOG_FILENAME_EXT) + 3;								// +1 for '\', +1 for '-', +1 for '\0'

	// allocate memory for URI and parse name
	pwstrURI = (TCHAR *) calloc(sztNCharsURI, sizeof(TCHAR));
	if(pwstrURI == NULL)
		return;

	if(blnUsePrefix)
	{
		if(dwrdReturnCode == 0)
		{
			_stprintf_s(pwstrURI,
						sztNCharsURI,
						TEXT("%s\\%s%s-%s-%s%s"),
						strLogFilePath, strPrefix, APPLOG_FILENAME_DEFAULT, strDateTime, strRandomChars, APPLOG_FILENAME_EXT);
		}
		else
		{
			_stprintf_s(pwstrURI,
						sztNCharsURI,
						TEXT("%s\\%s%s-%s%s"),
						strLogFilePath, strPrefix, APPLOG_FILENAME_DEFAULT, strDateTime, APPLOG_FILENAME_EXT);
		}
	}
	else
	{
		if(dwrdReturnCode == 0)
		{
			_stprintf_s(pwstrURI,
						sztNCharsURI,
						TEXT("%s\\%s-%s-%s%s"),
						strLogFilePath, APPLOG_FILENAME_DEFAULT, strDateTime, strRandomChars, APPLOG_FILENAME_EXT);
		}
		else
		{
			_stprintf_s(pwstrURI,
						sztNCharsURI,
						TEXT("%s\\%s-%s%s"),
						strLogFilePath, APPLOG_FILENAME_DEFAULT, strDateTime, APPLOG_FILENAME_EXT);
		}
	}
	
	// convert URI from TCHAR to char
	pstrURI = (char *) calloc(sztNCharsURI, sizeof(char));
	if(pstrURI == NULL)
	{
		free(pwstrURI);

		return;
	}

	wcstombs_s(&sztNConverted, pstrURI, sztNCharsURI, pwstrURI, sztNCharsURI);
	if(sztNConverted != sztNCharsURI)
	{
		free(pwstrURI);
		free(pstrURI);

		return;
	}
	
	//
	// initialize log file
	//
    // create new XmlWriter for URI, with no compression
    m_pXMLWriter = xmlNewTextWriterFilename(pstrURI, 0);
    if (m_pXMLWriter == NULL)
	{
		free(pwstrURI);
		free(pstrURI);

        return;
	}

    // create document with the xml default for the version, default encoding and default for the standalone declaration
    intReturnCode = xmlTextWriterStartDocument(m_pXMLWriter, NULL, APPLOG_FILE_ENCODING, NULL);
    if (intReturnCode < 0)
	{
		xmlFreeTextWriter(m_pXMLWriter);
		xmlCleanupParser();
		free(pwstrURI);
		free(pstrURI);

        return;
	}

	// write root element
	intReturnCode = xmlTextWriterStartElement(m_pXMLWriter, BAD_CAST "LOG");
    if (intReturnCode < 0)
	{
		xmlFreeTextWriter(m_pXMLWriter);
		xmlCleanupParser();
		free(pwstrURI);
		free(pstrURI);

        return;
	}

	//
	// misc. init
	//
	// initialize mutex 
	m_hSerializationMutex =  CreateMutex(NULL,              // default security attributes
										 FALSE,             // initially not owned
										 NULL);             // unnamed mutex
    if (m_hSerializationMutex == NULL) 
    {
		// log failure
		applog_logevent(SoftwareError, TEXT("AppLog"), TEXT("applog_init() - mutex creation failed."), GetLastError(), TRUE);
		
		// clean up
		xmlTextWriterEndDocument(m_pXMLWriter);
		xmlFreeTextWriter(m_pXMLWriter);
		xmlCleanupParser();
		free(pwstrURI);
		free(pstrURI);
        
		return;
    }

	// if we got to this point, application log module is ready to be used
	m_blnLogInitialized = TRUE;
	
	// log version of libxml2
	applog_logevent(Version, TEXT("libxml2"), TEXT(LIBXML_DOTTED_VERSION), 0, FALSE);
	
	//
	// clean up
	//
	// free allocated memory
	free(pstrURI);
	free(pwstrURI);
}

void applog_logevent(LogEventType letType, TCHAR * pstrModule, TCHAR * pstrMessage, int intCode, BOOL blnAddTimestamp)
{
	char * pstrBuffer;
	int intReturnCode;
	size_t szLength, szConverted;
	struct tm tmDateTime;
	
	// acquire serialization mutex
	WaitForSingleObject(m_hSerializationMutex, INFINITE);

	if(m_blnLogInitialized)
	{
		// write appropriate starting element according to what kind of event is being logged
		intReturnCode = xmlTextWriterStartElement(m_pXMLWriter, BAD_CAST mc_strEventElements[letType]);
		if (intReturnCode < 0)
			return;
		
		// write timestamp (if requested)
		if(blnAddTimestamp)
		{
			tmDateTime = util_GetCurrentDateTime();
			xmlTextWriterWriteFormatAttribute(m_pXMLWriter,
											  BAD_CAST "timestamp",
											  mc_TimestampFormat,
											  tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, tmDateTime.tm_mday, tmDateTime.tm_mon + 1, tmDateTime.tm_year + 1900);
		}

		// write module information
		if(pstrModule != NULL)
		{
			szLength = _tcslen(pstrModule) + 1;
			pstrBuffer = (char *) calloc(szLength, sizeof(char));
			wcstombs_s(&szConverted, pstrBuffer, szLength, pstrModule, szLength);

			xmlTextWriterWriteElement(m_pXMLWriter, BAD_CAST "Module", BAD_CAST pstrBuffer);

			free(pstrBuffer);
		}

		// write message
		if(pstrMessage != NULL)
		{
			szLength = _tcslen(pstrMessage) + 1;
			pstrBuffer = (char *) calloc(szLength, sizeof(char));
			wcstombs_s(&szConverted, pstrBuffer, szLength, pstrMessage, szLength);

			xmlTextWriterWriteElement(m_pXMLWriter, BAD_CAST "Message", BAD_CAST pstrBuffer);

			free(pstrBuffer);
		}
		
		// write message code
		xmlTextWriterWriteFormatElement(m_pXMLWriter, BAD_CAST "Code", "%d", intCode);

		// write appropriate ending element according to what kind of event is being logged
	    xmlTextWriterEndElement(m_pXMLWriter);
	}

	// release serialization mutex
	ReleaseMutex(m_hSerializationMutex);
}

void applog_startgrouping(TCHAR * pstrGroupName, BOOL blnAddTimestamp)
{
	char * pstrBuffer;
	size_t szLength, szConverted;
	struct tm tmDateTime;

	// acquire serialization mutex
	WaitForSingleObject(m_hSerializationMutex, INFINITE);

	if(m_blnLogInitialized)
	{
		if(pstrGroupName == NULL || _tcslen(pstrGroupName) == 0)
			return;

		szLength = _tcslen(pstrGroupName) + 1;
		pstrBuffer = (char *) calloc(szLength, sizeof(char));
		wcstombs_s(&szConverted, pstrBuffer, szLength, pstrGroupName, szLength);

		// write appropriate starting element according to what kind of event is being logged
		xmlTextWriterStartElement(m_pXMLWriter, BAD_CAST pstrBuffer);

		free(pstrBuffer);
		
		if(blnAddTimestamp)
		{
			tmDateTime = util_GetCurrentDateTime();
			xmlTextWriterWriteFormatAttribute(m_pXMLWriter,
											  BAD_CAST "timestamp",
											  mc_TimestampFormat,
											  tmDateTime.tm_hour, tmDateTime.tm_min, tmDateTime.tm_sec, tmDateTime.tm_mday, tmDateTime.tm_mon + 1, tmDateTime.tm_year + 1900);
		}
	}
	
	// release serialization mutex
	ReleaseMutex(m_hSerializationMutex);
}

void applog_endgrouping(void)
{
	// acquire serialization mutex
	WaitForSingleObject(m_hSerializationMutex, INFINITE);
	
	if(m_blnLogInitialized)
	{
		// write appropriate ending element
	    xmlTextWriterEndElement(m_pXMLWriter);
	}

	// release serialization mutex
	ReleaseMutex(m_hSerializationMutex);
}

void applog_close(void)
{
	if(m_blnLogInitialized)
	{
		// close elements that are still open and end document
		xmlTextWriterEndDocument(m_pXMLWriter);

		xmlFreeTextWriter(m_pXMLWriter);
    
		// cleanup function for the XML library.
		xmlCleanupParser();
	}
}