/******************************************************************************\
 * Copyright (c) 2004-2022
 *
 * Author(s):
 *  Volker Fischer
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
\******************************************************************************/

#ifdef HEADLESS
#    if defined( Q_OS_IOS )
#        error HEADLES mode is not valid for IOS
#    endif
#    if defined( ANDROID )
#        error HEADLES mode is not valid for ANDROID
#    endif
#endif

#include <QCoreApplication>
#include <QDir>
#include <iostream>
#include "global.h"
#include "messages.h"
#include "cmdlnoptions.h"
#include "settings.h"
#include "util.h"
#include <memory>

#ifndef HEADLESS
#    include <QApplication>
#    include <QMessageBox>
#    include "serverdlg.h"
#    ifndef SERVER_ONLY
#        include "clientdlg.h"
#    endif
#endif

#include "server.h"
#ifndef SERVER_ONLY
#    include "client.h"
#    include "testbench.h"
#endif

#ifdef ANDROID
#    include <QtAndroidExtras/QtAndroid>
#endif

#if defined( Q_OS_MACX )
#    include "mac/activity.h"
extern void qt_set_sequence_auto_mnemonic ( bool bEnable );
#endif

// Global App Name *************************************************************

static QString strAppName = APP_NAME; // Will be appended by " - ClientName" or "Server - ServerName" if those names are defined

void SetClientAppName ( QString strClientname ) { strAppName = QString ( APP_NAME ) + " - " + strClientname; }

void SetServerAppName ( QString strServername ) { strAppName = QString ( APP_NAME ) + "Server - " + strServername; }

const QString GetAppName() { return strAppName; }

// Helpers *********************************************************************

QString UsageArguments ( char* argv );

const QString getServerNameFromInfo ( const QString strServerInfo )
{
    if ( !strServerInfo.isEmpty() )
    {
        QStringList servInfoParams = strServerInfo.split ( ";" );
        if ( servInfoParams.count() > 0 )
        {
            return servInfoParams[0];
        }
    }

    return QString();
}

void OnFatalError ( QString errMsg ) { throw CErrorExit ( qUtf8Printable ( errMsg ) ); }

// Implementation **************************************************************

