// $Id: fclsMain.cs 40 2009-01-16 10:34:30Z andrei-jakab $

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Globalization;
using System.Security.Permissions;
using System.Text;
using System.Windows.Forms;

[assembly: FileIOPermission(SecurityAction.RequestMinimum)]

namespace FileConverter
{
    public partial class fclsMain : Form
    {
        // same mask as in main.c
        const string ASCIIMASK = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz ^~@!#$%&()[]{}*+,-|./0123456789:;<>=?@\b\'\"\\";

        // same constants as in edfHeader.h
     
        private bool m_blnOpenedTMPFile;
        private char m_chrGender;
        
        private enum DataSection:int {PatientInfo, RecordingInfo, AdditionalComments };
        private EDF m_edfFile;
        private string m_strInputFile;

        public fclsMain()
        {
            string[] strCommandLineArguments;
            string strPath;

            InitializeComponent();

            m_chrGender = 'X';

            // set initial directory of the open file dialog if a directoty path
            // has been provided to the application as a command line argument
            strCommandLineArguments = Environment.GetCommandLineArgs();
            if (strCommandLineArguments.Length == 2)
            {
                strPath = strCommandLineArguments[1].Replace('"', ' ').Trim();
                if (System.IO.Directory.Exists(strPath))
                    this.ofdTempFile.InitialDirectory = strPath;
            }
        }

        private void btnSelectTempFile_Click(object sender, EventArgs e)
        {
            if(this.ofdTempFile.ShowDialog() == DialogResult.OK)
            {
                m_strInputFile = this.ofdTempFile.FileName;

                if (System.IO.Path.GetExtension(m_strInputFile) == ".tmp")
                    m_blnOpenedTMPFile = true;
                else
                    m_blnOpenedTMPFile = false;

                m_edfFile = new EDF(m_strInputFile, false);
                if (m_edfFile.FileReady)
                {
                    this.txtFilePath.Text = m_strInputFile;

                    // enable group boxes
                    this.gpbPatientInformation.Enabled = true;
                    this.gpbRecordingInformation.Enabled = true;
                    this.gpbCommitChanges.Enabled = true;

                    // display data stored in EDF+ file
                    this.txtName.Text = m_edfFile.FileHeader.PatientInformation.Name;
                    this.txtPersonalIdentityCode.Text = m_edfFile.FileHeader.PatientInformation.PatientCode;
                    this.dtpBirthDate.Value = m_edfFile.FileHeader.PatientInformation.BirthDate;
                    switch (m_edfFile.FileHeader.PatientInformation.Gender)
                    {
                        case 'M':
                            this.rbtnMale.Checked = true;
                            break;

                        case 'F':
                            this.rbtnFemale.Checked = true;
                            break;
                    }

                    this.txtEEGNumber.Text = m_edfFile.FileHeader.RecordingInformation.HospitalAdministrationCode;
                    this.txtTechnician.Text = m_edfFile.FileHeader.RecordingInformation.Technician;
                    this.txtDevice.Text = m_edfFile.FileHeader.RecordingInformation.Equipment;
                    this.txtComments.Text = m_edfFile.FileHeader.RecordingInformation.Comments;

                    this.txtAdditionalComents.Text = m_edfFile.FileHeader.Reserved;

                    this.SetNCharsRemaing(DataSection.PatientInfo);
                    this.SetNCharsRemaing(DataSection.RecordingInfo);
                    this.SetNCharsRemaing(DataSection.AdditionalComments);
                }
                else
                {
                    if(this.RightToLeft == RightToLeft.Yes)
                        MessageBox.Show("Error: " + m_edfFile.GetLastError(), this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, MessageBoxOptions.RtlReading | MessageBoxOptions.RightAlign);
                    else
                        MessageBox.Show("Error: " + m_edfFile.GetLastError(), this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, (MessageBoxOptions)0);
                }
            }
        }

