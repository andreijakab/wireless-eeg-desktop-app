/**
 * \file		thread_storage.cpp
 * \since		23.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 * \version		1.0.0
 *
 * \brief		???
 *
 * $Id: thread_storage.cpp 78 2013-02-21 17:23:21Z jakab $
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
// Windows libraries
#pragma comment(lib, "shlwapi.lib")

//---------------------------------------------------------------------------
//   							Includes
//------------------------------------------------------------ ---------------
// Windows libaries
#include <windows.h>
#include <Shlwapi.h>

// CRT libraries
#include <time.h>

// custom libraries
#include <eegem_beep.h>

// program headers
#include "globals.h"
#include "applog.h"
#include "edfPlus.h"
#include "linkedlist.h"
#include "thread_stream.h"
#include "util.h"
#include "thread_storage.h"

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define IOPACKETTIMEOUT			50								///< number of milliseconds to wait for a timeout packet
# define TEMPFILECOMPLETIONKEY		2155486							///< per-handle user-defined completion key that is included in every I/O completion packet for the specified file handle (see Remarks section of CreateIoCompletionPort MSDN page)
# define MIN_AVAILABLE_DISK_SPACE	400000000						///< Minimum amount of bytes that must be available before recording can begin
# define FILEPREFIX					TEXT("EEGEM")
# define NWRITEOPERATIONS_MAX		20								///< maximum number of records to store in one call to the Storage_WriteRecords() function (ensures that main FSM won't remain stuck in the Write state for too long a time)
# define NCLEANUPATTEMPTS_MAX		5								///< maximum number of times that the Storage_CleanUp() function will attempt to store records

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
# pragma pack(push)  // push current alignment to stack
# pragma pack(1)     // set alignment to 1 byte boundary
/**
 * EEGEM EDF Data Record.
 */
typedef struct
{
	BYTE *						WriteBuffer;						///< contains all of the signals of the EEGEM EDF+ data record, as they will be written to the EDF+ file
	unsigned int				WriteBufferLen;						///< size of WriteBuffer, in bytes
	OVERLAPPED *				pOverlapped;						///< contains information needed for asynchronous writing of temporary EDF+ file
} StorageDataRecord;
# pragma pack (pop)	// restore original alignment from stack

/**
 * Contains the properties of the EDF+ file being stored currently.
 */
typedef struct
{
	unsigned int				HeaderRecordSize;					///< length of the EDF+ header record, in bytes
	unsigned int				DataRecordSize;						///< length of a EDF+ data record, in bytes
	unsigned int				SamplingFrequency;					///< frequency at which the EEG and acceleration signals are sampled
	unsigned int				NDataRecords;						///< counter that tracks number of data records that have been added to the write-pending linked list (i.e., total amount of recors that have been written to the file, including those for which I/O completion packets are pending)
} EDFFileProperties;

//---------------------------------------------------------------------------
//							Global variables
//---------------------------------------------------------------------------
static EDFFileProperties		m_EDFFileProperties;				///< stores some properties of the EDF+ file that are needed by the storage thread
static HANDLE					m_hEDFTempFile;						///< handle to the temporary EDF+ file
static HANDLE					m_hIOCP;							///< handle for the I/O completion port associated with the temporary EDF+ file
static linkedlist *				m_pllIOCompletionPending;			///< linked list for records that have been written but for which the I/O completion packet hasn't been received
static linkedlist *				m_pllWritePending;					///< linked list for records that are to be written
static StorageThreadState		m_stsStorageThreadState;			///< keeps track of the main FSM's state
static TCHAR					m_strTempEDFFilePath[MAX_PATH + 1];	///< NULL-terminated string that stores the full path of the temporary EDF+ file

//---------------------------------------------------------------------------
//							Internally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief Free all of the memory resources allocated to a StorageDataRecord structure.
 *
 * \param[in]	pdrDataRecord	pointer to the StorageDataRecord the memory resources of which are to be released
 */
static void Storage_FreeDataRecord(StorageDataRecord * pdrDataRecord)
{
	if(pdrDataRecord != NULL)
	{
		// allocate memory for write buffer
		if(pdrDataRecord->WriteBuffer != NULL)
			free(pdrDataRecord->WriteBuffer);

		// asynchronous IO structures
		if(pdrDataRecord->pOverlapped != NULL)
			free(pdrDataRecord->pOverlapped);

		free(pdrDataRecord);
	}
}

