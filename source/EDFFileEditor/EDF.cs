// $Id: EDF.cs 40 2009-01-16 10:34:30Z andrei-jakab $

using System;
using System.Collections;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Permissions;
using System.Text;

[assembly: CLSCompliant(false)]
[assembly: FileIOPermission(SecurityAction.RequestMinimum)]

namespace FileConverter
{
    class EDF: IDisposable
    {
        #region EDF Constants
        // EDF(+) file header fields lengths
        public const int EDFFILEHEADERLENGTH        = 256;				// # of bytes in the EDF(+) file header
        public const int EDFVERSIONLENGTH           = 8;				// # of bytes in the 'version of this data format' field
        public const int EDFPATIENTLENGTH           = 80;				// # of bytes in the 'local patient identification' field
        public const int EDFRECORDINGLENGTH         = 80;				// # of bytes in the 'local recording identification' field
        public const int EDFSTARTDATELENGTH         = 8;				// # of bytes in the 'startdate of recording' field
        public const int EDFSTARTTIMELENGTH         = 8;				// # of bytes in the 'starttime of recording' field
        public const int EDFNUMBEROFBYTESLENGTH     = 8;				// # of bytes in the 'number of bytes in header record' field
        public const int EDFRESERVEDLENGTH          = 44;				// # of bytes in the 'reserved' field (EDF)
        public const int EDFNOFDATARECORDSLENGTH    = 8;				// # of bytes in the 'number of data records' field
        public const int EDFDURATIONOFDRLENGTH      = 8;				// # of bytes in the 'duration of a data record' field
        public const int EDFNOFSIGNALSINDRLENGTH    = 4;				// # of bytes in the 'number of signals in data record' field

        // EDF(+) signal header fields lengths
        public const int EDFSIGNALHEADERLENGTH      = 256;              // # of bytes in the EDF(+) signal header
        public const int EDFLABELFORSIGNALLENGTH	= 16;				// # of bytes in the 'label' field
        public const int EDFTRANSDUCERTYPELENGTH	= 80;				// # of bytes in the 'transducer type' field
        public const int EDFDIMONOFSIGNALLENGTH     = 8;				// # of bytes in the 'physical dimension' field
        public const int EDFPHYSMINOFSIGNLENGTH	    = 8;				// # of bytes in the 'physical minimum' field
        public const int EDFPHYSMAXOFSIGNLENGTH	    = 8;				// # of bytes in the 'physical maximum' field
        public const int EDFDIGMINOFSIGNLENGTH	    = 8;				// # of bytes in the 'digital minimum' field
        public const int EDFDIGMAXOFSIGNLENGTH	    = 8;				// # of bytes in the 'digital maximum' field
        public const int EDFPREFILTLENGTH		    = 80;				// # of bytes in the 'prefiltering' field
        public const int EDFNSAMPLESINSIGNLENGTH	= 8;				// # of bytes in the 'nr of samples in each data record' fielD (Number of samples in data record for each signal separately)
        public const int EDFRESERVERSIGNALLENGTH    = 32;				// # of bytes in the 'reserved' field

        public const int EDFPATIENTSPACE = 63;
        public const int EDFRECORDINGSPACE = 52;
        public const int EDFRESERVEDSPACE = 38;

        // EDF(+) header field default values
        public const string EDFVERSIONDEFAULT       = "0       ";
        public const string EDFRECORDINGDEFAULT     = "Startdate";
        public const string EDFRESERVEDDEFAULT1     = "EDF+C";
        public const string EDFRESERVEDDEFAULT2     = "EDF+D";
        public const string EDFINVALIDSTARTDATE     = "00.00.0000.00.00";

        // EDF(+) special characters
        public const char EDFSUBFIELDSEPARATOR      = ' ';
        public const char EDFFIELDPADDING           = ' ';
        public const char EDFDATESEPARATOR          = '-';
        public const char EDFDATETIMESEPARATOR      = '.';
        public const char EDFSPACEREPLACEMENT       = '_';
        public const char EDFUNKNOWNSUBFIELD        = 'X';

        public const int EDFNBYTESPERSAMPLE         = 2;
        #endregion

        #region Structs/Enums
        public struct EDFFileHeader
        {
            public int Version;
            public LocalPatientIdentification PatientInformation;
            public LocalRecordingIdentification RecordingInformation;
            public DateTime RecordingStartDateTime;
            public int NBytesHeaderRecord;
            public RecordingType SignalFormat;
            public string Reserved;
            public int NDataRecords;
            public float NSecondsPerDataRecord;
            public int NSignals;

