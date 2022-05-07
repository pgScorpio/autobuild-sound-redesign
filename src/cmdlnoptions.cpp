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
 Helpers:
 ******************************************************************************/

bool GetServerInfo ( const QString strServerInfo, QString& strServerName, QString& strServerCity, QLocale::Country& eServerCountry )
{
    bool ok = false;

    strServerName.clear();
    strServerCity.clear();
    eServerCountry = QLocale::AnyCountry;

    if ( !strServerInfo.isEmpty() )
    {
        // split the info string
        QStringList slServInfoSeparateParams = strServerInfo.split ( ";" );
        int         iServInfoNumSplitItems   = slServInfoSeparateParams.count();

        if ( iServInfoNumSplitItems >= 1 )
        {
            ok            = ( !strServerName.isEmpty() && strServerName.length() <= MAX_LEN_SERVER_NAME );
            strServerName = slServInfoSeparateParams[0].left ( MAX_LEN_SERVER_NAME );

            if ( iServInfoNumSplitItems >= 2 )
            {
                ok &= ( strServerCity.length() <= MAX_LEN_SERVER_CITY );
                strServerCity = slServInfoSeparateParams[1].left ( MAX_LEN_SERVER_CITY );

                if ( iServInfoNumSplitItems >= 3 )
                {
                    bool      countryOk;
                    const int iCountry = slServInfoSeparateParams[2].toInt ( &countryOk );
                    ok &= countryOk;
                    if ( countryOk && ( iCountry >= 0 ) && ( iCountry <= QLocale::LastCountry ) )
                    {
                        eServerCountry = static_cast<QLocale::Country> ( iCountry );
                    }
                }
            }
        }
    }

    return ok;
}

void SetServerInfo ( QString& strServerInfo, const QString& strServerName, const QString& strServerCity, const QLocale::Country eServerCountry )
{
    strServerInfo = strServerName;
    strServerInfo += ";";
    strServerInfo += strServerCity;
    strServerInfo += ";";
    strServerInfo += (int) eServerCountry;
}

/******************************************************************************
 CCmdlnOptBase: base class for all commandline option classes
 ******************************************************************************/

bool CCmdlnOptBase::set()
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

    return bSet;
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

bool CCmndlnStringOption::set ( QString val )
{
    bool res = true;

    if ( pReplacedBy )
    {
        // Set replacement instead...
        res      = static_cast<CCmndlnStringOption*> ( pReplacedBy )->set ( val );
        strValue = static_cast<CCmndlnStringOption*> ( pReplacedBy )->strValue;
    }
    else
    {
        strValue = val;

        if ( strValue > iMaxLen )
        {
            strValue.truncate ( iMaxLen );
            res = false;
        }
    }

    CCmdlnOptBase::set();

    return res;
}

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
            CheckRes = ECmdlnOptCheckResult::InvalidString;
        }
    }

    return CheckRes;
}

/******************************************************************************
CCmndlnDoubleOption: Defines a real number commandline option
******************************************************************************/

bool CCmndlnDoubleOption::set ( double val )
{
    bool res = true;

    if ( pReplacedBy )
    {
        // Set replacement instead...
        res    = static_cast<CCmndlnDoubleOption*> ( pReplacedBy )->set ( val );
        dValue = static_cast<CCmndlnDoubleOption*> ( pReplacedBy )->dValue;
    }
    else
    {
        dValue = val;
        bSet   = true;

        if ( dValue < dMin )
        {
            dValue = dMin;
            res    = false;
        }
        else if ( dValue > dMax )
        {
            dValue = dMax;
            res    = false;
        }
    }

    CCmdlnOptBase::set();

    return res;
}

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

bool CCmndlnIntOption::set ( int val )
{
    bool res = true;

    if ( pReplacedBy )
    {
        // Set replacement instead...
        res    = static_cast<CCmndlnIntOption*> ( pReplacedBy )->set ( val );
        iValue = static_cast<CCmndlnIntOption*> ( pReplacedBy )->iValue;
    }
    else
    {
        iValue = val;
        bSet   = true;

        if ( iValue < iMin )
        {
            iValue = iMin;
            res    = false;
        }
        else if ( iValue > iMax )
        {
            iValue = iMax;
            res    = false;
        }
    }

    CCmdlnOptBase::set();

    return res;
}

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

bool CCmndlnUIntOption::set ( unsigned int val )
{
    bool res = true;

    if ( pReplacedBy )
    {
        // Set replacement instead...
        res     = static_cast<CCmndlnUIntOption*> ( pReplacedBy )->set ( val );
        uiValue = static_cast<CCmndlnUIntOption*> ( pReplacedBy )->uiValue;
    }
    else
    {
        uiValue = val;

        if ( uiValue < uiMin )
        {
            uiValue = uiMin;
            res     = false;
        }
        else if ( uiValue > uiMax )
        {
            uiValue = uiMax;
            res     = false;
        }
    }

    CCmdlnOptBase::set();

    return res;
}

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
 CCmndlnServerInfoOption: Compound StringOption with server info
 ******************************************************************************/

bool CCmndlnServerInfoOption::set ( QString val )
{
    CCmndlnStringOption::set ( val );

    if ( bSet )
    {
        return GetServerInfo ( strValue, strServerName, strServerCity, eServerCountry );
    }

    return false;
}

ECmdlnOptCheckResult CCmndlnServerInfoOption::check ( bool              bNoOverride,
                                                      ECmdlnOptDestType destType,
                                                      QStringList&      arguments,
                                                      int&              i,
                                                      QString&          strParam,
                                                      QString&          strValue )
{
    ECmdlnOptCheckResult res = CCmndlnStringOption::check ( bNoOverride, destType, arguments, i, strParam, strValue );

    if ( res == ECmdlnOptCheckResult::OkString )
    {
        QString strCheckInfo;
        SetServerInfo ( strCheckInfo, strServerName, strServerCity, eServerCountry );
        if ( strCheckInfo != strValue )
        {
            // Name, City truncated or invalid Country number !
            res = ECmdlnOptCheckResult::InvalidString;
        }
    }

    return res;
}

/******************************************************************************
 CCommandlineOptions: The class containing all commandline options
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

            case ECmdlnOptCheckResult::InvalidString:
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
