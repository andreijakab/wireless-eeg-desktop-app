/**
 * 
 * \file serial.cpp
 * \brief The implementation file for the module that handles the serial port connection to WEEG device.
 *
 * 
 * $Id: serialV4.cpp 76 2013-02-14 14:26:17Z jakab $
 */

# include "serialV4.h"

static HANDLE	m_hCOMPort;

/**
 * \brief Open and initialize the requested serial port. Also, test selected serial port for suitability (must support baud rate of 230kbps).
 *
 * The function first attempts to create a handle for the requested COM port (\cintCOMPort). If successfull,
 * it configures the port for 230400N81 serial communication and configures the size of the port's buffers.
 * Finally, it sets the port's read timeout.
 *
 * \param[in]	intCOMPort		windows serial port number (1 - 256)
 * \return Handle to the COM port if the function is successful; otherwise, INVALID_HANDLE_VALUE.
 */
DWORD serial_OpenPort (int intCOMPort)
{
	COMMTIMEOUTS ctTimeout;
	DWORD dwReturnCode = ERROR_SUCCESS;
	DCB dcbConfig;
	int i, intRC;
	TCHAR strCOMPort[10];
	tReceivedData trdReceivedData;
		
	//
	// establish and configure communication link
	//
	// Open COM port
	_stprintf_s (strCOMPort, sizeof(strCOMPort)/sizeof(TCHAR), TEXT("\\\\.\\COM%d"), intCOMPort);
	m_hCOMPort = CreateFile (strCOMPort, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (m_hCOMPort == INVALID_HANDLE_VALUE)
	{
		dwReturnCode = GetLastError();
		return dwReturnCode;
	}
	
	// Retrieves the current control settings for COM port
	SecureZeroMemory(&dcbConfig, sizeof(DCB));
	dcbConfig.DCBlength = sizeof(DCB);
	if(!GetCommState (m_hCOMPort, &dcbConfig))
	{
		dwReturnCode = GetLastError();
		
		CloseHandle(m_hCOMPort);
		m_hCOMPort = INVALID_HANDLE_VALUE;
		return dwReturnCode;
	}

	// Set to 230400N81
	dcbConfig.BaudRate = SERPORT_SPEED;
	dcbConfig.StopBits = ONESTOPBIT;
	dcbConfig.Parity = NOPARITY;
	dcbConfig.ByteSize = 8;
	dcbConfig.fOutxCtsFlow = FALSE;
	dcbConfig.fOutxDsrFlow  = FALSE;
	dcbConfig.fDtrControl = DTR_CONTROL_ENABLE;
	dcbConfig.fDsrSensitivity = FALSE;
	dcbConfig.fOutX = FALSE;
	dcbConfig.fInX = FALSE;
	dcbConfig.fNull = FALSE;
	dcbConfig.fRtsControl = RTS_CONTROL_ENABLE;
	dcbConfig.fAbortOnError = FALSE;
	
	// Configure COM port 
	if (!SetCommState (m_hCOMPort, &dcbConfig))
	{
		dwReturnCode = GetLastError();
		CloseHandle (m_hCOMPort);

		m_hCOMPort = INVALID_HANDLE_VALUE;
		return dwReturnCode;
	}
	
	// Set serial port buffers
	if (!SetupComm (m_hCOMPort, SERPORT_INQUEUE, SERPORT_OUTQUEUE))
	{
		dwReturnCode = GetLastError();
		CloseHandle (m_hCOMPort);

		m_hCOMPort = INVALID_HANDLE_VALUE;
		return dwReturnCode;
	}
	
	// Set COM port read timeout: return immediately with the bytes that have already been received, even if no bytes have been received.
	SecureZeroMemory(&ctTimeout, sizeof (ctTimeout));
	ctTimeout.ReadIntervalTimeout = MAXDWORD;     
	if(!SetCommTimeouts (m_hCOMPort, &ctTimeout))
	{
		dwReturnCode = GetLastError();

		CloseHandle(m_hCOMPort);
		m_hCOMPort = INVALID_HANDLE_VALUE;
		return dwReturnCode;
	}

	//
	// poll coordinator
	//
	// try to poll 3X
	SecureZeroMemory(&trdReceivedData, sizeof(trdReceivedData));
	for (i = 0; i < MAX_RETRIES; i++)
	{
		// flush COM port buffers
		PurgeComm(m_hCOMPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
		
		// send poll packet
		if(serial_SendPacket(SER_POLL) == ERROR_SUCCESS)
		{
			// wait for MCU's response
			Sleep (20);
			while ((intRC = serial_ReceivedDataStateMachine (&trdReceivedData)) != ERR_NODATA)
			{
				if (intRC == ERR_NOERROR && trdReceivedData.PacketType == SER_ACK)
					return dwReturnCode;
			}
			
			// if the MCU's response is not an ACK, restart loop
			if (intRC == ERR_NODATA)
				continue;
		}
		else
			continue;
	}	
	
	// if we got to this point, coordinator did not respond to poll packet
	serial_ClosePort();
	
	return ERROR_BAD_UNIT;
}

void serial_ClosePort (void)
{
	if(m_hCOMPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hCOMPort);
		m_hCOMPort = INVALID_HANDLE_VALUE;
	}
}

/**
 * \brief Construct and send a packet to the MCU.
 *
 * The function appends \c uintOutputLength elements from the array \c pshrOutput (of type \e short) to the
 * one dimensional (1xN) MATLAB matrix \c strMatrixName (of type \e int16) located in the .MAT file whose path is \c strMatFile.
 * The function fails if the .MAT file cannot be opened, if the MATLAB matrix cannot be found or if
 * the .MAT file cannot be successfully closed.
 *
 * \param[in]	SP				pointer to character array containing the path of the .MAT file (can be relative or fully qualified)
 * \param[in]	hCOMPort		pointer to character array containing the name of the \e int16 MATLAB matrix in the .MAT file
 * \return TRUE(1) if successfull, FALSE(0) otherswise.
 */
DWORD serial_SendPacket(WEEGPacketTypes wptPacketType, ...)
{
	BYTE * pbytPayload;
	DWORD dwrdNBytesWritten, dwrdData;
	tPacket_Basic Packet;
	tPacket_PARAMS Payload_Parameters[8];
	tPacket_DEVMASK Payload_DeviceMask;
	unsigned int i;
	va_list vaArguments;

	// initialize header fields
	SecureZeroMemory(&Packet, sizeof(tPacket_Basic));
	Packet.Preamble[0] = Packet.Preamble[1] = Packet.Preamble[2] = Packet.Preamble[3] = PREAMBLE;
	Packet.PacketType = (BYTE) wptPacketType;
	Packet.Checksum = 0;
	
	//
	// set packet payload based on packet type
	//
	switch(wptPacketType)
	{
		case SER_POLL:
		case SER_ACK:
		case SER_NACK:
		case SER_STATUSQRY:
		case SER_CHANQRY:
			// set packet length
			Packet.DataLength = 0;
		break;

		case SER_PARAMS:
			SecureZeroMemory(&Payload_Parameters, sizeof(Payload_Parameters));

			// set packet length
			Packet.DataLength = sizeof(Payload_Parameters);
			
			//
			// parse input arguments
			//
			// initialize variable arguments
			va_start( vaArguments, wptPacketType);

			Payload_Parameters[0].ChannelMask = va_arg(vaArguments, BYTE);
			Payload_Parameters[0].SampleRate = va_arg(vaArguments, WORD);
			
			// reset variable arguments.
			va_end(vaArguments);

			pbytPayload = (BYTE *) &Payload_Parameters;
		break;

		case SER_DEVMASK:
			// set packet length
			Packet.DataLength = sizeof(tPacket_DEVMASK);
			
			//
			// parse input arguments
			//
			// initialize variable arguments
			va_start( vaArguments, wptPacketType);

			Payload_DeviceMask.DeviceMask = va_arg(vaArguments, BYTE);
			Payload_DeviceMask.NetworkNr = va_arg(vaArguments, WORD);
			
			// reset variable arguments.
			va_end(vaArguments);

			pbytPayload = (BYTE *) &Payload_DeviceMask;
		break;
		
		case SER_CHMASK:
			// set packet length
			Packet.DataLength = sizeof(DWORD);
			
			//
			// parse input arguments
			//
			// initialize variable arguments
			va_start( vaArguments, wptPacketType);

			dwrdData = va_arg( vaArguments, int);
		
			// reset variable arguments.
			va_end(vaArguments);

			pbytPayload = (BYTE *) &dwrdData;
		break;

		default:
			return 999999999;
	}
	
	//
	// create checksum
	//
	// compute header checksum
	for (i = sizeof (Packet.Preamble); i < SERHDR_SIZE; i++)
		Packet.Checksum += ((LPBYTE) &Packet) [i];
	
	// compute data checksum
	if(Packet.DataLength > 0)
	{
		for (i = 0; i < Packet.DataLength; i++)
			Packet.Checksum += pbytPayload [i];
	}

	// one's complement of checksum
	Packet.Checksum = ~Packet.Checksum;
	
	//
	// send packet
	//
	// header
	if(!WriteFile (m_hCOMPort, &Packet, SERHDR_SIZE, &dwrdNBytesWritten, NULL))
		return GetLastError();

	// payload (if any)
	if(Packet.DataLength > 0)
		if(!WriteFile (m_hCOMPort, pbytPayload, Packet.DataLength, &dwrdNBytesWritten, NULL))
			return GetLastError();

	// checksum
	if(!WriteFile (m_hCOMPort, &Packet.Checksum, 1, &dwrdNBytesWritten, NULL))
		return GetLastError();

	return ERROR_SUCCESS;
}


/**
 * \brief Construct and send "stop sampling" packet to the MCU.
 *
 * ...
 *
 * \return Nothing.
 */
DWORD serial_StopSampling (void)
{
	// Stop active transmissions
	return serial_SendPacket (SER_PARAMS, 0x00, 0x0000);
}

/**
 * \brief Function that reads serial data and assembles packets.
 *
 * ...
 *
 * \param[in]	RD
 * \param[in]	NReceived
 * \return ...
 */
// Handles received characters
SerialCommunicationResult serial_ReceivedDataStateMachine (tReceivedData * RD)
{
	BYTE B;

	while (1)
	{
		if (RD->BufferPos >= RD->BufferLen)
		{
			RD->BufferPos = 0;
			ReadFile (m_hCOMPort, RD->Buffer, SERBUF_RECVSTATE, &RD->BufferLen, NULL);
			
			if (!RD->BufferLen)
				return (ERR_NODATA);                                  // No more characters
		}

		for (; RD->BufferPos < RD->BufferLen; RD->BufferPos++)
		{
			switch (RD->State)
			{
				case RPPS_PREAMBLE1:
					if (RD->Buffer [RD->BufferPos] == PREAMBLE)
						RD->State = RPPS_PREAMBLE2;
				break;

				case RPPS_PREAMBLE2:
					if (RD->Buffer [RD->BufferPos] == PREAMBLE)
						RD->State = RPPS_PREAMBLE3;
					else
						RD->State = RPPS_PREAMBLE1;
				break;

				case RPPS_PREAMBLE3:
					if (RD->Buffer [RD->BufferPos] == PREAMBLE)
						RD->State = RPPS_PREAMBLE4;
					else
						RD->State = RPPS_PREAMBLE1;
				break;

				case RPPS_PREAMBLE4:
					if (RD->Buffer [RD->BufferPos] == PREAMBLE)
						RD->State = RPPS_TYPE;
					else
						RD->State = RPPS_PREAMBLE1;
				break;

				case RPPS_TYPE:
					RD->Checksum = RD->PacketType = RD->Buffer [RD->BufferPos];
					RD->State = RPPS_LENGTH;
				break;

				case RPPS_LENGTH:
					RD->PacketDataLen = RD->Buffer [RD->BufferPos];
					RD->Checksum += RD->Buffer [RD->BufferPos]; 
				
					if (RD->PacketDataLen)
						RD->State = RPPS_DATA;
					else
						RD->State = RPPS_CHECKSUM;
					
					RD->PacketDataPos = 0;
				break;

				case RPPS_DATA:
					RD->PacketData [RD->PacketDataPos++] = RD->Buffer [RD->BufferPos];
					RD->Checksum += RD->Buffer [RD->BufferPos];
					
					if (RD->PacketDataPos >= RD->PacketDataLen)
						RD->State = RPPS_CHECKSUM;
				break;

				case RPPS_CHECKSUM:
					B = (~RD->Buffer [RD->BufferPos]) & 0xFF;
					
					// Start from next character next time
					RD->State = RPPS_PREAMBLE1;    
					RD->BufferPos++;
					
					if (RD->Checksum == B)
						return (ERR_NOERROR);
					else
						return (ERR_CHECKSUM);
				break;

				default:
					RD->State = RPPS_PREAMBLE1;
			}
		}
	}
}

DWORD serial_StartSampling(BYTE bytDeviceMask, WORD wrdNetworkNr, DWORD drwdRadioChannelMask, BYTE bytMeasurementChannelMask, WORD wrdSampleRate)
{
	int intRC;
	tReceivedData trdReceivedData;
	unsigned int i;
	
	SecureZeroMemory(&trdReceivedData, sizeof(tReceivedData));

	if(m_hCOMPort != INVALID_HANDLE_VALUE)
	{
		// try to start sampling 3X
		for (i = 0; i < MAX_RETRIES; i++)
		{
			// flush COM port buffers
			PurgeComm(m_hCOMPort, PURGE_RXCLEAR | PURGE_TXCLEAR);
			
			//
			// configure device mask
			//
			if(serial_SendPacket(SER_DEVMASK, bytDeviceMask, wrdNetworkNr) == ERROR_SUCCESS)
			{
				// wait for MCU's response
				Sleep (20);
				while ((intRC = serial_ReceivedDataStateMachine (&trdReceivedData)) != ERR_NODATA)
				{
					if (intRC == ERR_NOERROR && trdReceivedData.PacketType == SER_ACK)
						break;
				}
				
				// if the MCU's response is not an ACK, restart loop
				if (intRC == ERR_NODATA)
					continue;
			}
			else
				continue;

			//
			// configure radio channel mask
			//
			if(drwdRadioChannelMask > 0)
			{
				if(serial_SendPacket(SER_CHMASK, drwdRadioChannelMask) == ERROR_SUCCESS)
				{
					// wait for MCU's response
					Sleep (5000);
					while ((intRC = serial_ReceivedDataStateMachine (&trdReceivedData)) != ERR_NODATA)
					{
						if (intRC == ERR_NOERROR && trdReceivedData.PacketType == SER_ACK)
							break;
					}
					
					// if the MCU's response is not an ACK, restart loop
					if (intRC == ERR_NODATA)
						continue;
				}
				else
					continue;
			}

			//
			// configure measurement parameters
			//
			if(serial_SendPacket(SER_PARAMS, bytMeasurementChannelMask, wrdSampleRate) == ERROR_SUCCESS)
			{
				// wait for MCU's response
				Sleep (20);
				while ((intRC = serial_ReceivedDataStateMachine (&trdReceivedData)) != ERR_NODATA)
				{
					if (intRC == ERR_NOERROR && trdReceivedData.PacketType == SER_ACK)
						return ERROR_SUCCESS;
				}
				
				// if the MCU's response is not an ACK, restart loop
				if (intRC == ERR_NODATA)
					continue;
			}
			else
				continue;
		}
		
		if(i == MAX_RETRIES)
			return ERR_OPERATIONFAIL;
	}
	
	return ERR_OPERATIONFAIL;
}

unsigned char serial_DetectWEEGPort(unsigned char * puchrPortBuffer, unsigned char uchrPortBufferLen)
{
	TCHAR strBuf[4096], strTemp[4];
	TCHAR * pchrM;
	HDEVINFO hDevInfo;
	int i, intTemp;
	GUID guidPortClass;
	SP_DEVINFO_DATA DeviceInfoData;
	SP_DRVINFO_DATA DriverInfoData;
	unsigned char uchrNDevicesFound;
	unsigned int uintWEEGCOMPort, uintMemberIndex;

#ifdef _DEBUG
	// make sure buffer is actually as long as advertised
	memset(puchrPortBuffer, 0xAE, uchrPortBufferLen*sizeof(unsigned char));
#endif

	// initalize variables
	i = 0;
	strBuf[0] = TEXT('\0');
	uintWEEGCOMPort = 0;
	uintMemberIndex = 0;
	uchrNDevicesFound = 0;
	
	// initialize DeviceInfoData struct
	SecureZeroMemory(&DeviceInfoData, sizeof(SP_DEVINFO_DATA));
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	// initialize DriverInfoData struct
	SecureZeroMemory(&DriverInfoData, sizeof(SP_DRVINFO_DATA));
	DriverInfoData.cbSize = sizeof(SP_DRVINFO_DATA);

	// initialize "Ports (COM & LPT ports)" ClassGuid 
	guidPortClass.Data1 = 0x4d36e978;
	guidPortClass.Data2 = 0xe325;
	guidPortClass.Data3 = 0x11ce;
	guidPortClass.Data4[0] = 0xbf;
	guidPortClass.Data4[1] = 0xc1;
	guidPortClass.Data4[2] = 0x08;
	guidPortClass.Data4[3] = 0x00;
	guidPortClass.Data4[4] = 0x2b;
	guidPortClass.Data4[5] = 0xe1;
	guidPortClass.Data4[6] = 0x03;
	guidPortClass.Data4[7] = 0x18;

	// get handle to device information set that contains device information elements about the available ports
	hDevInfo = SetupDiGetClassDevs(&guidPortClass,
								   NULL,							// no specific PnP enumerator
								   NULL,							// no top-level window required since no device instance will be installed
								   DIGCF_PRESENT | DIGCF_PROFILE);	// return only devices that are present in the system and that that are part of the current hardware profile
	if(hDevInfo != INVALID_HANDLE_VALUE)
	{
		// loop through all the system's ports
		while(uchrNDevicesFound < uchrPortBufferLen)
		{
			if(SetupDiEnumDeviceInfo(hDevInfo, uintMemberIndex, &DeviceInfoData))
			{
				// get the port's 'friendly name'
				if(SetupDiGetDeviceRegistryProperty(hDevInfo,
													&DeviceInfoData,
													SPDRP_FRIENDLYNAME,
													NULL,
													(PBYTE) strBuf,
													sizeof(strBuf)/sizeof(TCHAR),
													NULL))
				{
					// check to see if the device's name matches the WEEG3's USB driver device name
					if(_tcsstr(strBuf, DEVICE_FRIENDLY_NAME) != NULL)
					{
						// parse COM port number
						pchrM = _tcsrchr (strBuf, (int) TEXT('M')) + 1;							// +1 in order to get index of the first digit
						while(pchrM != NULL && *pchrM != TEXT(')') && *pchrM != TEXT('\0') && i<3)
							strTemp[i++] = *(pchrM++);
						strTemp[i] = TEXT('\0');
					}
				}
				
				// ensure that this is the right COM port by checking driver details
				if(SetupDiGetDeviceRegistryProperty(hDevInfo,
													&DeviceInfoData,
													SPDRP_MFG,
													NULL,
													(PBYTE) strBuf,
													sizeof(strBuf)/sizeof(TCHAR),
													NULL))
				{
					// check to see if the device's manufacturer matches the WEEG3's USB driver device manufacturer name
					if(_tcsstr(strBuf, DEVICE_MANUFACTURER) != NULL)
					{
						intTemp = _tstoi(strTemp);
						puchrPortBuffer[uchrNDevicesFound++] = (unsigned char) intTemp;
					}

				}
			}
			else
				break;
			
			// check next port in list
			uintMemberIndex++;
		}
	}

	return uchrNDevicesFound;
}