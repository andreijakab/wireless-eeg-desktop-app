/**
 * 
 * \file graphics.h
 * \brief The header file for the program's graphic engine.
 *
 * 
 * $Id: graphics.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __GRAPHICS_H__
# define __GRAPHICS_H__


//----------------------------------------------------------------------------------------------------------
//   								Definitions
//----------------------------------------------------------------------------------------------------------
# define	NCHANNELLABELS			7

// aEEG
# define	AEEG_AMP_MARKER_uV_1	10.0			// amplitude that should be indicated in the aEEG graph by a horizontal line
# define	AEEG_AMP_MARKER_uV_2	60.0			// amplitude that should be indicated in the aEEG graph by a horizontal line

//----------------------------------------------------------------------------------------------------------
//   								Structs/Enums
//----------------------------------------------------------------------------------------------------------
typedef struct
{
    LONG    left;
    LONG    top;
    LONG    right;
    LONG    bottom;
	LONG	vcenter;
	LONG	height;
	LONG	width;
} RECTex;

typedef struct
{
	RECTex AvailableDrawingArea;					// client space that is not taken up by rebar and statusbar
	RECTex UseableDrawingArea;						// AvailableDrawingArea allocated for the rendering of data signals
	RECTex EEGDrawingArea;							// UseableDrawingArea allocated for rendering of EEG signals
	RECTex GyroDrawingArea;							// UseableDrawingArea allocated for rednering of Gyroscope signals
	RECTex EEGChannels[EEGCHANNELS];				// UseableDrawingArea allocated for rendering each EEG channel
	RECTex ChannelHeadings[EEGCHANNELS + 1];		// AvailableDrawingArea allocated for the signal headings
} DrawingAreas;

typedef struct
{
	double XScale;
	double GyroYScale;
} DrawingScales;

typedef struct
{
	HPEN SignalTraces[EEGCHANNELS + ACCCHANNELS];
	HPEN SignalEraser;
	HPEN EEGGyroBorder;
	HPEN HeadingsBorder;
	HPEN EEGChannelBorder;
	HPEN TimeMarker;
	HPEN AmplitudeMarker;
} DrawingPens;

typedef struct
{
	HBRUSH SignalEraser;
	HBRUSH Heading;
} DrawingBrushes;

typedef struct
{
	int				EEGChannelSwitchbox [EEGCHANNELS];
	LONG			CurrentYPos[EEGCHANNELS + ACCCHANNELS];
	LONG		    CurrentXPos;
	LONG			HeadingWidth;
	unsigned int	NEEGTraces;
	unsigned int	NGyroTraces;
} DrawingVariables;

typedef struct
{
	unsigned int	NTraces;
	unsigned int *	pHorizontalLocations;
} TimeMarkers;

typedef struct
{
	TCHAR			Label[4];
	unsigned int	LabelLocation_y;
	unsigned int	LineLocation_y;
} AmplitudeMarker;

typedef struct
{
	unsigned int	NMarkers;
	AmplitudeMarker * pAmplitudeMarkers;
} AmplitudeMarkers;

//----------------------------------------------------------------------------------------------------------
//   								Prototypes
//----------------------------------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void		GraphicsEngine_AEEG_DrawDynamicNew(HDC hDC, double ** dblData, unsigned int uintStartIndex, unsigned int uintNNewSamples);
void		GraphicsEngine_AEEG_DrawDynamicOld(HDC hDC, double ** dblData, unsigned int uintDisplayBufferID);
void		GraphicsEngine_AEEG_DrawStatic(HDC hDC);
void		GraphicsEngine_CalculateDrawingAreas(BOOL blnIsFullScreen);
void		GraphicsEngine_CalculateScales(void);
void		GraphicsEngine_CleanUp(void);
void		GraphicsEngine_EEG_DrawStatic(HDC hDC);
void		GraphicsEngine_EEG_DrawDynamicNew(HDC hDC, double ** dblData, unsigned int uintStartIndex, unsigned int uintNNewSamples, double dblEEGYScale);
void		GraphicsEngine_EEG_DrawDynamicOld(HDC hDC, double ** dblData, unsigned int uintDisplayBufferID, double dblEEGYScale);
BOOL		GraphicsEngine_Init(HWND hwndMainWindow, HWND hwndRebar, HWND hwndStatusBar, unsigned int uintNEEGTraces, BOOL blnDrawGyroTraces, CONFIGURATION cfgConfiguration, BOOL blnIsFullScreen, unsigned int uintDisplayBufferLength, unsigned int uintAEEGBufferLength, FILE ** pflGraphicsDebug);
BOOL		GraphicsEngine_IsInit(void);
RECT		GraphicsEngine_GetDrawingRect(void);
void		GraphicsEngine_SetDisplayBufferLength(unsigned int uintDisplayBufferLength);

#ifdef __cplusplus
}
#endif

#endif