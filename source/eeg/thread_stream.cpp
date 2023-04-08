/**
 * \file		thread_stream.cpp
 * \since		14.01.2013
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 * \version		1.0.0
 *
 * \brief		???
 *
 * $Id: thread_stream.cpp 76 2013-02-14 14:26:17Z jakab $
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
// custom
#pragma comment(lib, "libvortex-1.1.lib")
#pragma comment(lib, "libvortex-tls-1.1.lib")
#pragma comment(lib, "shlwapi.lib")

//---------------------------------------------------------------------------
//   							Includes
//------------------------------------------------------------ ---------------
// Windows libaries
#include <windows.h>

// CRT libraries
#include <conio.h>
#include <stdlib.h>
#include <tchar.h>
#include <shlwapi.h>

// custom libraries
#include <vortex_tls.h>		// includes <vortex.h>
#include <eegem_beep.h>

// program headers
#include "globals.h"
#include "applog.h"
#include "thread_stream.h"

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
#define FIFO_QUEUE_LENGTH			1800				///< maximum amount of elements that can be stored in the FIFO queue

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
/**
 * 
 */
typedef struct {HANDLE				hevTLSNegotiationComplete;	///< 
				VortexConnection	* connection;				///<
				BOOL				IsTLSEnabled;				///<
} VortexTLSStatusData;

/**
 * States of the client's finite-state machine that transmits packets to the server.
 */
typedef enum {VortexClientSPState_None = 0,						///<
			  VortexClientSPState_SendPacket,					///< 
			  VortexClientSPState_SendPacketFailure,			///< 
			  VortexClientSPState_Wait4Reply,					///< 
			  VortexClientSPState_Wait4ReplyFailure,			///< 
			  VortexClientSPState_ReportError,					///< 
} VortexClientSPState;

//---------------------------------------------------------------------------
//							Global variables
//---------------------------------------------------------------------------
static unsigned int				m_uintFIFOElementSize;					///< size, in bytes, of the elements stored in the queue
static volatile LONG			m_intFIFOReadId;						///< index of the element right after the element that was last read by the consumer
static volatile LONG			m_intFIFOWriteId;						///< index of the element right after the element that was last written by the producer
static volatile void *			m_FIFOQueue[FIFO_QUEUE_LENGTH];			///< FIFO queue
static CRITICAL_SECTION			m_csFIFOGuard;							///< 

static StreamingClientState		m_vcsState;								///< stores the current state of the main FSM

//---------------------------------------------------------------------------
//							Internally-accessible functions
//---------------------------------------------------------------------------
/**
 * \brief Allocate memory for the FIFO elements and initialize indexing variables.
 *
 * \param[in]	intElementSize		size, in bytes, of the elements that will be stored in the queue
 *
 * \return TRUE if memory was succesfully allocated, FALSE otherwise.
 */
BOOL fifo_create(unsigned int intElementSize)
{
	BOOL blnResult = TRUE;
	int i;

	// initialize global variables
	m_intFIFOReadId = m_intFIFOWriteId = 0;
	m_uintFIFOElementSize = intElementSize;
	InitializeCriticalSection(&m_csFIFOGuard);
	
	// allocate memory for FIFO elements
	for(i = 0; i < FIFO_QUEUE_LENGTH; i++)
	{
		// perform memory allocation
		m_FIFOQueue[i] = malloc(m_uintFIFOElementSize);

		// break out of loop and return from function if memory allocation has failed
		if(m_FIFOQueue[i] == NULL)
		{
			blnResult = FALSE;
			break;
		}
	}

	// in malloc failure has occured, deallocate memory that has already been allocated
	if(!blnResult)
	{
		for(; i >= 0; i--)
		{
			if(m_FIFOQueue[i] != NULL)
				free((void *) m_FIFOQueue[i]);
		}
		
		DeleteCriticalSection(&m_csFIFOGuard);
	}

	return blnResult;
}

