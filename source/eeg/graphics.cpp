/**
 * 
 * \file graphics.cpp
 * \brief Module containing all the graphics engine functions.
 *
 * 
 * $Id: graphics.cpp 76 2013-02-14 14:26:17Z jakab $
 */

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WINVER			0x0502									// application requires at least Windows XP SP2
#define _WIN32_WINNT	0x0502									// application requires at least Windows XP SP2
#define _WIN32_IE		0x0600									// application requires  Comctl32.dll version 6.0 and later, and Shell32.dll and Shlwapi.dll version 6.0 and later

//----------------------------------------------------------------------------------------------------------
//   								Includes
//----------------------------------------------------------------------------------------------------------
// Windows libaries
# include <windows.h>

// CRT libraries
# include <stdio.h>
# include <tchar.h>

// custom libraries
# include "libmath.h"
# include "verttextlib.h"

// program headers
# include "globals.h"
# include "util.h"
# include "graphics.h"

//----------------------------------------------------------------------------------------------------------
//   								Constants
//----------------------------------------------------------------------------------------------------------
// EEG channel labels
const unsigned int mc_uintHCharSpacing = 3;
const unsigned int mc_uintVCharSpacing = 5;
const TCHAR * mc_strChannelLabels[NCHANNELLABELS] = {TEXT("P10"),
													 TEXT("F8"),
													 TEXT("FP2"),
													 TEXT("FP1"),
													 TEXT("F7"),
													 TEXT("P9"),
													 TEXT("Accelerometers")};

// trace colors for each channel
const COLORREF mc_clrTraces[EEGCHANNELS + ACCCHANNELS] = { RGB (0xFF, 0x00, 0x00), RGB (0xFF, 0x00, 0x00), RGB (0xFF, 0x00, 0x00),
														   RGB (0xFF, 0x00, 0x00), RGB (0xFF, 0x00, 0x00), RGB (0xFF, 0x00, 0x00),
														   RGB (0xC0, 0xC0, 0x00), RGB (0xC0, 0x00, 0x00), RGB (0x00, 0x00, 0xC0) };

// height of the window where the gyroscope data is rendered (in pixels)
const unsigned int mc_uintGyroWindowHeight = 170;

// width of border used around drawing areas (in pixels)
const unsigned short mc_shrDrawingAreaBorder = 1;

// 'amount of signal' to be cleared in front of new samples (in physical mm)
const unsigned int mc_uintClearingSpace = 10;

// color used for borders
const COLORREF mc_clrBorder = RGB (0x00, 0x00, 0x00);

// width of border between EEG channels (in pixels)
const unsigned int mc_uintEEGBorder = 1;

// width of the border between the EEG drawing area and the Gyro drawing area (in pixels)
const unsigned int mc_uintEGBorder = 2;

// width of the border between headings and drawing areas (in pixels)
const unsigned int mc_uintHeadingsBorder = 2;

// total amount of padding between the heading text and the borders (in pixels)
const unsigned short mc_shrHeadingsPadding = 4;

// width of the time marker line (in pixels)
const unsigned int mc_uintTimeMarkerWidth = 2;

// color used for time marker line
const COLORREF mc_clrTimerMarker = RGB (0x60, 0x60, 0x60);

// width of the amplitude marker line (in pixels)
const unsigned int mc_uintAmplitudeMarkerWidth = 2;

// color used for time marker line
const COLORREF mc_clrAmplitudeMarker = RGB (0x60, 0x60, 0x60);

//----------------------------------------------------------------------------------------------------------
//   								Local variables
//----------------------------------------------------------------------------------------------------------
// graphics engine variables
static AmplitudeMarkers		m_amAEEGAmplitudeMarkers[EEGCHANNELS];
static BOOL					m_blnDrawAccelerometerTraces;
static BOOL					m_blnIsGraphicsInit;
static COLORREF				m_clrHeadingsText;
static double				m_dblAEEGAmplitudeMarkersValues[2];
static DrawingAreas			m_daAEEGDrawingAreas;			// contains the coordinates of all the drawing areas needed by the rendering engine (in client coordinates)
static DrawingAreas			m_daEEGDrawingAreas;			// contains the coordinates of all the drawing areas needed by the rendering engine (in client coordinates)
static DrawingBrushes		m_dbDrawingBrushes;
static DrawingPens			m_dpPens;
static DrawingScales		m_dsSignalScales;
static DrawingVariables		m_dvDrawingVariables;
static HBITMAP				m_hbmDrawingArea;
static HFONT				m_fntEEGChannelLabel;
static HFONT				m_fntAEEGLabels;
static int					m_intSampleFrequency;
static TimeMarkers			m_tiAEEGTimeIndicators;
static unsigned int			m_uintAEEGBufferLength;
static unsigned int			m_uintClearingWidth;
static unsigned int			m_uintDisplayBufferLength;

// application variables
static HWND					m_hwndMainWindow, m_hwndRebar, m_hwndStatusBar;

#ifdef _DEBUG
FILE *						m_pflGraphicsDebug;
#endif

