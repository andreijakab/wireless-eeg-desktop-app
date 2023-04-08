// $Id: Installer1.cs 63 2011-07-08 13:46:59Z andrei-jakab $

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration.Install;
using System.Diagnostics;
using System.Threading;

[assembly: CLSCompliant(false)]
namespace InstallUSBDrivers
{
    [RunInstaller(true)]
    public partial class Installer1 : Installer
    {
        const int ERROR_FILE_NOT_FOUND = 2;
        const int ERROR_ACCESS_DENIED = 5;

        public Installer1()
        {
            InitializeComponent();
        }

        public override void  Commit(System.Collections.IDictionary savedState)
        {
            System.Diagnostics.Process prcUSBDrivers = new System.Diagnostics.Process();
            
            // call the base Commit method
            base.Commit(savedState);

            try
            {
                prcUSBDrivers.StartInfo.FileName = this.Context.Parameters["path"] + "\\USB Driver\\" + this.Context.Parameters["driver"];
                prcUSBDrivers.Start();

                // wait for the main process to finish
                while(!prcUSBDrivers.HasExited)
                {
                    // Discard cached information about the process.
                    prcUSBDrivers.Refresh();

                    // Wait 0.5 seconds.
                    Thread.Sleep(500);
                }

                // make sure driver installers are done running
                while (this.IsProcessOpen("DPInst_Monx86"));
                while (this.IsProcessOpen("DPInstx86")) ;
            }
            catch (Win32Exception e)
            {
                if (e.NativeErrorCode == ERROR_FILE_NOT_FOUND)
                {
                    Console.WriteLine(e.Message + ". Check the path.");
                }

                else if (e.NativeErrorCode == ERROR_ACCESS_DENIED)
                {
                    // Note that if your word processor might generate exceptions
                    // such as this, which are handled first.
                    Console.WriteLine(e.Message +
                        ". You do not have permission to access this file.");
                }
            }

        }
        
        private bool IsProcessOpen(string strName)
        {
            bool blnIsOpen = false;
            Process [] prcRunningProcesses = Process.GetProcesses();

            foreach (Process prcRunning in prcRunningProcesses)
            {
                //now we're going to see if any of the running processes
                //match the currently running processes. Be sure to not
                //add the .exe to the name you provide
                if (prcRunning.ProcessName.Contains(strName))
                {
                    //if the process is found to be running then we
                    //return a true
                    blnIsOpen = true;
                    break;
                }
            }

            return blnIsOpen;
        }
    }
}