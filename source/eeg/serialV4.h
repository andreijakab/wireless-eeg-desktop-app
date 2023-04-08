/**
 * 
 * \file serial.h
 * \brief The header file for the module that handles the serial port connection to WEEG device.
 *
 * 
 * $Id: serialV4.h 74 2013-01-22 16:29:42Z jakab $
 */

# ifndef __SERIAL_H__
# define __SERIAL_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER			0x0502								// application requires at least Windows XP SP2
#define _WIN32_WINNT	0x0502									// application requires at least Windows XP SP2
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//---------------------------------------------------------------------------
//   								Includes
//---------------------------------------------------------------------------
# include <stdio.h>
# include <stdarg.h>
# include <tchar.h>

# include <windows.h>
# include <Setupapi.h>

# include "globals.h"

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define SERPORT_SPEED			230400				// Transmission speed
# define SERPORT_INQUEUE		16384				// Serial receive buffer length 
# define SERPORT_OUTQUEUE		256					// Serial transmit buffer length

# define SERBUF_RECVSTATE		512					// Temporary buffer for the receive state machine

# define SERHDR_SIZE			6					// # bytes in packet header (i.e. Preamble + PacketType + DataLength)

# define PREAMBLE				0xFF				// preamble character

# define SAMPLES_PER_PACKET		50					// Maximum number of 16b WORDs per packet

// FTDI USB Driver 2.04.06 data
# define DEVICE_FRIENDLY_NAME	TEXT("USB Serial Port (COM")
# define DEVICE_MANUFACTURER	TEXT("FTDI")

# define TRANSFER_IDLE			10							// Idle time between polling rounds in SampleThread (milliseconds)
# define TRANSFER_CHARWAIT		100							// Maximum wait time between characters when header is received
# define TRANSFER_PACKWAIT		2500						// Maximum wait time between packets (milliseconds)
# define MAX_RETRIES			3							// Maximum number of re-transmissions

//----------------------------------------------------------------------------------------------------------
//   								Enums
//----------------------------------------------------------------------------------------------------------
// received packet parsing state
typedef enum {RPPS_PREAMBLE1	= 0x00,		// preamble byte 1
			  RPPS_PREAMBLE2,				// preamble byte 2
			  RPPS_PREAMBLE3,				// preamble byte 3
			  RPPS_PREAMBLE4,				// preamble byte 4
			  RPPS_TYPE,					// packet type byte
			  RPPS_LENGTH,					// data length byte
			  RPPS_DATA,					// data section
			  RPPS_CHECKSUM,				// checksum byte
			 } ReceivedPacketParseState;

// WEEG serial packet types
typedef enum {SER_POLL      = 0x00,				// POLL: existence check used for serial port autodetect (PC -> MC)
			  SER_ACK       = 0x10,				// ACK: acknowledge received packet (PC <-> MC)
			  SER_NACK      = 0x11,				// NACK: negative acknolwedgment (PC <-> MC)
			  SER_DATA      = 0x20,				// DATA: measurement data from radios (PC <- MC)
			  SER_PARAMS	= 0x30,				// PARAMS: set sampling params (PC -> MC)
			  SER_DEVMASK	= 0x40,				// DEVMASK: set list of allowed devices (PC -> MC)
			  SER_CHMASK	= 0x50,				// CHMASK: set list of allowed radio channels (PC -> MC)
			  SER_BEACONCMD	= 0x60,				// BEACONCMD: free-format commands to the net (PC -> MC)
			  SER_BEACONRPL	= 0x61,				// BEACONREPLY: reply to BEACONCMD (PC <- MC)
			  SER_STATUSQRY	= 0x70,				// STATUSQUERY: query network status (PC -> MC)
			  SER_STATUSRPL	= 0x71,				// STATUSREPLY: reply to STATUSQUERY (PC <- MC)
			  SER_CHANQRY	= 0x80,				// CHANNELQUERY: query for current radio channel (PC -> MC)
			  SER_CHANRPL	= 0x81,				// CHANNELREPLY: reply to CHANNELQUERY (PC <- MC)
			 } WEEGPacketTypes;

// Serial communication constants
typedef enum {ERR_NOERROR		= 0,
			  ERR_SERPORT_OPEN	= -10,			// Serial port can't be opened
			  ERR_WRITE			= -200,			// File write failed
 			  ERR_NOTPRESENT	= -100,
			  ERR_INVVALUE		= -300,
			  ERR_NODATA		= -400,
			  ERR_CHECKSUM		= -500,
			  ERR_OPERATIONFAIL	= -600
			} SerialCommunicationResult;