/**
 * \brief Deallocate memory that was assigned to FIFO queue elements.
 */
void fifo_destroy(void)
{
	unsigned int i;

	for(i = 0; i < FIFO_QUEUE_LENGTH; i++)
	{
		// free allocated memory
		free((void *) m_FIFOQueue[i]);
	}

	DeleteCriticalSection(&m_csFIFOGuard);
}

/**
 * \brief Reset FIFO queue to its empty state.
 */
void fifo_reset(void)
{
	InterlockedExchange(&m_intFIFOWriteId, 0);
	InterlockedExchange(&m_intFIFOReadId, 0);
}

/**
 * \brief Store element in FIFO queue.
 *
 * \param[in]	Element		pointer to element to be added to the queue
 * \param[in]	ElementSize	size, in bytes, of element to be added to FIFO queue
 *
 * \return TRUE if element was succesfully added to the queue, FALSE if queue is full.
 */
BOOL fifo_pushElement(const void * Element, unsigned int ElementSize)
{
	BOOL blnResult = FALSE;
	LONG intNextElementId;
	
#ifdef _DEBUG
	TCHAR strBuffer[256];

	_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Thread %u has entered fifo_pushElement(). m_intWriteId value: %d\n"), GetCurrentThreadId(), m_intFIFOWriteId);
	OutputDebugString(strBuffer);
#endif

	EnterCriticalSection(&m_csFIFOGuard);
	__try
	{
		intNextElementId = (m_intFIFOWriteId + 1) % FIFO_QUEUE_LENGTH;
		
		if(intNextElementId != m_intFIFOReadId)
		{
			// add element to queue
			memcpy_s((void *) m_FIFOQueue[m_intFIFOWriteId], m_uintFIFOElementSize,
					 Element, ElementSize);

			// update write index to point to next array location
			InterlockedExchange(&m_intFIFOWriteId, intNextElementId);

			// indicate that store was succesfull
			blnResult = TRUE;
		}
	}
	__finally
	{
		LeaveCriticalSection(&m_csFIFOGuard);
	}

#ifdef _DEBUG
	_stprintf_s(strBuffer, sizeof(strBuffer)/sizeof(TCHAR), TEXT("Thread %u has left fifo_pushElement(). m_intWriteId value: %d\n\n"), GetCurrentThreadId(), m_intFIFOWriteId);
	OutputDebugString(strBuffer);
#endif

	return blnResult;
}

/**
 * \brief Remove oldest element from the FIFO and return it in the provided buffer.
 *
 * \param[out]	Element		pointer to buffer where element is to be stored
 * \param[in]	ElementSize	length of Element buffer, in bytes
 *
 * \return TRUE if an element was succesfully removed, FALSE otherwise.
 */
BOOL fifo_popElement(void * Element, unsigned int ElementSize)
{
	BOOL blnResult = FALSE;
	int intNextElementId;

#ifdef _DEBUG
	// make sure buffer is actually as long as advertised
	memset(Element, 0xAE, ElementSize);
#endif

	// check if there is a new element to be removed
	if(m_intFIFOReadId != m_intFIFOWriteId)
	{
		// calculate next position of read index
		intNextElementId = (m_intFIFOReadId + 1) % FIFO_QUEUE_LENGTH;

		// pop element from FIFO queue
		memcpy_s(Element, ElementSize,
				 (void *) m_FIFOQueue[m_intFIFOReadId], m_uintFIFOElementSize);

		// set read index to next queue position
		InterlockedExchange(&m_intFIFOReadId, intNextElementId);

		// indicate that element removal was succesfull
		blnResult = TRUE;
	}

	return blnResult;
}

