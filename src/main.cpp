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

#include "rpcserver.h"
#include "serverrpc.h"
#ifndef SERVER_ONLY
#    include "clientrpc.h"
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

#if defined( Q_OS_MACX )
    // Mnemonic keys are default disabled in Qt for MacOS. The following function enables them.
    // Qt will not show these with underline characters in the GUI on MacOS.
    qt_set_sequence_auto_mnemonic ( true );
#endif

#ifndef HEADLESS
    QApplication* pApplication = NULL;
#endif
    QCoreApplication* pCoreApplication = NULL;

    CRpcServer* pRpcServer = NULL;
    CServerRpc* pServerRpc = NULL;
#ifndef SERVER_ONLY
    CClientRpc* pClientRpc = NULL;
#endif

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

        bool bIsClient = true;
        bool bUseGUI   = true;

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
            throw CErrorExit ( "Only --server mode is supported in this build." );
        }
#endif

#ifdef HEADLESS
        if ( bUseGUI )
        {
            throw CErrorExit ( "Only --nogui mode is supported in this build." );
        }
#endif;

        // Application setup ---------------------------------------------------

#ifndef HEADLESS
        if ( bUseGUI )
        {
            pApplication = new QApplication ( argc, argv );
        }
        else
        {
            pCoreApplication = new QCoreApplication ( argc, argv );
        }
#else
        pCoreApplication = new QCoreApplication ( argc, argv );
#endif

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
#    ifndef HEADLESS
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
#    else
        // For accessible support we need to add a plugin to qt. The plugin has to
        // be located in the install directory of the software by the installer.
        // Here, we set the path to our application path.
        {
            QDir ApplDir ( QCoreApplication::applicationDirPath() );
            pCoreApplication->addLibraryPath ( QString ( ApplDir.absolutePath() ) );
        }
#    endif
#endif

#if defined( Q_OS_MACX )
        // On OSX we need to declare an activity to ensure the process doesn't get
        // throttled by OS level Nap, Sleep, and Thread Priority systems.
        CActivity activity;

        activity.BeginActivity();
#endif

        // init resources
        Q_INIT_RESOURCE ( resources );

        // Get other commandline options ---------------------------------------

        CCommandlineOptions commandlineOptions;

        if ( !commandlineOptions.Load ( bIsClient, bUseGUI, argc, argv ) )
        {
#ifdef HEADLESS
            throw CErrorExit ( "Parameter Error(s), Exiting" );
#endif
        }

#ifndef SERVER_ONLY
        //#### TEST: BEGIN ###/
        // activate the following line to activate the test bench,
        // CTestbench Testbench ( "127.0.0.1", DEFAULT_PORT_NUMBER );
        //#### TEST: END ###/
#endif

        // JSON-RPC ------------------------------------------------------------

        if ( commandlineOptions.jsonrpcport.IsSet() )
        {
            if ( !commandlineOptions.jsonrpcsecretfile.IsSet() )
            {
                throw CErrorExit ( qUtf8Printable ( QString ( "- JSON-RPC: --jsonrpcsecretfile is required. Exiting." ) ) );
            }

            QFile qfJsonRpcSecretFile ( commandlineOptions.jsonrpcsecretfile.Value() );
            if ( !qfJsonRpcSecretFile.open ( QFile::OpenModeFlag::ReadOnly ) )
            {
                throw CErrorExit ( qUtf8Printable (
                    QString ( "- JSON-RPC: Unable to open secret file %1. Exiting." ).arg ( commandlineOptions.jsonrpcsecretfile.Value() ) ) );
            }

            QTextStream qtsJsonRpcSecretStream ( &qfJsonRpcSecretFile );
            QString     strJsonRpcSecret = qtsJsonRpcSecretStream.readLine();
            if ( strJsonRpcSecret.length() < JSON_RPC_MINIMUM_SECRET_LENGTH )
            {
                throw CErrorExit ( qUtf8Printable ( QString ( "JSON-RPC: Refusing to run with secret of length %1 (required: %2). Exiting." )
                                                        .arg ( strJsonRpcSecret.length() )
                                                        .arg ( JSON_RPC_MINIMUM_SECRET_LENGTH ) ) );
            }

            qWarning() << "- JSON-RPC: This interface is experimental and is subject to breaking changes even on patch versions "
                          "(not subject to semantic versioning) during the initial phase.";

#ifndef HEADLESS
            pRpcServer = new CRpcServer ( bUseGUI ? pApplication : pCoreApplication, commandlineOptions.jsonrpcport.Value(), strJsonRpcSecret );
#else
            pRpcServer = new CRpcServer ( pCoreApplication, commandlineOptions.jsonrpcport.Value(), strJsonRpcSecret );
#endif
            if ( !pRpcServer->Start() )
            {
                throw CErrorExit ( qUtf8Printable ( QString ( "- JSON-RPC: Server failed to start. Exiting." ) ) );
            }
        }

