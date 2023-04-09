# Overview
I wrote the Windows desktop application contained in this repository for the wireless EEG (WEEG) system that I worked with as part of my Master of Science in Biomedical Engineering thesis. If you're curious and want to know more about why such a thing is useful, you can read my thesis in its entirety [here](http://urn.fi/URN:NBN:fi:tty-201104111176).

# Developer PC setup
## Software
|Software          		             |Version 	       |Installation Notes                                                            |
|------------------------------------|-----------------|------------------------------------------------------------------------------|
|Visual Studio 2010 Pro	             |10.0.40219.1 SP1 |Install options: Visual C++, Setup and Deployment                             |
|Microsoft Windows SDK for Windows 7 |7.1              |Install options: Windows Native Code Development (all), Common Utilities (all)|

## Library dependencies
|Name           |Version|Source                                                                           |Notes                                                   |
|---------------|-------|---------------------------------------------------------------------------------|--------------------------------------------------------|
|libcurl        |7.28.0 |[click here](https://curl.se/download/archeology/curl-7.28.0.tar.gz)             |Build per my guide [Using libcurl with SSH support in Visual Studio 2010](https://silo.tips/download/using-libcurl-with-ssh-support-in-visual-studio-2010)|
|libssh2        |1.4.3  |[click here](https://www.libssh2.org/download/libssh2-1.4.3.tar.gz)              |See dedidcated section in my libcurl build guide        |
|OpenSSL        |1.0.1c |[click here](https://www.openssl.org/source/old/1.0.1/openssl-1.0.1c.tar.gz)     |See dedidcated section in my libcurl build guide        |
|libxml2        |2.9.0  |[click here](https://download.gnome.org/sources/libxml2/2.9/libxml2-2.9.0.tar.xz)|Build by following README in the tarball's win32 folder |
|Vortex Library |1.1.12 |[click here](https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/vortexlibrary/vortex-installer-1.1.12-5014-5015-w32.exe)|Install using default options|

Additional software packages will be required to successfully build libcurl and its dependencies. See my guide for more information.