//----------------------------------------------------------------------------------------------------------
//   								Locally-accessible Code
//----------------------------------------------------------------------------------------------------------
static BOOL GraphicsEngine_AEEG_CalculateDrawingAreas(BOOL blnIsFullScreen)
{
	BOOL blnResult = TRUE;
	double dblChannelDrawingAreaHeight = 0.0;
	LONG lngEEGDrawingAreaHeight = 0;
	RECT rectChild;
	unsigned int i = 0, j, uintNHorizPixelsPerHour;
	
	// clear structure
	SecureZeroMemory(&m_daAEEGDrawingAreas, sizeof(DrawingAreas));

	//
	// calculate available client area
	//
	// get client area
	GetClientRect (m_hwndMainWindow, (RECT *) &m_daAEEGDrawingAreas.AvailableDrawingArea);

	// subtract rebar height
	// (GetClientRect() returns the window's height in the rectangle's bottom member)
	if(!blnIsFullScreen)
	{
		GetClientRect (m_hwndRebar, &rectChild);
		m_daAEEGDrawingAreas.AvailableDrawingArea.top += rectChild.bottom;
	}
	
	// subtract statusbar height
	// (GetClientRect() returns the window's height in the rectangle's bottom member)
	GetClientRect (m_hwndStatusBar, &rectChild);
	m_daAEEGDrawingAreas.AvailableDrawingArea.bottom -= rectChild.bottom;

	// adjust right and bottom values, which represent width and height as given by GetClientRect()
	m_daAEEGDrawingAreas.AvailableDrawingArea.bottom--;
	m_daAEEGDrawingAreas.AvailableDrawingArea.right--;

	// calculate height, width and vcenter
	m_daAEEGDrawingAreas.AvailableDrawingArea.height = m_daAEEGDrawingAreas.AvailableDrawingArea.bottom - m_daAEEGDrawingAreas.AvailableDrawingArea.top + 1;
	m_daAEEGDrawingAreas.AvailableDrawingArea.width  = m_daAEEGDrawingAreas.AvailableDrawingArea.right - m_daAEEGDrawingAreas.AvailableDrawingArea.left + 1;
	m_daAEEGDrawingAreas.AvailableDrawingArea.vcenter = (m_daAEEGDrawingAreas.AvailableDrawingArea.top + m_daAEEGDrawingAreas.AvailableDrawingArea.bottom)/2;

	//
	// calculate available client area for signal rendering
	//
	m_daAEEGDrawingAreas.UseableDrawingArea.top     = m_daAEEGDrawingAreas.AvailableDrawingArea.top;
	m_daAEEGDrawingAreas.UseableDrawingArea.left    = m_daAEEGDrawingAreas.AvailableDrawingArea.left + m_dvDrawingVariables.HeadingWidth + mc_uintHeadingsBorder;
	m_daAEEGDrawingAreas.UseableDrawingArea.right   = m_daAEEGDrawingAreas.AvailableDrawingArea.right;
	m_daAEEGDrawingAreas.UseableDrawingArea.bottom  = m_daAEEGDrawingAreas.AvailableDrawingArea.bottom;
	m_daAEEGDrawingAreas.UseableDrawingArea.height  = m_daAEEGDrawingAreas.AvailableDrawingArea.height;
	m_daAEEGDrawingAreas.UseableDrawingArea.width   = m_daAEEGDrawingAreas.UseableDrawingArea.right - m_daAEEGDrawingAreas.UseableDrawingArea.left + 1;
	m_daAEEGDrawingAreas.UseableDrawingArea.vcenter = m_daAEEGDrawingAreas.AvailableDrawingArea.vcenter;
		
	//
	// calculate aEEG drawing area (in client coordinates)
	//
	memcpy_s(&m_daAEEGDrawingAreas.EEGDrawingArea, sizeof(RECTex), &m_daAEEGDrawingAreas.UseableDrawingArea, sizeof(RECTex));
	
	//
	// calculate horizontal coordinates of time indicators 
	//
	// calculate the number of horizontal pixels per hour
	uintNHorizPixelsPerHour = (unsigned int) floor(((double) m_intSampleFrequency * 3600) / ((double) AEEG_TIME_INTERVAL));
	
	// calculate # of possible time indicators
	m_tiAEEGTimeIndicators.NTraces = (m_daAEEGDrawingAreas.EEGDrawingArea.right - m_daAEEGDrawingAreas.EEGDrawingArea.left)/uintNHorizPixelsPerHour;

	// calculate horizontal location of time indicators
	free(m_tiAEEGTimeIndicators.pHorizontalLocations);
	m_tiAEEGTimeIndicators.pHorizontalLocations = (unsigned int *) malloc(sizeof(unsigned int)*m_tiAEEGTimeIndicators.NTraces);
	if(m_tiAEEGTimeIndicators.pHorizontalLocations != NULL)
	{
		for(i = 0; i < m_tiAEEGTimeIndicators.NTraces; i++)
			m_tiAEEGTimeIndicators.pHorizontalLocations[i] = m_daAEEGDrawingAreas.EEGDrawingArea.left + (i + 1)*uintNHorizPixelsPerHour;

		// restrict AEEG drawing area to the number of hours calculated
		if(m_tiAEEGTimeIndicators.NTraces > 0)
			m_daAEEGDrawingAreas.EEGDrawingArea.right = m_tiAEEGTimeIndicators.pHorizontalLocations[m_tiAEEGTimeIndicators.NTraces - 1];
	}
	else
		blnResult = FALSE;

	//
	// calculate the individual drawing areas for each EEG channel and its associated heading (in client coordinates)
	//
	if(m_dvDrawingVariables.NEEGTraces > 0)
	{
		// calculate space available for each channel
		lngEEGDrawingAreaHeight = m_daAEEGDrawingAreas.EEGDrawingArea.bottom - m_daAEEGDrawingAreas.EEGDrawingArea.top + 1 - (m_dvDrawingVariables.NEEGTraces - 1)*mc_uintEEGBorder;
		dblChannelDrawingAreaHeight = ((double) lngEEGDrawingAreaHeight)/((double) m_dvDrawingVariables.NEEGTraces);
		
		// calculate drawing areas for each EEG signal and its associated heading
		// (+1 in order to correct for fencepost error)
		m_daAEEGDrawingAreas.EEGChannels[0].right  = m_daAEEGDrawingAreas.EEGDrawingArea.right;
		m_daAEEGDrawingAreas.EEGChannels[0].left   = m_daAEEGDrawingAreas.EEGDrawingArea.left;
		m_daAEEGDrawingAreas.EEGChannels[0].top    = m_daAEEGDrawingAreas.EEGDrawingArea.top;
		if(m_dvDrawingVariables.NEEGTraces > 1)
			m_daAEEGDrawingAreas.EEGChannels[0].bottom = (LONG) round(m_daAEEGDrawingAreas.EEGChannels[0].top + dblChannelDrawingAreaHeight);
		else
			m_daAEEGDrawingAreas.EEGChannels[0].bottom = m_daAEEGDrawingAreas.EEGDrawingArea.bottom;
		m_daAEEGDrawingAreas.EEGChannels[0].height = m_daAEEGDrawingAreas.EEGChannels[0].bottom - m_daAEEGDrawingAreas.EEGChannels[0].top + 1;
		m_daAEEGDrawingAreas.EEGChannels[0].width  = m_daAEEGDrawingAreas.EEGChannels[0].right - m_daAEEGDrawingAreas.EEGChannels[0].left + 1;
		m_daAEEGDrawingAreas.EEGChannels[0].vcenter = (m_daAEEGDrawingAreas.EEGChannels[0].bottom + m_daAEEGDrawingAreas.EEGChannels[0].top)/2;
				
		for(i=1; i<m_dvDrawingVariables.NEEGTraces; i++)
		{
			m_daAEEGDrawingAreas.EEGChannels[i].right  = m_daAEEGDrawingAreas.EEGDrawingArea.right;
			m_daAEEGDrawingAreas.EEGChannels[i].left   = m_daAEEGDrawingAreas.EEGDrawingArea.left;
			m_daAEEGDrawingAreas.EEGChannels[i].top    = m_daAEEGDrawingAreas.EEGChannels[i - 1].bottom + mc_uintEEGBorder + 1;
			if(i != m_dvDrawingVariables.NEEGTraces - 1)
				m_daAEEGDrawingAreas.EEGChannels[i].bottom = (LONG) round(m_daAEEGDrawingAreas.EEGChannels[i].top + dblChannelDrawingAreaHeight);
			else
				m_daAEEGDrawingAreas.EEGChannels[i].bottom = m_daAEEGDrawingAreas.EEGDrawingArea.bottom;
			m_daAEEGDrawingAreas.EEGChannels[i].height = m_daAEEGDrawingAreas.EEGChannels[i].bottom - m_daAEEGDrawingAreas.EEGChannels[i].top + 1;
			m_daAEEGDrawingAreas.EEGChannels[i].width  = m_daAEEGDrawingAreas.EEGChannels[i].right - m_daAEEGDrawingAreas.EEGChannels[i].left + 1;
			m_daAEEGDrawingAreas.EEGChannels[i].vcenter = (m_daAEEGDrawingAreas.EEGChannels[i].bottom + m_daAEEGDrawingAreas.EEGChannels[i].top)/2;
		}
	}

	//
	// calculate vertical coordinates of amplitude indicators 
	//
	for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
	{
		for(j = 0; j < m_amAEEGAmplitudeMarkers[i].NMarkers; j++)
		{
			m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].LineLocation_y = util_map(m_dblAEEGAmplitudeMarkersValues[j],
																			  0, 20,
																			  m_daAEEGDrawingAreas.EEGChannels[i].top, m_daAEEGDrawingAreas.EEGChannels[i].bottom);
			
			m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].LabelLocation_y = m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].LineLocation_y + 3;
			
			// set label text
			if(j == 0)
				_stprintf_s(m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].Label,
							sizeof(m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].Label)/sizeof(TCHAR),
							TEXT("%d"),
							(int) AEEG_AMP_MARKER_uV_1);
			else
				_stprintf_s(m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].Label,
							sizeof(m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].Label)/sizeof(TCHAR),
							TEXT("%d"),
							(int) AEEG_AMP_MARKER_uV_2);
		}
	}

	//
	// calculate heading location (in client coordinates)
	//
	// calculate EEG headings
	for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
	{
		m_daAEEGDrawingAreas.ChannelHeadings[i].left	= m_daAEEGDrawingAreas.AvailableDrawingArea.left;
		// there should be a -1 at the end of next line in order to get the actual pixel where heading stops;
		// however, when windows fills in rectangles it ignores the last pixel on the right; thus +1 is needed in order to fill the whole headings area
		m_daAEEGDrawingAreas.ChannelHeadings[i].right   = m_daAEEGDrawingAreas.ChannelHeadings[i].left + m_dvDrawingVariables.HeadingWidth;
		m_daAEEGDrawingAreas.ChannelHeadings[i].top	    = m_daAEEGDrawingAreas.EEGChannels[i].top;
		m_daAEEGDrawingAreas.ChannelHeadings[i].bottom  = m_daAEEGDrawingAreas.EEGChannels[i].bottom;
		m_daAEEGDrawingAreas.ChannelHeadings[i].height  = m_daAEEGDrawingAreas.EEGChannels[i].height;
		m_daAEEGDrawingAreas.ChannelHeadings[i].width   = m_dvDrawingVariables.HeadingWidth;
		m_daAEEGDrawingAreas.ChannelHeadings[i].vcenter = (m_daAEEGDrawingAreas.ChannelHeadings[i].bottom + m_daAEEGDrawingAreas.ChannelHeadings[i].top)/2;
	}

	return blnResult;
}

static void GraphicsEngine_AEEG_DrawAmplitudeIndicators(HDC hDC)
{
	COLORREF clrOld;
	HFONT hfntOld;
	HPEN hpenOld;
	unsigned int i, j;

	// select marker pen into DC
	hpenOld = (HPEN) SelectObject(hDC, m_dpPens.AmplitudeMarker);

	// select label font into DC
	hfntOld = (HFONT) SelectObject(hDC, m_fntAEEGLabels);
	clrOld = SetTextColor(hDC, mc_clrAmplitudeMarker);

	// draw indicators
	for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
	{
		for(j = 0; j < m_amAEEGAmplitudeMarkers[i].NMarkers; j++)
		{
			// draw line
			MoveToEx (hDC, (int) m_daEEGDrawingAreas.EEGChannels[i].left, m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].LineLocation_y, NULL);
			LineTo (hDC, (int) m_daEEGDrawingAreas.EEGChannels[i].right, m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].LineLocation_y);

			// draw label
			TextOut(hDC,
					m_daAEEGDrawingAreas.ChannelHeadings[i].right + 5,
					m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].LabelLocation_y,
					m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].Label,
					_tcslen(m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers[j].Label));
		}
	}

	// put old GDI objects back into DC
	SelectObject(hDC, hfntOld);
	SelectObject(hDC, hpenOld);
	SetTextColor(hDC, clrOld);
}

