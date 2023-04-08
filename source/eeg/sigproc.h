/**
 * \ingroup		grp_drivers 
 *
 * \file		sigproc.c
 * \since		15.11.2010
 * \author		Andrei Jakab (andrei.jakab@tut.fi)
 *
 * \brief		Header file of the module that contains all of the signal processing functions of the software.
 *
 * $Id: sigproc.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __SIGPROC_H__
# define __SIGPROC_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define LP_FILTER_BUFFER_LENGTH		43
# define NFILTER_STAGES_aEEG			3

//---------------------------------------------------------------------------
//   								Constants
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//   								Structs/Enums
//---------------------------------------------------------------------------
struct FIR_Filter
{
	double * Coefficients;
	unsigned int Order;
	double * Buffer;
	unsigned int BufferID;
};

struct LocalMax
{
	double Value;
	unsigned int Count;
};

//---------------------------------------------------------------------------
//   								Prototypes
//---------------------------------------------------------------------------
BOOL	sp_init(void);
void	sp_cleanup(void);
void	sp_FilterAEEGSignal(short ** pshrSampleBuffer, double ** pdblDisplayBuffer, unsigned int uintDisplayBufferLength, unsigned int * puintDisplayBufferID, unsigned int uintNNewSamples);
void	sp_FilterEEGSignal(short ** pshrSampleBuffer, double ** pdblDisplayBuffer, unsigned int uintDisplayBufferLength, unsigned int * puintDisplayBufferID, unsigned int uintNNewSamples, int intLPFilterIndex);
void	sp_FilterAllPass(short ** pshrSampleBuffer, double ** pdblDisplayBuffer, unsigned int uintDisplayBufferLength, unsigned int * puintDisplayBufferID, unsigned int uintNNewSamples);
void	sp_GetLPFiltersFc(float * pfltLPCutOffFrequenciesBuffer, unsigned int uintLPCutOffFrequenciesBufferLength);

# endif