int main ( int argc, char** argv )
{
    int exit_code = 0;

    CCommandline::argc = argc;
    CCommandline::argv = argv;
    CMessages::init ( NULL, GetAppName() );

#if defined( Q_OS_MACX )
    // Mnemonic keys are default disabled in Qt for MacOS. The following function enables them.
    // Qt will not show these with underline characters in the GUI on MacOS. (#1873)
    qt_set_sequence_auto_mnemonic ( true );
#endif

#ifdef HEADLESS
    // note: pApplication will never be used in headless mode,
    // but if we define it here we can use the same source code
    // with far less ifdef's
#    define QApplication QCoreApplication
#    define pApplication pCoreApplication
#else
    QApplication* pApplication = NULL;
#endif
    QCoreApplication* pCoreApplication = NULL;

    try
    {
#if !defined( HEADLESS ) && defined( _WIN32 )
        if ( AttachConsole ( ATTACH_PARENT_PROCESS ) )
        {
            (void) freopen ( "CONOUT$", "w", stdout );
            (void) freopen ( "CONOUT$", "w", stderr );
        }
#endif

        // Commandline arguments -----------------------------------------------

        CCommandline cmdLine ( OnFatalError );

        // Help ----------------------------------------------------------------
        if ( cmdLine.GetFlagArgument ( CMDLN_HELP ) || cmdLine.GetFlagArgument ( CMDLN_HELP2 ) )
        {
            throw CInfoExit ( UsageArguments ( argv[0] ) );
        }

        // Version number ------------------------------------------------------
        if ( cmdLine.GetFlagArgument ( CMDLN_VERSION ) )
        {
            throw CInfoExit ( GetVersionAndNameStr ( false ) );
        }

        // Client/Server/GUI ---------------------------------------------------
        bool bIsClient = !cmdLine.GetFlagArgument ( CMDLN_SERVER );
        bool bUseGUI   = !cmdLine.GetFlagArgument ( CMDLN_NOGUI );

        // Check validity of Server and GUI mode flags ----------------------------

#if ( defined( SERVER_BUNDLE ) && defined( Q_OS_MACX ) ) || defined( SERVER_ONLY )
        // if we are on MacOS and we are building a server bundle or requested build with serveronly, start Jamulus in server mode
        bIsClient = false;
        if ( !bUseGUI )
        {
            qInfo() << "- no GUI mode chosen";
        }
#else
        // IOS is Client with gui only - TODO: maybe a switch in interface to change to server?
#    if defined( Q_OS_IOS )
        bIsClient = true; // Client only - TODO: maybe a switch in interface to change to server?
#    else
        if ( !bIsClient )
        {
            qInfo() << "- server mode chosen";
        }
#    endif

#    ifdef HEADLESS
        bUseGUI   = false;
#    else
        if ( !bUseGUI )
        {
            qInfo() << "- no GUI mode chosen";
        }

#    endif
#endif

        // Application setup ---------------------------------------------------

        if ( bUseGUI )
        {
            pApplication = new QApplication ( argc, argv );
        }
        else
        {
            pCoreApplication = new QCoreApplication ( argc, argv );
        }

#ifdef ANDROID
        // special Android coded needed for record audio permission handling
        auto result = QtAndroid::checkPermission ( QString ( "android.permission.RECORD_AUDIO" ) );

        if ( result == QtAndroid::PermissionResult::Denied )
        {
            QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync ( QStringList ( { "android.permission.RECORD_AUDIO" } ) );

            if ( resultHash["android.permission.RECORD_AUDIO"] == QtAndroid::PermissionResult::Denied )
            {
                throw CInfoExit ( "android permission(s) denied" );
            }
        }
#endif

#ifdef _WIN32
        // set application priority class -> high priority
        SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

        // For accessible support we need to add a plugin to qt. The plugin has to
        // be located in the install directory of the software by the installer.
        // Here, we set the path to our application path.
        if ( bUseGUI )
        {
            QDir ApplDir ( QApplication::applicationDirPath() );
            pApplication->addLibraryPath ( QString ( ApplDir.absolutePath() ) );
        }
        else
        {
            QDir ApplDir ( QCoreApplication::applicationDirPath() );
            pCoreApplication->addLibraryPath ( QString ( ApplDir.absolutePath() ) );
        }
#endif

#if defined( Q_OS_MACX )
        // On OSX we need to declare an activity to ensure the process doesn't get
        // throttled by OS level Nap, Sleep, and Thread Priority systems.
        CActivity activity;

        activity.BeginActivity();
#endif

        // init resources
        Q_INIT_RESOURCE ( resources );

#ifndef SERVER_ONLY
        //#### TEST: BEGIN ###/
        // activate the following line to activate the test bench,
        // CTestbench Testbench ( "127.0.0.1", DEFAULT_PORT_NUMBER );
        //#### TEST: END ###/
#endif

#ifndef SERVER_ONLY
        if ( bIsClient )
        {
            // actual client object
            CClient Client ( bUseGUI );

#    ifndef HEADLESS
            if ( bUseGUI )
            {
                // GUI object
                CClientDlg ClientDlg ( Client );

                // initialise message boxes
                CMessages::init ( &ClientDlg, GetAppName() );

                // show dialog
                ClientDlg.show();

                exit_code = pApplication->exec();
            }
            else
#    endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                // initialise message boxes
                CMessages::init ( NULL, GetAppName() );

                exit_code = pCoreApplication->exec();
            }
        }
        else
#endif
        {
            // Server:

            // actual server object
            CServer Server ( bUseGUI );

#ifndef HEADLESS
            if ( bUseGUI )
            {
                // GUI object for the server
                CServerDlg ServerDlg ( Server );

                // initialise message boxes
                CMessages::init ( &ServerDlg, GetAppName() );

                // show dialog (if not the minimized flag is set)
                if ( !Server.Settings.CommandlineOptions.startminimized.IsSet() )
                {
                    ServerDlg.show();
                }

                exit_code = pApplication->exec();
            }
            else
#endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                // initialise message boxes
                CMessages::init ( NULL, GetAppName() );

                exit_code = pCoreApplication->exec();
            }
        }
    }

    catch ( const CInfoExit& info )
    {
        // show info and exit normally
        qInfo() << qUtf8Printable ( QString ( "%1: %2" ).arg ( strAppName, info.GetInfoMessage() ) );
        exit_code = 0;
    }

    catch ( const CErrorExit& fatalerr )
    {
        // show deliberate exit message
        qCritical() << qUtf8Printable ( QString ( "%1: %2" ).arg ( strAppName, fatalerr.GetErrorMessage() ) );
        exit_code = fatalerr.GetExitCode();
    }

    catch ( const CGenErr& generr )
    {
        // show unhandled generic error message
        qCritical() << qUtf8Printable ( QString ( "%1: %2" ).arg ( strAppName, generr.GetErrorText() ) );
        exit_code = generr.GetExitCode();
    }

    catch ( ... )
    {
        // show all other unhandled (standard) exceptions
        qCritical() << qUtf8Printable ( QString ( "%1: Unhandled Exception, Exiting" ).arg ( strAppName ) );
        exit_code = -1;
    }

    if ( pCoreApplication )
    {
        delete pCoreApplication;
    }