static void GraphicsEngine_AEEG_DrawTimeIndicators(HDC hDC)
{
	HPEN hpenOld;
	unsigned int i;

	// select marker pen into DC
	hpenOld = (HPEN) SelectObject(hDC, m_dpPens.TimeMarker);

	// draw indicators
	for(i = 0; i < m_tiAEEGTimeIndicators.NTraces; i++)
	{
		MoveToEx (hDC, (int) m_tiAEEGTimeIndicators.pHorizontalLocations[i], m_daAEEGDrawingAreas.AvailableDrawingArea.top, NULL);
		LineTo (hDC, (int) m_tiAEEGTimeIndicators.pHorizontalLocations[i], m_daAEEGDrawingAreas.AvailableDrawingArea.bottom);
	}

	// put old pen back into DC
	SelectObject(hDC, hpenOld);
}

static void GraphicsEngine_EEG_CalculateDrawingAreas(BOOL blnIsFullScreen)
{
	double dblChannelDrawingAreaHeight = 0.0;
	LONG lngEEGDrawingAreaHeight = 0;
	RECT rectChild;
	unsigned int i = 0;
	
	// clear structure
	SecureZeroMemory(&m_daEEGDrawingAreas, sizeof(DrawingAreas));

	//
	// calculate available client area
	//
	// get client area
	GetClientRect (m_hwndMainWindow, (RECT *) &m_daEEGDrawingAreas.AvailableDrawingArea);

	// subtract rebar height
	// (GetClientRect() returns the window's height in the rectangle's bottom member)
	if(!blnIsFullScreen)
	{
		GetClientRect (m_hwndRebar, &rectChild);
		m_daEEGDrawingAreas.AvailableDrawingArea.top += rectChild.bottom;
	}
	
	// subtract statusbar height
	// (GetClientRect() returns the window's height in the rectangle's bottom member)
	GetClientRect (m_hwndStatusBar, &rectChild);
	m_daEEGDrawingAreas.AvailableDrawingArea.bottom -= rectChild.bottom;

	// adjust right and bottom values, which represent width and height as given by GetClientRect()
	m_daEEGDrawingAreas.AvailableDrawingArea.bottom--;
	m_daEEGDrawingAreas.AvailableDrawingArea.right--;

	// calculate height, width and vcenter
	m_daEEGDrawingAreas.AvailableDrawingArea.height = m_daEEGDrawingAreas.AvailableDrawingArea.bottom - m_daEEGDrawingAreas.AvailableDrawingArea.top + 1;
	m_daEEGDrawingAreas.AvailableDrawingArea.width  = m_daEEGDrawingAreas.AvailableDrawingArea.right - m_daEEGDrawingAreas.AvailableDrawingArea.left + 1;
	m_daEEGDrawingAreas.AvailableDrawingArea.vcenter = (m_daEEGDrawingAreas.AvailableDrawingArea.top + m_daEEGDrawingAreas.AvailableDrawingArea.bottom)/2;

	//
	// calculate available client area for signal rendering
	//
	m_daEEGDrawingAreas.UseableDrawingArea.top     = m_daEEGDrawingAreas.AvailableDrawingArea.top;
	m_daEEGDrawingAreas.UseableDrawingArea.left    = m_daEEGDrawingAreas.AvailableDrawingArea.left + m_dvDrawingVariables.HeadingWidth + mc_uintHeadingsBorder;
	m_daEEGDrawingAreas.UseableDrawingArea.right   = m_daEEGDrawingAreas.AvailableDrawingArea.right;
	m_daEEGDrawingAreas.UseableDrawingArea.bottom  = m_daEEGDrawingAreas.AvailableDrawingArea.bottom;
	m_daEEGDrawingAreas.UseableDrawingArea.height  = m_daEEGDrawingAreas.AvailableDrawingArea.height;
	m_daEEGDrawingAreas.UseableDrawingArea.width   = m_daEEGDrawingAreas.UseableDrawingArea.right - m_daEEGDrawingAreas.UseableDrawingArea.left + 1;
	m_daEEGDrawingAreas.UseableDrawingArea.vcenter = m_daEEGDrawingAreas.AvailableDrawingArea.vcenter;
		
	//
	// calculate EEG and Gyro drawing areas (in client coordinates)
	//
	if(!m_blnDrawAccelerometerTraces)
		memcpy_s(&m_daEEGDrawingAreas.EEGDrawingArea, sizeof(RECTex), &m_daEEGDrawingAreas.UseableDrawingArea, sizeof(RECTex));
	else
	{
		if(m_dvDrawingVariables.NEEGTraces == 0)
			memcpy_s(&m_daEEGDrawingAreas.GyroDrawingArea, sizeof(RECTex), &m_daEEGDrawingAreas.UseableDrawingArea, sizeof(RECTex));
		else
		{
			// Calculate location of EEG drawing area
			// (+1 in order to correct for fencepost error)
			m_daEEGDrawingAreas.EEGDrawingArea.top     = m_daEEGDrawingAreas.UseableDrawingArea.top;
			m_daEEGDrawingAreas.EEGDrawingArea.left    = m_daEEGDrawingAreas.UseableDrawingArea.left;
			m_daEEGDrawingAreas.EEGDrawingArea.right   = m_daEEGDrawingAreas.UseableDrawingArea.right;
			m_daEEGDrawingAreas.EEGDrawingArea.bottom  = m_daEEGDrawingAreas.UseableDrawingArea.bottom - mc_uintGyroWindowHeight - mc_uintEGBorder;
			m_daEEGDrawingAreas.EEGDrawingArea.height  = m_daEEGDrawingAreas.EEGDrawingArea.bottom - m_daEEGDrawingAreas.EEGDrawingArea.top + 1;
			m_daEEGDrawingAreas.EEGDrawingArea.width   = m_daEEGDrawingAreas.EEGDrawingArea.right - m_daEEGDrawingAreas.EEGDrawingArea.left + 1;
			m_daEEGDrawingAreas.EEGDrawingArea.vcenter = (m_daEEGDrawingAreas.EEGDrawingArea.top + m_daEEGDrawingAreas.EEGDrawingArea.bottom)/2;

			// Calculate location of gyro drawing area
			// (+1 in order to correct for fencepost error)
			m_daEEGDrawingAreas.GyroDrawingArea.top     = m_daEEGDrawingAreas.UseableDrawingArea.bottom - mc_uintGyroWindowHeight + 1;
			m_daEEGDrawingAreas.GyroDrawingArea.left    = m_daEEGDrawingAreas.UseableDrawingArea.left;
			m_daEEGDrawingAreas.GyroDrawingArea.right   = m_daEEGDrawingAreas.UseableDrawingArea.right; 
			m_daEEGDrawingAreas.GyroDrawingArea.bottom  = m_daEEGDrawingAreas.UseableDrawingArea.bottom;
			m_daEEGDrawingAreas.GyroDrawingArea.height  = m_daEEGDrawingAreas.GyroDrawingArea.bottom - m_daEEGDrawingAreas.GyroDrawingArea.top + 1;
			m_daEEGDrawingAreas.GyroDrawingArea.width   = m_daEEGDrawingAreas.GyroDrawingArea.right - m_daEEGDrawingAreas.GyroDrawingArea.left + 1;
			m_daEEGDrawingAreas.GyroDrawingArea.vcenter = (m_daEEGDrawingAreas.GyroDrawingArea.top + m_daEEGDrawingAreas.GyroDrawingArea.bottom)/2;
		}
	}
	
	//
	// calculate the individual drawing areas for each EEG channel and its associated heading (in client coordinates)
	//
	if(m_dvDrawingVariables.NEEGTraces > 0)
	{
		// calculate space available for each channel
		lngEEGDrawingAreaHeight = m_daEEGDrawingAreas.EEGDrawingArea.bottom - m_daEEGDrawingAreas.EEGDrawingArea.top + 1 - (m_dvDrawingVariables.NEEGTraces - 1)*mc_uintEEGBorder;
		dblChannelDrawingAreaHeight = ((double) lngEEGDrawingAreaHeight)/((double) m_dvDrawingVariables.NEEGTraces);
		
		// calculate drawing areas for each EEG signal and its associated heading
		// (+1 in order to correct for fencepost error)
		m_daEEGDrawingAreas.EEGChannels[0].right  = m_daEEGDrawingAreas.EEGDrawingArea.right;
		m_daEEGDrawingAreas.EEGChannels[0].left   = m_daEEGDrawingAreas.EEGDrawingArea.left;
		m_daEEGDrawingAreas.EEGChannels[0].top    = m_daEEGDrawingAreas.EEGDrawingArea.top;
		if(m_dvDrawingVariables.NEEGTraces > 1)
			m_daEEGDrawingAreas.EEGChannels[0].bottom = (LONG) round(m_daEEGDrawingAreas.EEGChannels[0].top + dblChannelDrawingAreaHeight);
		else
			m_daEEGDrawingAreas.EEGChannels[0].bottom = m_daEEGDrawingAreas.EEGDrawingArea.bottom;
		m_daEEGDrawingAreas.EEGChannels[0].height = m_daEEGDrawingAreas.EEGChannels[0].bottom - m_daEEGDrawingAreas.EEGChannels[0].top + 1;
		m_daEEGDrawingAreas.EEGChannels[0].width  = m_daEEGDrawingAreas.EEGChannels[0].right - m_daEEGDrawingAreas.EEGChannels[0].left + 1;
		m_daEEGDrawingAreas.EEGChannels[0].vcenter = (m_daEEGDrawingAreas.EEGChannels[0].bottom + m_daEEGDrawingAreas.EEGChannels[0].top)/2;
				
		for(i=1; i<m_dvDrawingVariables.NEEGTraces; i++)
		{
			m_daEEGDrawingAreas.EEGChannels[i].right  = m_daEEGDrawingAreas.EEGDrawingArea.right;
			m_daEEGDrawingAreas.EEGChannels[i].left   = m_daEEGDrawingAreas.EEGDrawingArea.left;
			m_daEEGDrawingAreas.EEGChannels[i].top    = m_daEEGDrawingAreas.EEGChannels[i - 1].bottom + mc_uintEEGBorder + 1;
			if(i != m_dvDrawingVariables.NEEGTraces - 1)
				m_daEEGDrawingAreas.EEGChannels[i].bottom = (LONG) round(m_daEEGDrawingAreas.EEGChannels[i].top + dblChannelDrawingAreaHeight);
			else
				m_daEEGDrawingAreas.EEGChannels[i].bottom = m_daEEGDrawingAreas.EEGDrawingArea.bottom;
			m_daEEGDrawingAreas.EEGChannels[i].height = m_daEEGDrawingAreas.EEGChannels[i].bottom - m_daEEGDrawingAreas.EEGChannels[i].top + 1;
			m_daEEGDrawingAreas.EEGChannels[i].width  = m_daEEGDrawingAreas.EEGChannels[i].right - m_daEEGDrawingAreas.EEGChannels[i].left + 1;
			m_daEEGDrawingAreas.EEGChannels[i].vcenter = (m_daEEGDrawingAreas.EEGChannels[i].bottom + m_daEEGDrawingAreas.EEGChannels[i].top)/2;
		}
	}

	//
	// calculate heading location (in client coordinates)
	//
	// calculate EEG headings
	for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
	{
		m_daEEGDrawingAreas.ChannelHeadings[i].left	= m_daEEGDrawingAreas.AvailableDrawingArea.left;
		// there should be a -1 at the end of next line in order to get the actual pixel where heading stops;
		// however, when windows fills in rectangles it ignores the last pixel on the right; thus +1 is needed in order to fill the whole headings area
		m_daEEGDrawingAreas.ChannelHeadings[i].right   = m_daEEGDrawingAreas.ChannelHeadings[i].left + m_dvDrawingVariables.HeadingWidth;
		m_daEEGDrawingAreas.ChannelHeadings[i].top	    = m_daEEGDrawingAreas.EEGChannels[i].top;
		m_daEEGDrawingAreas.ChannelHeadings[i].bottom  = m_daEEGDrawingAreas.EEGChannels[i].bottom;
		m_daEEGDrawingAreas.ChannelHeadings[i].height  = m_daEEGDrawingAreas.EEGChannels[i].height;
		m_daEEGDrawingAreas.ChannelHeadings[i].width   = m_dvDrawingVariables.HeadingWidth;
		m_daEEGDrawingAreas.ChannelHeadings[i].vcenter = (m_daEEGDrawingAreas.ChannelHeadings[i].bottom + m_daEEGDrawingAreas.ChannelHeadings[i].top)/2;
	}
	
	// calculate gyroscopes heading
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].left	 = m_daEEGDrawingAreas.AvailableDrawingArea.left;
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].right   = m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].left + m_dvDrawingVariables.HeadingWidth;
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].top	 = m_daEEGDrawingAreas.GyroDrawingArea.top;
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].bottom  = m_daEEGDrawingAreas.GyroDrawingArea.bottom;
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].height  = m_daEEGDrawingAreas.GyroDrawingArea.height;
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].width   = m_dvDrawingVariables.HeadingWidth;
	m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].vcenter = (m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].bottom + m_daEEGDrawingAreas.ChannelHeadings[NCHANNELLABELS - 1].top)/2;
}

