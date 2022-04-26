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

#include <QCoreApplication>
#include <QDir>
#include <iostream>
#include "global.h"
#include "messages.h"
#include "cmdlnoptions.h"
#ifndef HEADLESS
#    include <QApplication>
#    include <QMessageBox>
#    include "serverdlg.h"
#    ifndef SERVER_ONLY
#        include "clientdlg.h"
#    endif
#endif
#include "settings.h"
#ifndef SERVER_ONLY
#    include "testbench.h"
#endif
#include "util.h"
#ifdef ANDROID
#    include <QtAndroidExtras/QtAndroid>
#endif
#if defined( Q_OS_MACX )
#    include "mac/activity.h"
extern void qt_set_sequence_auto_mnemonic ( bool bEnable );
#endif
#include <memory>
#include "rpcserver.h"
#include "serverrpc.h"
#ifndef SERVER_ONLY
#    include "clientrpc.h"
#endif

// Implementation **************************************************************

QString UsageArguments ( char* argv );

void OnFatalError ( QString errMsg )
{
    qCritical() << qUtf8Printable ( errMsg );
    exit ( 1 );
}

int main ( int argc, char** argv )
{
    CCommandline::argc = argc;
    CCommandline::argv = argv;

    int          exit_code  = 0;
    bool         bIsClient  = true;
    bool         bUseGUI    = true;
    CRpcServer*  pRpcServer = nullptr;
    CClientRpc*  pClientRpc = NULL;
    CServerRpc*  pServerRpc = NULL;
    CCommandline cmdLine ( OnFatalError );

#if defined( Q_OS_MACX )
    // Mnemonic keys are default disabled in Qt for MacOS. The following function enables them.
    // Qt will not show these with underline characters in the GUI on MacOS.
    qt_set_sequence_auto_mnemonic ( true );
#endif

#if !defined( HEADLESS ) && defined( _WIN32 )
    if ( AttachConsole ( ATTACH_PARENT_PROCESS ) )
    {
        freopen ( "CONOUT$", "w", stdout );
        freopen ( "CONOUT$", "w", stderr );
    }
#endif

    // Commandline arguments -----------------------------------------------

    // Help ----------------------------------------------------------------
    if ( cmdLine.GetFlagArgument ( CMDLN_HELP ) || cmdLine.GetFlagArgument ( CMDLN_HELP2 ) )
    {
        std::cout << qUtf8Printable ( UsageArguments ( argv[0] ) );
        exit ( 0 );
    }

    // Version number ------------------------------------------------------
    if ( cmdLine.GetFlagArgument ( CMDLN_VERSION ) )
    {
        std::cout << qUtf8Printable ( GetVersionAndNameStr ( false ) );
        exit ( 0 );
    }

    // Server and Gui mode flags -------------------------------------------

#if ( defined( SERVER_BUNDLE ) && defined( Q_OS_MACX ) ) || defined( SERVER_ONLY )
    // if we are on MacOS and we are building a server bundle or requested build with serveronly, start Jamulus in server mode
    bIsClient = false;
    qInfo() << "- Starting in server mode by default (due to compile time option)";
    if ( cmdLine.GetFlagArgument ( CMDLN_NOGUI ) )
    {
        bUseGUI = false;
        qInfo() << "- no GUI mode chosen";
    }
#else
    // IOS is Client with gui only - TODO: maybe a switch in interface to change to server?
#    if defined( Q_OS_IOS )
    bIsClient = true; // Client only - TODO: maybe a switch in interface to change to server?
#    else
    if ( cmdLine.GetFlagArgument ( CMDLN_SERVER ) )
    {
        bIsClient = false;
        qInfo() << "- server mode chosen";
    }
#    endif

#    ifdef HEADLESS
#        if defined( Q_OS_IOS )
#            error HEADLESS mode defined for IOS
#        endif
    bUseGUI   = false;
    qInfo() << "- Starting in no GUI mode by default (due to compile time option)";
#    else
    if ( cmdLine.GetFlagArgument ( CMDLN_NOGUI ) )
    {
        bUseGUI = false;
        qInfo() << "- no GUI mode chosen";
    }

#    endif

#endif

    // Final checks on bIsClient and bUseGUI -------------------------------

#ifdef SERVER_ONLY
    if ( bIsClient )
    {
        qCritical() << "Only --server mode is supported in this build.";
        exit ( 1 );
    }
#endif

#ifdef HEADLESS
    if ( bUseGUI )
    {
        qCritical() << "Only --nogui mode is supported in this build.";
        exit ( 1 );
    }
#endif;

    // Application setup ---------------------------------------------------

    QCoreApplication* pApp = bUseGUI ? new QApplication ( argc, argv ) : new QCoreApplication ( argc, argv );

#ifdef ANDROID
    // special Android coded needed for record audio permission handling
    auto result = QtAndroid::checkPermission ( QString ( "android.permission.RECORD_AUDIO" ) );

    if ( result == QtAndroid::PermissionResult::Denied )
    {
        QtAndroid::PermissionResultMap resultHash = QtAndroid::requestPermissionsSync ( QStringList ( { "android.permission.RECORD_AUDIO" } ) );

        if ( resultHash["android.permission.RECORD_AUDIO"] == QtAndroid::PermissionResult::Denied )
        {
            return 0;
        }
    }
#endif

#ifdef _WIN32
    // set application priority class -> high priority
    SetPriorityClass ( GetCurrentProcess(), HIGH_PRIORITY_CLASS );

    // For accessible support we need to add a plugin to qt. The plugin has to
    // be located in the install directory of the software by the installer.
    // Here, we set the path to our application path.
    QDir ApplDir ( QApplication::applicationDirPath() );
    pApp->addLibraryPath ( QString ( ApplDir.absolutePath() ) );
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

    // Get other commandline options ---------------------------------------

    CCommandlineOptions CommandlineOptions;

    if ( !CommandlineOptions.Load ( bIsClient, bUseGUI, argc, argv ) )
    {
#ifdef HEADLESS
        delete pApp;
        exit ( 1 );
#endif
    }

    // JSON-RPC ------------------------------------------------------------

    if ( CommandlineOptions.jsonrpcport.IsSet() )
    {
        if ( !CommandlineOptions.jsonrpcsecretfile.IsSet() )
        {
            qCritical() << qUtf8Printable ( QString ( "- JSON-RPC: --jsonrpcsecretfile is required. Exiting." ) );
            delete pApp;
            exit ( 1 );
        }

        QFile qfJsonRpcSecretFile ( CommandlineOptions.jsonrpcsecretfile.Value() );
        if ( !qfJsonRpcSecretFile.open ( QFile::OpenModeFlag::ReadOnly ) )
        {
            qCritical() << qUtf8Printable (
                QString ( "- JSON-RPC: Unable to open secret file %1. Exiting." ).arg ( CommandlineOptions.jsonrpcsecretfile.Value() ) );
            delete pApp;
            exit ( 1 );
        }

        QTextStream qtsJsonRpcSecretStream ( &qfJsonRpcSecretFile );
        QString     strJsonRpcSecret = qtsJsonRpcSecretStream.readLine();
        if ( strJsonRpcSecret.length() < JSON_RPC_MINIMUM_SECRET_LENGTH )
        {
            qCritical() << qUtf8Printable ( QString ( "JSON-RPC: Refusing to run with secret of length %1 (required: %2). Exiting." )
                                                .arg ( strJsonRpcSecret.length() )
                                                .arg ( JSON_RPC_MINIMUM_SECRET_LENGTH ) );
            delete pApp;
            exit ( 1 );
        }

        qWarning() << "- JSON-RPC: This interface is experimental and is subject to breaking changes even on patch versions "
                      "(not subject to semantic versioning) during the initial phase.";

        pRpcServer = new CRpcServer ( pApp, CommandlineOptions.jsonrpcport.Value(), strJsonRpcSecret );
        if ( !( pRpcServer && pRpcServer->Start() ) )
        {
            if ( pRpcServer )
            {
                delete pRpcServer;
                pRpcServer = NULL;
            }
            qCritical() << qUtf8Printable ( QString ( "- JSON-RPC: Server failed to start. Exiting." ) );
            delete pApp;
            exit ( 1 );
        }
    }

    try
    {
#ifndef SERVER_ONLY
        if ( bIsClient )
        {
            // Client:
            //
            // load settings from init-file (command line options override)
            CClientSettings Settings ( CommandlineOptions );

            // load translation
            if ( bUseGUI && !CommandlineOptions.notranslation.IsSet() )
            {
                CLocale::LoadTranslation ( Settings.strLanguage, pApp );
                CInstPictures::UpdateTableOnLanguageChange();
            }

            // actual client object
            CClient Client ( Settings );

            if ( pRpcServer )
            {
                pClientRpc = new CClientRpc ( &Client, pRpcServer, pRpcServer );
            }

#    ifndef HEADLESS
            if ( bUseGUI )
            {
                // GUI object
                CClientDlg ClientDlg ( Client, Settings );

                // initialise message boxes
                CMessages::init ( &ClientDlg, Settings.GetWindowTitle() );

                // show dialog
                ClientDlg.show();

                Client.ApplySettings();
                exit_code = pApp->exec();
            }
            else
#    endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                // initialise message boxes
                CMessages::init ( NULL, Settings.GetWindowTitle() );

                Client.ApplySettings();
                exit_code = pApp->exec();
            }
        }
        else
#endif
        {
            // Server:

            if ( CommandlineOptions.licence.IsSet() )
            {
                qInfo() << "- licence required";
            }

            // load settings from init-file (command line options override)
            CServerSettings Settings ( CommandlineOptions );
            ;

            // load translation
            if ( bUseGUI && !CommandlineOptions.notranslation.IsSet() )
            {
                CLocale::LoadTranslation ( Settings.strLanguage, pApp );
            }

            // actual server object
            CServer Server ( Settings );

            if ( pRpcServer )
            {
                pServerRpc = new CServerRpc ( &Server, pRpcServer, pRpcServer );
            }

#ifndef HEADLESS
            if ( bUseGUI )
            {
                // GUI object for the server
                CServerDlg ServerDlg ( &Server, &Settings, nullptr );

                // initialise message boxes
                CMessages::init ( &ServerDlg, Settings.GetWindowTitle() );

                // show dialog (if not the minimized flag is set)
                if ( !CommandlineOptions.startminimized.IsSet() )
                {
                    ServerDlg.show();
                }

                Server.ApplySettings();
                exit_code = pApp->exec();
            }
            else
#endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                Server.ApplySettings();

                if ( CommandlineOptions.directoryserver.Value().isEmpty() )
                {
                    Server.SetDirectoryType ( AT_CUSTOM );
                }

                // initialise message boxes
                CMessages::init ( NULL,
                                  Settings.GetServerName().isEmpty() ? QString ( APP_NAME ) : QString ( APP_NAME ) + " " + Settings.GetServerName() );

                exit_code = pApp->exec();
            }
        }
    }

    catch ( const CGenErr& generr )
    {
        // show generic error
        CMessages::ShowError ( generr.GetErrorText() );

        if ( exit_code == 0 )
        {
            exit_code = 1;
        }
    }

    // Cleanup -------------------------------------------------------------

    if ( pClientRpc )
    {
        delete pClientRpc;
    }

    if ( pServerRpc )
    {
        delete pServerRpc;
    }

    if ( pRpcServer )
    {
        delete pRpcServer;
    }

#if defined( Q_OS_MACX )
    activity.EndActivity();
#endif

    delete pApp;

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
           "                        [name];[city];[country as QLocale ID]\n"
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
