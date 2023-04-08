/**
 * \file		thread_stream.h
 * \since		14.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 *
 * \brief		???
 *
 * $Id: thread_stream.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __STREAM_BEEP_H__
# define __STREAM_BEEP_H__

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define LIBVORTEX_VERSION			TEXT("1.1.12")

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
/**
 * 
 */
typedef struct {BOOL				ExitThread;							///<
				BOOL				EndTransmission;					///<
				HANDLE				hevThreadInit_Complete;				///<
				HANDLE				hevVortexClient_Connect_Start;		///< 
				HANDLE				hevVortexClient_Connecting_Start;	///<
				HANDLE				hevVortexClient_Connecting_End;		///< 
				HANDLE				hevVortexClient_Transmit;			///< 
				HANDLE				hevVortexClient_Exiting;			///< 
				HANDLE				hevVortexClient_WaitingToConnect;	///< 
				HANDLE				hevVortexClient_WaitingToTransmit;	///< 
				char				EDFFileName[MAX_PATH + 1];			///< (+1 for terminating NULL character)
				int *				pMaxNSendMsgFailures;				///<
				int	*				pMaxNWait4ReplyFailures;			///<
				BYTE *				pServerIPv4Address_Field0;			///<
				BYTE *				pServerIPv4Address_Field1;			///<
				BYTE *				pServerIPv4Address_Field2;			///<
				BYTE *				pServerIPv4Address_Field3;			///<
				int	*				pServerPort;						///<
} StreamingClientThreadData;

/**
 * States of the client's finite-state machine running in the main thread.
 */
typedef enum {StreamingClientState_None = 0,					///<
			  StreamingClientState_Init,						///< 
			  StreamingClientState_Waiting2Connect,				///< 
			  StreamingClientState_Connecting,					///< 
			  StreamingClientState_DataStreaming,				///< 
			  StreamingClientState_Cleanup,						///< 
			  StreamingClientState_Exit							///< 
} StreamingClientState;

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
 /**
 * \brief Returns the current state of the streaming client's main FSM.
 *
 * \return	A member of the StreamingClientState enumeration.
 */
 StreamingClientState Streaming_GetMainFSMState(void);

 /**
 * \brief Adds packet to the FIFO transmission queue of the streaming thread. Function executes in the execution context of the calling thread.
 *
 * \param[in]	PacketType			member of EEGEMPacketType that indicates type of packet to that is to be transmitted
 * \param[in]	bytPayload			packet payload
 * \param[in]	ushrPayloadLength	size of packet payload, in bytes
 * \param[in]	hEvent				event to be signaled once packet has been added to queue
 *
 * \return TRUE if packet was successfully added to the transmission queue, FALSE otherwise.
 */
BOOL Streaming_SendPacket(EEGEMPacketType PacketType, BYTE * bytPayload, unsigned short ushrPayloadLength, HANDLE hEvent);

/**
 * \brief Function executed when streaming thread is created using the CreateThread function.
 *
 * \param[in]	lParam		thread data passed to the function using the lpParameter parameter of the CreateThread function
 *
 * \return Indicates success or failure of the function.
 */
long WINAPI Streaming_Thread (LPARAM lParam);

#endif