//----------------------------------------------------------------------------------------------------------
//   								Globally-accessible Code
//----------------------------------------------------------------------------------------------------------
void GraphicsEngine_AEEG_DrawDynamicNew(HDC hDC,
									    double ** dblData,
									    unsigned int uintStartIndex,
									    unsigned int uintEndIndex)
{
	BOOL blnTwoErasers;
	double dblSample;
	HBRUSH hbrOld;
	HPEN hpenOld;
	LONG lngOldXPos, lngOldYPos;
	RECT rc1, rc2;
	unsigned int i, j, uintIndex;
	unsigned int uintNNewSamples;

	// variable init.
	uintIndex = uintStartIndex;
	uintNNewSamples = uintEndIndex - uintStartIndex;

	// remove existing pen and brush from DC
	hpenOld = (HPEN) SelectObject(hDC, GetStockObject(WHITE_PEN));
	hbrOld = (HBRUSH) SelectObject(hDC, GetStockObject(WHITE_BRUSH));
	
	// draw new samples
	for(i = 0; i < uintNNewSamples; i++)
	{
		//
		// x-coordinate calculation
		//
		// save old position
		lngOldXPos = m_dvDrawingVariables.CurrentXPos;

		// calculate new x coordinate
		if(m_dvDrawingVariables.CurrentXPos == m_daAEEGDrawingAreas.EEGDrawingArea.right)
			m_dvDrawingVariables.CurrentXPos = m_daAEEGDrawingAreas.EEGDrawingArea.left;
		else
			m_dvDrawingVariables.CurrentXPos++;

		//
		// eraser calculations
		//
		// shift old x-coordinate back to beginning of drawing area if the new x-coordinate has wrapped around
		if(m_dvDrawingVariables.CurrentXPos < lngOldXPos)
			lngOldXPos = m_daAEEGDrawingAreas.UseableDrawingArea.left;
		
		// configure first eraser
		rc1.right = m_dvDrawingVariables.CurrentXPos + m_uintClearingWidth;
		rc1.left = rc1.right - (m_dvDrawingVariables.CurrentXPos - lngOldXPos);
		if(rc1.left < m_daAEEGDrawingAreas.UseableDrawingArea.left)
			rc1.left = m_daAEEGDrawingAreas.UseableDrawingArea.left;
		
		// if needed, configure and enable second eraser
		blnTwoErasers = FALSE;
		if(rc1.right > m_daAEEGDrawingAreas.UseableDrawingArea.right)
		{
			blnTwoErasers = TRUE;

			// configure second eraser
			rc2.left = m_daAEEGDrawingAreas.UseableDrawingArea.left;
			rc2.right = rc2.left + rc1.right - m_daAEEGDrawingAreas.UseableDrawingArea.right;
			
			// adjust border of first eraser
			rc1.right = m_daAEEGDrawingAreas.UseableDrawingArea.right; 
		}
		
		//
		// draw EEG traces
		//
		for(j = 0; j < m_dvDrawingVariables.NEEGTraces; j++)
		{
			// erase 'old' signal
			SelectObject(hDC, m_dpPens.SignalEraser);
			SelectObject(hDC, m_dbDrawingBrushes.SignalEraser);
			Rectangle(hDC, rc1.left, m_daAEEGDrawingAreas.EEGChannels[j].top, rc1.right, m_daAEEGDrawingAreas.EEGChannels[j].bottom + 1);
			if(blnTwoErasers)
				Rectangle(hDC, rc2.left, m_daAEEGDrawingAreas.EEGChannels[j].top, rc2.right, m_daAEEGDrawingAreas.EEGChannels[j].bottom + 1);
			
			// save old y-position
			lngOldYPos = m_dvDrawingVariables.CurrentYPos[j];	

			// calculate new y coordinate
			dblSample = dblData [m_dvDrawingVariables.EEGChannelSwitchbox [j]][uintIndex];
			m_dvDrawingVariables.CurrentYPos[j] = util_map(dblSample, 0, 20, m_daEEGDrawingAreas.EEGChannels[j].top, m_daEEGDrawingAreas.EEGChannels[j].bottom);

			// draw new sample
			if(uintIndex > 0)
			{
				SelectObject(hDC, m_dpPens.SignalTraces[j]);
				MoveToEx (hDC, lngOldXPos, lngOldYPos, NULL);
				LineTo (hDC, m_dvDrawingVariables.CurrentXPos, m_dvDrawingVariables.CurrentYPos[j]);
			}
		}
	
		uintIndex = (++uintIndex)%m_uintAEEGBufferLength;
	}

	//
	// draw vertical hour markers
	//
	GraphicsEngine_AEEG_DrawTimeIndicators(hDC);

	//
	// draw horizontal amplitude markers
	//
	GraphicsEngine_AEEG_DrawAmplitudeIndicators(hDC);

	//
	// clean-up
	//
	// reset DC to previous state
	if(hbrOld)
		SelectObject(hDC, hbrOld);
	if(hpenOld)
		SelectObject(hDC, hpenOld);
}

