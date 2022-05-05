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

#pragma once

#include <QDomDocument>
#include <QObject>
#include "global.h"
#include "cmdlnoptions.h"

/* Classes ********************************************************************/

class CSettings : public QObject
{
    Q_OBJECT

public:
    CSettings ( bool bIsClient, bool bUseGUI, const QString& iniRootSection, const QString& iniDataSection );
    virtual ~CSettings();

public: // common settings
    // Commandline options with their values
    CCommandlineOptions CommandlineOptions;

    // Commandline arguments to store in ini file
    QStringList CommandlineArguments;

    QByteArray vecWindowPosMain;
    QString    strLanguage;

protected:
    QString strFileName;
    QString strRootSection;
    QString strDataSection;
    bool    bSettingsLoaded;

protected:
    void SetFileName ( const QString& sNFiName, const QString& sDefaultFileName );

    bool Load();
    bool Save();

    void ReadFromFile ( const QString& strCurFileName, QDomDocument& XMLDocument );
    void WriteToFile ( const QString& strCurFileName, const QDomDocument& XMLDocument );

    bool WriteCommandlineArgumentsToXML ( QDomNode& section );
    bool ReadCommandlineArgumentsFromXML ( const QDomNode& section );

    // The following functions implement the conversion from the general string
    // to base64 (which should be used for binary data in XML files). This
    // enables arbitrary utf8 characters to be used as the names in the GUI.
    //
    // ATTENTION: The "FromBase64[...]" functions must be used with caution!
    //            The reason is that if the FromBase64ToByteArray() is used to
    //            assign the stored value to a QString, this is incorrect but
    //            will not generate a compile error since there is a default
    //            conversion available for QByteArray to QString.
    QString    ToBase64 ( const QByteArray strIn ) const { return QString::fromLatin1 ( strIn.toBase64() ); }
    QString    ToBase64 ( const QString strIn ) const { return ToBase64 ( strIn.toUtf8() ); }
    QByteArray FromBase64ToByteArray ( const QString strIn ) const { return QByteArray::fromBase64 ( strIn.toLatin1() ); }
    QString    FromBase64ToString ( const QString strIn ) const { return QString::fromUtf8 ( FromBase64ToByteArray ( strIn ) ); }

    // xml section access functions for read/write
    // note: if bForceChild == false section itself will be used if it has a matching name (needed for backwards compatibility)
    const QDomNode GetSectionForRead ( const QDomNode& section, QString strSectionName, bool bForceChild = true );
    QDomNode       GetSectionForWrite ( QDomNode& section, QString strSectionName, bool bForceChild = true );

    // actual working functions for init-file access
    QString GetIniSetting ( const QDomNode& section, const QString& sKey, const QString& sDefaultVal = "" );
    bool    PutIniSetting ( QDomNode& section, const QString& sKey, const QString& sValue = "" );

    bool GetNumericIniSet ( const QDomNode& section, const QString& strKey, const int iRangeStart, const int iRangeStop, int& iValue );
    void SetNumericIniSet ( QDomNode& section, const QString& strKey, const int iValue = 0 );

    bool GetFlagIniSet ( const QDomNode& section, const QString& strKey, bool& bValue );
    void SetFlagIniSet ( QDomNode& section, const QString& strKey, const bool bValue = false );

protected:
    virtual void WriteSettingsToXML ( QDomNode& root )        = 0;
    virtual bool ReadSettingsFromXML ( const QDomNode& root ) = 0;
};
