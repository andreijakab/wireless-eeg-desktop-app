/**
 * 
 * \file applog.h
 * \brief The header file for the program's application logging module.
 *
 * 
 * $Id: applog.h 76 2013-02-14 14:26:17Z jakab $
 */

# ifndef __APPLOG_H__
# define __APPLOG_H__

//----------------------------------------------------------------------------------------------------------
//   								Structs/Enums
//----------------------------------------------------------------------------------------------------------
// types of message logs
typedef enum {General,
			  Version,
			  SoftwareError,
			  HardwareError
			 } LogEventType;

//----------------------------------------------------------------------------------------------------------
//   								Prototypes
//----------------------------------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void applog_close(void);
void applog_endgrouping(void);
void applog_init(TCHAR * strPrefix, TCHAR * strLogFilePath, TCHAR * strLogFileNameBuf, unsigned int uintLogFileNameBufLen);
void applog_logevent(LogEventType letType, TCHAR * pstrModule, TCHAR * pstrMessage, int intCode, BOOL blnAddTimestamp);
void applog_startgrouping(TCHAR * pstrGroupName, BOOL blnAddTimestamp);

#ifdef __cplusplus
}
#endif

#endif