#ifndef SERVER_ONLY
        if ( bIsClient )
        {
            // Client:
            if ( !commandlineOptions.clientname.Value().isEmpty() )
            {
                strAppName += " - " + commandlineOptions.clientname.Value();
            }

            //
            // load settings from init-file (command line options override)
            CClientSettings Settings ( commandlineOptions );

            // load translation
            if ( bUseGUI && !commandlineOptions.notranslation.IsSet() )
            {
#    ifndef HEADLESS
                CLocale::LoadTranslation ( Settings.strLanguage, pApplication );
#    else
                CLocale::LoadTranslation ( Settings.strLanguage, pCoreApplication );
#    endif
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
                CMessages::init ( &ClientDlg, GetAppName() );

                // show dialog
                ClientDlg.show();

                Client.ApplySettings();
                exit_code = pApplication->exec();
            }
            else
#    endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                // initialise message boxes
                CMessages::init ( NULL, GetAppName() );

                Client.ApplySettings();
                exit_code = pCoreApplication->exec();
            }
        }
        else
#endif
        {
            // Server:
            strAppName += "Server";
            if ( !commandlineOptions.serverinfo.Value().isEmpty() )
            {
                QString strServerName = getServerNameFromInfo ( commandlineOptions.serverinfo.Value() );
                if ( !strServerName.isEmpty() )
                {
                    strAppName += " - " + strServerName;

                    if ( strServerName.length() > MAX_LEN_SERVER_NAME )
                    {
                        qWarning() << qUtf8Printable (
                            QString ( "- server name will be truncated to %1" ).arg ( strServerName.left ( MAX_LEN_SERVER_NAME ) ) );
                    }
                }
            }

            if ( commandlineOptions.licence.IsSet() )
            {
                qInfo() << "- licence required";
            }

            // load settings from init-file (command line options override)
            CServerSettings Settings ( commandlineOptions );

            // load translation
            if ( bUseGUI && !commandlineOptions.notranslation.IsSet() )
            {
#ifndef HEADLESS
                CLocale::LoadTranslation ( Settings.strLanguage, pApplication );
#else
                CLocale::LoadTranslation ( Settings.strLanguage, pCoreApplication );
#endif
                CInstPictures::UpdateTableOnLanguageChange();
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
                CMessages::init ( &ServerDlg, GetAppName() );

                // show dialog (if not the minimized flag is set)
                if ( !commandlineOptions.startminimized.IsSet() )
                {
                    ServerDlg.show();
                }

                Server.ApplySettings();
                exit_code = pApplication->exec();
            }
            else
#endif
            {
                // only start application without using the GUI
                qInfo() << qUtf8Printable ( GetVersionAndNameStr ( false ) );

                Server.ApplySettings();

                if ( commandlineOptions.directoryserver.Value().isEmpty() )
                {
                    Server.SetDirectoryType ( AT_CUSTOM );
                }

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

    if ( pServerRpc )
    {
        delete pServerRpc;
    }

#ifndef SERVER_ONLY
    if ( pClientRpc )
    {
        delete pClientRpc;
    }
#endif

    if ( pRpcServer )
    {
        delete pRpcServer;
    }

    if ( pCoreApplication )
    {
        delete pCoreApplication;
    }

#ifndef HEADLESS
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