//---------------------------------------------------------------------------
//								Callbacks
//---------------------------------------------------------------------------
/**
 * \brief Handler called when TLS negotiation is finished (no matter the result). 
 *
 * For more information click <a href="http://www.aspl.es/fact/files/af-arch/vortex-1.1/html/group__vortex__tls_ga9d30465b906d99b8673c2059ace1b31d.html#ga9d30465b906d99b8673c2059ace1b31d"> here</a>.
 *
 * \param[in]	connection		connection where the TLS activation status is being notified
 * \param[in]	status			negotiation status
 * \param[in]	status_message	text message representing the negotiation status
 * \param[in]	user_data		user-defined data passed in to this handler
 */
void process_tls_status (VortexConnection * connection, VortexStatus status, char * status_message, axlPointer user_data)
{
	VortexTLSStatusData * vtlssd = (VortexTLSStatusData *) user_data;

	switch (status)
	{
		case VortexOk:
			// use the new connection reference provided by 
			// this function. Connection provided at 
			// vortex_tls_start_negotiation have been unrefered.
			// In this case, my_server_connection have been 
			// already unrefered by the TLS activation
			vtlssd->connection = connection;
			vtlssd->IsTLSEnabled = TRUE;
		break;

		case VortexError: 
			// ok, TLS process have failed but, do we have a connection
			// still working?. This is checked using
			/*if (vortex_connection_is_ok (connection, axl_false))
			{
				// well we don't have TLS activated but the connection
				// still works
				my_server_connection = connection;
			}*/
			vtlssd->IsTLSEnabled = FALSE;
		break;
	}

	// signal main thread
	SetEvent(vtlssd->hevTLSNegotiationComplete);

	return;
 }

 /**
 * \brief Returns the current state of the streaming client's main FSM.
 *
 * \return	A member of the StreamingClientState enumeration.
 */
 StreamingClientState Streaming_GetMainFSMState(void)
 {
	 return m_vcsState;
 }
 
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
BOOL Streaming_SendPacket(EEGEMPacketType PacketType, BYTE * bytPayload, unsigned short ushrPayloadLength, HANDLE hEvent)
{
	BOOL					blnResult = FALSE;
	EEGEMPacket				packet;
	static unsigned int		uintDataRecordID = 0;

	if(m_vcsState == StreamingClientState_Connecting || m_vcsState == StreamingClientState_DataStreaming)
	{
		switch(PacketType)
		{
			case EEGEMPacketType_EDFhdr:
				uintDataRecordID = EEGEM_DATARECORDID_FIRST;
				packet.DataRecordID = 0;						// NOTE: the DataRecordID counter can be reset here since the header record is only sent at the beginning or end of a transmission
			break;

			case EEGEMPacketType_EDFdr:
				packet.DataRecordID = uintDataRecordID++;
			break;
		}

		// assemble packet
		packet.Type = PacketType;
		packet.PayloadLength = ushrPayloadLength;
		memcpy_s(packet.Payload,
				 EEGEM_PACKET_PAYLOAD_MAX_LENGTH_BYT,
				 bytPayload,
				 packet.PayloadLength);

		// store packet in FIFO queue
		if(fifo_pushElement(&packet, sizeof(EEGEMPacket)))
			blnResult = TRUE;
		else
			applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_SendPacket() - FIFO queue has overflowed."), 0, TRUE);

		// signal streaming thread
		SetEvent(hEvent);
	}
	else
	{
		// log error
		applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_SendPacket(): Unable to send packet to since server is not in the correct state. (StreamingClientState #)"), m_vcsState, TRUE);
	}

	return blnResult;
}

/**
 * \brief Function executed when streaming thread is created using the CreateThread function.
 *
 * \param[in]	lParam		thread data passed to the function using the lpParameter parameter of the CreateThread function
 *
 * \return Indicates success or failure of the function.
 */
long WINAPI Streaming_Thread (LPARAM lParam)
{
	BOOL						blnStateErrorOccured;					///<
	BOOL						blnSendingPacket;						///<
	char						strServerIPv4[16];						///<
	char						strServerPort[6];						///<
	EEGEMPacket					Packet;									///<
	HWND						hwndMainWindow;							///< handle to main window
	int							reply;									///<
	int							msg_no;									///<
	int							intNSendMsgFailures;					///<
	int							intNWait4ReplyFailures;					///<
	size_t						sztReturnValue;							///<
	StreamingClientThreadData *	pvctd;									///<
	TCHAR						strBuffer1[256];						///<
	TCHAR						strBuffer2[256];						///<
	VortexChannel *				channel;								///<
	VortexClientSPState			vcspsState;								///<
	VortexConnection *			connection;								///<
	VortexCtx *					ctx;									///<
	VortexFrame *				frame;									///<
	VortexTLSStatusData			vtlssd;									///<
	WaitReplyData *				wait_reply;								///<
	unsigned int				uintEEGEMPacketHeaderLengthByt;			///<

	// get handle to main window
	hwndMainWindow = FindWindow (WINDOW_CLASSID_MAIN, NULL);
		
	// initialize variables
	pvctd = (StreamingClientThreadData *) lParam;
	blnStateErrorOccured = FALSE;
	connection = NULL;
	m_vcsState = StreamingClientState_Init;
	vtlssd.hevTLSNegotiationComplete = CreateEvent(NULL, FALSE, FALSE, NULL);
	if(vtlssd.hevTLSNegotiationComplete == NULL)
	{
		applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread(): Unable to create hevTLSNegotiationComplete event. (GetLastError #)"), GetLastError(), TRUE);
		SetEvent(pvctd->hevThreadInit_Complete);
		ExitThread(0);
	}

	vtlssd.IsTLSEnabled = FALSE;
	vtlssd.connection = NULL;
	uintEEGEMPacketHeaderLengthByt = sizeof(Packet) - sizeof(Packet.Payload);

	// log libvortex version
	applog_logevent(Version, TEXT("libvortex"), LIBVORTEX_VERSION, 0, FALSE);

	// initialize FIFO queue
	if(fifo_create(sizeof(EEGEMPacket)))
	{
		SetEvent(pvctd->hevThreadInit_Complete);
	}
	else
	{
		applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread(): Unable to create FIFO queue."), 0, TRUE);
		SetEvent(pvctd->hevThreadInit_Complete);
		ExitThread(0);
	}

	//
	// streaming client - main FSM
	// 
	while(1)
	{
		switch(m_vcsState)
		{
			case StreamingClientState_Init:
				// update status displayed in main window
				PostMessage(hwndMainWindow, EEGEMMsg_Streaming_DisplayStatus, m_vcsState, 0);

				blnStateErrorOccured = FALSE;

				// initialize fifo queue
				fifo_reset();

				// init vortex library
				ctx = vortex_ctx_new ();
				if (! vortex_init_ctx (ctx))
				{
					applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientState_Init: Failed to initialize Vortex library."), 0, TRUE);
					blnStateErrorOccured = TRUE;
				}

				// initialize TLS library
				if (!vortex_tls_init (ctx))
				{
					applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientState_Init: Current Vortex Library is not prepared for TLS profile."), 0, TRUE);
					blnStateErrorOccured = TRUE;
				}

				//
				// state transition
				//
				if(blnStateErrorOccured)
					m_vcsState = StreamingClientState_Exit;
				else
					m_vcsState = StreamingClientState_Waiting2Connect;
			break;

			case StreamingClientState_Waiting2Connect:
				// update status displayed in main window
				PostMessage(hwndMainWindow, EEGEMMsg_Streaming_DisplayStatus, m_vcsState, 0);

				// signal that thread is entering waiting mode
				SetEvent(pvctd->hevVortexClient_WaitingToConnect);

				// wait for signal from main thread before attempting to connect
				WaitForSingleObject(pvctd->hevVortexClient_Connect_Start, INFINITE);
				
				//
				// state transition
				//
				if(pvctd->ExitThread)
					m_vcsState = StreamingClientState_Exit;
				else
					m_vcsState = StreamingClientState_Connecting;
			break;

			case StreamingClientState_Connecting:
				// update status displayed in main window
				PostMessage(hwndMainWindow, EEGEMMsg_Streaming_DisplayStatus, m_vcsState, 0);

				// signal that main FSM has entered connecting state
				SetEvent(pvctd->hevVortexClient_Connecting_Start);

				blnStateErrorOccured = FALSE;
				
				// create new connection to server
				sprintf_s(strServerIPv4,
						  sizeof(strServerIPv4),
						  "%d.%d.%d.%d",
						  *pvctd->pServerIPv4Address_Field0, *pvctd->pServerIPv4Address_Field1, *pvctd->pServerIPv4Address_Field2, *pvctd->pServerIPv4Address_Field3);
				sprintf_s(strServerPort,
						  sizeof(strServerPort),
						  "%d",
						  *pvctd->pServerPort);
				connection = vortex_connection_new (ctx,					// context where the operation will be performed.
													strServerIPv4,			// IPv4 of server to connect to
													strServerPort,			// port of server to connect to
													NULL,					// optional handler to process connection new status
													NULL);					// user defined data to be passed in to the connection new handler
				if (vortex_connection_is_ok (connection, axl_false))
				{
					// start the TLS profile negotiation process
					vortex_tls_start_negotiation (connection,				// connection where the secure transport will be started
												  NULL,						// optional server name value transmitted the remote peer so it could react in a different way depending on this value
												  process_tls_status,		// required handler that is executed once the negotiation has finished, no matter its result
												  &vtlssd);					// user defined data to be passed in to the process_tls_status handler
					
					// wait for TLS negotiation to complete
					WaitForSingleObject(vtlssd.hevTLSNegotiationComplete, INFINITE);

					// check result of TLS negotiation
					if(!vtlssd.IsTLSEnabled)
					{
						mbstowcs_s(&sztReturnValue, strBuffer1, sizeof(strBuffer1)/sizeof(WORD), vortex_connection_get_message (connection), _countof(strBuffer1) - 1);
						_stprintf_s(strBuffer2, sizeof(strBuffer2)/sizeof(TCHAR), TEXT("Streaming_Thread() - VortexClientState_Connecting: Unable to establish secure TLS connection with remote server. (Vortex library error: %s)"), strBuffer1);
						applog_logevent(SoftwareError, TEXT("Streaming"), strBuffer2, 0, TRUE);
						blnStateErrorOccured = TRUE;
					}
				}
				else
				{
					mbstowcs_s(&sztReturnValue, strBuffer1, sizeof(strBuffer1)/sizeof(WORD), vortex_connection_get_message (connection), _countof(strBuffer1) - 1);
					_stprintf_s(strBuffer2, sizeof(strBuffer2)/sizeof(TCHAR), TEXT("Streaming_Thread() - VortexClientState_Connecting: Unable to connect to remote server. (Vortex library error: %s)"), strBuffer1);
					applog_logevent(SoftwareError, TEXT("Streaming"), strBuffer2, 0, TRUE);
					blnStateErrorOccured = TRUE;
				}
				
				// create a new channel over the secured connection
				if(!blnStateErrorOccured)
				{
					// change connection object to the one provided by the process_tls_status handler
					connection = vtlssd.connection;

					// create channel
					channel = vortex_channel_new_full(connection,								// session where channel will be created
													  0,										// channel number (by chosing 0 as channel number the Vortex Library will automatically assign the new channel number free)
													  NULL,										// optional server name value transmitted the remote peer so it could react in a different way depending on this value
													  EEGEM_BEEP_PROFILE,						// profile to be used for channel creation
													  EncodingNone,								// profile content encoding used
													  pvctd->EDFFileName,						// piggyback data sent during channel creation
													  strlen(pvctd->EDFFileName)*sizeof(char),	// size of piggyback data, in bytes
													  NULL, NULL,								// no handler for channel closing
													  NULL, NULL,								// no handler for frame reception
													  NULL, NULL);								// no async channel creation handler
					if (channel == NULL)
					{
						applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientState_Connecting: Unable to create channel."), 0, TRUE);
						blnStateErrorOccured = TRUE;
					}
				}

				//
				// state transition
				//
				if(blnStateErrorOccured)
					m_vcsState = StreamingClientState_Cleanup;
				else
					m_vcsState = StreamingClientState_DataStreaming;

				// signal that main FSM has finished connecting state
				SetEvent(pvctd->hevVortexClient_Connecting_End);
			break;

			case StreamingClientState_DataStreaming:
				// update status displayed in main window
				PostMessage(hwndMainWindow, EEGEMMsg_Streaming_DisplayStatus, m_vcsState, 0);

				blnStateErrorOccured = FALSE;

				// signal that thread is entering waiting mode
				SetEvent(pvctd->hevVortexClient_WaitingToTransmit);

				// wait for signal from main thread
				WaitForSingleObject(pvctd->hevVortexClient_Transmit, INFINITE);

				//
				// send data
				//
				while(fifo_popElement(&Packet, sizeof(EEGEMPacket)) && !blnStateErrorOccured)
				{
					// initialize state machine variables
					blnSendingPacket = TRUE;
					vcspsState = VortexClientSPState_SendPacket;
					intNSendMsgFailures = intNWait4ReplyFailures = 0;					// NOTE: since these counters are reset outside of the FSM,
																						//       MaxNSendMsgFailures and MaxNWait4ReplyFailures specify
																						//		 the amount of failures on a per-packet basis.

					// creates a new wait reply to be used to wait for a specific reply
					wait_reply = vortex_channel_create_wait_reply ();

					do
					{
						switch(vcspsState)
						{
							case VortexClientSPState_SendPacket:
								// send the message using msg_and_wait and start a wait_reply
								if (vortex_channel_send_msg_and_wait (channel,													// channel where message will be sent
																	  &Packet,													// message to be sent
																	  uintEEGEMPacketHeaderLengthByt + Packet.PayloadLength,	// size of message to be sent
																	  &msg_no,													// required integer reference to store the message number used 
																	  wait_reply))												// Wait Reply object (created using vortex_channel_create_wait_reply)
								{
									vcspsState = VortexClientSPState_Wait4Reply;
								}
								else
								{
									vcspsState = VortexClientSPState_SendPacketFailure;
								}

							break;
							
							case VortexClientSPState_SendPacketFailure:
								intNSendMsgFailures++;

								if(intNSendMsgFailures < *pvctd->pMaxNSendMsgFailures)
								{
									applog_logevent(General, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientSPState_SendPacketFailure: Unable to send packet."), 0, TRUE);

									Sleep (intNSendMsgFailures*1000);			// sleep intNSendMsgFailures seconds

									vcspsState = VortexClientSPState_SendPacket;
								}
								else
								{
									applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientSPState_SendPacketFailure:  Failed to send packet. Channel is likely stalled."), 0, TRUE);

									// dealloc wait_reply object
									vortex_channel_free_wait_reply (wait_reply);

									vcspsState = VortexClientSPState_ReportError;
								}
							break;
							
							case VortexClientSPState_Wait4Reply:
								// get blocked until the reply arrives
								// NOTE: wait_reply object must not be freed after this function because it will be free automatically
								frame = vortex_channel_wait_reply (channel, msg_no, wait_reply);
								if (frame != NULL)
								{
									reply = *((int *) vortex_frame_get_payload (frame));

									// dellocate frame resources
									vortex_frame_unref(frame);
									
									// reply received successfully so we can stop trying to send current packet
									blnSendingPacket = FALSE;
								}
								else
								{
									vcspsState = VortexClientSPState_Wait4ReplyFailure;
								}
							break;

							case VortexClientSPState_Wait4ReplyFailure:
								// check if the connection is broken
								if (vortex_connection_is_ok (connection, axl_false))
								{
									intNWait4ReplyFailures++;

									if(intNWait4ReplyFailures < *pvctd->pMaxNWait4ReplyFailures)
									{
										applog_logevent(General, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientSPState_Wait4ReplyFailure: Timeout occured while waiting for reply."), 0, TRUE);

										vcspsState = VortexClientSPState_SendPacket;
									}
									else
									{
										applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientSPState_Wait4ReplyFailure: Failed to receive reply from server."), 0, TRUE);

										// dealloc wait_reply object
										vortex_channel_free_wait_reply (wait_reply);

										vcspsState = VortexClientSPState_ReportError;
									}
								}
								else
								{
									// connection is not available, try to reconnect.
									if (vortex_connection_reconnect (connection,	// connection to reconnect to
																	 NULL,			// no async notify upon reconnection
																	 NULL))			// user data to be passed to the handler
									{
										applog_logevent(General, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientSPState_Wait4ReplyFailure: Link failure detected. Succesfully reconnected to server."), 0, TRUE);

										vcspsState = VortexClientSPState_SendPacket;
									}
									else
									{
										applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - VortexClientSPState_Wait4ReplyFailure: Link failure detected. Unable to reconnect to server."), 0, TRUE);

										// dealloc wait_reply object
										vortex_channel_free_wait_reply (wait_reply);

										vcspsState = VortexClientSPState_ReportError;
									}
								}
							break;

							case VortexClientSPState_ReportError:
								blnStateErrorOccured = TRUE;
								blnSendingPacket = FALSE;
							break;

							default:
								applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - Packet transmission FSM: Unknown state reached."), 0, TRUE);
								vcspsState = VortexClientSPState_ReportError;
						}
					} while(blnSendingPacket);
				}

				//
				// state transition
				//
				if(blnStateErrorOccured)
					m_vcsState = StreamingClientState_Cleanup;
				else
				{
					if(pvctd->ExitThread)
						m_vcsState = StreamingClientState_Exit;
					else if (pvctd->EndTransmission)
						m_vcsState = StreamingClientState_Cleanup;
				}
			break;
			
			case StreamingClientState_Cleanup:
				// update status displayed in main window
				PostMessage(hwndMainWindow, EEGEMMsg_Streaming_DisplayStatus, m_vcsState, 0);

				//
				// Vortex cleanup
				//
				// close connection
				if (vortex_connection_is_ok (connection, axl_false))
					vortex_connection_close (connection);

				// terminate execution context
				vortex_exit_ctx (ctx, axl_false);

				// free execution context
				vortex_ctx_free (ctx);

				//
				// state transition
				//
				m_vcsState = StreamingClientState_Init;
			break;

			case StreamingClientState_Exit:
				// update status displayed in main window
				// NOTE: this is done ONLY if main thread hasn't set the ExitThread because the hwndMainWindow becomes invalid
				//       once the main window receives the WM_DESTROY message
				if(!(pvctd->ExitThread))
					PostMessage(hwndMainWindow, EEGEMMsg_Streaming_DisplayStatus, m_vcsState, 0);

				//
				// Vortex cleanup
				//
				// close connection
				if(connection != NULL)
				{
					if (vortex_connection_is_ok (connection, axl_false))
						vortex_connection_close (connection);
				}

				// terminate execution context
				vortex_exit_ctx (ctx, axl_false);

				// free execution context
				vortex_ctx_free (ctx);

				//
				// free resources
				//
				fifo_destroy();

				// signal main thread that thread is about to exit
				SetEvent(pvctd->hevVortexClient_Exiting);

				// exit thread
				ExitThread(0);
			break;

			default:
				applog_logevent(SoftwareError, TEXT("Streaming"), TEXT("Streaming_Thread() - Main FSM: Unknown state reached."), 0, TRUE);
				m_vcsState = StreamingClientState_Exit;
		}
	}
}