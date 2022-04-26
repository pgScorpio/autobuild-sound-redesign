/******************************************************************************\
 * Copyright (c) 2022
 *
 * Author(s):
 *  Peter Goderie (pgScorpio)
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

#include "global.h"
#include "cmdlnoptions.h"
#include "messages.h"
#include <vector>

#ifdef HEADLESS
#    define TR( s ) QString ( s )
#else
#    define TR( s ) QObject::tr ( s )
#endif

/******************************************************************************
 CCmdlnOptBase: base class for all commandline option classes
 ******************************************************************************/

ECmdlnOptCheckResult CCmdlnOptBase::doCheck ( ECmdlnOptDestType destType,
                                              int               argc,
                                              char**            argv,
                                              int&              i,
                                              QString&          strParam,
                                              QString&          strValue,
                                              double&           dblValue )
{
    if ( ( i < 1 ) || ( i >= argc ) )
    {
        strParam.clear();
        strValue.clear();
        return ECmdlnOptCheckResult::NoParam;
    }

    ECmdlnOptCheckResult res    = ECmdlnOptCheckResult::NoParam;
    int                  iParam = i;
    int                  iValue = ( i + 1 );

    strParam = argv[iParam];
    strValue = ( iValue < argc ) ? argv[iValue] : "";

    if ( ( strParam != strShortName ) && ( strParam != strLongName ) )
    {
        return ECmdlnOptCheckResult::NoMatch;
    }

    switch ( eValueType )
    {
    case ECmdlnOptValType::None:
    case ECmdlnOptValType::Flag:
        res = ECmdlnOptCheckResult::OkFlag;
        break;

    case ECmdlnOptValType::String:
        if ( strValue.isEmpty() || ( strValue[0] == "-" ) ) // One should use "" if string starts with - !
        {
            res = ECmdlnOptCheckResult::NoValue;
        }
        else
        {
            i++;
            res = ECmdlnOptCheckResult::OkString;
        }
        break;

    default:
        // Numbers
        if ( strValue.isEmpty() )
        {
            res = ECmdlnOptCheckResult::NoValue;
        }
        else
        {
            char* p;
            dblValue = strtod ( argv[iValue], &p );
            if ( *p )
            {
                res = ECmdlnOptCheckResult::InvalidNumber;
            }
            else
            {
                i++;
                res = ECmdlnOptCheckResult::OkNumber;

                // Check if valid (unsigned) int
                switch ( eValueType )
                {
                case ECmdlnOptValType::UInt:
                    if ( ( dblValue < 0 ) || ( dblValue > (double) ( UINT_MAX ) ) )
                    {
                        res = ECmdlnOptCheckResult::InvalidRange;
                    }
                    else
                    {
                        unsigned int n = dblValue;
                        if ( n != dblValue )
                        {
                            // not an integer !
                            res = ECmdlnOptCheckResult::InvalidNumber;
                        }
                    }
                    break;

                case ECmdlnOptValType::Int:
                    if ( ( dblValue < (double) ( INT_MIN ) ) || ( dblValue > (double) ( INT_MAX ) ) )
                    {
                        res = ECmdlnOptCheckResult::InvalidRange;
                    }
                    else
                    {
                        int n = dblValue;
                        if ( n != dblValue )
                        {
                            // not an integer !
                            res = ECmdlnOptCheckResult::InvalidNumber;
                        }
                    }
                    break;

                case ECmdlnOptValType::Double:
                default:
                    break;
                }
            }
        }
        break;
    }

    if ( ( ( (unsigned int) destType ) & ( (unsigned int) eDestType ) ) == 0 )
    {
        return ECmdlnOptCheckResult::InvalidDest;
    }

    return res;
}

/******************************************************************************
 CCmndlnFlagOption: Defines a Flag commandline option
 ******************************************************************************/

ECmdlnOptCheckResult CCmndlnFlagOption::check ( ECmdlnOptDestType destType, int argc, char** argv, int& i, QString& strParam, QString& strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = doCheck ( destType, argc, argv, i, strParam, strValue, dblValue );

    if ( CheckRes == ECmdlnOptCheckResult::OkFlag )
    {
        Set();
    }

    return CheckRes;
}