#ifndef HEADLESS
    // note: pApplication is the same pointer as pCoreApplication in headless mode!
    //       so pApplication is already deleted!
    if ( pApplication )
    {
        delete pApplication;
    }
#endif

    return exit_code;
}

/******************************************************************************\
* Command Line Argument Parsing                                                *
\******************************************************************************/
QString UsageArguments ( char* argv )
{
    // clang-format off
    return QString (
           "\n"
           "Usage: %1 [option] [option argument] ...\n"
           "\n"
           "  -h, -?, --help        display this help text and exit\n"
           "  -v, --version         display version information and exit\n"
           "\n"
           "Common options:\n"
           "  -i, --inifile         initialization file name\n"
           "                        (not supported for headless Server mode)\n"
           "  -n, --nogui           disable GUI (\"headless\")\n"
           "  -p, --port            set the local port number\n"
           "      --jsonrpcport     enable JSON-RPC server, set TCP port number\n"
           "                        (EXPERIMENTAL, APIs might still change;\n"
           "                        only accessible from localhost)\n"
           "      --jsonrpcsecretfile\n"
           "                        path to a single-line file which contains a freely\n"
           "                        chosen secret to authenticate JSON-RPC users.\n"
           "  -Q, --qos             set the QoS value. Default is 128. Disable with 0\n"
           "                        (see the Jamulus website to enable QoS on Windows)\n"
           "  -t, --notranslation   disable translation (use English language)\n"
           "  -6, --enableipv6      enable IPv6 addressing (IPv4 is always enabled)\n"
           "\n"
           "Server only:\n"
           "  -d, --discononquit    disconnect all Clients on quit\n"
           "  -e, --directoryserver address of the directory Server with which to register\n"
           "                        (or 'localhost' to host a server list on this Server)\n"
           "      --directoryfile   Remember registered Servers even if the Directory is restarted. Directory Servers only.\n"
           "  -f, --listfilter      Server list whitelist filter.  Format:\n"
           "                        [IP address 1];[IP address 2];[IP address 3]; ...\n"
           "  -F, --fastupdate      use 64 samples frame size mode\n"
           "  -l, --log             enable logging, set file name\n"
           "  -L, --licence         show an agreement window before users can connect\n"
           "  -m, --htmlstatus      enable HTML status file, set file name\n"
           "  -o, --serverinfo      registration info for this Server.  Format:\n"
           "                        [name];[city];[country as Qt5 QLocale ID]\n"
           "      --serverpublicip  public IP address for this Server.  Needed when\n"
           "                        registering with a server list hosted\n"
           "                        behind the same NAT\n"
           "  -P, --delaypan        start with delay panning enabled\n"
           "  -R, --recording       sets directory to contain recorded jams\n"
           "      --norecord        disables recording (when enabled by default by -R)\n"
           "  -s, --server          start Server\n"
           "      --serverbindip    IP address the Server will bind to (rather than all)\n"
           "  -T, --multithreading  use multithreading to make better use of\n"
           "                        multi-core CPUs and support more Clients\n"
           "  -u, --numchannels     maximum number of channels\n"
           "  -w, --welcomemessage  welcome message to display on connect\n"
           "                        (string or filename, HTML supported)\n"
           "  -z, --startminimized  start minimizied\n"
           "\n"
           "Client only:\n"
           "  -c, --connect         connect to given Server address on startup\n"
           "  -j, --nojackconnect   disable auto JACK connections\n"
           "  -M, --mutestream      starts the application in muted state\n"
           "      --mutemyown       mute me in my personal mix (headless only)\n"
           "      --clientname      Client name (window title and JACK client name)\n"
           "      --ctrlmidich      MIDI controller channel to listen\n"
           "\n"
           "Example: %1 -s --inifile myinifile.ini\n"
           "\n"
           "For more information and localized help see:\n"
           "https://jamulus.io/wiki/Command-Line-Options\n"
    ).arg( argv );
    // clang-format on
}