            public EDFFileHeader(int Version, LocalPatientIdentification PatientInformation, LocalRecordingIdentification RecordingInformation, DateTime RecordingStartDateTime, int NBytesHeaderRecord, RecordingType SignalFormat, string Reserved, int NDataRecords, float NSecondsPerDataRecord, int NSignals)
            {
                this.Version = Version;
                this.PatientInformation = PatientInformation;
                this.RecordingInformation = RecordingInformation;
                this.RecordingStartDateTime = RecordingStartDateTime;
                this.NBytesHeaderRecord = NBytesHeaderRecord;
                this.SignalFormat = SignalFormat;
                this.Reserved = Reserved;
                this.NDataRecords = NDataRecords;
                this.NSecondsPerDataRecord = NSecondsPerDataRecord;
                this.NSignals = NSignals;
            }
        }

        public struct LocalPatientIdentification
        {
            public string EDFString;
            public string PatientCode;
            public string Name;
            public char Gender;
            public DateTime BirthDate;

            public LocalPatientIdentification(string EDFString)
            {
                this.EDFString = EDFString;
                this.PatientCode = "";
                this.Name = "";
                this.Gender = 'X';
                this.BirthDate = EDF.m_dtInvalid;
            }

            public LocalPatientIdentification(string PatientCode, string Name, char Gender, DateTime BirthDate)
            {
                this.EDFString = "";
                this.PatientCode = PatientCode;
                this.Name = Name;
                this.Gender = Gender;
                this.BirthDate = BirthDate;
            }
        }

        public struct LocalRecordingIdentification
        {
            public string EDFString;
            public DateTime StartDate;
            public string HospitalAdministrationCode;
            public string Technician;
            public string Equipment;
            public string Comments;
            
            public LocalRecordingIdentification(DateTime StartDate, string HospitalAdministrationCode, string Technician, string Equipment, string Comments)
            {
                this.EDFString = "";
                this.StartDate = StartDate;
                this.HospitalAdministrationCode = HospitalAdministrationCode;
                this.Technician = Technician;
                this.Equipment = Equipment;
                this.Comments = Comments;
            }

            public LocalRecordingIdentification(string EDFString)
            {
                this.EDFString = EDFString;
                this.StartDate = EDF.m_dtInvalid;
                this.HospitalAdministrationCode = "";
                this.Technician = "";
                this.Equipment = "";
                this.Comments = "";
            }            
        }

        public struct EDFSignalHeader
        {
            public string Label;
            public string Transducer;
            public string PhysicalDimension;
            public int PhysicalMinimum;
            public int PhysicalMaximum;
            public int DigitalMinimum;
            public int DigitalMaximum;
            public string Prefiltering;
            public int NSamplesPerDataRecord;
            public string Reserved;

            public EDFSignalHeader(string Label, string Transducer, string PhysicalDimension, int PhysicalMinimum, int PhysicalMaximum, int DigitalMinimum, int DigitalMaximum, string Prefiltering, int NSamplesPerDataRecord, string Reserved)
            {
                this.Label = Label;
                this.Transducer = Transducer;
                this.PhysicalDimension = PhysicalDimension;
                this.PhysicalMinimum = PhysicalMinimum;
                this.PhysicalMaximum = PhysicalMaximum;
                this.DigitalMinimum = DigitalMinimum;
                this.DigitalMaximum = DigitalMaximum;
                this.Prefiltering = Prefiltering;
                this.NSamplesPerDataRecord = NSamplesPerDataRecord;
                this.Reserved = Reserved;
            }
        }

        public enum RecordingType : int { Continuous, Discontinuous, Unknown };
        #endregion

        private bool m_blnFileReady, m_blnEDFPlus;
        private FileStream m_fsEDFFile;
        private EDFFileHeader m_edffhFileHeader;
        private EDFSignalHeader[] m_edfshHeader;
        private string m_strError;

        private static DateTime m_dtInvalid = new DateTime(1899, 12, 31);

        public EDF(string strFilePath, bool blnCreate)
        {
            // initialize variables
            this.ResetFields();
            
            try
            {
                if (blnCreate)
                    m_fsEDFFile = new FileStream(strFilePath, FileMode.Create, FileAccess.ReadWrite);
                else
                {
                    if (File.Exists(strFilePath))
                    {
                        m_fsEDFFile = new FileStream(strFilePath, FileMode.Open, FileAccess.ReadWrite);

                        if (this.IsEDFFile())
                        {
                            m_blnFileReady = true;

                            this.IsEDFPlusFile();
                            this.ReadFileHeaderRecord();
                            this.ReadSignalHeaderRecords();
                        }
                    }
                }
            }
            catch
            {
                m_strError = "Unable to open EDF+ file.";
                throw;
            }
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                // dispose managed resources
                m_fsEDFFile.Close();
            }
            // free native resources
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }
        
