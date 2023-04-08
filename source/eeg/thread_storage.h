/**
 * \file		thread_storage.h
 * \since		17.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 *
 * \brief		???
 *
 * $Id: thread_storage.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __THREAD_STORAGE_H__
# define __THREAD_STORAGE_H__

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
/**
 * States of the storage thread's main FSM.
 */
typedef enum {STS_Idle							= 0,
			  STS_Init							= 1,
			  STS_Write							= 2,
			  STS_ProcessIOCompletionPackets	= 3,
			  STS_CleanUp						= 4
} StorageThreadState;

/**
 * Structure used by main thread to control the storage thread.
 */
typedef struct{ BOOL				StopStorage;					///< flag used for signaling that the storage of records should be stopped
				HANDLE				hevStorageThread_Idling;		///< event that signals that the Storage thread is in the Idle state
				HANDLE				hevStorageThread_Init_Start;	///< event used to signal the main FSM of the Storage thread to transition to the Init state
				HANDLE				hevStorageThread_Init_End;		///< event used to signal that the main FSM of the Storage thread has finished executing the instructions of the Init state
				HANDLE				hevStorageThread_Write;			///< event that signals that there are records to be stored
				CONFIGURATION *		pcfg;							///< pointer to the CONFIGURATION struct of the main module
} StorageThreadData;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
 /**
 * \brief Adds record to the write queue of the storage thread. Function executes in the execution context of the calling thread.
 *
 * \param[in]	bytRecord				pointer to BYTE array that stores the record to be stored in the EDF+ file
 * \param[in]	uintRecordLen			size of bytRecord array, in bytes
 * \param[in]	blnHeaderRecord			TRUE if bytRecord is a header record, FALSE otherwise
 * \param[in]	hEvent					event to be signaled once record has been added to linked list
 */
BOOL Storage_AddToQueue(BYTE * bytRecord, unsigned int uintRecordLen, BOOL blnHeaderRecord, HANDLE hEvent);

 /**
 * \brief Returns the current state of the storage client's main FSM.
 *
 * \return	A member of the StorageThreadState enumeration.
 */
StorageThreadState Storage_GetMainFSMState(void);

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
 BOOL Storage_GetTemporaryEDFFilePath(TCHAR * strBuffer, unsigned int uintBufferLen);

/**
 * \brief Function executed when sampling thread is created using the CreateThread function. This stores the EDF+ data records to the HDD and sends them to the streaming server.
 *
 * \param[in]	lParam		thread data in the form of a StorageThreadData passed to the function using the lpParameter parameter of the CreateThread function
 *
 * \return Indicates success or failure of the function.
 */
long WINAPI Storage_Thread (LPARAM lParam);

#endif