/******************************************************************************
CCmndlnStringOption: Defines a String commandline option
******************************************************************************/

ECmdlnOptCheckResult CCmndlnStringOption::check ( ECmdlnOptDestType destType, int argc, char** argv, int& i, QString& strParam, QString& strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = doCheck ( destType, argc, argv, i, strParam, strValue, dblValue );

    if ( CheckRes == ECmdlnOptCheckResult::OkString )
    {
        if ( !Set ( strValue ) )
        {
            CheckRes = ECmdlnOptCheckResult::InvalidLength;
        }
    }

    return CheckRes;
}

/******************************************************************************
CCmndlnIntOption: Defines an integer commandline option
******************************************************************************/

ECmdlnOptCheckResult CCmndlnIntOption::check ( ECmdlnOptDestType destType, int argc, char** argv, int& i, QString& strParam, QString& strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = doCheck ( destType, argc, argv, i, strParam, strValue, dblValue );

    if ( CheckRes == ECmdlnOptCheckResult::OkNumber )
    {
        if ( !Set ( static_cast<int> ( dblValue ) ) )
        {
            CheckRes = ECmdlnOptCheckResult::InvalidRange;
        }
    }

    return CheckRes;
}

/******************************************************************************
 CCmndlnUIntOption: Defines a unsigned int commandline option
 ******************************************************************************/

ECmdlnOptCheckResult CCmndlnUIntOption::check ( ECmdlnOptDestType destType, int argc, char** argv, int& i, QString& strParam, QString& strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = doCheck ( destType, argc, argv, i, strParam, strValue, dblValue );

    if ( CheckRes == ECmdlnOptCheckResult::OkNumber )
    {
        uiValue = static_cast<int> ( dblValue );
        bSet    = true;
        if ( uiValue < uiMin )
        {
            uiValue  = uiMin;
            CheckRes = ECmdlnOptCheckResult::InvalidRange;
        }
        else if ( uiValue > uiMax )
        {
            uiValue  = uiMax;
            CheckRes = ECmdlnOptCheckResult::InvalidRange;
        }
    }

    return CheckRes;
}

/******************************************************************************
 CCommandlineOptions:
 ******************************************************************************/

