#pragma once

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

#include "cmdline.h"
#include <float.h>

/******************************************************************************
 CmdlnOptions classes:
 ******************************************************************************/

enum class ECmdlnOptDestType
{
    Invalid = 0,
    Client  = 1,
    Server  = 2,
    Common  = 3,

    Main = 128 // help, version
};

enum class ECmdlnOptValType
{
    None   = 0,
    Flag   = 1,
    String = 2,
    // Number
    Double = 3,
    UInt   = 4,
    Int    = 5
};

enum class ECmdlnOptCheckResult
{

    // CCmdlnOptBase::check results
    InvalidDest   = -7, // parameter Ok, but not applicable for destType(s)
    InvalidType   = -6, // defined parameter eValueType is invalid (coding error!)
    InvalidLength = -5, // strValue is truncated (CCmndlnStringOption::check result only)
    InvalidRange  = -4, // number is out of range
    InvalidNumber = -3, // parameter is not a (integer) number as expected
    NoValue       = -2, // missing value (end of commandline after parameter)
    NoParam       = -1, // end of commandline
    NoMatch       = 0,  // no parameter name match

    OkFlag   = 1, // name match, flag parameter
    OkString = 2, // name match, String parameter
    OkNumber = 3  // name match, Ok Number parameter (dblValue param is set to the number by baseCheck)
};

/******************************************************************************
 CCmdlnOptBase: base class for all commandline option classes
 ******************************************************************************/

class CCmdlnOptBase
{
public:
    CCmdlnOptBase ( ECmdlnOptValType valueType, ECmdlnOptDestType destType, const QString& shortName, const QString& longName ) :
        bSet ( false ),
        eValueType ( valueType ),
        eDestType ( destType ),
        strShortName ( shortName ),
        strLongName ( longName ),
        bDepricated ( false ),
        pReplacedBy ( NULL )
    {}

    inline bool IsDepricated() const { return bDepricated; };

protected:
    friend class CCommandlineOptions;

    bool                    bDepricated;
    CCmdlnOptBase*          pReplacedBy;
    bool                    bSet;
    const ECmdlnOptValType  eValueType;
    const ECmdlnOptDestType eDestType;
    const QString           strShortName;
    const QString           strLongName;

protected:
    ECmdlnOptCheckResult baseCheck ( ECmdlnOptDestType destType,
                                     QStringList&      arguments,
                                     int&              i,
                                     QString&          strParam,
                                     QString&          strValue,
                                     double&           dblValue );

protected:
    friend class CSettings;
    friend class CClientSettings;
    friend class CServerSettings;

    void setIsDepricated ( CCmdlnOptBase* pReplacement = NULL )
    {
        pReplacedBy = pReplacement;
        bDepricated = true;
    }

    void set();
    void unset() { bSet = false; }

public:
    inline bool IsSet() const { return bSet; }
    inline bool IsOption ( const QString& argument ) const { return ( ( argument == strShortName ) || ( argument == strLongName ) ); }

protected:
    friend class CCommandlineOptions;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) = 0;
};

/******************************************************************************
 CCmndlnFlagOption: Defines a Flag commandline option
 ******************************************************************************/

class CCmndlnFlagOption : public CCmdlnOptBase
{
public:
    CCmndlnFlagOption ( ECmdlnOptDestType destType, const QString& shortName, const QString& longName ) :
        CCmdlnOptBase ( ECmdlnOptValType::Flag, destType, shortName, longName )
    {}

protected:
    friend class CCommandlineOptions;