void GraphicsEngine_AEEG_DrawDynamicOld(HDC hDC,
									    double ** dblData,
									    unsigned int uintDisplayBufferID)
{
	double dblSample;
	HPEN hpenOld;
	LONG lngOldXPos, lngOldYPos;
	unsigned int i, j;

#ifdef _DEBUG
	if(m_pflGraphicsDebug != NULL)
		fprintf(m_pflGraphicsDebug, "Drawing Old Signal up until index %d\n", uintDisplayBufferID);
#endif

	// remove existing pen and brush from DC
	hpenOld = (HPEN) SelectObject(hDC, GetStockObject(WHITE_PEN));
	
	// reset position variables
	m_dvDrawingVariables.CurrentXPos = m_daAEEGDrawingAreas.UseableDrawingArea.left;
	for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
		m_dvDrawingVariables.CurrentYPos[i] = m_daAEEGDrawingAreas.EEGChannels[i].vcenter;

	// draw old signal samples
	for(i = 0; i < uintDisplayBufferID; i++)
	{
		//
		// x-coordinate calculation
		//
		// save old position
		lngOldXPos = m_dvDrawingVariables.CurrentXPos;

		// calculate new x coordinate
		if(lngOldXPos == m_daAEEGDrawingAreas.UseableDrawingArea.right)
			m_dvDrawingVariables.CurrentXPos = m_daEEGDrawingAreas.UseableDrawingArea.left;
		else
			m_dvDrawingVariables.CurrentXPos++;

		//
		// re-draw aEEG traces
		//
		for(j = 0; j < m_dvDrawingVariables.NEEGTraces; j++)
		{
			// save old y-position
			lngOldYPos = m_dvDrawingVariables.CurrentYPos[j];	

			// calculate new y coordinate
			dblSample = dblData [m_dvDrawingVariables.EEGChannelSwitchbox [j]][i];
			m_dvDrawingVariables.CurrentYPos[j] = util_map(dblSample, 0, 20, m_daEEGDrawingAreas.EEGChannels[j].top, m_daEEGDrawingAreas.EEGChannels[j].bottom);

			// draw new sample
			if(i > 0)
			{
				SelectObject(hDC, m_dpPens.SignalTraces[j]);
				MoveToEx (hDC, lngOldXPos, lngOldYPos, NULL);
				LineTo (hDC, m_dvDrawingVariables.CurrentXPos, m_dvDrawingVariables.CurrentYPos[j]);
			}
		}
	}

	//
	// draw vertical hour markers
	//
	GraphicsEngine_AEEG_DrawTimeIndicators(hDC);

	//
	// draw horizontal amplitude markers
	//
	GraphicsEngine_AEEG_DrawAmplitudeIndicators(hDC);
	
	//
	// clean-up
	//
	// reset DC to previous state
	if(hpenOld)
		SelectObject(hDC, hpenOld);
}

void GraphicsEngine_AEEG_DrawStatic(HDC hDC)
{
	COLORREF clrOld;
	HDC hdcMemory;
	HBITMAP hbmMemory, hbmOld;
	HFONT hfntOld;
	HPEN hpenOld;
	RECT rc;
	unsigned int i = 0, j = 0;

	static HBITMAP hbmpHeading;

	// initialize variables
	hfntOld = NULL;
	hpenOld = NULL;

	//
	// draw EEG related items (only if EEG traces are to be displayed)
	//
	if(m_dvDrawingVariables.NEEGTraces > 0)
	{
		//
		// draw EEG channel borders
		//
		// select border pen
		hpenOld = (HPEN) SelectObject(hDC, m_dpPens.EEGChannelBorder);
		
		// draw borders
		for(i = 0; i < (m_dvDrawingVariables.NEEGTraces - 1); i++)
		{
			MoveToEx (hDC, m_daAEEGDrawingAreas.AvailableDrawingArea.left, m_daAEEGDrawingAreas.EEGChannels[i].bottom + 1, NULL);
			LineTo (hDC, m_daAEEGDrawingAreas.AvailableDrawingArea.right + 1, m_daAEEGDrawingAreas.EEGChannels[i].bottom + 1);
		}

		//
		// draw Headings border
		//
		// select border pen
		SelectObject(hDC, m_dpPens.HeadingsBorder);
		
		// draw border
		for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
		{
			MoveToEx (hDC, (int) m_dvDrawingVariables.HeadingWidth + 1, m_daAEEGDrawingAreas.EEGChannels[i].top, NULL);
			LineTo (hDC, (int) m_dvDrawingVariables.HeadingWidth + 1, m_daAEEGDrawingAreas.EEGChannels[i].bottom);
		}

		//
		// draw Headings
		//
		// create compatible DC
		hdcMemory = CreateCompatibleDC (hDC);

		// select heading text font and color
		hfntOld = (HFONT) SelectObject(hdcMemory, m_fntEEGChannelLabel);
		clrOld = SetTextColor(hdcMemory, m_clrHeadingsText);
		
		// draw background and text
		rc.top = rc.left = 0;
		for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
		{
			// prepare DC and set variables
			hbmMemory = CreateCompatibleBitmap (hDC, m_daAEEGDrawingAreas.ChannelHeadings[i].width, m_daAEEGDrawingAreas.ChannelHeadings[i].height);
			hbmOld = (HBITMAP) SelectObject (hdcMemory, hbmMemory);
			rc.bottom = m_daAEEGDrawingAreas.ChannelHeadings[i].height;
			rc.right = m_daAEEGDrawingAreas.ChannelHeadings[i].right;

			// fill in background
			FillRect(hdcMemory, &rc, m_dbDrawingBrushes.Heading);

			// draw text
			DrawVertText(hdcMemory,
						 mc_strChannelLabels[m_dvDrawingVariables.EEGChannelSwitchbox[i]],
						 _tcslen(mc_strChannelLabels[m_dvDrawingVariables.EEGChannelSwitchbox[i]]),
						 &rc,
						 DV_HCENTER | DV_VCENTER,
						 mc_uintHCharSpacing,
						 mc_uintVCharSpacing);

			// bit-block transfer of the color data corresponding to a rectangle of pixels from memory to the client window
			BitBlt (hDC,
					m_daAEEGDrawingAreas.ChannelHeadings[i].left,
					m_daAEEGDrawingAreas.ChannelHeadings[i].top,
					m_daAEEGDrawingAreas.ChannelHeadings[i].width,
					m_daAEEGDrawingAreas.ChannelHeadings[i].height,
					hdcMemory,
					0, 0,
					SRCCOPY);
			
			// reset DC
			SelectObject (hdcMemory, hbmOld);
			DeleteObject (hbmMemory);
		}

		//
		// Clean-up
		//
		// reset and delete memory DC
		SetTextColor(hdcMemory, clrOld);
		SelectObject(hdcMemory, hfntOld);
		DeleteDC(hdcMemory);

		// reset window DC
		if(hpenOld)
			SelectObject(hDC, hpenOld);
	}
}

void GraphicsEngine_CalculateDrawingAreas(BOOL blnIsFullScreen)
{
	GraphicsEngine_AEEG_CalculateDrawingAreas(blnIsFullScreen);
	GraphicsEngine_EEG_CalculateDrawingAreas(blnIsFullScreen);
}

void GraphicsEngine_CalculateScales(void)
{
	// calculate x-scale
	m_dsSignalScales.XScale = (double) (m_daEEGDrawingAreas.UseableDrawingArea.width) / (double) (m_uintDisplayBufferLength);
	
	// calculate Gyro y-scale
	m_dsSignalScales.GyroYScale = (double) (m_daEEGDrawingAreas.GyroDrawingArea.height) / (double) (NGYROSAMPLES);

#ifdef _DEBUG
	if(m_pflGraphicsDebug != NULL)
		fprintf(m_pflGraphicsDebug, "Calculated Scales: EEG_X	%f	Gyro_Y	%f\n", m_dsSignalScales.XScale, m_dsSignalScales.GyroYScale);
#endif
}