CCommandlineOptions::CCommandlineOptions() :
    // NOTE: help, version are handled in main !
    //       they do not belong in this list, since CCommandlineOptions is never instantiated when they are on the commandline

    // Common options:
    inifile ( ECmdlnOptDestType::Common, CMDLN_INIFILE ),
    nogui ( ECmdlnOptDestType::Common, CMDLN_NOGUI ),
    port ( ECmdlnOptDestType::Common, CMDLN_PORT, DEFAULT_PORT_NUMBER, 0, 0xFFFF ),
    jsonrpcport ( ECmdlnOptDestType::Common, CMDLN_JSONRPCPORT, INVALID_PORT, 0, 0xFFFF ),
    jsonrpcsecretfile ( ECmdlnOptDestType::Common, CMDLN_JSONRPCSECRETFILE ),
    qos ( ECmdlnOptDestType::Common, CMDLN_QOS, DEFAULT_QOS_NUMBER, 0, 0xFFFF ),
    notranslation ( ECmdlnOptDestType::Common, CMDLN_NOTRANSLATION ),
    enableipv6 ( ECmdlnOptDestType::Common, CMDLN_ENABLEIPV6 ),

    // Client Only options:
    connect ( ECmdlnOptDestType::Client, CMDLN_CONNECT ),
    nojackconnect ( ECmdlnOptDestType::Client, CMDLN_NOJACKCONNECT ),
    mutestream ( ECmdlnOptDestType::Client, CMDLN_MUTESTREAM ),
    mutemyown ( ECmdlnOptDestType::Client, CMDLN_MUTEMYOWN ),
    clientname ( ECmdlnOptDestType::Client, CMDLN_CLIENTNAME ),
    ctrlmidich ( ECmdlnOptDestType::Client, CMDLN_CTRLMIDICH ),

    showallservers ( ECmdlnOptDestType::Client, CMDLN_SHOWALLSERVERS ),
    showanalyzerconsole ( ECmdlnOptDestType::Client, CMDLN_SHOWANALYZERCONSOLE ),

    // Server Only options:
    // note that the server option is already checked in main
    // and commandline options should be read as type Server if --server is on the commandline
    // so here server is only valid in Server mode
    server ( ECmdlnOptDestType::Server, CMDLN_SERVER ),
    discononquit ( ECmdlnOptDestType::Server, CMDLN_DISCONONQUIT ),
    directoryserver ( ECmdlnOptDestType::Server, CMDLN_DIRECTORYSERVER ),
    directoryfile ( ECmdlnOptDestType::Server, CMDLN_DIRECTORYFILE ),
    listfilter ( ECmdlnOptDestType::Server, CMDLN_LISTFILTER ),
    fastupdate ( ECmdlnOptDestType::Server, CMDLN_FASTUPDATE ),
    log ( ECmdlnOptDestType::Server, CMDLN_LOG ),
    licence ( ECmdlnOptDestType::Server, CMDLN_LICENCE ),
    htmlstatus ( ECmdlnOptDestType::Server, CMDLN_HTMLSTATUS ),
    serverinfo ( ECmdlnOptDestType::Server, CMDLN_SERVERINFO ),
    serverpublicip ( ECmdlnOptDestType::Server, CMDLN_SERVERPUBLICIP ),
    delaypan ( ECmdlnOptDestType::Server, CMDLN_DELAYPAN ),
    recording ( ECmdlnOptDestType::Server, CMDLN_RECORDING ),
    norecord ( ECmdlnOptDestType::Server, CMDLN_NORECORD ),
    serverbindip ( ECmdlnOptDestType::Server, CMDLN_SERVERBINDIP ),
    multithreading ( ECmdlnOptDestType::Server, CMDLN_MULTITHREADING ),
    numchannels ( ECmdlnOptDestType::Server, CMDLN_NUMCHANNELS, DEFAULT_USED_NUM_CHANNELS, 1, MAX_NUM_CHANNELS ),
    welcomemessage ( ECmdlnOptDestType::Server, CMDLN_WELCOMEMESSAGE ),
    startminimized ( ECmdlnOptDestType::Server, CMDLN_STARTMINIMIZED ),
    centralserver ( ECmdlnOptDestType::Server, CMDLN_CENTRALSERVER ),

    // Special option:
    // ignore invalid options after this option
    special ( ECmdlnOptDestType::Common, CMDLN_SPECIAL )
{
    centralserver.SetDepricated ( directoryserver );
}

bool CCommandlineOptions::check ( ECmdlnOptDestType eDestType,
                                  const QString&    unknowOptions,
                                  const QString&    invalidDests,
                                  const QString&    invalidParams,
                                  const QString&    depricatedParams )
{
    bool ok = !( unknowOptions.size() || invalidDests.size() || invalidParams.size() || unknowOptions.size() );

    if ( !ok )
    {
        QString message;

        if ( invalidDests.size() )
        {
            if ( eDestType == ECmdlnOptDestType::Client )
            {
                message = TR ( "The folowing option(s) are not valid for %1 mode" ).arg ( TR ( "Client" ) );
            }
            else if ( eDestType == ECmdlnOptDestType::Server )
            {
                message = TR ( "The folowing option(s) are not valid for %1 mode" ).arg ( TR ( "Server" ) );
            }
            else
            {
                message = TR ( "Invalid option(s) on commandline" );
            }
            CMessages::ShowError ( message + ":" + invalidDests );
        }
        else if ( invalidParams.size() )
        {
            message = TR ( "Option(s) with invalid values" );
            CMessages::ShowError ( message + ":" + invalidParams );
        }
        else if ( unknowOptions.size() )
        {
            message = TR ( "Unknown option(s) on commandline" );
            CMessages::ShowError ( message + ":" + unknowOptions );
        }
    }
    else if ( depricatedParams.size() )
    {
        QString message;
        message = TR ( "Depricated option(s) on commandline" );
        CMessages::ShowInfo ( message + ":" + depricatedParams + ", " +
                              TR ( "run %1 with %2 option for the correct options." ).arg ( APP_NAME, "--help" ) );
    }

    return ok;
}

