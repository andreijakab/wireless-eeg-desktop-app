/**
 * 
 * \file iniFile.cpp
 * \brief Module handling the creation and the update of an ini file.
 *
 * 
 * $Id: iniFile.cpp 76 2013-02-14 14:26:17Z jakab $ 
 */

# include "iniFile.h"

static TCHAR m_strINIFilePath[MAX_PATH_UNICODE + 1];

/**
 * \brief Gets a hexadecimal parameter from the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section containing the requested key
 * \param[in]  strKeyName		pointer to the name of the key whose associated value is to be retrieved
 * \param[in]  intDefaultValue	default value that is returned if \e strSectionName and/or \e strKeyName cannot be found
 * \param[out] intValue			pointer to the variable that receives the retrieved value
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_GetValueH(TCHAR *strSectionName, TCHAR *strKeyName, int intDefaultValue, int *intValue)
{
	BOOL blnResult = FALSE;
	TCHAR strValue[BUFFER_SIZE];

	GetPrivateProfileString(strSectionName, strKeyName, NULL, strValue, sizeof(strValue)/sizeof(TCHAR), m_strINIFilePath);

	if(strValue[0] == '\0')
		*intValue = intDefaultValue;
	else
	{
		*intValue = (int) _tcstol(strValue,NULL,16);
	}

	if(*intValue != intDefaultValue)
		blnResult =  TRUE;

	return blnResult;
}

/**
 * \brief Gets an integer parameter from the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section containing the requested key
 * \param[in]  strKeyName		pointer to the name of the key whose associated value is to be retrieved
 * \param[in]  intDefaultValue	default value that is returned if \e strSectionName and/or \e strKeyName cannot be found
 * \param[out] intValue			pointer to the variable that receives the retrieved value
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_GetValueI(TCHAR *strSectionName, TCHAR *strKeyName, int intDefaultValue, int *intValue)
{
	BOOL blnResult = FALSE;

	*intValue = (int) GetPrivateProfileInt(strSectionName, strKeyName, intDefaultValue, m_strINIFilePath); 

	if(*intValue != intDefaultValue)
		blnResult =  TRUE;

	return blnResult;
}

/**
 * \brief Gets a string parameter from the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section containing the requested key
 * \param[in]  strKeyName		pointer to the name of the key whose associated string is to be retrieved
 * \param[in]  strDefaultString	pointer to the default value that is returned if \e strSectionName and/or \e strKeyName cannot be found
 * \param[out] strBuffer		pointer to the buffer that receives the retrieved string
 * \param[out] intBufferSize	size of the buffer pointed to by the \e strBuffer parameter, in characters
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_GetValueS(TCHAR *strSectionName, TCHAR *strKeyName, TCHAR *strDefaultString, TCHAR *strBuffer, int intBufferLen)
{
	BOOL blnResult = FALSE;

#ifdef _DEBUG
	// make sure buffer is actually as long as advertised
	memset(strBuffer, 0xAE, intBufferLen*sizeof(TCHAR));
#endif

	GetPrivateProfileString(strSectionName, strKeyName, strDefaultString, strBuffer, intBufferLen, m_strINIFilePath);

	if(strDefaultString == NULL ||
	   CompareString(LOCALE_INVARIANT,
					 0,
					 strDefaultString,
					 _tcslen(strDefaultString),
					 strBuffer,
					 _tcslen(strBuffer)) != CSTR_EQUAL)
		blnResult = TRUE;

	return blnResult;
}

/**
 * \brief Sets the ini file to be used.
 *
 * Stores a pointer to the ini file's full path locally in the module.
 * This function needs to be called before any other functions in this module.
 *
 * \param[in] strINIFilePath pointer to the full path of the ini file
 * \return Nothing.
 */
void iniFile_init(TCHAR *strINIFilePath)
{
	_tcscpy_s(m_strINIFilePath, sizeof(m_strINIFilePath)/sizeof(TCHAR), strINIFilePath);
}

