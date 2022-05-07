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

// Translations:
//  define a macro for translation here so we can easily turn on/off translation
//  for all messages created here.
//  note that translation is probably not yet initialized anyway when loading the commandline options.

#ifdef HEADLESS
// Do never translate in headless mode !
#    define TR( s ) QString ( s )
#else
#    define TR( s ) QObject::tr ( s )
#endif

/******************************************************************************
 CCmdlnOptBase: base class for all commandline option classes
 ******************************************************************************/

void CCmdlnOptBase::set()
{
    if ( bDepricated && pReplacedBy )
    {
        // if pReplacedBy is set and *pReplacedBy is already set don't set this one !
        bSet = !pReplacedBy->bSet;
    }
    else
    {
        bSet = true;
    }
}

ECmdlnOptCheckResult CCmdlnOptBase::baseCheck ( ECmdlnOptDestType destType,
                                                QStringList&      arguments,
                                                int&              i,
                                                QString&          strParam,
                                                QString&          strValue,
                                                double&           dblValue )
{
    if ( ( i < 0 ) || ( i >= arguments.count() ) )
    {
        strParam.clear();
        strValue.clear();
        return ECmdlnOptCheckResult::NoParam;
    }

    ECmdlnOptCheckResult res    = ECmdlnOptCheckResult::NoParam;
    int                  iParam = i;
    int                  iValue = ( i + 1 );

    strParam = arguments[iParam];
    strValue = ( iValue < arguments.count() ) ? arguments[iValue] : "";

    if ( !IsOption ( strParam ) )
    {
        return ECmdlnOptCheckResult::NoMatch;
    }

    switch ( eValueType )
    {
    case ECmdlnOptValType::None:
    case ECmdlnOptValType::Flag:
        strValue.clear();
        res = ECmdlnOptCheckResult::OkFlag;
        break;

    case ECmdlnOptValType::String:
        if ( strValue.isEmpty() )
        {
            res = ECmdlnOptCheckResult::NoValue;
        }
        else if ( strValue[0] == "-" )
        {
            // One should use quotes if a string parameter value starts with - !
            strValue.clear();
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
            bool dblOk;
            dblValue = strValue.toDouble ( &dblOk );
            if ( !dblOk )
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
                        unsigned int n = static_cast<unsigned int> ( dblValue );
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
                        int n = static_cast<int> ( dblValue );
                        if ( n != dblValue )
                        {
                            // not an integer !
                            res = ECmdlnOptCheckResult::InvalidNumber;
                        }
                    }
                    break;

                case ECmdlnOptValType::Double:
                    break;

                default:
                    res = ECmdlnOptCheckResult::InvalidType;
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

ECmdlnOptCheckResult CCmndlnFlagOption::check ( bool              bNoOverride,
                                                ECmdlnOptDestType destType,
                                                QStringList&      arguments,
                                                int&              i,
                                                QString&          strParam,
                                                QString&          strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = baseCheck ( destType, arguments, i, strParam, strValue, dblValue );

    if ( ( CheckRes == ECmdlnOptCheckResult::OkFlag ) && !( bSet && bNoOverride ) )
    {
        set();
    }

    return CheckRes;
}

/******************************************************************************
CCmndlnStringOption: Defines a String commandline option
******************************************************************************/

ECmdlnOptCheckResult CCmndlnStringOption::check ( bool              bNoOverride,
                                                  ECmdlnOptDestType destType,
                                                  QStringList&      arguments,
                                                  int&              i,
                                                  QString&          strParam,
                                                  QString&          strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = baseCheck ( destType, arguments, i, strParam, strValue, dblValue );

    if ( ( CheckRes == ECmdlnOptCheckResult::OkString ) && !( bSet && bNoOverride ) )
    {
        if ( !set ( strValue ) )
        {
            CheckRes = ECmdlnOptCheckResult::InvalidLength;
        }
    }

    return CheckRes;
}

/******************************************************************************
CCmndlnDoubleOption: Defines a real number commandline option
******************************************************************************/

ECmdlnOptCheckResult CCmndlnDoubleOption::check ( bool              bNoOverride,
                                                  ECmdlnOptDestType destType,
                                                  QStringList&      arguments,
                                                  int&              i,
                                                  QString&          strParam,
                                                  QString&          strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = baseCheck ( destType, arguments, i, strParam, strValue, dblValue );

    if ( ( CheckRes == ECmdlnOptCheckResult::OkNumber ) && !( bSet && bNoOverride ) )
    {
        if ( !set ( dblValue ) )
        {
            CheckRes = ECmdlnOptCheckResult::InvalidRange;
        }
    }

    return CheckRes;
}

/******************************************************************************
CCmndlnIntOption: Defines an integer commandline option
******************************************************************************/

ECmdlnOptCheckResult CCmndlnIntOption::check ( bool              bNoOverride,
                                               ECmdlnOptDestType destType,
                                               QStringList&      arguments,
                                               int&              i,
                                               QString&          strParam,
                                               QString&          strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = baseCheck ( destType, arguments, i, strParam, strValue, dblValue );

    if ( ( CheckRes == ECmdlnOptCheckResult::OkNumber ) && !( bSet && bNoOverride ) )
    {
        if ( !set ( static_cast<int> ( dblValue ) ) )
        {
            CheckRes = ECmdlnOptCheckResult::InvalidRange;
        }
    }

    return CheckRes;
}

/******************************************************************************
 CCmndlnUIntOption: Defines a unsigned int commandline option
 ******************************************************************************/

ECmdlnOptCheckResult CCmndlnUIntOption::check ( bool              bNoOverride,
                                                ECmdlnOptDestType destType,
                                                QStringList&      arguments,
                                                int&              i,
                                                QString&          strParam,
                                                QString&          strValue )
{
    double dblValue;

    ECmdlnOptCheckResult CheckRes = baseCheck ( destType, arguments, i, strParam, strValue, dblValue );

    if ( ( CheckRes == ECmdlnOptCheckResult::OkNumber ) && !( bSet && bNoOverride ) )
    {
        if ( !set ( static_cast<unsigned int> ( dblValue ) ) )
        {
            CheckRes = ECmdlnOptCheckResult::InvalidRange;
        }
    }

    return CheckRes;
}

/******************************************************************************
 CCommandlineOptions:
 ******************************************************************************/

CCommandlineOptions::CCommandlineOptions() :
    // NOTE: help and version are handled in main so they do not need to be in this list,
    //       since when they are on the commandline, main will exit and CCommandlineOptions is never used

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
    // NOTE: server desttype is set to common here since it may be overidden in main
    //       if overidden by main a message will be shown.
    server ( ECmdlnOptDestType::Common, CMDLN_SERVER ),
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
    special ( ECmdlnOptDestType::Common, CMDLN_SPECIAL ),
    store ( ECmdlnOptDestType::Common, "--store", "--store" )
{
    centralserver.setDepricated ( &directoryserver );
}

bool CCommandlineOptions::showErrorMessage ( ECmdlnOptDestType eDestType,
                                             const QString&    unknowOptions,
                                             const QString&    invalidDests,
                                             const QString&    invalidParams,
                                             const QString&    depricatedParams )
{
    bool ok = !( unknowOptions.size() || invalidDests.size() || invalidParams.size() || unknowOptions.size() || depricatedParams.size() );

    if ( ok )
    {
        return false;
    }

    QString message = htmlBold ( TR ( "Commandline option errors" ) + ":" ) + htmlNewLine() htmlNewLine();
    QString msgHelp = QString ( htmlNewLine() ) + TR ( "Run %1 %2 for help on commandline options." ).arg ( APP_NAME, "--help" );

    if ( invalidDests.size() )
    {
        if ( eDestType == ECmdlnOptDestType::Client )
        {
            message += htmlBold ( TR ( "These commandline option(s) are not valid for %1 mode" ).arg ( TR ( "Client" ) ) );
        }
        else if ( eDestType == ECmdlnOptDestType::Server )
        {
            message += htmlBold ( TR ( "These commandline option(s) are not valid for %1 mode" ).arg ( TR ( "Server" ) ) );
        }
        else
        {
            message += htmlBold ( TR ( "Invalid commandline option(s)" ) );
        }
        message += ":" htmlNewLine() + invalidDests + htmlNewLine();
    }

    if ( invalidParams.size() )
    {
        message += htmlBold ( TR ( "Commandline option(s) with invalid values" ) );
        message += ":" htmlNewLine() + invalidParams + htmlNewLine();
    }

    if ( depricatedParams.size() )
    {
        message += htmlBold ( TR ( "Depricated commandline option(s)" ) );
        message += ":" htmlNewLine() + depricatedParams + htmlNewLine();
    }

    if ( unknowOptions.size() )
    {
        message += htmlBold ( TR ( "Unknown commandline option(s)" ) );
        message += ":" htmlNewLine() + unknowOptions + htmlNewLine();
    }

    message += msgHelp;
    if ( !CMessages::ShowErrorWait ( message, TR ( "Ignore" ), TR ( "Exit" ), false ) )
    {
        throw CErrorExit ( TR ( "Aborted on Commandline Option errors" ), 1 );
    }

    return true;
}

bool CCommandlineOptions::Load ( bool bIsClient, bool bUseGUI, QStringList& arguments, bool bIsStored )
{
    ECmdlnOptDestType eDestType = bIsClient ? ECmdlnOptDestType::Client : ECmdlnOptDestType::Server;

    CCmdlnOptBase* cmdLineOption[] = // Array for sequencial checking

        { // Common
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
          &special,
          &store };

    const int optionsCount = ( sizeof ( cmdLineOption ) / sizeof ( cmdLineOption[0] ) );

    QString strParam;
    QString strValue;

    QString unknowOptions;
    QString invalidDests;
    QString invalidParams;
    QString depricatedParams;

    for ( int i = 0; i < arguments.size(); i++ )
    {
        bool optionFound = false;

        for ( int option = 0; option < optionsCount; option++ )
        {
            ECmdlnOptCheckResult res = cmdLineOption[option]->check ( bIsStored, eDestType, arguments, i, strParam, strValue );
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
                invalidParams += " " + strParam + " " + strValue;
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

        if ( !optionFound )
        {
            if ( !special.IsSet() )
            {
                unknowOptions += " " + strParam;
            }
        }
    }

    // Check for errors
    if ( !showErrorMessage ( eDestType, unknowOptions, invalidDests, invalidParams, depricatedParams ) )
    {
        if ( !bIsStored )
        {
            // Check for commandline arguments to store
            if ( store.IsSet() )
            {
                QString argument;

                QStringList StoreCommandlineArguments;

                for ( int i = 0; i < arguments.size(); i++ )
                {
                    argument = arguments[i];

                    if ( inifile.IsOption ( argument ) )
                    {
                        // don't store inifile argument and inifile name
                        i++;
                    }
                    else if ( server.IsOption ( argument ) || nogui.IsOption ( argument ) || store.IsOption ( argument ) )
                    {
                        // don't store these arguments
                    }
                    else
                    {
                        // return arguments to be stored
                        StoreCommandlineArguments.append ( argument );
                    }
                }

                arguments = StoreCommandlineArguments;
            }
            else
            {
                // return NO arguments to be stored
                arguments.clear();
            }
        }

        QString message ( TR ( "Forcing %1 mode due to application version." ) );

        if ( bIsClient && server.IsSet() )
        {
            server.unset();
            CMessages::ShowWarningWait ( message.arg ( TR ( "Client" ) ) );
        }
        else if ( !bIsClient && !server.IsSet() )
        {
            server.set();
            CMessages::ShowWarningWait ( message.arg ( TR ( "Server" ) ) );
        }

        if ( bUseGUI && nogui.IsSet() )
        {
            nogui.unset();
            CMessages::ShowWarningWait ( message.arg ( TR ( "GUI" ) ) );
        }
        else if ( !bUseGUI && !nogui.IsSet() )
        {
            nogui.set();
            CMessages::ShowWarningWait ( message.arg ( TR ( "HEADLESS" ) ) );
        }

        return true;
    }

    if ( !bIsStored )
    {
        // return NO arguments to be stored
        arguments.clear();
    }

    return false;
}