bool CCommandlineOptions::Load ( bool bIsClient, bool bUseGUI, int argc, char** argv )
{
    ECmdlnOptDestType eDestType = bIsClient ? ECmdlnOptDestType::Client : ECmdlnOptDestType::Server;

    CCmdlnOptBase* cmdLineOption[] = { // Common
                                       &inifile,
                                       &nogui,
                                       &port,
                                       &jsonrpcport,
                                       &jsonrpcsecretfile,
                                       &qos,
                                       &notranslation,
                                       &enableipv6,
                                       // Client:
                                       &connect,
                                       &nojackconnect,
                                       &mutestream,
                                       &mutemyown,
                                       &clientname,
                                       &ctrlmidich,
                                       &centralserver,
                                       &showallservers,
                                       &showanalyzerconsole,
                                       // Server:
                                       &server,
                                       &discononquit,
                                       &directoryserver,
                                       &directoryfile,
                                       &listfilter,
                                       &fastupdate,
                                       &log,
                                       &licence,
                                       &htmlstatus,
                                       &serverinfo,
                                       &serverpublicip,
                                       &delaypan,
                                       &recording,
                                       &norecord,
                                       &serverbindip,
                                       &multithreading,
                                       &numchannels,
                                       &welcomemessage,
                                       &startminimized,
                                       // Special
                                       &special };

    const int optionsCount = ( sizeof ( cmdLineOption ) / sizeof ( cmdLineOption[0] ) );

    QString strParam;
    QString strValue;

    QString unknowOptions;
    QString invalidDests;
    QString invalidParams;
    QString depricatedParams;

    for ( int i = 1; i < argc; i++ )
    {
        bool optionFound = false;

        for ( int option = 0; option < optionsCount; option++ )
        {
            ECmdlnOptCheckResult res = cmdLineOption[option]->check ( eDestType, argc, argv, i, strParam, strValue );
            optionFound              = ( res != ECmdlnOptCheckResult::NoMatch );

            switch ( res )
            {
            case ECmdlnOptCheckResult::NoMatch:
                break;

            case ECmdlnOptCheckResult::InvalidDest:
                invalidDests += " " + strParam;
                break;

            case ECmdlnOptCheckResult::NoValue:
                invalidParams += " " + strParam + " ???";
                break;

            case ECmdlnOptCheckResult::InvalidLength:
            case ECmdlnOptCheckResult::InvalidRange:
            case ECmdlnOptCheckResult::InvalidNumber:
                invalidParams += " " + strParam;
                break;

            case ECmdlnOptCheckResult::OkFlag:
            case ECmdlnOptCheckResult::OkString:
            case ECmdlnOptCheckResult::OkNumber:
            case ECmdlnOptCheckResult::NoParam:
            default:
                break;
            }

            if ( optionFound )
            {
                if ( cmdLineOption[option]->IsDepricated() )
                {
                    depricatedParams += " " + strParam;
                }
                break;
            }
        }

        if ( !optionFound && !special.IsSet() )
        {
            unknowOptions += " " + strParam;
        }
    }

    if ( check ( eDestType, unknowOptions, invalidDests, invalidParams, depricatedParams ) )
    {
        QString message ( "- Forcing %1 mode due to application version." );

        if ( bIsClient && server.IsSet() )
        {
            server.Unset();
            qWarning() << message.arg ( "Client" );
        }
        else if ( !bIsClient && !server.IsSet() )
        {
            server.Set();
            qWarning() << message.arg ( "Server" );
        }

        if ( bUseGUI && nogui.IsSet() )
        {
            nogui.Unset();
            qWarning() << message.arg ( "GUI" );
        }
        else if ( !bUseGUI && !nogui.IsSet() )
        {
            nogui.Set();
            qWarning() << message.arg ( "HEADLESS" );
        }

        return true;
    }

    return false;
}
