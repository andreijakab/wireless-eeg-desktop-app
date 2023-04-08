/**
 * \file		thread_sample.h
 * \since		17.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 *
 * \brief		???
 *
 * $Id: thread_sample.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __THREAD_SAMPLE_H__
# define __THREAD_SAMPLE_H__

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//   								Enums/Structs
//---------------------------------------------------------------------------
/**
 * Sample thread operating modes
 */
typedef enum {SampleThreadMode_Recording,					///< displaying and recording data received from the WEEG measurement unit
			  SampleThreadMode_WEEGSystemCheck,				///< checking status of WEEG system
			  SampleThreadMode_Simulation,					///< displaying and recording data from EDF+ file
			  SampleThreadMode_WEEGDCCalibration			///< measuring offset voltages of the EEG inputs of the WEEG measurement unit
} SampleThreadMode;

/**
 * States of the Simulation mode FSM
 */
typedef enum {SimulationModeState_Initialize,				///< initialize
			  SimulationModeState_Acquire,					///< read data from EDF+ file & display and record it
			  SimulationModeState_CleanUp					///< clean-up
} SimulationModeState;

/**
 * States of the WEEG System Check mode FSM
 */
typedef enum {WEEGSystemCheckModeState_Stage1,				///< testing of measurement device - first round of tests and configuration for last test
			  WEEGSystemCheckModeState_Stage2,				///< testing of measurement device - data acquisition for second test
			  WEEGSystemCheckModeState_CleanUp				///< clean-up
} WEEGSystemCheckModeState;

/**
 * States of the Recording mode FSM
 */
typedef enum {RecordingModeState_Initialize,				///< initializing sampling
			  RecordingModeState_Acquire,					///< acquiring data
			  RecordingModeState_CleanUp					///< clean-up after measurements
} RecordingModeState;

/**
 * Sample thread main FSM states
 */
typedef enum {SampleThreadState_Idle,						///< idle state
			  SampleThreadState_Working						///< performing work
} SampleThreadState;

/**
 * Data passed to the Sampling thread when operating in the Simulation mode
 */
typedef struct
{
	char	strSimulationEDFFile[MAX_PATH + 1];				///< stores the full path to the EDF+ used during simulation mode
} SimulationModeData;

/**
 * 
 */
typedef struct
{
	BOOL						EndActivity;				///< 
	GUIElements	*				pgui;						///< 
	SampleThreadMode			Mode;						///< mode in which the sampling thread is operating
	void *						pModeData;					///< pointer to data structure neeeded by the operating mode specified in Mode

	// pointers to configuration variables
	int *						pSamplingFrequency;			///< pointer to variable containing the sampling frequency of the current recording
	
	// Handles belonging to the Sampling thread
	HANDLE						hevSampleThread_Start;		///< event used to start the Sampling thread activity
	HANDLE						hevSampleThread_Idle;		///< event that signals that the Sampling thread is in the Idle state
	HANDLE						hevSampleThread_Init;		///< event that signals that the Sampling thread is in the Init state
	
	// Handles belonging to other threads
	HANDLE						hevStorageThread_Write;		///< event that signals the Storage thread that there are records to be stored
	HANDLE						hevVortexClient_Transmit;	///< event that signals the Streaming thread that there are records to be streamed
} SampleThreadData;

/**
 * 
 */
typedef struct
{
	BYTE *						WriteBuffer;			///< contains all of the signals of the EEGEM EDF+ data record, as they will be written to the EDF+ file
	unsigned int				WriteBufferLen;			///< size of WriteBuffer, in bytes
	short **					MeasurementData;		///< 
} SampleDataRecord;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
/**
 * \brief Free all of the memory resources allocated to the members of a SampleDataRecord structure.
 *
 * \param[in]	pdrDataRecord	pointer to the SampleDataRecord the memory resources of which are to be released
 */
void Sample_FreeDataRecord(SampleDataRecord * pdrDataRecord);

/**
 * \brief Allocates memory for the MeasurementData and WriteBuffer members of a SampleDataRecord structure.
 *
 * \param[in]	pdrDataRecord			pointer to the SampleDataRecord structure to be initialized
 * \param[in]	intSamplingFrequency	frequency at which the EEG and acceleration signals are sampled
 *
 * \return TRUE if succesfull, FALSE otherwise.
 */
BOOL Sample_InitDataRecord(SampleDataRecord * pdrSamplingDataRecord, int intSamplingFrequency);

#endif