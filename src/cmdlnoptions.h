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
 Helpers:
 ******************************************************************************/

extern bool GetServerInfo ( const QString strServerInfo, QString& strServerName, QString& strServerCity, QLocale::Country& eServerCountry );
extern void SetServerInfo ( QString&               strServerInfo,
                            const QString&         strServerName,
                            const QString&         strServerCity,
                            const QLocale::Country eServerCountry );

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
    InvalidString = -5, // string content invalid (truncated ?)
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
    friend class CCommandlineOptions;
    friend class CSettings;
    friend class CClientSettings;
    friend class CServerSettings;

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
    void setIsDepricated ( CCmdlnOptBase* pReplacement = NULL )
    {
        pReplacedBy = pReplacement;
        bDepricated = true;
    }

    bool set();
    void unset() { bSet = false; }

public:
    inline bool IsSet() const { return bSet; }
    inline bool IsOption ( const QString& argument ) const { return ( ( argument == strShortName ) || ( argument == strLongName ) ); }

protected:
    virtual void clear() { unset(); }

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
    friend class CCommandlineOptions;

public:
    CCmndlnFlagOption ( ECmdlnOptDestType destType, const QString& shortName, const QString& longName ) :
        CCmdlnOptBase ( ECmdlnOptValType::Flag, destType, shortName, longName )
    {}

protected:
    inline void setDepricated ( CCmndlnFlagOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

protected:
    virtual void clear() { unset(); }

    virtual bool set() { return CCmdlnOptBase::set(); }

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
    friend class CCommandlineOptions;

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

    inline const QString& Value() const { return strValue; }

protected:
    int     iMaxLen;
    QString strValue;

    inline void setDepricated ( CCmndlnStringOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

protected:
    virtual void clear()
    {
        strValue.clear();
        bSet = false;
    }

    virtual bool set ( QString val );

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCmndlnStringListOption: Defines a commandline option containing a ; separated stringlist
 ******************************************************************************/

class CCmndlnStringListOption : public CCmndlnStringOption
{
    friend class CCommandlineOptions;

public:
    CCmndlnStringListOption ( ECmdlnOptDestType destType,
                              const QString&    shortName,
                              const QString&    longName,
                              const QString     defVal = "",
                              int               maxLen = -1 ) :
        CCmndlnStringOption ( destType, shortName, longName, defVal, maxLen ),
        strList()
    {}

    inline const QStringList& Value() const { return strList; }
    inline int                Size() const { return strList.size(); }

    inline const QString operator[] ( int index ) const
    {
        if ( ( index >= 0 ) && ( index < strList.size() ) )
        {
            return strList[index];
        }

        return QString();
    }

protected:
    QStringList strList;

protected:
    virtual void clear()
    {
        CCmndlnStringOption::clear();
        strList.clear();
    }

    virtual bool set ( QString val )
    {
        bool res = CCmndlnStringOption::set ( val );

        if ( bSet )
        {
            strList = strValue.split ( ";" );
            strValue.clear();
        }

        return res;
    }
    /*
        virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                             ECmdlnOptDestType destType,
                                             QStringList&      arguments,
                                             int&              i,
                                             QString&          strParam,
                                             QString&          strValue ) override;
    */
};

/******************************************************************************
 CCmndlnDoubleOption: Defines a double commandline option
 ******************************************************************************/

class CCmndlnDoubleOption : public CCmdlnOptBase
{
    friend class CCommandlineOptions;

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
    double dValue;
    double dMin;
    double dMax;

    inline void SetDepricated ( CCmndlnDoubleOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

protected:
    virtual void clear()
    {
        unset();
        dValue = 0;
    }

    virtual bool set ( double val );

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
    friend class CCommandlineOptions;

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
    int iValue;
    int iMin;
    int iMax;

    inline void SetDepricated ( CCmndlnIntOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

public:
    virtual void clear()
    {
        unset();
        iValue = 0;
    }

protected:
    virtual bool set ( int val );

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
    unsigned int uiValue;
    unsigned int uiMin;
    unsigned int uiMax;

    inline void SetDepricated ( CCmndlnUIntOption* pReplacement = NULL ) { CCmdlnOptBase::setIsDepricated ( pReplacement ); }

protected:
    virtual void clear()
    {
        unset();
        uiValue = 0;
    }

    virtual bool set ( unsigned int val );

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCmndlnServerInfoOption: Compound StringOption with server info
 ******************************************************************************/

class CCmndlnServerInfoOption : public CCmndlnStringOption
{
    friend class CCommandlineOptions;

public:
    CCmndlnServerInfoOption ( ECmdlnOptDestType destType,
                              const QString&    shortName,
                              const QString&    longName,
                              const QString     defVal = "",
                              int               maxLen = -1 ) :
        CCmndlnStringOption ( destType, shortName, longName, defVal, maxLen ),
        strServerName(),
        strServerCity(),
        eServerCountry ( QLocale::AnyCountry )
    {}

    const QString&   GetServerName() const { return strServerName; }
    const QString&   GetServerCity() const { return strServerCity; }
    QLocale::Country GetServerCountry() const { return eServerCountry; }

protected:
    QString          strServerName;
    QString          strServerCity;
    QLocale::Country eServerCountry;

public:
    virtual void clear()
    {
        CCmndlnStringOption::clear();
        strServerName.clear();
        strServerCity.clear();
        eServerCountry = QLocale::AnyCountry;
    }

protected:
    virtual bool set ( QString val ) override;

    virtual ECmdlnOptCheckResult check ( bool              bNoOverride,
                                         ECmdlnOptDestType destType,
                                         QStringList&      arguments,
                                         int&              i,
                                         QString&          strParam,
                                         QString&          strValue ) override;
};

/******************************************************************************
 CCommandlineOptions: The class containing all commandline options
 ******************************************************************************/

class CCommandlineOptions
{
public:
    CCommandlineOptions();

    // Load: Parses commandline and sets all given options.
    //       if bIsStored == false we are parsing the real commandline and on return arguments will contain any arguments to store
    //       if bIsStored == true we are parsing the stored commandline arguments and on return arguments will be unchanged
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
    //       and add the option to the array in the CCommandlineOptions::Load function

    // NOTE: server and nogui are read seperately in main. main corrects them depending on build configurarion
    //       and passes bIsClientand bUseGui to the Load function which will adjust server and nogui accordingly

    // NOTE: since the server option itself is already checked in main and, if it is present,
    //       CommandlineOptions are loaded for Server mode, as so --server is a server-only option here

    // NOTE: help and version are not in this list, since they are already handled in main
    //       and, since main will exit with one of these parameters, CommandlineOptions will never be created/loaded.

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
    CCmndlnFlagOption       server;
    CCmndlnFlagOption       discononquit;
    CCmndlnStringOption     directoryserver;
    CCmndlnStringOption     directoryfile;
    CCmndlnStringOption     listfilter;
    CCmndlnFlagOption       fastupdate;
    CCmndlnStringOption     log;
    CCmndlnFlagOption       licence;
    CCmndlnStringOption     htmlstatus;
    CCmndlnServerInfoOption serverinfo;
    CCmndlnStringOption     serverpublicip;
    CCmndlnFlagOption       delaypan;
    CCmndlnStringOption     recording;
    CCmndlnFlagOption       norecord;
    CCmndlnStringOption     serverbindip;
    CCmndlnFlagOption       multithreading;
    CCmndlnUIntOption       numchannels;
    CCmndlnStringOption     welcomemessage;
    CCmndlnFlagOption       startminimized;

    // Special options:
    CCmndlnFlagOption special;
    CCmndlnFlagOption store;
};