//----------------------------------------------------------------------------------------------------------
//   								Structs
//----------------------------------------------------------------------------------------------------------
# pragma pack (push, 1)
//
// basic packet types
//
// basic packet structure
typedef struct
{
	BYTE Preamble [4];							// BYTE[4] preamble with each byte equal to 0xFF. It is guaranteed that the measurement data packets won’t contain this
												// pattern in the data field. Thus, if the PC detects a serial link error, it should try to
												// regain synchronization using the preamble field.

	BYTE PacketType;							// BYTE[1] specifying the packet type (see WEEGPacketTypes declaration)
	
	BYTE DataLength;							// BYTE[1] specifies the payload length; for
												// example, with packet 0x50 (CHMASK) the value is 4, because the actual channel
												// mask is represented as a 32b bitfield. As the length byte doesn’t specify the actual
												// packet length, it can be zero.
	
	LPBYTE Data;								// extra data (parameters, measurements, etc.)
	
	BYTE Checksum;								// BYTE[1] checksum byte. The checksum is calculated simply by
												// summing all the bytes together (excluding the preamble) and then inverting the result.
}
tPacket_Basic;

// structure used for received serial packets
typedef struct
{
	// state machine variables
	BYTE	Buffer [SERBUF_RECVSTATE];			// buffer for the reception state machine
	DWORD	BufferLen;							// amount of data in buffer
	DWORD	BufferPos;							// current position within the buffer
	ReceivedPacketParseState State;				// state representing the field being currently parsed in the received packet
	BYTE	Checksum;							// checksum calculated thus far
	BYTE	PacketDataPos;						// data buffer position 

	// packet variables
	BYTE PacketType;							// BYTE[1] specifying the packet type (see WEEGPacketTypes declaration)
	
	BYTE PacketDataLen;							// BYTE[1] specifies the payload length; for
												// example, with packet 0x50 (CHMASK) the value is 4, because the actual channel
												// mask is represented as a 32b bitfield. As the length byte doesn’t specify the actual
												// packet length, it can be zero.
	
	BYTE PacketData[256 + 1];					// extra data (parameters, measurements, etc.)
}
tReceivedData;

//
// packet payloads
//
// measurement parameters (PC -> MC)
typedef struct
{
	BYTE ChannelMask;							// BYTE[1] specifying the measurement channel mask. The measurement devices
												// have (at most) eight 16b measurement channels and this bit mask can be used to
												// enable/disable them individually. The LSB signifies the channel 0. The
												// measurement is stopped, if this field is zero (0x00).

	WORD SampleRate;							// WORD[1] indicating the sample rate in Hz. This has to be in the range
												// 200…1000 or zero. Please note that the maximum throughput is 4000
												// measurements per second, meaning that at most 4 channels can be used at the
												// maximum sample rate. Similarly, the maximum allowed sampling speed is
												// 500Hz when all the eight measurement channels are in use.
}
tPacket_PARAMS;

// device mask (PC -> MC)
typedef struct
{
	BYTE DeviceMask;							// BYTE[1] specifying the device mask. Each measurement device has a 3b device
												// number and this number (0…7) has a corresponding bit in the device mask.
												// Thus, if the device mask is 0x41, devices 0 and 6 are allowed.
	
	WORD NetworkNr;								// WORD[1] specifying the 16b network number.
}
tPacket_DEVMASK;

// data  (PC <- MC)
typedef struct
{
	BYTE DeviceNr;								// BYTE[1] specifying the device number. Devices are numbered from 0 to 7.
	
	BYTE ChannelMask;							// BYTE[1] specifying the measurement channel mask. Exact copy of field received in PARAMS packet.
	
	DWORD TimeStamp;							// DWORD[1] specifying the time stamp. Time is measured in samples. This
												// field signifies the sample number of the first sample in the packet. If the packet
												// contains, say, 25 samples from two channels, this field is incremented by 25 on
												// every packet. It is possible to detect the missing packets by comparing the two
												// successive time stamps.
	
	WORD BatteryLevel;							// WORD[1] specifying the battery level in millivolts.

	WORD Auxilliary;							// WORD[1]: unused auxilliary measurment channel

	WORD Accelerometers [ACCCHANNELS];			// WORD[3]: acceleometer data (1 - X, 2 - Y, 3 - Z)

	WORD Measurements [SAMPLES_PER_PACKET];		// WORD[50]: Actual measurement data; contains integral multiple of measurements from all channels
}
tPacket_DATA;

# pragma pack (pop)

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
void						serial_ClosePort (void);
unsigned char				serial_DetectWEEGPort(unsigned char * puchrPortBuffer, unsigned char uchrPortBufferLen);
DWORD						serial_OpenPort (int intCOMPort);
SerialCommunicationResult	serial_ReceivedDataStateMachine (tReceivedData * RD);
DWORD						serial_SendPacket(WEEGPacketTypes wptPacketType, ...);
DWORD						serial_StartSampling(BYTE bytDeviceMask, WORD wrdNetworkNr, DWORD drwdRadioChannelMask, BYTE bytMeasurementChannelMask, WORD wrdSampleRate);
DWORD						serial_StopSampling (void);

# endif