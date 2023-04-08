/**
 * 
 * \file annotations.h
 * \brief The header file defining certain constants for the handling of annotations that are stored in EDF+ file.
 *
 * 
 * $Id: annotations.h 74 2013-01-22 16:29:42Z jakab $
 */


# ifndef __ANNOTATIONS_H__
# define __ANNOTATIONS_H__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

//---------------------------------------------------------------------------
//   								Definitions
//---------------------------------------------------------------------------
# define ANNOTATION_MAX_TYPES			7					// Number of different possible annotations
# define ANNOTATION_MAX_CHARS			60					// Maximum number of characters per annotation (needs to be an even number)
# define ANNOTATION_MAX_NO  			3					// Maximum # of annotations per datarecord
# define ANNOTATION_TOTAL_NCHARS		192					// Number of bytes per data record [has to be >= than ANNOTATION_MAX_NO * (ANNOTATION_MAX_CHARS + 1) + 9] -> +7*24*3600__ = 9 chars
# define ANNOTATION_MUTEX_TIMEOUT		1 					// 

# endif