        #region Methods

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public bool Close()
        {
            bool blnSuccess = false;

            try
            {
                m_fsEDFFile.Close();

                this.ResetFields();

                blnSuccess = true;
            }
            catch
            {
                m_strError = "Unable to close the EDF+ file successfully.";
                throw;
            }

            return blnSuccess;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="dtDate"></param>
        /// <returns></returns>
        private static string GetEDFDate(DateTime dtDate)
        {
            string strDate;
            string[] strMonths = { "JAN","FEB","MAR","APR","MAY","JUN",
                                   "JUL","AUG","SEP","OCT","NOV","DEC" };

            strDate = dtDate.Day.ToString("00", CultureInfo.InvariantCulture) + EDFDATESEPARATOR;
            strDate += strMonths[dtDate.Month - 1] + EDFDATESEPARATOR;
            strDate += dtDate.Year.ToString("0000", CultureInfo.InvariantCulture);

            return strDate;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public string GetLastError()
        {
            return m_strError;
        }

        /// <summary>
        /// Checks whether the supplied file is a valid EDF+ file by verifying that the local recording informatin
        /// starts with 'Startdate' and that the first reserved field starts reserved field starts either with 'EDF+C'
        /// or with 'EDF+D'.
        /// </summary>
        /// <returns>True if the file is a valid EDF+ file, false otherwise.</returns>
        private bool IsEDFPlusFile()
        {
            bool blnCheck;
            char[] chrHeader;
            int intStartId;
            StreamReader srEDFFile;

            // variable initialization
            blnCheck = false;
            chrHeader = new char[EDFFILEHEADERLENGTH];
            intStartId = 0;

            if (this.FileReady)
            {
                // set the file pointer at the beginning of the file stream
                m_fsEDFFile.Position = 0;

                // read the EDF file's header record
                srEDFFile = new StreamReader(m_fsEDFFile, Encoding.ASCII);
                srEDFFile.ReadBlock(chrHeader, 0, EDFFILEHEADERLENGTH);

                // check local recording informatin
                intStartId = EDFVERSIONLENGTH + EDFPATIENTLENGTH;
                blnCheck = true;
                for (int i = intStartId; i < intStartId + EDFRECORDINGDEFAULT.Length; i++)
                {
                    if (chrHeader[i] != EDFRECORDINGDEFAULT[i - intStartId])
                    {
                        blnCheck = false;
                        break;
                    }
                }

                // check first reserved field
                if (blnCheck)
                {
                    intStartId = EDFVERSIONLENGTH + EDFPATIENTLENGTH + EDFRECORDINGLENGTH + EDFSTARTDATELENGTH + EDFSTARTTIMELENGTH + EDFNUMBEROFBYTESLENGTH;
                    for (int i = intStartId; i < intStartId + EDFRESERVEDDEFAULT1.Length; i++)
                    {
                        if (chrHeader[i] != EDFRESERVEDDEFAULT1[i - intStartId])
                        {
                            blnCheck = false;
                            break;
                        }
                    }

                    // only check the second possible value if the first one wasn't found
                    if (!blnCheck)
                    {
                        blnCheck = true;
                        for (int i = intStartId; i < intStartId + EDFRESERVEDDEFAULT2.Length; i++)
                        {
                            if (chrHeader[i] != EDFRESERVEDDEFAULT2[i - intStartId])
                            {
                                blnCheck = false;
                                break;
                            }
                        }
                    }
                }

                this.m_blnEDFPlus = true;
            }

            return blnCheck;
        }

        /// <summary>
        /// Checks whether the supplied file is a valid EDF file by verifying that the version number
        /// is 0 and that the header size is bigger than 0.
        /// </summary>
        /// <returns>True if the file is a valid EDF file, false otherwise.</returns>
        private bool IsEDFFile()
        {
            bool blnIsEDFFile, blnCheck;
            char [] chrHeader;
            int intStartId;
            StreamReader srEDFFile;
            string strTemp;

            // variable initialization
            blnCheck = true;
            blnIsEDFFile = false;
            chrHeader = new char[EDFFILEHEADERLENGTH];
            intStartId = 0;
            strTemp = "";
            m_strError = "File is not in EDF format!";

            if (m_fsEDFFile.Length >= EDFFILEHEADERLENGTH)
            {
                // set the file pointer at the beginning of the file stream
                m_fsEDFFile.Position = 0;

                // read the EDF file's header record
                srEDFFile = new StreamReader(m_fsEDFFile, Encoding.ASCII);
                srEDFFile.ReadBlock(chrHeader, 0, EDFFILEHEADERLENGTH);
                
                // check version
                for(int i = 0; i < EDFVERSIONLENGTH; i++)
                {
                    if (chrHeader[i] != EDFVERSIONDEFAULT[i])
                    {
                        blnCheck = false;
                        break;
                    }
                }
                
                // check # of bytes in header record 
                if (blnCheck)
                {
                    intStartId = EDFVERSIONLENGTH + EDFPATIENTLENGTH + EDFRECORDINGLENGTH + EDFSTARTDATELENGTH + EDFSTARTTIMELENGTH;

                    // read and parse the field
                    for (int j = intStartId; j < intStartId + EDFNUMBEROFBYTESLENGTH; j++)
                        strTemp += chrHeader[j];

                    if (int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture) > 0)
                        blnIsEDFFile = true;
                }
            }
            
            return blnIsEDFFile;
        }
        
        /// <summary>
        /// Parse a date string of the format '12-JAN-2001' into a DateTime object.
        /// </summary>
        /// <param name="strEDFDate"></param>
        /// <returns>Returns the parsed date if the function completes successfully, <c>DateTime.MinValue + 1 ms</c> otherwise.</returns>
        private static DateTime ParseEDFDate(string strEDFDate)
        {
            DateTime dtDate;
            int intDay, intMonth, intYear;
            string[] strTokens;

            dtDate = m_dtInvalid;
            intDay = intMonth = intYear = 0;
            strTokens = strEDFDate.Split(EDFDATESEPARATOR);

            if (strTokens.Length == 3)
            {
                intDay = int.Parse(strTokens[0].Trim(), CultureInfo.InvariantCulture);
                intYear = int.Parse(strTokens[2].Trim(), CultureInfo.InvariantCulture);

                switch (strTokens[1])
                {
                    case "JAN": intMonth = 1; break;
                    case "FEB": intMonth = 2; break;
                    case "MAR": intMonth = 3; break;
                    case "APR": intMonth = 4; break;
                    case "MAY": intMonth = 5; break;
                    case "JUN": intMonth = 6; break;
                    case "JUL": intMonth = 7; break;
                    case "AUG": intMonth = 8; break;
                    case "SEP": intMonth = 9; break;
                    case "OCT": intMonth = 10; break;
                    case "NOV": intMonth = 11; break;
                    case "DEC": intMonth = 12; break;
                }

                dtDate = new DateTime(intYear, intMonth, intDay);
            }

            return dtDate;
        }

        /// <summary>
        /// Parses the fields in the EDF(+) file's header record and stores the information in the
        /// object's properties.
        /// </summary>
        public void ReadFileHeaderRecord()
        {
            char[] chrHeader;
            System.Globalization.CultureInfo ciCultureInformation;
            int intStartId, intDay, intMonth, intYear, intHour, intSecond, intMinute;
            StreamReader srEDFFile;
            string strTemp;
            string [] strTokens;

            ciCultureInformation = (System.Globalization.CultureInfo)System.Threading.Thread.CurrentThread.CurrentCulture.Clone();
            ciCultureInformation.NumberFormat.NumberDecimalSeparator = ".";

            // variable initialization
            chrHeader = new char[EDFFILEHEADERLENGTH];
            intDay = intMonth = intYear = intHour = intSecond = intMinute = 0;
            strTemp = "";

            if (this.FileReady)
            {
                // set the file pointer at the beginning of the file stream
                m_fsEDFFile.Position = 0;

                // read the EDF file's header record
                srEDFFile = new StreamReader(m_fsEDFFile, Encoding.ASCII);
                srEDFFile.ReadBlock(chrHeader, 0, EDFFILEHEADERLENGTH);

                // parse version
                for (int i = 0; i < EDFVERSIONLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.Version = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);

                // parse local patient identification
                intStartId = EDFVERSIONLENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFPATIENTLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.PatientInformation.EDFString = strTemp.Trim();
                if (this.EDFPlus)
                {
                    strTokens = strTemp.TrimEnd().Split(EDFSUBFIELDSEPARATOR);
                    if (strTokens.Length == 4)
                    {
                        this.m_edffhFileHeader.PatientInformation.PatientCode = strTokens[0];
                        this.m_edffhFileHeader.PatientInformation.Gender = strTokens[1][0];
                        this.m_edffhFileHeader.PatientInformation.BirthDate = EDF.ParseEDFDate(strTokens[2]);
                        this.m_edffhFileHeader.PatientInformation.Name = strTokens[3];
                    }
                }

                // parse local recording identification
                intStartId += EDFPATIENTLENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFRECORDINGLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.RecordingInformation.EDFString = strTemp;
                if (this.EDFPlus)
                {
                    strTokens = strTemp.TrimEnd().Split(EDFSUBFIELDSEPARATOR);
                    if (strTokens.Length >= 5)
                    {
                        this.m_edffhFileHeader.RecordingInformation.StartDate = EDF.ParseEDFDate(strTokens[1]);
                        this.m_edffhFileHeader.RecordingInformation.HospitalAdministrationCode = strTokens[2];
                        this.m_edffhFileHeader.RecordingInformation.Technician = strTokens[3];
                        this.m_edffhFileHeader.RecordingInformation.Equipment = strTokens[4];

                        this.m_edffhFileHeader.RecordingInformation.Comments = "";
                        for (int i = 5; i < strTokens.Length; i++)
                            this.m_edffhFileHeader.RecordingInformation.Comments += strTokens[i];
                    }
                }

                // parse 'startdate of recording' and 'starttime of recording' fields
                intStartId += EDFRECORDINGLENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFSTARTDATELENGTH; i++)
                    strTemp += chrHeader[i];
                strTokens = strTemp.TrimEnd().Split(EDFDATETIMESEPARATOR);
                if (strTokens.Length == 3)
                {
                    intDay = int.Parse(strTokens[0].Trim(), CultureInfo.InvariantCulture);
                    intMonth = int.Parse(strTokens[1].Trim(), CultureInfo.InvariantCulture);
                    intYear = int.Parse(strTokens[2].Trim(), CultureInfo.InvariantCulture);
                    if (intYear > 85)
                        intYear += 1900;
                    else
                        intYear += 2000;
                }
                intStartId += EDFSTARTDATELENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFSTARTTIMELENGTH; i++)
                    strTemp += chrHeader[i];
                strTokens = strTemp.TrimEnd().Split(EDFDATETIMESEPARATOR);
                if (strTokens.Length == 3)
                {
                    intHour = int.Parse(strTokens[0].Trim(), CultureInfo.InvariantCulture);
                    intMinute = int.Parse(strTokens[1].Trim(), CultureInfo.InvariantCulture);
                    intSecond = int.Parse(strTokens[2].Trim(), CultureInfo.InvariantCulture);
                }
                if ((intDay > 0) && (intDay < 32) && (intMonth > 0) && (intMonth < 13) && (intYear > 1992) && (intYear < DateTime.Now.Year + 1) &&
                    (intHour >= 0) && (intHour < 24) && (intMinute >= 0) && (intMinute < 60) && (intSecond >= 0) && (intSecond < 60))
                    this.m_edffhFileHeader.RecordingStartDateTime = new DateTime(intYear, intMonth, intDay, intHour, intMinute, intSecond);
                else
                    this.m_edffhFileHeader.RecordingStartDateTime = m_dtInvalid;

                // parse 'number of bytes in header record' field
                intStartId += EDFSTARTTIMELENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFNUMBEROFBYTESLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.NBytesHeaderRecord = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);