/**
 * \brief Copies a string into the specified section of the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section to which the string will be copied; if it doesn't exist, it is created
 * \param[in]  strKeyName		pointer to the name of the key to be associated with the value to be stored; if it doesn't exist, it is created
 * \param[in]  strValue			pointer to the null-terminated string to be written to the file
 * \param[in]  blnCreate		NOT USED
 * \return TRUE if functions completes successfully, FALSE otherwise.
 */
BOOL iniFile_SetValue(TCHAR *strSectionName, TCHAR *strKeyName, TCHAR *strValue, BOOL blnCreate)
{
	return WritePrivateProfileString(strSectionName, strKeyName, strValue, m_strINIFilePath);
}

/**
 * \brief Copies a hexadecimal value into the specified section of the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section to which the string will be copied; if it doesn't exist, it is created
 * \param[in]  strKeyName		pointer to the name of the key to be associated with the value to be stored; if it doesn't exist, it is created
 * \param[in]  uintValue		hexadecimal value to be written to the file
 * \param[in]  blnCreate		NOT USED
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_SetValueH(TCHAR *strSectionName, TCHAR *strKeyName, unsigned int uintValue, BOOL blnCreate)
{
	TCHAR strValue[BUFFER_SIZE];

	_stprintf_s(strValue, sizeof(strValue)/sizeof(TCHAR), TEXT("%x"), uintValue);

	return iniFile_SetValue(strSectionName, strKeyName, strValue, blnCreate);
}

/**
 * \brief Copies an integer value into the specified section of the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section to which the string will be copied; if it doesn't exist, it is created
 * \param[in]  strKeyName		pointer to the name of the key to be associated with the value to be stored; if it doesn't exist, it is created
 * \param[in]  intValue			integer value to be written to the file
 * \param[in]  blnCreate		NOT USED
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_SetValueI(TCHAR *strSectionName, TCHAR *strKeyName, int intValue, BOOL blnCreate)
{
	TCHAR strValue[BUFFER_SIZE];

	_stprintf_s(strValue, sizeof(strValue)/sizeof(TCHAR), TEXT("%d"), intValue);

	return iniFile_SetValue(strSectionName, strKeyName, strValue, blnCreate);
}

/**
 * \brief Copies a double value into the specified section of the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section to which the string will be copied; if it doesn't exist, it is created
 * \param[in]  strKeyName		pointer to the name of the key to be associated with the value to be stored; if it doesn't exist, it is created
 * \param[in]  dblValue			double value to be written to the file
 * \param[in]  blnCreate		NOT USED
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_SetValueF(TCHAR *strSectionName, TCHAR *strKeyName, double dblValue, BOOL blnCreate)
{
	TCHAR strValue[BUFFER_SIZE];

	_stprintf_s(strValue, sizeof(strValue)/sizeof(TCHAR), TEXT("%f"), dblValue);

	return iniFile_SetValue(strSectionName, strKeyName, strValue, blnCreate);
}

/**
 * \brief Copies a variable-argument list into the specified section of the ini file.
 *
 * \param[in]  strSectionName	pointer to the name of the section to which the string will be copied; if it doesn't exist, it is created
 * \param[in]  strKeyName		pointer to the name of the key to be associated with the value to be stored; if it doesn't exist, it is created
 * \param[in]  blnCreate		NOT USED
 * \param[in]  strFormat		pointer to the format of the variable-argument list
 * \return TRUE if the function completes successfully, FALSE otherwise.
 */
BOOL iniFile_SetValueV(TCHAR *strSectionName, TCHAR *strKeyName, BOOL blnCreate, TCHAR *strFormat, ...)
{
	va_list valArguments;
	TCHAR strValue[BUFFER_SIZE];

	va_start(valArguments, strFormat);
	_vstprintf_s(strValue, sizeof(strValue)/sizeof(TCHAR), strFormat, valArguments);
	va_end(valArguments);

	return iniFile_SetValue(strSectionName, strKeyName, strValue, blnCreate);
}