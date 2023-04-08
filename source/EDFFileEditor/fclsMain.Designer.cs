namespace FileConverter
{
    partial class fclsMain
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing)
                if(m_edfFile != null)
                    m_edfFile.Dispose();

            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(fclsMain));
            this.ofdTempFile = new System.Windows.Forms.OpenFileDialog();
            this.gpbTemporaryFile = new System.Windows.Forms.GroupBox();
            this.btnSelectTempFile = new System.Windows.Forms.Button();
            this.lblFilePath = new System.Windows.Forms.Label();
            this.txtFilePath = new System.Windows.Forms.TextBox();
            this.gpbPatientInformation = new System.Windows.Forms.GroupBox();
            this.rbtnFemale = new System.Windows.Forms.RadioButton();
            this.rbtnMale = new System.Windows.Forms.RadioButton();
            this.dtpBirthDate = new System.Windows.Forms.DateTimePicker();
            this.txtPersonalIdentityCode = new System.Windows.Forms.TextBox();
            this.txtName = new System.Windows.Forms.TextBox();
            this.lblNCharactersRemainingPatientInfo = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.lblName = new System.Windows.Forms.Label();
            this.gpbRecordingInformation = new System.Windows.Forms.GroupBox();
            this.gpbAdditionalComments = new System.Windows.Forms.GroupBox();
            this.txtAdditionalComents = new System.Windows.Forms.TextBox();
            this.lblNCharactersRemainingAdditionalComments = new System.Windows.Forms.Label();
            this.gpbRecordingInfo_Data = new System.Windows.Forms.GroupBox();
            this.lblNCharactersRemainingRecordingInfo = new System.Windows.Forms.Label();
            this.txtComments = new System.Windows.Forms.TextBox();
            this.txtDevice = new System.Windows.Forms.TextBox();
            this.txtTechnician = new System.Windows.Forms.TextBox();
            this.txtEEGNumber = new System.Windows.Forms.TextBox();
            this.lblComments = new System.Windows.Forms.Label();
            this.lblDevice = new System.Windows.Forms.Label();
            this.lblTechnician = new System.Windows.Forms.Label();
            this.lblEEGNumber = new System.Windows.Forms.Label();
            this.gpbCommitChanges = new System.Windows.Forms.GroupBox();
            this.btnSave = new System.Windows.Forms.Button();
            this.sfdEDFFile = new System.Windows.Forms.SaveFileDialog();
            this.gpbTemporaryFile.SuspendLayout();
            this.gpbPatientInformation.SuspendLayout();
            this.gpbRecordingInformation.SuspendLayout();
            this.gpbAdditionalComments.SuspendLayout();
            this.gpbRecordingInfo_Data.SuspendLayout();
            this.gpbCommitChanges.SuspendLayout();
            this.SuspendLayout();
            // 
            // ofdTempFile
            // 
            this.ofdTempFile.DefaultExt = "tmp";
            this.ofdTempFile.Filter = "EEGEM Temporary files (*.tmp)|*.tmp|EDF(+) Files (*.edf)|*.edf|All files (*.*)|*." +
    "*";
            this.ofdTempFile.Title = "Please Select the File Containing the EEGEM Recording";
            // 
            // gpbTemporaryFile
            // 
            this.gpbTemporaryFile.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gpbTemporaryFile.Controls.Add(this.btnSelectTempFile);
            this.gpbTemporaryFile.Controls.Add(this.lblFilePath);
            this.gpbTemporaryFile.Controls.Add(this.txtFilePath);
            this.gpbTemporaryFile.Location = new System.Drawing.Point(12, 12);
            this.gpbTemporaryFile.Name = "gpbTemporaryFile";
            this.gpbTemporaryFile.Size = new System.Drawing.Size(608, 49);
            this.gpbTemporaryFile.TabIndex = 3;
            this.gpbTemporaryFile.TabStop = false;
            this.gpbTemporaryFile.Text = "1. Select File";
            // 
            // btnSelectTempFile
            // 
            this.btnSelectTempFile.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.btnSelectTempFile.Location = new System.Drawing.Point(527, 19);
            this.btnSelectTempFile.Name = "btnSelectTempFile";
            this.btnSelectTempFile.Size = new System.Drawing.Size(75, 23);
            this.btnSelectTempFile.TabIndex = 4;
            this.btnSelectTempFile.Text = "Browse";
            this.btnSelectTempFile.UseVisualStyleBackColor = true;
            this.btnSelectTempFile.Click += new System.EventHandler(this.btnSelectTempFile_Click);
            // 
            // lblFilePath
            // 
            this.lblFilePath.AutoSize = true;
            this.lblFilePath.Location = new System.Drawing.Point(6, 24);
            this.lblFilePath.Name = "lblFilePath";
            this.lblFilePath.Size = new System.Drawing.Size(29, 13);
            this.lblFilePath.TabIndex = 3;
            this.lblFilePath.Text = "Path";
            // 
            // txtFilePath
            // 
            this.txtFilePath.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtFilePath.Location = new System.Drawing.Point(41, 20);
            this.txtFilePath.Name = "txtFilePath";
            this.txtFilePath.ReadOnly = true;
            this.txtFilePath.Size = new System.Drawing.Size(480, 20);
            this.txtFilePath.TabIndex = 2;
            // 
            // gpbPatientInformation
            // 
            this.gpbPatientInformation.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gpbPatientInformation.Controls.Add(this.rbtnFemale);
            this.gpbPatientInformation.Controls.Add(this.rbtnMale);
            this.gpbPatientInformation.Controls.Add(this.dtpBirthDate);
            this.gpbPatientInformation.Controls.Add(this.txtPersonalIdentityCode);
            this.gpbPatientInformation.Controls.Add(this.txtName);
            this.gpbPatientInformation.Controls.Add(this.lblNCharactersRemainingPatientInfo);
            this.gpbPatientInformation.Controls.Add(this.label4);
            this.gpbPatientInformation.Controls.Add(this.label3);
            this.gpbPatientInformation.Controls.Add(this.label2);
            this.gpbPatientInformation.Controls.Add(this.lblName);
            this.gpbPatientInformation.Enabled = false;
            this.gpbPatientInformation.Location = new System.Drawing.Point(12, 67);
            this.gpbPatientInformation.Name = "gpbPatientInformation";
            this.gpbPatientInformation.Size = new System.Drawing.Size(608, 159);
            this.gpbPatientInformation.TabIndex = 4;
            this.gpbPatientInformation.TabStop = false;
            this.gpbPatientInformation.Text = "2. Fill In Patient Information";
            // 
            // rbtnFemale
            // 
            this.rbtnFemale.AutoSize = true;
            this.rbtnFemale.Location = new System.Drawing.Point(125, 96);
            this.rbtnFemale.Name = "rbtnFemale";
            this.rbtnFemale.Size = new System.Drawing.Size(59, 17);
            this.rbtnFemale.TabIndex = 9;
            this.rbtnFemale.TabStop = true;
            this.rbtnFemale.Text = "Female";
            this.rbtnFemale.UseVisualStyleBackColor = true;
            this.rbtnFemale.CheckedChanged += new System.EventHandler(this.rbtnFemale_CheckedChanged);
            // 
            // rbtnMale
            // 
            this.rbtnMale.AutoSize = true;
            this.rbtnMale.Location = new System.Drawing.Point(125, 119);
            this.rbtnMale.Name = "rbtnMale";
            this.rbtnMale.Size = new System.Drawing.Size(48, 17);
            this.rbtnMale.TabIndex = 8;
            this.rbtnMale.TabStop = true;
            this.rbtnMale.Text = "Male";
            this.rbtnMale.UseVisualStyleBackColor = true;
            this.rbtnMale.CheckedChanged += new System.EventHandler(this.rbtnMale_CheckedChanged);
            // 
            // dtpBirthDate
            // 
            this.dtpBirthDate.Location = new System.Drawing.Point(125, 71);
            this.dtpBirthDate.Name = "dtpBirthDate";
            this.dtpBirthDate.Size = new System.Drawing.Size(142, 20);
            this.dtpBirthDate.TabIndex = 7;
            // 
            // txtPersonalIdentityCode
            // 
            this.txtPersonalIdentityCode.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtPersonalIdentityCode.Location = new System.Drawing.Point(125, 45);
            this.txtPersonalIdentityCode.Name = "txtPersonalIdentityCode";
            this.txtPersonalIdentityCode.Size = new System.Drawing.Size(477, 20);
            this.txtPersonalIdentityCode.TabIndex = 6;
            this.txtPersonalIdentityCode.TextChanged += new System.EventHandler(this.txtPersonalIdentityCode_TextChanged);
            this.txtPersonalIdentityCode.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtPersonalIdentityCode_KeyPress);
            // 
            // txtName
            // 
            this.txtName.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtName.Location = new System.Drawing.Point(125, 19);
            this.txtName.Name = "txtName";
            this.txtName.Size = new System.Drawing.Size(477, 20);
            this.txtName.TabIndex = 5;
            this.txtName.TextChanged += new System.EventHandler(this.txtName_TextChanged);
            this.txtName.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtName_KeyPress);
            // 
            // lblNCharactersRemainingPatientInfo
            // 
            this.lblNCharactersRemainingPatientInfo.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.lblNCharactersRemainingPatientInfo.BackColor = System.Drawing.Color.White;
            this.lblNCharactersRemainingPatientInfo.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.lblNCharactersRemainingPatientInfo.Location = new System.Drawing.Point(6, 139);
            this.lblNCharactersRemainingPatientInfo.Name = "lblNCharactersRemainingPatientInfo";
            this.lblNCharactersRemainingPatientInfo.Size = new System.Drawing.Size(596, 15);
            this.lblNCharactersRemainingPatientInfo.TabIndex = 4;
            this.lblNCharactersRemainingPatientInfo.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(6, 105);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(42, 13);
            this.label4.TabIndex = 3;
            this.label4.Text = "Gender";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(6, 75);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(45, 13);
            this.label3.TabIndex = 2;
            this.label3.Text = "Birthday";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(6, 49);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(113, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Personal Identity Code";
            // 
            // lblName
            // 
            this.lblName.AutoSize = true;
            this.lblName.Location = new System.Drawing.Point(6, 23);
            this.lblName.Name = "lblName";
            this.lblName.Size = new System.Drawing.Size(35, 13);
            this.lblName.TabIndex = 0;
            this.lblName.Text = "Name";
            // 
            // gpbRecordingInformation
            // 
            this.gpbRecordingInformation.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gpbRecordingInformation.Controls.Add(this.gpbAdditionalComments);
            this.gpbRecordingInformation.Controls.Add(this.gpbRecordingInfo_Data);
            this.gpbRecordingInformation.Enabled = false;
            this.gpbRecordingInformation.Location = new System.Drawing.Point(12, 232);
            this.gpbRecordingInformation.Name = "gpbRecordingInformation";
            this.gpbRecordingInformation.Size = new System.Drawing.Size(608, 213);
            this.gpbRecordingInformation.TabIndex = 5;
            this.gpbRecordingInformation.TabStop = false;
            this.gpbRecordingInformation.Text = "3. Fill In Recording Information";
            // 
            // gpbAdditionalComments
            // 
            this.gpbAdditionalComments.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gpbAdditionalComments.Controls.Add(this.txtAdditionalComents);
            this.gpbAdditionalComments.Controls.Add(this.lblNCharactersRemainingAdditionalComments);
            this.gpbAdditionalComments.Location = new System.Drawing.Point(9, 144);
            this.gpbAdditionalComments.Name = "gpbAdditionalComments";
            this.gpbAdditionalComments.Size = new System.Drawing.Size(593, 63);
            this.gpbAdditionalComments.TabIndex = 1;
            this.gpbAdditionalComments.TabStop = false;
            this.gpbAdditionalComments.Text = "Additional Comments";
            // 
            // txtAdditionalComents
            // 
            this.txtAdditionalComents.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtAdditionalComents.Location = new System.Drawing.Point(6, 19);
            this.txtAdditionalComents.Name = "txtAdditionalComents";
            this.txtAdditionalComents.Size = new System.Drawing.Size(581, 20);
            this.txtAdditionalComents.TabIndex = 6;
            this.txtAdditionalComents.TextChanged += new System.EventHandler(this.txtAdditionalComents_TextChanged);
            this.txtAdditionalComents.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtAdditionalComents_KeyPress);
            // 
            // lblNCharactersRemainingAdditionalComments
            // 
            this.lblNCharactersRemainingAdditionalComments.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.lblNCharactersRemainingAdditionalComments.BackColor = System.Drawing.Color.White;
            this.lblNCharactersRemainingAdditionalComments.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.lblNCharactersRemainingAdditionalComments.Location = new System.Drawing.Point(6, 42);
            this.lblNCharactersRemainingAdditionalComments.Name = "lblNCharactersRemainingAdditionalComments";
            this.lblNCharactersRemainingAdditionalComments.Size = new System.Drawing.Size(581, 15);
            this.lblNCharactersRemainingAdditionalComments.TabIndex = 5;
            this.lblNCharactersRemainingAdditionalComments.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // gpbRecordingInfo_Data
            // 
            this.gpbRecordingInfo_Data.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gpbRecordingInfo_Data.Controls.Add(this.lblNCharactersRemainingRecordingInfo);
            this.gpbRecordingInfo_Data.Controls.Add(this.txtComments);
            this.gpbRecordingInfo_Data.Controls.Add(this.txtDevice);
            this.gpbRecordingInfo_Data.Controls.Add(this.txtTechnician);
            this.gpbRecordingInfo_Data.Controls.Add(this.txtEEGNumber);
            this.gpbRecordingInfo_Data.Controls.Add(this.lblComments);
            this.gpbRecordingInfo_Data.Controls.Add(this.lblDevice);
            this.gpbRecordingInfo_Data.Controls.Add(this.lblTechnician);
            this.gpbRecordingInfo_Data.Controls.Add(this.lblEEGNumber);
            this.gpbRecordingInfo_Data.Location = new System.Drawing.Point(9, 19);
            this.gpbRecordingInfo_Data.Name = "gpbRecordingInfo_Data";
            this.gpbRecordingInfo_Data.Size = new System.Drawing.Size(593, 119);
            this.gpbRecordingInfo_Data.TabIndex = 0;
            this.gpbRecordingInfo_Data.TabStop = false;
            // 
            // lblNCharactersRemainingRecordingInfo
            // 
            this.lblNCharactersRemainingRecordingInfo.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.lblNCharactersRemainingRecordingInfo.BackColor = System.Drawing.Color.White;
            this.lblNCharactersRemainingRecordingInfo.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.lblNCharactersRemainingRecordingInfo.Location = new System.Drawing.Point(9, 91);
            this.lblNCharactersRemainingRecordingInfo.Name = "lblNCharactersRemainingRecordingInfo";
            this.lblNCharactersRemainingRecordingInfo.Size = new System.Drawing.Size(578, 15);
            this.lblNCharactersRemainingRecordingInfo.TabIndex = 8;
            this.lblNCharactersRemainingRecordingInfo.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // txtComments
            // 
            this.txtComments.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.txtComments.Location = new System.Drawing.Point(291, 16);
            this.txtComments.Multiline = true;
            this.txtComments.Name = "txtComments";
            this.txtComments.Size = new System.Drawing.Size(296, 72);
            this.txtComments.TabIndex = 7;
            this.txtComments.TextChanged += new System.EventHandler(this.txtComments_TextChanged);
            this.txtComments.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtComments_KeyPress);
            // 
            // txtDevice
            // 
            this.txtDevice.Location = new System.Drawing.Point(81, 68);
            this.txtDevice.Name = "txtDevice";
            this.txtDevice.Size = new System.Drawing.Size(143, 20);
            this.txtDevice.TabIndex = 6;
            this.txtDevice.TextChanged += new System.EventHandler(this.txtDevice_TextChanged);
            this.txtDevice.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtDevice_KeyPress);
            // 
            // txtTechnician
            // 
            this.txtTechnician.Location = new System.Drawing.Point(81, 42);
            this.txtTechnician.Name = "txtTechnician";
            this.txtTechnician.Size = new System.Drawing.Size(143, 20);
            this.txtTechnician.TabIndex = 5;
            this.txtTechnician.TextChanged += new System.EventHandler(this.txtTechnician_TextChanged);
            this.txtTechnician.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtTechnician_KeyPress);
            // 
            // txtEEGNumber
            // 
            this.txtEEGNumber.Location = new System.Drawing.Point(81, 16);
            this.txtEEGNumber.Name = "txtEEGNumber";
            this.txtEEGNumber.Size = new System.Drawing.Size(143, 20);
            this.txtEEGNumber.TabIndex = 4;
            this.txtEEGNumber.TextChanged += new System.EventHandler(this.txtEEGNumber_TextChanged);
            this.txtEEGNumber.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.txtEEGNumber_KeyPress);
            // 
            // lblComments
            // 
            this.lblComments.AutoSize = true;
            this.lblComments.Location = new System.Drawing.Point(230, 46);
            this.lblComments.Name = "lblComments";
            this.lblComments.Size = new System.Drawing.Size(56, 13);
            this.lblComments.TabIndex = 3;
            this.lblComments.Text = "Comments";
            // 
            // lblDevice
            // 
            this.lblDevice.AutoSize = true;
            this.lblDevice.Location = new System.Drawing.Point(6, 72);
            this.lblDevice.Name = "lblDevice";
            this.lblDevice.Size = new System.Drawing.Size(41, 13);
            this.lblDevice.TabIndex = 2;
            this.lblDevice.Text = "Device";
            // 
            // lblTechnician
            // 
            this.lblTechnician.AutoSize = true;
            this.lblTechnician.Location = new System.Drawing.Point(6, 46);
            this.lblTechnician.Name = "lblTechnician";
            this.lblTechnician.Size = new System.Drawing.Size(60, 13);
            this.lblTechnician.TabIndex = 1;
            this.lblTechnician.Text = "Technician";
            // 
            // lblEEGNumber
            // 
            this.lblEEGNumber.AutoSize = true;
            this.lblEEGNumber.Location = new System.Drawing.Point(6, 20);
            this.lblEEGNumber.Name = "lblEEGNumber";
            this.lblEEGNumber.Size = new System.Drawing.Size(69, 13);
            this.lblEEGNumber.TabIndex = 0;
            this.lblEEGNumber.Text = "EEG Number";
            // 
            // gpbCommitChanges
            // 
            this.gpbCommitChanges.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.gpbCommitChanges.Controls.Add(this.btnSave);
            this.gpbCommitChanges.Enabled = false;
            this.gpbCommitChanges.Location = new System.Drawing.Point(12, 451);
            this.gpbCommitChanges.Name = "gpbCommitChanges";
            this.gpbCommitChanges.Size = new System.Drawing.Size(608, 55);
            this.gpbCommitChanges.TabIndex = 6;
            this.gpbCommitChanges.TabStop = false;
            this.gpbCommitChanges.Text = "4. Save Changes";
            // 
            // btnSave
            // 
            this.btnSave.Anchor = System.Windows.Forms.AnchorStyles.Bottom;
            this.btnSave.Location = new System.Drawing.Point(267, 19);
            this.btnSave.Name = "btnSave";
            this.btnSave.Size = new System.Drawing.Size(75, 23);
            this.btnSave.TabIndex = 0;
            this.btnSave.Text = "Save";
            this.btnSave.UseVisualStyleBackColor = true;
            this.btnSave.Click += new System.EventHandler(this.btnSave_Click);
            // 
            // sfdEDFFile
            // 
            this.sfdEDFFile.DefaultExt = "edf";
            this.sfdEDFFile.Filter = "EDF(+) Files (*.edf)|*.edf|All files (*.*)|*.*";
            this.sfdEDFFile.Title = "Save EDF(+) File As";
            // 
            // fclsMain
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(632, 514);
            this.Controls.Add(this.gpbCommitChanges);
            this.Controls.Add(this.gpbRecordingInformation);
            this.Controls.Add(this.gpbPatientInformation);
            this.Controls.Add(this.gpbTemporaryFile);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "fclsMain";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "EEGEM - EDF File Editor";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.fclsMain_FormClosing);
            this.Load += new System.EventHandler(this.fclsMain_Load);
            this.gpbTemporaryFile.ResumeLayout(false);
            this.gpbTemporaryFile.PerformLayout();
            this.gpbPatientInformation.ResumeLayout(false);
            this.gpbPatientInformation.PerformLayout();
            this.gpbRecordingInformation.ResumeLayout(false);
            this.gpbAdditionalComments.ResumeLayout(false);
            this.gpbAdditionalComments.PerformLayout();
            this.gpbRecordingInfo_Data.ResumeLayout(false);
            this.gpbRecordingInfo_Data.PerformLayout();
            this.gpbCommitChanges.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.OpenFileDialog ofdTempFile;
        private System.Windows.Forms.GroupBox gpbTemporaryFile;
        private System.Windows.Forms.Button btnSelectTempFile;
        private System.Windows.Forms.Label lblFilePath;
        private System.Windows.Forms.TextBox txtFilePath;
        private System.Windows.Forms.GroupBox gpbPatientInformation;
        private System.Windows.Forms.Label lblName;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.DateTimePicker dtpBirthDate;
        private System.Windows.Forms.TextBox txtPersonalIdentityCode;
        private System.Windows.Forms.TextBox txtName;
        private System.Windows.Forms.Label lblNCharactersRemainingPatientInfo;
        private System.Windows.Forms.RadioButton rbtnFemale;
        private System.Windows.Forms.RadioButton rbtnMale;
        private System.Windows.Forms.GroupBox gpbRecordingInformation;
        private System.Windows.Forms.GroupBox gpbAdditionalComments;
        private System.Windows.Forms.GroupBox gpbRecordingInfo_Data;
        private System.Windows.Forms.TextBox txtAdditionalComents;
        private System.Windows.Forms.Label lblNCharactersRemainingAdditionalComments;
        private System.Windows.Forms.TextBox txtEEGNumber;
        private System.Windows.Forms.Label lblComments;
        private System.Windows.Forms.Label lblDevice;
        private System.Windows.Forms.Label lblTechnician;
        private System.Windows.Forms.Label lblEEGNumber;
        private System.Windows.Forms.TextBox txtDevice;
        private System.Windows.Forms.TextBox txtTechnician;
        private System.Windows.Forms.Label lblNCharactersRemainingRecordingInfo;
        private System.Windows.Forms.TextBox txtComments;
        private System.Windows.Forms.GroupBox gpbCommitChanges;
        private System.Windows.Forms.Button btnSave;
        private System.Windows.Forms.SaveFileDialog sfdEDFFile;

    }
}