BOOL GraphicsEngine_Init(HWND hwndMainWindow,
						 HWND hwndRebar,
						 HWND hwndStatusBar,
						 unsigned int uintNEEGTraces,
						 BOOL blnDrawGyroTraces,
						 CONFIGURATION cfgConfiguration,
						 BOOL blnIsFullScreen,
						 unsigned int uintDisplayBufferLength,
						 unsigned int uintAEEGBufferLength,
						 FILE ** pflGraphicsDebug)
{
	HDC hDC;
	HFONT hfntOld;
	LOGBRUSH lb;
	NONCLIENTMETRICS ncm;
	RECT rc;
	unsigned int i, k;
	
	// initialize local variables
	m_blnIsGraphicsInit = FALSE;
	m_blnDrawAccelerometerTraces = blnDrawGyroTraces;
	m_hwndRebar = hwndRebar;
	m_hwndStatusBar = hwndStatusBar;
	m_hwndMainWindow = FindWindow (WINDOW_CLASSID_MAIN, NULL);
	m_intSampleFrequency = cfgConfiguration.SamplingFrequency;
	m_uintDisplayBufferLength = uintDisplayBufferLength;
	m_uintAEEGBufferLength = uintAEEGBufferLength;
#ifdef _DEBUG
	if(pflGraphicsDebug != NULL)
		m_pflGraphicsDebug = *pflGraphicsDebug;
#endif

	// initialize global variables
	m_tiAEEGTimeIndicators.pHorizontalLocations = NULL;

	//
	// initialize required GDI objects
	//
	// retrieve window metrics
	ncm.cbSize = sizeof(NONCLIENTMETRICS);
	SystemParametersInfo(SPI_GETNONCLIENTMETRICS, 0, &ncm, 0);

	// create EEG channel font box font either by using the Windows message box font
	// or a default font
	m_fntEEGChannelLabel = CreateFontIndirect(&ncm.lfMessageFont);
	if(m_fntEEGChannelLabel == NULL)
	{
		m_fntEEGChannelLabel = CreateFont (DEFAULTTEXTHEIGHT,
										   0,
										   0,
										   0,
										   FW_NORMAL,
										   0,
										   0,
										   0,
										   DEFAULT_CHARSET, 
										   OUT_DEFAULT_PRECIS,
										   CLIP_DEFAULT_PRECIS,
										   DEFAULT_QUALITY, 
										   VARIABLE_PITCH | FF_SWISS,
										   TEXT("MS Sans Serif"));
	}

	// create aEEG marker font
	m_fntAEEGLabels = CreateFont (10,
							      0,
								  0,
								  0,
								  FW_NORMAL,
								  0,
								  0,
								  0,
								  DEFAULT_CHARSET, 
								  OUT_DEFAULT_PRECIS,
								  CLIP_DEFAULT_PRECIS,
								  DEFAULT_QUALITY, 
								  VARIABLE_PITCH | FF_SWISS,
								  TEXT("MS Sans Serif"));

	// initialize misc. brushes
	m_dbDrawingBrushes.Heading = (HBRUSH) (COLOR_MENUBAR+1);
	m_dbDrawingBrushes.SignalEraser = (HBRUSH) GetStockObject(WHITE_BRUSH);

	// Initialize EEG and Gyro trace pens
	for (i = 0; i < (EEGCHANNELS + ACCCHANNELS); i++)
		m_dpPens.SignalTraces[i] = CreatePen (PS_SOLID, 1, mc_clrTraces[i]);
	m_dpPens.SignalEraser = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));

	// initialize misc. pens
	m_dpPens.EEGChannelBorder = CreatePen(PS_SOLID, mc_uintEEGBorder, mc_clrBorder);
	m_dpPens.EEGGyroBorder = CreatePen(PS_SOLID, mc_uintEGBorder, mc_clrBorder);
	m_dpPens.HeadingsBorder = CreatePen(PS_SOLID, mc_uintHeadingsBorder, mc_clrBorder);
	lb.lbStyle = BS_SOLID;
	lb.lbHatch = 0;
	lb.lbColor = mc_clrAmplitudeMarker;
	m_dpPens.AmplitudeMarker = ExtCreatePen (PS_GEOMETRIC | PS_DOT, mc_uintAmplitudeMarkerWidth, &lb, 0, NULL);
	lb.lbColor = mc_clrTimerMarker;
	m_dpPens.TimeMarker = ExtCreatePen (PS_GEOMETRIC | PS_DOT, mc_uintTimeMarkerWidth, &lb, 0, NULL);

	// initialize misc. colosr
	m_clrHeadingsText = GetSysColor(COLOR_MENUTEXT);

	//
	// initialize drawing variables
	//
	// initialize headings and compute heading column width
	// select EEG label font into it
	hDC = GetDC(hwndMainWindow);
	hfntOld = (HFONT) SelectObject(hDC, m_fntEEGChannelLabel);

	// compute heading width
	SecureZeroMemory(&rc, sizeof(RECT));
	m_dvDrawingVariables.HeadingWidth = 0;
	for(i = 0; i < NCHANNELLABELS; i++)
	{
		DrawVertText(hDC, mc_strChannelLabels[i], _tcslen(mc_strChannelLabels[i]), &rc, DV_CALCRECT, mc_uintHCharSpacing, mc_uintVCharSpacing);
		if((rc.right - rc.left) > m_dvDrawingVariables.HeadingWidth)
			m_dvDrawingVariables.HeadingWidth = rc.right - rc.left;
	}

	// reset & release DC
	SelectObject(hDC, hfntOld);
	ReleaseDC(hwndMainWindow, hDC);
	
	// calculate number of traces currently selected
	m_dvDrawingVariables.NEEGTraces = uintNEEGTraces;
	if(blnDrawGyroTraces)
		m_dvDrawingVariables.NGyroTraces = ACCCHANNELS;
	else
		m_dvDrawingVariables.NGyroTraces = 0;

	// initialize channel number switchbox
	for (i = k = 0; i < EEGCHANNELS; i++)							
		if ((0x01 << i) & cfgConfiguration.DisplayChannelMask)
			m_dvDrawingVariables.EEGChannelSwitchbox [k++] = i;

	// calculate clearing width (in pixels)
	m_uintClearingWidth = (unsigned int) (cfgConfiguration.HorizontalDPC*(mc_uintClearingSpace/10.0f));

	// amplitude markers: initialize variables
	for(i = 0; i < EEGCHANNELS; i++)
	{
		// aEEG markers
		m_amAEEGAmplitudeMarkers[i].NMarkers = 2;
		m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers = (AmplitudeMarker *) malloc(sizeof(AmplitudeMarker)*m_amAEEGAmplitudeMarkers[i].NMarkers);
	}

	// amplitude markers: map uV to log scale
	m_dblAEEGAmplitudeMarkersValues[0] = 10*log10(AEEG_AMP_MARKER_uV_1);
	m_dblAEEGAmplitudeMarkersValues[1] = 10*log10(AEEG_AMP_MARKER_uV_2);

	//
	// init display
	//
	// calculate drawing areas
	GraphicsEngine_CalculateDrawingAreas(blnIsFullScreen);

	// calculate scaling values
	GraphicsEngine_CalculateScales();

	m_blnIsGraphicsInit = TRUE;

	return TRUE;
}

BOOL GraphicsEngine_IsInit(void)
{
	return m_blnIsGraphicsInit;
}

void GraphicsEngine_SetDisplayBufferLength(unsigned int uintDisplayBufferLength)
{
	m_uintDisplayBufferLength = uintDisplayBufferLength;
}

void GraphicsEngine_CleanUp(void)
{
	RECT rc;
	unsigned int i;

	m_blnIsGraphicsInit = FALSE;

	//
	// release GDI objects
	//
	// delete font(s)
	if (m_fntEEGChannelLabel)
		DeleteObject (m_fntEEGChannelLabel);

	// misc. brushes
	if(m_dbDrawingBrushes.SignalEraser)
		DeleteObject(m_dbDrawingBrushes.SignalEraser);

	// EEG and Gyro trace pens
	for (i = 0; i < (EEGCHANNELS + ACCCHANNELS); i++)
		if(m_dpPens.SignalTraces[i])
			DeleteObject(m_dpPens.SignalTraces[i]);

	// misc. pens
	if(m_dpPens.EEGChannelBorder)
		DeleteObject(m_dpPens.EEGChannelBorder);
	if(m_dpPens.EEGGyroBorder)
		DeleteObject(m_dpPens.EEGGyroBorder);
	if(m_dpPens.HeadingsBorder)
		DeleteObject(m_dpPens.HeadingsBorder);

	// force redraw of EEG recording area
	// NOTE: RedrawWindow function does not take into account right and bottom border of rectangle => compensated with ++
	memcpy_s(&rc, sizeof(RECT), &m_daEEGDrawingAreas.AvailableDrawingArea, sizeof(RECT));
	rc.right++;
	rc.bottom++;
	RedrawWindow(m_hwndMainWindow, &rc, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW);

	//
	// Deallocate memory
	//
	free(m_tiAEEGTimeIndicators.pHorizontalLocations);
	for(i = 0; i < EEGCHANNELS; i++)
		free(m_amAEEGAmplitudeMarkers[i].pAmplitudeMarkers);
}