        private int SetNCharsRemaing(DataSection dsSection)
        {
            int intNCharsRemaining = 0;;

            switch (dsSection)
            {
                case DataSection.PatientInfo:
                    intNCharsRemaining = EDF.EDFPATIENTSPACE;

                    intNCharsRemaining -= this.txtName.Text.Length;
                    intNCharsRemaining -= this.txtPersonalIdentityCode.Text.Length;

                    this.txtName.MaxLength = this.txtName.Text.Length + intNCharsRemaining;
                    this.txtPersonalIdentityCode.MaxLength = this.txtPersonalIdentityCode.Text.Length + intNCharsRemaining;
                    
                    this.lblNCharactersRemainingPatientInfo.Text = intNCharsRemaining.ToString(CultureInfo.CurrentCulture) + " characters remaining";
                break;

                case DataSection.RecordingInfo:
                    intNCharsRemaining = EDF.EDFRECORDINGSPACE;

                    intNCharsRemaining -= this.txtEEGNumber.Text.Length;
                    intNCharsRemaining -= this.txtTechnician.Text.Length;
                    intNCharsRemaining -= this.txtDevice.Text.Length;
                    intNCharsRemaining -= this.txtComments.Text.Length;

                    this.txtEEGNumber.MaxLength = this.txtEEGNumber.Text.Length + intNCharsRemaining;
                    this.txtTechnician.MaxLength = this.txtTechnician.Text.Length + intNCharsRemaining;
                    this.txtDevice.MaxLength = this.txtDevice.Text.Length + intNCharsRemaining;
                    this.txtComments.MaxLength = this.txtComments.Text.Length + intNCharsRemaining;

                    this.lblNCharactersRemainingRecordingInfo.Text = intNCharsRemaining.ToString(CultureInfo.CurrentCulture) + " characters remaining";
                break;

                case DataSection.AdditionalComments:
                    intNCharsRemaining = EDF.EDFRESERVEDSPACE - this.txtAdditionalComents.Text.Length;

                    this.txtAdditionalComents.MaxLength = this.txtAdditionalComents.Text.Length + intNCharsRemaining;

                    this.lblNCharactersRemainingAdditionalComments.Text = intNCharsRemaining.ToString(CultureInfo.CurrentCulture) + " characters remaining";
                break;
            }

            return intNCharsRemaining;
        }