    inline void setDepricated ( CCmndlnFlagOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

protected:
    friend class CCommandlineOptions;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCmndlnStringOption: Defines a String commandline option
 ******************************************************************************/

class CCmndlnStringOption : public CCmdlnOptBase
{
public:
    CCmndlnStringOption ( ECmdlnOptDestType destType,
                          const QString&    shortName,
                          const QString&    longName,
                          const QString     defVal = "",
                          int               maxLen = -1 ) :
        CCmdlnOptBase ( ECmdlnOptValType::String, destType, shortName, longName ),
        iMaxLen ( maxLen ),
        strValue ( defVal )
    {}

    void Clear()
    {
        strValue.clear();
        bSet = false;
    }

    inline const QString& Value() const { return strValue; }

protected:
    friend class CCommandlineOptions;

    int     iMaxLen;
    QString strValue;

    inline void setDepricated ( CCmndlnStringOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

    bool set ( QString val )
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

protected:
    friend class CCommandlineOptions;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCmndlnDoubleOption: Defines a double commandline option
 ******************************************************************************/

class CCmndlnDoubleOption : public CCmdlnOptBase
{
public:
    CCmndlnDoubleOption ( ECmdlnOptDestType destType,
                          const QString&    shortName,
                          const QString&    longName,
                          double            defVal = 0,
                          double            min    = DBL_MIN,
                          double            max    = DBL_MAX ) :
        CCmdlnOptBase ( ECmdlnOptValType::Double, destType, shortName, longName ),
        dValue ( defVal ),
        dMin ( min ),
        dMax ( max )
    {}

    inline double Value() { return dValue; };

protected:
    friend class CCommandlineOptions;

    double dValue;
    double dMin;
    double dMax;

    inline void SetDepricated ( CCmndlnDoubleOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

    bool set ( double val )
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

protected:
    friend class CCommandlineOptions;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCmndlnIntOption: Defines an integer commandline option
 ******************************************************************************/

class CCmndlnIntOption : public CCmdlnOptBase
{
public:
    CCmndlnIntOption ( ECmdlnOptDestType destType,
                       const QString&    shortName,
                       const QString&    longName,
                       int               defVal = 0,
                       int               min    = INT_MIN,
                       int               max    = INT_MAX ) :
        CCmdlnOptBase ( ECmdlnOptValType::Int, destType, shortName, longName ),
        iValue ( defVal ),
        iMin ( min ),
        iMax ( max )
    {}

    inline unsigned int Value() { return iValue; };

protected:
    friend class CCommandlineOptions;

    int iValue;
    int iMin;
    int iMax;

    inline void SetDepricated ( CCmndlnIntOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

    bool set ( int val )
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

protected:
    friend class CCommandlineOptions;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCmndlnUIntOption: Defines a unsigned int commandline option
 ******************************************************************************/

class CCmndlnUIntOption : public CCmdlnOptBase
{
public:
    CCmndlnUIntOption ( ECmdlnOptDestType destType,
                        const QString&    shortName,
                        const QString&    longName,
                        unsigned int      defVal = 0,
                        unsigned int      min    = 0,
                        unsigned int      max    = UINT_MAX ) :
        CCmdlnOptBase ( ECmdlnOptValType::UInt, destType, shortName, longName ),
        uiValue ( defVal ),
        uiMin ( min ),
        uiMax ( max )
    {}

    inline unsigned int Value() { return uiValue; };

protected:
    friend class CCommandlineOptions;

    unsigned int uiValue;
    unsigned int uiMin;
    unsigned int uiMax;

    inline void SetDepricated ( CCmndlnUIntOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

    bool set ( unsigned int val )
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

protected:
    friend class CCommandlineOptions;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

class CCommandlineOptions
{
public:
    CCommandlineOptions();

    // Load: Parses commandline and sets all given options.
    //       if bIsStored == false we are parsing the real commandline and arguments will contain the arguments to store on return
    //       if bIsStored == true we are parsing the stored commandline arguments and arguments will be unchanged on return
    bool Load ( bool bIsClient, bool bUseGUI, QStringList& arguments, bool bIsStored );

protected:
    // check: Called from Load
    //        Checks the Load result and shows any errors.
    bool CCommandlineOptions::showErrorMessage ( ECmdlnOptDestType eDestType,
                                                 const QString&    unknowOptions,
                                                 const QString&    invalidDests,
                                                 const QString&    invalidParams,
                                                 const QString&    depricatedParams );

public:
    // NOTE: when adding commandline options:
    //       first add short/long option name definitions in cmdline.h
    //       then add the appropriate option class here (name should be the same as the option long name)
    //       init the option class in the CCommandlineOptions constructor
    //       add the option to the array in the CCommandlineOptions::Load function

    // NOTE: server and nogui are read seperately in main. main corrects them depending on build configurarion
    //       and passes bIsClientand bUseGui to the Load function which will adjust server and nogui accordingly

    // NOTE: since the server option itself is already checked in main and, if it is present,
    //       CommandlineOptions are loaded as Server, as so --server is a server-only option here

    // NOTE: help and version are not in this list, since they are handled in main
    //       and CommandlineOptions are never loaded when any of those are on the commandline
    //       since main will exit before that.

    // Common options:
    CCmndlnStringOption inifile;
    CCmndlnFlagOption   nogui;
    CCmndlnUIntOption   port;
    CCmndlnUIntOption   jsonrpcport;
    CCmndlnStringOption jsonrpcsecretfile;
    CCmndlnUIntOption   qos;
    CCmndlnFlagOption   notranslation;
    CCmndlnFlagOption   enableipv6;

    // Client Only options:
    CCmndlnStringOption connect;
    CCmndlnFlagOption   nojackconnect;
    CCmndlnFlagOption   mutestream;
    CCmndlnFlagOption   mutemyown;
    CCmndlnStringOption clientname;
    CCmndlnStringOption ctrlmidich;
    CCmndlnStringOption centralserver;

    CCmndlnFlagOption showallservers;
    CCmndlnFlagOption showanalyzerconsole;

    // Server Only options:
    CCmndlnFlagOption   server;
    CCmndlnFlagOption   discononquit;
    CCmndlnStringOption directoryserver;
    CCmndlnStringOption directoryfile;
    CCmndlnStringOption listfilter;
    CCmndlnFlagOption   fastupdate;
    CCmndlnStringOption log;
    CCmndlnFlagOption   licence;
    CCmndlnStringOption htmlstatus;
    CCmndlnStringOption serverinfo;
    CCmndlnStringOption serverpublicip;
    CCmndlnFlagOption   delaypan;
    CCmndlnStringOption recording;
    CCmndlnFlagOption   norecord;
    CCmndlnStringOption serverbindip;
    CCmndlnFlagOption   multithreading;
    CCmndlnUIntOption   numchannels;
    CCmndlnStringOption welcomemessage;
    CCmndlnFlagOption   startminimized;

    // Special options:
    CCmndlnFlagOption special;
    CCmndlnFlagOption store;
};