void GraphicsEngine_EEG_DrawStatic(HDC hDC)
{
	COLORREF clrOld;
	HDC hdcMemory;
	HBITMAP hbmMemory, hbmOld;
	HFONT hfntOld;
	HPEN hpenOld;
	RECT rc;
	unsigned int i = 0, j = 0;

	static HBITMAP hbmpHeading;

	// initialize variables
	hfntOld = NULL;
	hpenOld = NULL;

	// if graphics system is not initialized yet, return
	if(!GraphicsEngine_IsInit())
		return;

	//
	// draw EEG related items (only if EEG traces are to be displayed)
	//
	if(m_dvDrawingVariables.NEEGTraces > 0)
	{
		//
		// draw EEG channel borders
		//
		// select border pen
		hpenOld = (HPEN) SelectObject(hDC, m_dpPens.EEGChannelBorder);
		
		// draw borders
		for(i = 0; i < (m_dvDrawingVariables.NEEGTraces - 1); i++)
		{
			MoveToEx (hDC, m_daEEGDrawingAreas.AvailableDrawingArea.left, m_daEEGDrawingAreas.EEGChannels[i].bottom + 1, NULL);
			LineTo (hDC, m_daEEGDrawingAreas.AvailableDrawingArea.right + 1, m_daEEGDrawingAreas.EEGChannels[i].bottom + 1);
		}

		//
		// draw Headings border
		//
		// select border pen
		SelectObject(hDC, m_dpPens.HeadingsBorder);
		
		// draw border
		for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
		{
			MoveToEx (hDC, (int) m_dvDrawingVariables.HeadingWidth + 1, m_daEEGDrawingAreas.EEGChannels[i].top, NULL);
			LineTo (hDC, (int) m_dvDrawingVariables.HeadingWidth + 1, m_daEEGDrawingAreas.EEGChannels[i].bottom);
		}

		//
		// draw Headings
		//
		// create compatible DC
		hdcMemory = CreateCompatibleDC (hDC);

		// select heading text font and color
		hfntOld = (HFONT) SelectObject(hdcMemory, m_fntEEGChannelLabel);
		clrOld = SetTextColor(hdcMemory, m_clrHeadingsText);
		
		// draw background and text
		rc.top = rc.left = 0;
		for(i = 0; i < m_dvDrawingVariables.NEEGTraces; i++)
		{
			// prepare DC and set variables
			hbmMemory = CreateCompatibleBitmap (hDC, m_daEEGDrawingAreas.ChannelHeadings[i].width, m_daEEGDrawingAreas.ChannelHeadings[i].height);
			hbmOld = (HBITMAP) SelectObject (hdcMemory, hbmMemory);
			rc.bottom = m_daEEGDrawingAreas.ChannelHeadings[i].height;
			rc.right = m_daEEGDrawingAreas.ChannelHeadings[i].right;

			// fill in background
			FillRect(hdcMemory, &rc, m_dbDrawingBrushes.Heading);

			// draw text
			DrawVertText(hdcMemory,
						 mc_strChannelLabels[m_dvDrawingVariables.EEGChannelSwitchbox[i]],
						 _tcslen(mc_strChannelLabels[m_dvDrawingVariables.EEGChannelSwitchbox[i]]),
						 &rc,
						 DV_HCENTER | DV_VCENTER,
						 mc_uintHCharSpacing,
						 mc_uintVCharSpacing);

			// bit-block transfer of the color data corresponding to a rectangle of pixels from memory to the client window
			BitBlt (hDC,
					m_daEEGDrawingAreas.ChannelHeadings[i].left,
					m_daEEGDrawingAreas.ChannelHeadings[i].top,
					m_daEEGDrawingAreas.ChannelHeadings[i].width,
					m_daEEGDrawingAreas.ChannelHeadings[i].height,
					hdcMemory,
					0, 0,
					SRCCOPY);
			
			// reset DC
			SelectObject (hdcMemory, hbmOld);
			DeleteObject (hbmMemory);
		}

		//
		// draw EEG-Gyro border (if needed)
		//
		if(m_blnDrawAccelerometerTraces)
		{
			// select border pen
			SelectObject(hDC, m_dpPens.EEGGyroBorder);

			// draw border
			MoveToEx (hDC, m_daEEGDrawingAreas.AvailableDrawingArea.left, m_daEEGDrawingAreas.EEGDrawingArea.bottom + 1, NULL);
			LineTo (hDC, m_daEEGDrawingAreas.AvailableDrawingArea.right, m_daEEGDrawingAreas.GyroDrawingArea.top - 1);
		}
		
		//
		// Clean-up
		//
		// reset and delete memory DC
		SetTextColor(hdcMemory, clrOld);
		SelectObject(hdcMemory, hfntOld);
		DeleteDC(hdcMemory);

		// reset window DC
		if(hpenOld)
			SelectObject(hDC, hpenOld);
	}
	
	//
	// draw Gyro related items (only if Gyro traces are to be displayed)
	//
	if(m_blnDrawAccelerometerTraces)
	{
		//
		// draw Headings border
		//
		// select border pen
		hpenOld = (HPEN) SelectObject(hDC, m_dpPens.HeadingsBorder);
		
		// draw border
		MoveToEx (hDC, (int) m_dvDrawingVariables.HeadingWidth + 1, m_daEEGDrawingAreas.GyroDrawingArea.top, NULL);
		LineTo (hDC, (int) m_dvDrawingVariables.HeadingWidth + 1, m_daEEGDrawingAreas.GyroDrawingArea.bottom);

		//
		// draw Headings
		//
		// create compatible DC
		hdcMemory = CreateCompatibleDC (hDC);
		hbmMemory = CreateCompatibleBitmap (hDC, m_dvDrawingVariables.HeadingWidth, m_daEEGDrawingAreas.GyroDrawingArea.height);

		// select heading text bitmap, font and color
		hbmOld = (HBITMAP) SelectObject (hdcMemory, hbmMemory);
		hfntOld = (HFONT) SelectObject(hdcMemory, m_fntEEGChannelLabel);
		clrOld = SetTextColor(hdcMemory, m_clrHeadingsText);
		
		// draw background and text
		rc.top = rc.left = 0;
		rc.bottom = m_daEEGDrawingAreas.GyroDrawingArea.height;
		rc.right = m_dvDrawingVariables.HeadingWidth;

		// fill in background
		FillRect(hdcMemory, &rc, (HBRUSH) (COLOR_MENUBAR+1));

		// draw text
		DrawVertText(hdcMemory, mc_strChannelLabels[NCHANNELLABELS - 1], _tcslen(mc_strChannelLabels[NCHANNELLABELS - 1]), &rc, DV_HCENTER | DV_VCENTER, mc_uintHCharSpacing, mc_uintVCharSpacing);

		// bit-block transfer of the color data corresponding to a rectangle of pixels from memory to the client window
		BitBlt (hDC,
				m_daEEGDrawingAreas.AvailableDrawingArea.left,
				m_daEEGDrawingAreas.GyroDrawingArea.top,
				m_dvDrawingVariables.HeadingWidth,
				m_daEEGDrawingAreas.GyroDrawingArea.height,
				hdcMemory,
				0, 0,
				SRCCOPY);
		
		//
		// Clean-up
		//
		// reset and delete memory DC
		SelectObject(hdcMemory, hbmOld);
		SetTextColor(hdcMemory, clrOld);
		SelectObject(hdcMemory, hfntOld);
		DeleteObject(hbmMemory);
		DeleteDC(hdcMemory);

		// reset window DC
		if(hpenOld)
			SelectObject(hDC, hpenOld);
	}
}