        private void txtName_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.PatientInfo);
        }

        private void txtPersonalIdentityCode_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.PatientInfo);
        }

        private void txtPersonalIdentityCode_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void txtName_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void txtAdditionalComents_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.AdditionalComments);
        }

        private void txtComments_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.RecordingInfo);
        }

        private void txtEEGNumber_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.RecordingInfo);
        }

        private void txtTechnician_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.RecordingInfo);
        }

        private void txtEEGNumber_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void txtTechnician_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void txtComments_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void txtAdditionalComents_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void txtDevice_TextChanged(object sender, EventArgs e)
        {
            this.SetNCharsRemaing(DataSection.RecordingInfo);
        }

        private void txtDevice_KeyPress(object sender, KeyPressEventArgs e)
        {
            if (ASCIIMASK.IndexOf(e.KeyChar) == -1)
                e.Handled = true;
        }

        private void btnSave_Click(object sender, EventArgs e)
        {
            int i = 0;
            EDF.EDFFileHeader edffhHeader;
            EDF.LocalPatientIdentification lpiPatientInformation;
            EDF.LocalRecordingIdentification lriRecordingInformation;
            string strFileName, strFolderPath;

            strFileName = "";
            strFolderPath = System.IO.Path.GetDirectoryName(this.ofdTempFile.FileName);

            // configure Save File As dialog
            this.sfdEDFFile.InitialDirectory = strFolderPath;
            
            // if the opened file is a temporary file, generate file name for EDF+ file
            if (m_blnOpenedTMPFile)
            {
                do
                {
                    if(i == 0)
                        strFileName = this.m_edfFile.FileHeader.RecordingStartDateTime.ToString("yyyy.MM.dd HH-mm", CultureInfo.InvariantCulture);
                    else
                        strFileName = this.m_edfFile.FileHeader.RecordingStartDateTime.ToString("yyyy.MM.dd HH-mm", CultureInfo.InvariantCulture) + i.ToString(" (00)", CultureInfo.InvariantCulture);

                    i++;

                } while (System.IO.File.Exists(strFolderPath + "\\" + strFileName + ".edf"));
            }

            this.sfdEDFFile.FileName = strFileName;
            
            if (this.sfdEDFFile.ShowDialog() == DialogResult.OK)
            {
                // save header information in new variables
                lpiPatientInformation = new EDF.LocalPatientIdentification(this.txtPersonalIdentityCode.Text,
                                                                           this.txtName.Text,
                                                                           m_chrGender,
                                                                           this.dtpBirthDate.Value);

                lriRecordingInformation = new EDF.LocalRecordingIdentification(m_edfFile.FileHeader.RecordingStartDateTime,
                                                                               this.txtEEGNumber.Text,
                                                                               this.txtTechnician.Text,
                                                                               this.txtDevice.Text,
                                                                               this.txtComments.Text);
                
                edffhHeader = new EDF.EDFFileHeader(m_edfFile.FileHeader.Version,
                                                    lpiPatientInformation,
                                                    lriRecordingInformation,
                                                    m_edfFile.FileHeader.RecordingStartDateTime,
                                                    m_edfFile.FileHeader.NBytesHeaderRecord,
                                                    m_edfFile.FileHeader.SignalFormat,
                                                    this.txtAdditionalComents.Text,
                                                    m_edfFile.FileHeader.NDataRecords,
                                                    m_edfFile.FileHeader.NSecondsPerDataRecord,
                                                    m_edfFile.FileHeader.NSignals);

                if (this.sfdEDFFile.FileName != m_strInputFile)
                {
                    // close input EDF file
                    this.m_edfFile.Close();

                    // copy input EDF file to output EDF file
                    System.IO.File.Copy(m_strInputFile, this.sfdEDFFile.FileName, false);

                    // open output EDF file
                    m_edfFile = new EDF(this.sfdEDFFile.FileName, false);
                }

                m_edfFile.FileHeader = edffhHeader;

                // write header information
                if (!this.m_edfFile.WriteFileHeaderRecord())
                {
                    if(this.RightToLeft == RightToLeft.Yes)
                        MessageBox.Show("Error: " + m_edfFile.GetLastError(), this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, MessageBoxOptions.RightAlign | MessageBoxOptions.RtlReading);
                    else
                        MessageBox.Show("Error: " + m_edfFile.GetLastError(), this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1, (MessageBoxOptions) 0);
                }

                // close output EDF file
                this.m_edfFile.Close();

                ResetFields();
                this.gpbPatientInformation.Enabled = false;
                this.gpbRecordingInformation.Enabled = false;
                this.gpbCommitChanges.Enabled = false;
            }
        }
        private void ResetFields()
        {
            this.txtFilePath.Text = "";

            this.txtName.Text = this.txtPersonalIdentityCode.Text = "";
            this.dtpBirthDate.Value = DateTime.Now;
            this.rbtnFemale.Checked = this.rbtnMale.Checked = false;

            this.txtEEGNumber.Text = this.txtTechnician.Text = this.txtDevice.Text = this.txtComments.Text = "";

            this.txtAdditionalComents.Text = "";
        }

        private void rbtnFemale_CheckedChanged(object sender, EventArgs e)
        {
            if (this.rbtnFemale.Checked)
                m_chrGender = 'F';
        }

        private void rbtnMale_CheckedChanged(object sender, EventArgs e)
        {
            if (this.rbtnMale.Checked)
                m_chrGender = 'M';
        }

        private void fclsMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (m_edfFile!= null && m_edfFile.FileReady)
                m_edfFile.Close();
            
            // unsubscribe from DisplaySettingsChanged event
            SystemEvents.DisplaySettingsChanged -= new EventHandler(SystemEvents_DisplaySettingsChanged);
        }

        private void fclsMain_Load(object sender, EventArgs e)
        {
            // subscribe to DisplaySettingsChanged event
            SystemEvents.DisplaySettingsChanged += new EventHandler(SystemEvents_DisplaySettingsChanged);
        }

        private void SystemEvents_DisplaySettingsChanged(object sender, EventArgs e)
        {
            this.CenterToScreen();
        }
    }
}