                // parse 'reserved' field
                intStartId += EDFNUMBEROFBYTESLENGTH;
                strTemp = "";
                if (this.EDFPlus)
                {
                    for (int i = intStartId; i < intStartId + EDFRESERVEDDEFAULT1.Length; i++)
                        strTemp += chrHeader[i];

                    if (strTemp.IndexOf(EDFRESERVEDDEFAULT1, StringComparison.Ordinal) == 0)
                        this.m_edffhFileHeader.SignalFormat = RecordingType.Continuous;
                    else
                        this.m_edffhFileHeader.SignalFormat = RecordingType.Discontinuous;

                    strTemp = "";
                    for (int i = intStartId + EDFRESERVEDDEFAULT1.Length; i < intStartId + EDFRESERVEDLENGTH; i++)
                        strTemp += chrHeader[i];
                }
                else
                    for (int i = intStartId; i < intStartId + EDFRESERVEDLENGTH; i++)
                        strTemp += chrHeader[i];

                this.m_edffhFileHeader.Reserved = strTemp.Trim();

                // parse 'number of data records' field
                intStartId += EDFRESERVEDLENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFNOFDATARECORDSLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.NDataRecords = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);

                // parse 'duration of a data record' field
                intStartId += EDFNOFDATARECORDSLENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFDURATIONOFDRLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.NSecondsPerDataRecord = float.Parse(strTemp, ciCultureInformation);

                // parse 'number of signals in data record' field
                intStartId += EDFDURATIONOFDRLENGTH;
                strTemp = "";
                for (int i = intStartId; i < intStartId + EDFNOFSIGNALSINDRLENGTH; i++)
                    strTemp += chrHeader[i];
                this.m_edffhFileHeader.NSignals = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);
            }
        }

        /// <summary>
        /// Parses the fields in all the EDF(+) file's signal header records and stores the information in the
        /// object's properties.
        /// </summary>
        public void ReadSignalHeaderRecords()
        {
            char[] chrHeader;
            int intStartId;
            StreamReader srEDFFile;
            string strTemp;
            
            if (this.FileReady && this.FileHeader.NSignals > 0)
            {
                // variable initialization
                chrHeader = new char[this.FileHeader.NSignals*EDFSIGNALHEADERLENGTH];
                intStartId = 0;
                m_edfshHeader = new EDFSignalHeader[this.FileHeader.NSignals];

                // set the file pointer at the beginning of the file stream
                m_fsEDFFile.Position = EDFFILEHEADERLENGTH;

                // read the EDF file's header record
                srEDFFile = new StreamReader(m_fsEDFFile, Encoding.ASCII);
                srEDFFile.ReadBlock(chrHeader, 0, this.FileHeader.NSignals*EDFSIGNALHEADERLENGTH);
                
                // parse 'label' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFLABELFORSIGNALLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].Label = strTemp.Trim();
                    intStartId += EDFLABELFORSIGNALLENGTH;
                }
                
                // parse 'transducer' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFTRANSDUCERTYPELENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].Transducer = strTemp.Trim();
                    intStartId += EDFTRANSDUCERTYPELENGTH;
                }
                
                // parse 'physical dimension' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFDIMONOFSIGNALLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].PhysicalDimension = strTemp.Trim();
                    intStartId += EDFDIMONOFSIGNALLENGTH;
                }

                // parse 'physical minimum' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFPHYSMINOFSIGNLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].PhysicalMinimum = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);
                    intStartId += EDFPHYSMINOFSIGNLENGTH;
                }

                // parse 'physical maximum' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFPHYSMAXOFSIGNLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].PhysicalMaximum = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);
                    intStartId += EDFPHYSMAXOFSIGNLENGTH;
                }

                // parse 'digital minimum' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFDIGMINOFSIGNLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].DigitalMinimum = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);
                    intStartId += EDFDIGMINOFSIGNLENGTH;
                }

                // parse 'digital maximum' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFDIGMAXOFSIGNLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].DigitalMaximum = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);
                    intStartId += EDFDIGMAXOFSIGNLENGTH;
                }

                // parse 'prefiltering' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFPREFILTLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].Prefiltering = strTemp.Trim();
                    intStartId += EDFPREFILTLENGTH;
                }

                // parse '# of samples in each data record' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFNSAMPLESINSIGNLENGTH; i++)
                        strTemp += chrHeader[i];
                    m_edfshHeader[j].NSamplesPerDataRecord = int.Parse(strTemp.Trim(), CultureInfo.InvariantCulture);
                    intStartId += EDFNSAMPLESINSIGNLENGTH;
                }

                // parse 'reserved' field
                for (int j = 0; j < this.FileHeader.NSignals; j++)
                {
                    strTemp = "";
                    for (int i = intStartId; i < intStartId + EDFRESERVERSIGNALLENGTH; i++)
                        strTemp += chrHeader[i];
                    intStartId += EDFRESERVERSIGNALLENGTH;
                    m_edfshHeader[j].Reserved = strTemp.Trim();
                }
            }
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="intLength"></param>
        /// <param name="strField"></param>
        /// <returns></returns>
        private static string MakeFieldRightLength(string strField, int intLength)
        {
            string strFixed = strField;

            if(strFixed.Length < intLength)
                strFixed = strFixed.PadRight(intLength, EDFFIELDPADDING);

            if(strFixed.Length > intLength)
                strFixed = strFixed.Substring(0, intLength);

            return strFixed;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <returns></returns>
        public bool WriteFileHeaderRecord()
        {
            bool blnSuccess = false;
            System.Globalization.CultureInfo ciCultureInformation;
            StreamWriter swEDFFile;
            string strFileHeader, strField;

            ciCultureInformation = (System.Globalization.CultureInfo) System.Threading.Thread.CurrentThread.CurrentCulture.Clone();
            ciCultureInformation.NumberFormat.NumberDecimalSeparator = ".";

            if (this.FileReady)
            {
                // ##version##
                strFileHeader = MakeFieldRightLength(this.m_edffhFileHeader.Version.ToString("0", CultureInfo.InvariantCulture), EDFVERSIONLENGTH);

                // ##local patient identification##
                strField = "";
                if(this.EDFPlus)
                {
                    // patient code
                    if (this.m_edffhFileHeader.PatientInformation.PatientCode.Length > 0)
                        strField += this.m_edffhFileHeader.PatientInformation.PatientCode.Replace(' ', EDFSPACEREPLACEMENT) + EDFSUBFIELDSEPARATOR;
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString() + EDFSUBFIELDSEPARATOR;

                    // gender
                    strField += this.m_edffhFileHeader.PatientInformation.Gender.ToString() + EDFSUBFIELDSEPARATOR;

                    // birthdate
                    if (this.m_edffhFileHeader.PatientInformation.BirthDate != this.InvalidDateTime)
                        strField += EDF.GetEDFDate(this.m_edffhFileHeader.PatientInformation.BirthDate) + EDFSUBFIELDSEPARATOR;
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString() + EDFSUBFIELDSEPARATOR;

                    // name
                    if (this.m_edffhFileHeader.PatientInformation.Name.Length > 0)
                        strField += this.m_edffhFileHeader.PatientInformation.Name.Replace(' ', EDFSPACEREPLACEMENT);
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString();
                }
                else
                    strField = this.m_edffhFileHeader.PatientInformation.EDFString;

                strFileHeader += EDF.MakeFieldRightLength(strField, EDFPATIENTLENGTH);

                // ##local recording identification field##
                strField = "";
                if (this.EDFPlus)
                {
                    // startdate
                    strField += EDFRECORDINGDEFAULT + EDFSUBFIELDSEPARATOR;
                    if (this.m_edffhFileHeader.RecordingInformation.StartDate != this.InvalidDateTime)
                        strField += EDF.GetEDFDate(this.m_edffhFileHeader.RecordingInformation.StartDate) + EDFSUBFIELDSEPARATOR;
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString() + EDFSUBFIELDSEPARATOR;

                    // hospital administration code
                    if (this.m_edffhFileHeader.RecordingInformation.HospitalAdministrationCode.Length > 0)
                        strField += this.m_edffhFileHeader.RecordingInformation.HospitalAdministrationCode.Replace(' ', EDFSPACEREPLACEMENT) + EDFSUBFIELDSEPARATOR;
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString() + EDFSUBFIELDSEPARATOR;

                    // technician
                    if (this.m_edffhFileHeader.RecordingInformation.Technician.Length > 0)
                        strField += this.m_edffhFileHeader.RecordingInformation.Technician.Replace(' ', EDFSPACEREPLACEMENT) + EDFSUBFIELDSEPARATOR;
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString() + EDFSUBFIELDSEPARATOR;

                    // equipment
                    if (this.m_edffhFileHeader.RecordingInformation.Equipment.Length > 0)
                        strField += this.m_edffhFileHeader.RecordingInformation.Equipment.Replace(' ', EDFSPACEREPLACEMENT);
                    else
                        strField += EDFUNKNOWNSUBFIELD.ToString();

                    // comments
                    if (this.m_edffhFileHeader.RecordingInformation.Comments.Length > 0)
                        strField += EDFSUBFIELDSEPARATOR + this.m_edffhFileHeader.RecordingInformation.Comments.Replace(' ', EDFSPACEREPLACEMENT);
                }
                else
                    strField = this.m_edffhFileHeader.RecordingInformation.EDFString;

                strFileHeader += EDF.MakeFieldRightLength(strField, EDFRECORDINGLENGTH);

                // ##start date/time##
                if(this.m_edffhFileHeader.RecordingStartDateTime != this.InvalidDateTime)
                    strField = this.m_edffhFileHeader.RecordingStartDateTime.ToString("dd.MM.yyHH.mm.ss", CultureInfo.InvariantCulture);
                else
                    strField = EDFINVALIDSTARTDATE;
                
                strFileHeader += strField;

                // ##number of bytes in header record##
                strFileHeader += EDF.MakeFieldRightLength(this.m_edffhFileHeader.NBytesHeaderRecord.ToString("########", CultureInfo.InvariantCulture), EDFNUMBEROFBYTESLENGTH);

                // ##reserved## field
                strField = "";
                if (this.EDFPlus)
                {
                    switch(this.m_edffhFileHeader.SignalFormat)
                    {
                        case RecordingType.Continuous:
                            strField += EDFRESERVEDDEFAULT1;
                        break;

                        case RecordingType.Discontinuous:
                            strField += EDFRESERVEDDEFAULT2;
                        break;
                    }
                    
                    strField += " " + this.m_edffhFileHeader.Reserved;
                    strField = MakeFieldRightLength(strField, EDFRESERVEDLENGTH);
                }
                else
                    strField = EDF.MakeFieldRightLength(this.m_edffhFileHeader.Reserved, EDFRESERVEDLENGTH);
                strFileHeader += strField;

                // 'number of data records' field
                if (this.m_edffhFileHeader.NDataRecords == -1)
                    this.m_edffhFileHeader.NDataRecords = CalculateNDataRecords();
                strFileHeader += EDF.MakeFieldRightLength(this.m_edffhFileHeader.NDataRecords.ToString("########", CultureInfo.InvariantCulture), EDFNOFDATARECORDSLENGTH);

                // 'duration of a data record' field
                if (((float)((int)this.m_edffhFileHeader.NSecondsPerDataRecord)) - this.m_edffhFileHeader.NSecondsPerDataRecord == 0.0)
                    strField = ((int) this.m_edffhFileHeader.NSecondsPerDataRecord).ToString("", CultureInfo.InvariantCulture);
                else
                    strField = this.m_edffhFileHeader.NSecondsPerDataRecord.ToString("0.000", ciCultureInformation);
                strFileHeader += EDF.MakeFieldRightLength(strField, EDFDURATIONOFDRLENGTH);

                // 'number of signals in data record'
                strFileHeader += EDF.MakeFieldRightLength(this.m_edffhFileHeader.NSignals.ToString("########", CultureInfo.InvariantCulture), EDFNOFSIGNALSINDRLENGTH);

                // set the file pointer at the beginning of the file stream
                m_fsEDFFile.Position = 0;

                // write the EDF file's header record
                swEDFFile = new StreamWriter(m_fsEDFFile, Encoding.ASCII);
                swEDFFile.Write(strFileHeader);
                swEDFFile.Flush();
                
                blnSuccess = true;
            }

            return blnSuccess;
        }

        /// <summary>
        /// Calculates the number of data records in the EDF(+) file, based on the number
        /// of signals, the sample rate of each signal, the size of he header record and
        /// the size of the whole file.
        /// </summary>
        public int CalculateNDataRecords()
        {
            int intNDataRecords = -1, intNBytesPerDataRecord = 0;

            if (this.FileReady && this.SignalHeaders != null)
            {
                for (int i = 0; i < this.FileHeader.NSignals; i++)
                    intNBytesPerDataRecord += this.SignalHeaders[i].NSamplesPerDataRecord * EDFNBYTESPERSAMPLE;

                intNDataRecords = ((int)m_fsEDFFile.Length - this.FileHeader.NBytesHeaderRecord) / intNBytesPerDataRecord;
            }

            return intNDataRecords;
        }

        private void ResetFields()
        {
            m_blnFileReady = m_blnEDFPlus = false;
            m_edffhFileHeader.Version = -1;
            m_edffhFileHeader.PatientInformation.EDFString = "";
            m_edffhFileHeader.PatientInformation.PatientCode = "";
            m_edffhFileHeader.PatientInformation.Gender = 'X';
            m_edffhFileHeader.PatientInformation.Name = "";
            m_edffhFileHeader.PatientInformation.BirthDate = m_dtInvalid;
            m_edffhFileHeader.RecordingInformation.EDFString = "";
            m_edffhFileHeader.RecordingInformation.StartDate = m_dtInvalid;
            m_edffhFileHeader.RecordingInformation.HospitalAdministrationCode = "";
            m_edffhFileHeader.RecordingInformation.Technician = "";
            m_edffhFileHeader.RecordingInformation.Equipment = "";
            m_edffhFileHeader.RecordingInformation.Comments = "";
            m_edffhFileHeader.NBytesHeaderRecord = this.m_edffhFileHeader.NDataRecords = this.m_edffhFileHeader.NSignals = -1;
            m_edffhFileHeader.NSecondsPerDataRecord = -1.0f;
            m_edffhFileHeader.RecordingStartDateTime = m_dtInvalid;
            m_edffhFileHeader.SignalFormat = RecordingType.Unknown;
            m_edffhFileHeader.Reserved = "";
            m_edfshHeader = null;
            m_strError = "";
        }

        #endregion

        #region Properties
        /// <summary>The EDFPlusFileHeader property represents the EDF(+) file's header.</summary>
        /// <value>The EDFPlusFileHeader property gets/sets the <c>m_edfpfhFileHeader</c> data member.</value>
        public EDFFileHeader FileHeader
        {
            get
            {
                return m_edffhFileHeader;
            }
            set
            {
                m_edffhFileHeader = value;
            }
        }

        /// <summary>The EDFPlus property represents whether the current file is a EDF+ file.</summary>
        /// <value>The EDFPlus property gets the <c>m_blnEDFPlus</c> data member.</value>
        public bool EDFPlus
        {
            get
            {
                return m_blnEDFPlus;
            }
        }
            
        /// <summary>The FileReady property represents whether the EDF(+) file is ready to be used.</summary>
        /// <value>The FileReady property gets the <c>m_blnFileReady</c> data member.</value>
        public bool FileReady
        {
            get
            {
                return m_blnFileReady;
            }
        }

        /// <summary>The InvalidDateTime property represents the value that is used to represent an invalid date/time.</summary>
        /// <value>The InvalidDateTime property gets the <c>m_dtInvalid</c> data member.</value>
        public DateTime InvalidDateTime
        {
            get
            {
                return m_dtInvalid;
            }
        }

        /// <summary>The SignalHeaders property gives access to the signal headers of the EDF(+) file.</summary>
        /// <value>The SignalHeaders property gets the <c>m_edfshHeader</c> data member.</value>
        public EDFSignalHeader[] SignalHeaders
        {
            get
            {
                return m_edfshHeader;
            }
            set
            {
                m_edfshHeader = value;
            }
        }
        #endregion
    }
}