void GraphicsEngine_EEG_DrawDynamicNew(HDC hDC,
									   double ** dblData,
									   unsigned int uintStartIndex,
									   unsigned int uintNNewSamples,
									   double dblEEGYScale)
{
	BOOL blnTwoErasers;
	double dblSample;
	HBRUSH hbrOld;
	HPEN hpenOld;
	LONG lngOldXPos, lngOldYPos;
	RECT rc1, rc2;
	unsigned int i, j, uintIndex;
	
	// if graphics system is not initialized yet, return
	if(!GraphicsEngine_IsInit())
		return;

	// variable init.
	uintIndex = uintStartIndex;
	
	// remove existing pen and brush from DC
	hpenOld = (HPEN) SelectObject(hDC, GetStockObject(WHITE_PEN));
	hbrOld = (HBRUSH) SelectObject(hDC, GetStockObject(WHITE_BRUSH));
	
	// draw new samples
	for(i=0; i < uintNNewSamples; i++)
	{
		//
		// x-coordinate calculation
		//
		// save old position
		lngOldXPos = m_dvDrawingVariables.CurrentXPos;

		// calculate new x coordinate
		if(uintIndex == m_uintDisplayBufferLength - 1)
			m_dvDrawingVariables.CurrentXPos = m_daEEGDrawingAreas.UseableDrawingArea.right;
		else
			m_dvDrawingVariables.CurrentXPos = ((LONG) (m_dsSignalScales.XScale * uintIndex) + m_daEEGDrawingAreas.UseableDrawingArea.left);
		
		//if(m_pflGraphicsDebug != NULL)
		//	fprintf(m_pflGraphicsDebug, "Index:	%d		Coordinate:	%d\n", uintIndex, m_dvDrawingVariables.CurrentXPos);

		//
		// eraser calculations
		//
		// shift old x-coordinate back to beginning of drawing area if the new x-coordinate has wrapped around
		if(m_dvDrawingVariables.CurrentXPos < lngOldXPos)
			lngOldXPos = m_daEEGDrawingAreas.UseableDrawingArea.left;
		
		// configure first eraser
		rc1.right = m_dvDrawingVariables.CurrentXPos + m_uintClearingWidth;
		rc1.left = rc1.right - (m_dvDrawingVariables.CurrentXPos - lngOldXPos);
		if(rc1.left < m_daEEGDrawingAreas.UseableDrawingArea.left)
			rc1.left = m_daEEGDrawingAreas.UseableDrawingArea.left;
		
		// if needed, configure and enable second eraser
		blnTwoErasers = FALSE;
		if(rc1.right > m_daEEGDrawingAreas.UseableDrawingArea.right)
		{
			blnTwoErasers = TRUE;

			// configure second eraser
			rc2.left = m_daEEGDrawingAreas.UseableDrawingArea.left;
			rc2.right = rc2.left + rc1.right - m_daEEGDrawingAreas.UseableDrawingArea.right;
			
			// adjust border of first eraser
			rc1.right = m_daEEGDrawingAreas.UseableDrawingArea.right; 
		}
		
		//
		// draw EEG traces
		//
		for(j=0; j < m_dvDrawingVariables.NEEGTraces; j++)
		{
			// erase 'old' signal
			SelectObject(hDC, m_dpPens.SignalEraser);
			SelectObject(hDC, m_dbDrawingBrushes.SignalEraser);
			Rectangle(hDC, rc1.left, m_daEEGDrawingAreas.EEGChannels[j].top, rc1.right, m_daEEGDrawingAreas.EEGChannels[j].bottom + 1);
			if(blnTwoErasers)
				Rectangle(hDC, rc2.left, m_daEEGDrawingAreas.EEGChannels[j].top, rc2.right, m_daEEGDrawingAreas.EEGChannels[j].bottom + 1);
			
			// save old y-position
			lngOldYPos = m_dvDrawingVariables.CurrentYPos[j];	

			// calculate new y coordinate
			dblSample = dblData [m_dvDrawingVariables.EEGChannelSwitchbox [j]][uintIndex];
			m_dvDrawingVariables.CurrentYPos[j] = (LONG) (((double) m_daEEGDrawingAreas.EEGChannels[j].vcenter) - dblSample*dblEEGYScale);

			// adjust new y coordinate if out of bounds of current drawing area
			if(m_dvDrawingVariables.CurrentYPos[j] < (m_daEEGDrawingAreas.EEGChannels[j].top))
				m_dvDrawingVariables.CurrentYPos[j] = m_daEEGDrawingAreas.EEGChannels[j].top;
			if(m_dvDrawingVariables.CurrentYPos[j] > m_daEEGDrawingAreas.EEGChannels[j].bottom)
				m_dvDrawingVariables.CurrentYPos[j] = m_daEEGDrawingAreas.EEGChannels[j].bottom;

			// draw new sample
			if(uintIndex > 0)
			{
				SelectObject(hDC, m_dpPens.SignalTraces[j]);
				MoveToEx (hDC, lngOldXPos, lngOldYPos, NULL);
				LineTo (hDC, m_dvDrawingVariables.CurrentXPos, m_dvDrawingVariables.CurrentYPos[j]);
			}
		}

		//
		// draw accelerometer traces
		//
		if(m_blnDrawAccelerometerTraces)
		{
			// erase 'old' signal
			SelectObject(hDC, m_dpPens.SignalEraser);
			SelectObject(hDC, m_dbDrawingBrushes.SignalEraser);
			Rectangle(hDC, rc1.left, m_daEEGDrawingAreas.GyroDrawingArea.top, rc1.right, m_daEEGDrawingAreas.GyroDrawingArea.bottom + 1);
			if(blnTwoErasers)
				Rectangle(hDC, rc2.left, m_daEEGDrawingAreas.GyroDrawingArea.top, rc2.right, m_daEEGDrawingAreas.GyroDrawingArea.bottom + 1);

			for(j=EEGCHANNELS; j < (EEGCHANNELS + m_dvDrawingVariables.NGyroTraces); j++)
			{
				// save old y-position
				lngOldYPos = m_dvDrawingVariables.CurrentYPos[j];

				// calculate new y coordinate
				dblSample = floor(dblData [j][uintIndex]*m_dsSignalScales.GyroYScale);
				m_dvDrawingVariables.CurrentYPos[j] = (LONG) (m_daEEGDrawingAreas.GyroDrawingArea.vcenter - dblSample);

				// draw new sample
				if(uintIndex > 0)
				{
					SelectObject(hDC, m_dpPens.SignalTraces[j]);
					MoveToEx (hDC, lngOldXPos, lngOldYPos, NULL);
					LineTo (hDC, m_dvDrawingVariables.CurrentXPos, m_dvDrawingVariables.CurrentYPos[j]);
				}
			}
		}
		
		uintIndex = (++uintIndex)%m_uintDisplayBufferLength;
	}

	//
	// clean-up
	//
	// reset DC to previous state
	if(hbrOld)
		SelectObject(hDC, hbrOld);
	if(hpenOld)
		SelectObject(hDC, hpenOld);
}

void GraphicsEngine_EEG_DrawDynamicOld(HDC hDC,
									   double ** dblData,
									   unsigned int uintDisplayBufferID,
									   double dblEEGYScale)
{
	double dblSample;
	HPEN hpenOld;
	LONG lngOldXPos, lngOldYPos;
	unsigned int i, j;

#ifdef _DEBUG
	if(m_pflGraphicsDebug != NULL)
		fprintf(m_pflGraphicsDebug, "Drawing Old Signal up until index %d\n", uintDisplayBufferID);
#endif

	// if graphics system is not initialized yet, return
	if(!GraphicsEngine_IsInit())
		return;

	// remove existing pen and brush from DC
	hpenOld = (HPEN) SelectObject(hDC, GetStockObject(WHITE_PEN));

	for(i = 0; i < uintDisplayBufferID; i++)
	{
		//
		// x-coordinate calculation
		//
		// save old position
		lngOldXPos = m_dvDrawingVariables.CurrentXPos;

		// calculate new x coordinate
		if(i == m_uintDisplayBufferLength - 1)
			m_dvDrawingVariables.CurrentXPos = m_daEEGDrawingAreas.UseableDrawingArea.right;
		else
			m_dvDrawingVariables.CurrentXPos = ((LONG) (m_dsSignalScales.XScale * i) + m_daEEGDrawingAreas.UseableDrawingArea.left);

		//
		// re-draw EEG traces
		//
		for(j = 0; j < m_dvDrawingVariables.NEEGTraces; j++)
		{
			// save old y-position
			lngOldYPos = m_dvDrawingVariables.CurrentYPos[j];	

			// calculate new y coordinate
			dblSample = dblData [m_dvDrawingVariables.EEGChannelSwitchbox [j]][i];
			m_dvDrawingVariables.CurrentYPos[j] = (LONG) (m_daEEGDrawingAreas.EEGChannels[j].vcenter - dblSample*dblEEGYScale);

			// adjust new y coordinate if out of bounds of current drawing area
			if(m_dvDrawingVariables.CurrentYPos[j] < (m_daEEGDrawingAreas.EEGChannels[j].top))
				m_dvDrawingVariables.CurrentYPos[j] = m_daEEGDrawingAreas.EEGChannels[j].top;
			if(m_dvDrawingVariables.CurrentYPos[j] > m_daEEGDrawingAreas.EEGChannels[j].bottom)
				m_dvDrawingVariables.CurrentYPos[j] = m_daEEGDrawingAreas.EEGChannels[j].bottom;

			// draw new sample
			if(i > 0)
			{
				SelectObject(hDC, m_dpPens.SignalTraces[j]);
				MoveToEx (hDC, lngOldXPos, lngOldYPos, NULL);
				LineTo (hDC, m_dvDrawingVariables.CurrentXPos, m_dvDrawingVariables.CurrentYPos[j]);
			}
		}

		//
		// draw accelerometer traces
		//
		for(j=EEGCHANNELS; j < (EEGCHANNELS + m_dvDrawingVariables.NGyroTraces); j++)
		{
			// save old y-position
			lngOldYPos = m_dvDrawingVariables.CurrentYPos[j];

			// calculate new y coordinate
			dblSample = floor(dblData [j][i]*m_dsSignalScales.GyroYScale);
			m_dvDrawingVariables.CurrentYPos[j] = (LONG) (m_daEEGDrawingAreas.GyroDrawingArea.vcenter - dblSample);

			// draw new sample
			if(i > 0)
			{
				SelectObject(hDC, m_dpPens.SignalTraces[j]);
				MoveToEx (hDC, lngOldXPos, lngOldYPos, NULL);
				LineTo (hDC, m_dvDrawingVariables.CurrentXPos, m_dvDrawingVariables.CurrentYPos[j]);
			}
		}
	}
	
	//
	// clean-up
	//
	// reset DC to previous state
	if(hpenOld)
		SelectObject(hDC, hpenOld);
}

RECT GraphicsEngine_GetDrawingRect(void)
{
	RECT rcDrawingRect;
	
	memcpy_s(&rcDrawingRect, sizeof(RECT), &m_daEEGDrawingAreas.AvailableDrawingArea, sizeof(RECT));

	return rcDrawingRect;
}