/**
 * \brief Generates a new variable of the StorageDataRecord type and initializes its memebers.
 *
 * \param[in]	blnHeaderRecord			TRUE if pdrDataRecord will be used to store a header record, FALSE otherwise
 *
 * \return Pointer to the initialized variable if succesfull, NULL otherwise.
 */
StorageDataRecord * Storage_InitDataRecord(BOOL blnHeaderRecord)
{
	BOOL					blnResult = TRUE;
	StorageDataRecord *		pdrDataRecord;
	
	//
	// allocate memory for structure
	//
	pdrDataRecord = (StorageDataRecord *) malloc(sizeof(StorageDataRecord));
	if(pdrDataRecord == NULL)
	{
		applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_InitDataRecord(): Failed to allocate memory for the StorageDataRecord structure. (errno #)"), errno, TRUE);
		blnResult = FALSE;
	}

	//
	// allocate memory for write buffer
	//
	if(blnResult)
	{
		if(blnHeaderRecord)
			pdrDataRecord->WriteBufferLen = m_EDFFileProperties.HeaderRecordSize;
		else
			pdrDataRecord->WriteBufferLen = m_EDFFileProperties.DataRecordSize;
		pdrDataRecord->WriteBuffer = (BYTE *) malloc(pdrDataRecord->WriteBufferLen);
		if(pdrDataRecord->WriteBuffer == NULL)
		{
			applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_InitDataRecord(): Failed to allocate memory for the WriteBuffer member of the StorageDataRecord structure. (errno #)"), errno, TRUE);
			blnResult = FALSE;
		}
		else
			SecureZeroMemory (pdrDataRecord->WriteBuffer, pdrDataRecord->WriteBufferLen);
	}

	//
	// initialize overlapped structure
	//
	if(blnResult)
	{
		pdrDataRecord->pOverlapped = (OVERLAPPED *) malloc(sizeof(OVERLAPPED));
		if(pdrDataRecord->pOverlapped == NULL)
		{
			applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_InitDataRecord(): Failed to allocate memory for the OVERLAPPED member of the StorageDataRecord structure. (errno #)"), errno, TRUE);
			blnResult = FALSE;
		}
		else
		{
			// initialize members of OVERLAPPED structure
			if(blnHeaderRecord)
			{
				// header records always go at the beginning of the EDF+ file
				pdrDataRecord->pOverlapped->Offset = 0;
			}
			else
			{
				// compute offset of data record based on the data that has already been written to the EDF+ file
				pdrDataRecord->pOverlapped->Offset = m_EDFFileProperties.HeaderRecordSize + m_EDFFileProperties.NDataRecords*m_EDFFileProperties.DataRecordSize;
			}
			
			// 'OffsetHigh' is not used since it is assumed that all EEGEM EDF+ files will be less than 4,294,967,295 bytes
			pdrDataRecord->pOverlapped->OffsetHigh = 0;

			// completion of I/O operations will be checked by polling so no event is required
			pdrDataRecord->pOverlapped->hEvent = NULL;
		}
	}

	if(blnResult)
		return pdrDataRecord;
	else
		return FALSE;
}

/**
 * \brief Handle I/O completion packets available at the compleiton port.
 *
 * \param[in]	dwrdTimeout		amount of time to wait for a completion packet to appear at the completion port, in milliseconds
 *
 * \return TRUE if function completes successfully, FALSE otherwise.
 */
static BOOL Storage_ProcessIOPackets(DWORD dwrdTimeout)
{
	BOOL					blnIOSuccess;			///< return value of the GetQueuedCompletionStatus() function
	BOOL					blnResult = TRUE;		///< stores function return value
	DWORD					dwrdNBytesTransferred;	///< variable that receives the number of bytes transferred during an I/O operation that has completed
	linkedlist_item *		pli;					///< stores nodes while traversing 'I/O completion pending' linked list
	OVERLAPPED *			pop;					///< variable that receives the address of the OVERLAPPED structure that was specified when the completed I/O operation was started
	StorageDataRecord *		pdrCurrentDataRecord;	///< stores data element of a given node while traversing 'I/O completion pending' linked list
	unsigned long			ulngCompletionKey;		///< variable that receives the completion key value associated with the file handle whose I/O operation has completed

	while(1)
	{
		// attempt to unqueue an I/O completion packet
		blnIOSuccess = GetQueuedCompletionStatus(m_hIOCP,					// handle to the I/O completion port
												 &dwrdNBytesTransferred,	// pointer to a variable that receives the number of bytes transferred during an I/O operation that has completed
												 &ulngCompletionKey,		// pointer to a variable that receives the completion key value associated with the file handle whose I/O operation has completed
												 (LPOVERLAPPED *) &pop,		// pointer to a variable that receives the address of the OVERLAPPED structure that was specified when the completed I/O operation was started
												 dwrdTimeout);				// number of milliseconds that the caller is willing to wait for a completion packet to appear at the completion port

		if(pop != NULL)
		{
			//
			// traverse 'I/O completion pending' linked list looking for record associated with the dequeued I/O completion packet
			//
			pli = m_pllIOCompletionPending->head;
			while(pli != NULL)
			{
				pdrCurrentDataRecord = (StorageDataRecord *) pli->value;
				if(pdrCurrentDataRecord->pOverlapped == pop)
				{
					// associated StorageDataRecord found: remove from linked list and break out of loop
					linkedlist_remove_element(m_pllIOCompletionPending, pli);
					break;
				}
				else
				{
					// associated StorageDataRecord found: cotinue traversing
					pli = pli->next;
				}
			}

			if(pli != NULL)
			{
				if(blnIOSuccess)
				{
					// completion packet represents a successfull I/O operation, free resources associated with storage data record structure
					Storage_FreeDataRecord(pdrCurrentDataRecord);
				}
				else
				{
					//
					// completion packet represents a failed I/O operation
					//
					// log event
					applog_logevent(General, TEXT("Storage"), TEXT("Storage_ProcessIOPackets(): Failed completion packet dequeued from completion port. (GetLastError() #)"), GetLastError(), TRUE);

					// add record to the 'write pending' linked list
					if(!linkedlist_add_element(m_pllWritePending, pdrCurrentDataRecord))
					{
						applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_ProcessIOPackets(): Failed to add record back to the write pending linked list."), 0, TRUE);
						blnResult = FALSE;
					}
				}
			}
			else
			{
				applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_ProcessIOPackets(): Unable to find the StorageDataRecord associated with the current completion packet."), 0, TRUE);
				blnResult = FALSE;
				break;
			}
		}
		else
		{
			// No completion packet dequeued from completion port
			//applog_logevent(General, TEXT("Storage"), TEXT("Storage_ProcessIOPackets(): No completion packet dequeued from completion port."), 0, TRUE);
			break;
		}
	}

	return blnResult;
}

/**
 * \brief Asynchronously write records present in the write pending linked list (up to MAX_NRECORDS_STORE are stored in one call).
 *
 * \param[in]	pstd		pointer to struct containing data passed to the storage thread by the main thread
 *
 * \return TRUE if function completes successfully, FALSE otherwise.
 */
static BOOL Storage_WriteRecords(void)
{
	BOOL					blnSuccess = TRUE;			///< stores function return value
	StorageDataRecord *		pdrCurrentDataRecord;		///< 
	unsigned int			uintNWriteOperations = 0;	///< amount of records for which the WriteFile operation completed succsfully
		
	do
	{
		// get one element from write pending linked list
		pdrCurrentDataRecord = (StorageDataRecord *) linkedlist_remove_headelement(m_pllWritePending);
		if(pdrCurrentDataRecord != NULL)
		{
			// write it in the temporary EDF+ file
			if(!WriteFile (m_hEDFTempFile,
						   pdrCurrentDataRecord->WriteBuffer,
						   pdrCurrentDataRecord->WriteBufferLen,
						   NULL,
						   pdrCurrentDataRecord->pOverlapped) && GetLastError() != ERROR_IO_PENDING)
			{
				// log event
				applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_WriteDataRecords: WriteFile() failed. CancelIo() called. (GetLastError #)"), GetLastError(), TRUE);

				// if the WriteFile function has failed and the error code is not I/O pending,
				// there may be too many outstanding asynchronous I/O requests (or some other exotic error may have occured);
				// cancel all pending I/O operations and don't attempt to write any more data for now
				CancelIo(m_hEDFTempFile);

				// add pdrCurrentDataRecord back to write pending liked list so that additional attempts can be made to write it
				if(!linkedlist_add_element(m_pllWritePending, pdrCurrentDataRecord))
				{
					applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_WriteRecords(): Unable to add record back to the write pending linked list."), 0, TRUE);
					blnSuccess = FALSE;
				}
								
				break;
			}
			else
			{
				// if call to write function is successfull, add record to 'I/O completion pending' linked list
				if(!linkedlist_add_element(m_pllIOCompletionPending, pdrCurrentDataRecord))
				{
					applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_WriteRecords(): Unable to add record to the I/O completion pending linked list."), 0, TRUE);
					blnSuccess = FALSE;
				}
			}

			// increase 'amount of write operations' counter
			uintNWriteOperations++;
		}
		else
			break;
	} while(m_pllWritePending->count > 0 && uintNWriteOperations < NWRITEOPERATIONS_MAX);

	return blnSuccess;
}

/**
 * \brief Attempts to write all outstanding records.
 *
 * \return TRUE if function completes successfully, FALSE otherwise.
 */
static BOOL Storage_CleanUp(void)
{
	unsigned int	uintNTries = 0;
	
	while((m_pllWritePending->count != 0 || m_pllIOCompletionPending->count != 0) && uintNTries < NCLEANUPATTEMPTS_MAX)
	{
		// write remaining records
		if(m_pllWritePending->count > 0)
			Storage_WriteRecords();
	
		// flushes the buffers of the temporary file and causes all buffered data to be written to the disk
		FlushFileBuffers(m_hEDFTempFile);

		// handle I/O completion packets
		if(m_pllIOCompletionPending->count > 0)
			Storage_ProcessIOPackets((uintNTries + 1)*IOPACKETTIMEOUT);

		uintNTries++;
	}
	
	// check if function was succesfull
	if(m_pllWritePending->count == 0 && m_pllIOCompletionPending->count == 0)
		return TRUE;
	else
		return FALSE;
}

//---------------------------------------------------------------------------
//								Globally-accessible functions
//---------------------------------------------------------------------------
 /**
 * \brief Adds record to the write queue of the storage thread. Function executes in the execution context of the calling thread.
 *
 * \param[in]	bytRecord				pointer to BYTE array that stores the record to be stored in the EDF+ file
 * \param[in]	uintRecordLen			size of bytRecord array, in bytes
 * \param[in]	blnHeaderRecord			TRUE if bytRecord is a header record, FALSE otherwise
 * \param[in]	hEvent					event to be signaled once record has been added to linked list
 */
BOOL Storage_AddToQueue(BYTE * bytRecord, unsigned int uintRecordLen, BOOL blnHeaderRecord, HANDLE hEvent)
{
	BOOL blnResult;
	StorageDataRecord * psdr;
	
	if(m_stsStorageThreadState == STS_Write || m_stsStorageThreadState == STS_ProcessIOCompletionPackets)
	{
		// create and initialize StorageDataRecord variable for record
		psdr = Storage_InitDataRecord(blnHeaderRecord);
		if(psdr != NULL)
		{
			// copy record to the StorageDataRecord variable
			memcpy_s(psdr->WriteBuffer, psdr->WriteBufferLen,
					 bytRecord, uintRecordLen);

			// if record to be stored is not a header record, increase 'number of data records in EDF+ file' counter
			if(!blnHeaderRecord)
				m_EDFFileProperties.NDataRecords++;

			// add to write pending linked list
			if(linkedlist_add_element(m_pllWritePending, psdr) == NULL)
			{
				Storage_FreeDataRecord(psdr);
				applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_AddToQueue(): Failed to add record to the write pending linked list."), 0, TRUE);
				blnResult = FALSE;
			}
			else
			{
				// signal storage thread
				SetEvent(hEvent);

				blnResult = TRUE;
			}
		}
		else
		{
			applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_AddToQueue(): Failed to initialize StorageDataRecord structure."), 0, TRUE);
			blnResult = FALSE;
		}
	}
	else
	{
		applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_AddToQueue(): Failed to add record to write queue since Storage thread is not in the correct state. (StorageThreadState #)"), m_stsStorageThreadState, TRUE);
		blnResult = FALSE;
	}

	return blnResult;
}

/**
 * \brief Returns the current state of the storage client's main FSM.
 *
 * \return	A member of the StorageThreadState enumeration.
 */
 StorageThreadState Storage_GetMainFSMState(void)
 {
	 return m_stsStorageThreadState;
 }

/**
 * \brief Get full path of the temporary EDF+ file.
 *
 * This function can be called even after the Storage thread's main FSM has entered the STS_Idle state since
 * the variable that stores the temporary EDF+ file path is reset at the beginning of the STS_Init state.
 *
 * \param[in]	strBuffer		pointer to buffer where the path should be stored
 * \param[in]	uintBufferLen	size of strBuffer, in TCHARs
 *
 * \return	TRUE if the function completes successfully and strBuffer contains the file path, FALSE otherwise.
 */
 BOOL Storage_GetTemporaryEDFFilePath(TCHAR * strBuffer, unsigned int uintBufferLen)
 {
	 size_t sztPathLength = _tcslen(m_strTempEDFFilePath);

#ifdef _DEBUG
	// make sure buffer is actually as long as advertised
	memset(strBuffer, 0xAE, uintBufferLen*sizeof(TCHAR));
#endif
	
	if(sztPathLength == 0)
	{
		applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_GetTemporaryEDFFilePath(): Full path of temporary EDF+ file not available for retrieval."), 0, TRUE);
	}
	else
	{
		// check if buffer has enough space to store the path and the terminating NULL-terminating character
		if(uintBufferLen > sztPathLength)
		{
			_stprintf_s(strBuffer, uintBufferLen, TEXT("%s"), m_strTempEDFFilePath);
			return TRUE;
		}
		else
			applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_GetTemporaryEDFFilePath(): Buffer is not large enough to store full path of temporary EDF+ file."), 0, TRUE);
	}
	return FALSE;
 }

/**
 * \brief Function executed when sampling thread is created using the CreateThread function. This stores the EDF+ data records to the HDD and sends them to the streaming server.
 *
 * \param[in]	lParam		thread data in the form of a StorageThreadData passed to the function using the lpParameter parameter of the CreateThread function
 *
 * \return Indicates success or failure of the function.
 */
long WINAPI Storage_Thread (LPARAM lParam)
{
	BOOL				blnStateErrorOccured;				///< indicates whether an error has occured in a given state
	CONFIGURATION *		pcfg;								///< pointer to the CONFIGURATION struct of the main module
	HWND				hwndMainWnd;						///< handle to the main application window
	struct tm			tmCurrentDateTime;					///< variable that stores the current tmCurrentDateTime and time
	StorageThreadData *	pstd;								///< pointer to struct containing data passed to the storage thread by the main thread
	TCHAR *				strMeasurementFolder;				///< NULL-terminated string that stores the full path of the folder where the recorded EDF+ files will be stored
	TCHAR *				strTemp;							///< pointer used to store temporary strings
	UINT64				lngpFreeBytesAvailable;				///< variable that receives the total number of free bytes on a disk that are available to the user who is associated with the calling thread

	// variable initialization
	hwndMainWnd = FindWindow (WINDOW_CLASSID_MAIN, NULL);
	pstd = (StorageThreadData *) lParam;
	pcfg = pstd->pcfg;
	strMeasurementFolder = NULL;
	m_stsStorageThreadState = STS_Idle;
	m_strTempEDFFilePath[0] = L'\0';

	while (1)
	{
		switch(m_stsStorageThreadState)
		{
			case STS_Idle:
				blnStateErrorOccured = FALSE;

				// signal that state machine is in Idle state and wait for Start event
				SignalObjectAndWait(pstd->hevStorageThread_Idling, pstd->hevStorageThread_Init_Start, INFINITE, FALSE);

				//
				// state transition
				//
				m_stsStorageThreadState = STS_Init;
			break;

			case STS_Init:
				blnStateErrorOccured = FALSE;

				//
				// initialize global variables
				// 
				m_EDFFileProperties.SamplingFrequency = pcfg->SamplingFrequency;
				m_EDFFileProperties.DataRecordSize = ((EEGCHANNELS + ACCCHANNELS) * m_EDFFileProperties.SamplingFrequency * sizeof(short)) + ANNOTATION_TOTAL_NCHARS*sizeof(char);
				m_EDFFileProperties.HeaderRecordSize = EDFFILEHEADERLENGTH + EDFSIGNALHEADERLENGTH*(EEGCHANNELS + ACCCHANNELS + 1);		// +1 for the annotations channel
				m_EDFFileProperties.NDataRecords = 0;
				m_hEDFTempFile = INVALID_HANDLE_VALUE;
				m_hIOCP = NULL;
				m_pllIOCompletionPending = m_pllWritePending = NULL;
				m_strTempEDFFilePath[0] = L'\0';

				//
				// initialize local variables
				//
				strTemp = NULL;

				//
				// check if the given save path location is valid
				//
				if(PathFileExists(pcfg->DestinationFolder))
				{
					// Check if available disk space is sufficient
					if(GetDiskFreeSpaceEx(pcfg->DestinationFolder, (PULARGE_INTEGER) &lngpFreeBytesAvailable, NULL, NULL))
					{
						if(lngpFreeBytesAvailable < MIN_AVAILABLE_DISK_SPACE)
						{
							applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Insufficient HDD space available for recording."), 0, TRUE);
							MsgPrintf (hwndMainWnd, 0, TEXT("Storage_Thread() - STS_Init: Insufficient HDD space available for recording.\nPlease free some space and try again."));
							blnStateErrorOccured = TRUE;
						}
					}
					else
					{
						applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to determine HDD space available for recording. (GetLastError #)"), GetLastError(), TRUE);
						MsgPrintf (hwndMainWnd, 0, TEXT("Storage_Thread() - STS_Init: Unable to determine HDD space available for recording. (GetLastError #%d)!"), GetLastError());
						blnStateErrorOccured = TRUE;
					}
				}
				else
				{
					applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to determine existance of destination folder. (GetLastError #)"), GetLastError(), TRUE);
					MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to determine existance of destination folder. (GetLastError #)"), GetLastError());
					blnStateErrorOccured = TRUE;
				}
				
				//
				// create folder with today's date in the destination folder
				//
				if(!blnStateErrorOccured)
				{
					// get the current tmCurrentDateTime
					tmCurrentDateTime = util_GetCurrentDateTime();

					// create folder for the day's recordings using the current tmCurrentDateTime
					strMeasurementFolder = (TCHAR *) calloc(MAX_PATH - 13, sizeof(TCHAR));
					if(strMeasurementFolder != NULL)
					{
						_stprintf_s(strMeasurementFolder,
									MAX_PATH - 13,
									TEXT("%s\\%4d.%02d.%02d"),
									pcfg->DestinationFolder, tmCurrentDateTime.tm_year + 1900, tmCurrentDateTime.tm_mon + 1, tmCurrentDateTime.tm_mday);

						if((CreateDirectory(strMeasurementFolder, NULL) == FALSE) && (GetLastError() != ERROR_ALREADY_EXISTS))
						{
							applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to create measurement folder for today's date. (GetLastError #)"), GetLastError(), TRUE);
							MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to create measurement folder for today's date. (GetLastError #%d).\nPlease make sure you have full read & write privileges for the destination folder."), GetLastError());
							blnStateErrorOccured = TRUE;
						}
					}
					else
					{
						applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to allocate memory for string buffer. (errno #)"), errno, TRUE);
						MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to allocate memory for string buffer. (errno #%d)."), errno);
						blnStateErrorOccured = TRUE;
					}
				}

				//
				// generate file name for temporary EDF file
				//
				if(!blnStateErrorOccured)
				{
					// get random prefix
					strTemp = (TCHAR *) calloc(TMPFILE_PRFX_LEN + 1, sizeof(TCHAR));
					if(strTemp != NULL)
					{
						if(util_GetRandomPrefix(strTemp, TMPFILE_PRFX_LEN + 1) != 0)
							_tcsncpy_s(strTemp, TMPFILE_PRFX_LEN + 1, FILEPREFIX, TMPFILE_PRFX_LEN);

						// create unique temporary filename in given destination path
						if (GetTempFileName(strMeasurementFolder, strTemp, 0, m_strTempEDFFilePath) == 0)
						{
							applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to obtain unique temporary file name. (GetLastError #)"), GetLastError(), TRUE);
							MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to obtain unique temporary file name. (GetLastError #%d)"), GetLastError());
							blnStateErrorOccured = TRUE;
						}
					}
					else
					{
						applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to allocate memory for temporary string buffer. (errno #)"), errno, TRUE);
						MsgPrintf (hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to allocate memory for temporary string buffer. (errno #%d)."), errno);
						blnStateErrorOccured = TRUE;
					}
				}

				//
				// create temporary EDF file and its associated I/O completion port
				//
				if(!blnStateErrorOccured)
				{
					// Create the new file to write the upper-case version to.
					m_hEDFTempFile = CreateFile((LPTSTR) m_strTempEDFFilePath,
												GENERIC_READ | GENERIC_WRITE,					// R/W rights (no execute)
												0,												// don't share file
												NULL,											// default security descriptor
												CREATE_ALWAYS,									// create new file, always
												FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
												NULL);
					if (m_hEDFTempFile == INVALID_HANDLE_VALUE) 
					{ 
						applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to create temporary EDF+ file. (GetLastError #)"), GetLastError(), TRUE);
						MsgPrintf(hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to create temporary EDF+ file. (GetLastError #)"), GetLastError());
						blnStateErrorOccured = TRUE;
					}
					else
					{
						// Create an I/O completion port for the temporary file
						m_hIOCP = CreateIoCompletionPort(m_hEDFTempFile, NULL, TEMPFILECOMPLETIONKEY, 0);
						if(m_hIOCP == NULL)
						{ 
							applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to create I/O completion port. (GetLastError #)"), GetLastError(), TRUE);
							MsgPrintf(hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to create I/O completion port. (GetLastError #)"), GetLastError());
							blnStateErrorOccured = TRUE;
						}
					}
				}

				//
				// created linked lists
				//
				// create linked list for records that are to be written
#ifdef _DEBUG
				m_pllWritePending = linkedlist_create('W');
#else
				m_pllWritePending = linkedlist_create(0);
#endif
				if(m_pllWritePending == NULL)
				{
					applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to create write pending linked list."), 0, TRUE);
					MsgPrintf(hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to create write pending linked list."));
					blnStateErrorOccured = TRUE;
				}

				// create linked list for records that have been writtenare to be written
#ifdef _DEBUG
				m_pllIOCompletionPending = linkedlist_create('C');
#else
				m_pllIOCompletionPending = linkedlist_create(0);
#endif
				if(m_pllIOCompletionPending == NULL)
				{
					applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_Init: Unable to create I/O completion pending linked list."), 0, TRUE);
					MsgPrintf(hwndMainWnd, MB_ICONSTOP, TEXT("Storage_Thread() - STS_Init: Unable to create I/O completion pending linked list."));
					blnStateErrorOccured = TRUE;
				}

				//
				// state transition
				//
				if(blnStateErrorOccured)
					m_stsStorageThreadState = STS_CleanUp;
				else
					m_stsStorageThreadState = STS_Write;

				// signal that main FSM has finished init state
				SetEvent(pstd->hevStorageThread_Init_End);
			break;
			
			case STS_Write:
				blnStateErrorOccured = FALSE;

				// wait for records to become available for storage
				WaitForSingleObject(pstd->hevStorageThread_Write, INFINITE);

				// write records to temporary EDF+ file
				if(m_pllWritePending->count > 0)
					blnStateErrorOccured = !Storage_WriteRecords();

				//
				// state transition
				//
				if(blnStateErrorOccured)
					m_stsStorageThreadState = STS_CleanUp;
				else
					m_stsStorageThreadState = STS_ProcessIOCompletionPackets;
			break;

			case STS_ProcessIOCompletionPackets:
				blnStateErrorOccured = FALSE;

				// process I/O completion packets
				blnStateErrorOccured = !Storage_ProcessIOPackets(IOPACKETTIMEOUT);

				//
				// state transition
				//
				if(pstd->StopStorage || blnStateErrorOccured)
					m_stsStorageThreadState = STS_CleanUp;
				else
					m_stsStorageThreadState = STS_Write;
			break;

			case STS_CleanUp:
				//
				// finish pending writes
				//
				if(!Storage_CleanUp())
					applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Storage_Thread() - STS_CleanUp: Unable to complete pending writes."), 0, TRUE);

				//
				// close temporary EDF+ file and associated I/O completion port
				//
				if(m_hEDFTempFile != INVALID_HANDLE_VALUE)
					CloseHandle(m_hEDFTempFile);
				if(m_hIOCP != NULL)
					CloseHandle(m_hIOCP);

				//
				// free resources
				//
				// memory allocations
				if(strMeasurementFolder != NULL)
					free(strMeasurementFolder);
				if(strTemp != NULL)
					free(strTemp);

				// linked lists
				linkedlist_free(m_pllWritePending);
				linkedlist_free(m_pllIOCompletionPending);
				
				//
				// state transition
				//				
				m_stsStorageThreadState = STS_Idle;
			break;

			default:
				applog_logevent(SoftwareError, TEXT("Storage"), TEXT("Main FSM: Unknown state reached."), 0, TRUE);
				m_stsStorageThreadState = STS_CleanUp;
		}
	}
}