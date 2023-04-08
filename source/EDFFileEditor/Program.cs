// $Id: Program.cs 40 2009-01-16 10:34:30Z andrei-jakab $

using System;
using System.Collections.Generic;
using System.Windows.Forms;

namespace FileConverter
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new fclsMain());
        }
    }
}