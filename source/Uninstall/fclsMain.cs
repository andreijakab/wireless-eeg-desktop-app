// $Id: fclsMain.cs 40 2009-01-16 10:34:30Z andrei-jakab $

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Threading;
using System.Windows.Forms;

[assembly: CLSCompliant(false)]
namespace Uninstall
{
    public partial class fclsMain : Form
    {
        public fclsMain()
        {
            InitializeComponent();
        }

        private void fclsMain_Load(object sender, EventArgs e)
        {
            const int ERROR_FILE_NOT_FOUND = 2;
            const int ERROR_ACCESS_DENIED = 5;

            System.Diagnostics.Process prcMSIExec;
            string[] strCommandLineArguments;
            string strMSIExecPath, strGUID;

            // variable initialization
            prcMSIExec = new System.Diagnostics.Process();

            // get command-line arguments
            strCommandLineArguments = Environment.GetCommandLineArgs();
            if (strCommandLineArguments.Length == 2 && strCommandLineArguments[1].Split('=')[0] == "/GUID")
            {
                // parse msiexec path
                strGUID = strCommandLineArguments[1].Split('=')[1];
                strMSIExecPath = Environment.GetFolderPath(Environment.SpecialFolder.System);
                strMSIExecPath += "\\msiexec.exe";

                try
                {
                    prcMSIExec.StartInfo.FileName = strMSIExecPath;
                    prcMSIExec.StartInfo.Arguments = "/X " + strGUID;
                    prcMSIExec.Start();

                    // wait for the main process to finish
                    while (!prcMSIExec.HasExited)
                    {
                        // Discard cached information about the process.
                        prcMSIExec.Refresh();

                        // Wait 0.5 seconds.
                        Thread.Sleep(500);
                    }
                }
                catch (Win32Exception ex)
                {
                    if (ex.NativeErrorCode == ERROR_FILE_NOT_FOUND)
                    {
                        MessageBox.Show(ex.Message + ". Check the path.", this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }

                    else if (ex.NativeErrorCode == ERROR_ACCESS_DENIED)
                    {
                        // Note that if your word processor might generate exceptions
                        // such as this, which are handled first.
                        MessageBox.Show(ex.Message + ". You do not have permission to access this file.", this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
                finally
                {
                    Application.Exit();
                }
            }
            else
                MessageBox.Show("Invalid/no program GUID was received!", this.Text, MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }
}