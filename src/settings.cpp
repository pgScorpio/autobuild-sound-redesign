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

#include "settings.h"
#include <QSettings>
#include "util.h"

/* Implementation *************************************************************/

CSettings::CSettings ( bool bIsClient, bool bUseGUI, const QString& iniRootSection, const QString& iniDataSection ) :
    bSettingsLoaded ( false ),
    strRootSection ( iniRootSection ),
    strDataSection ( iniDataSection ),
    CommandlineOptions(),
    CommandlineArguments(),
    vecWindowPosMain(), // empty array
    strLanguage(),
    strFileName()
{
    CCommandline::GetArgumentList ( CommandlineArguments );

    if ( !CommandlineOptions.Load ( bIsClient, bUseGUI, CommandlineArguments, false ) )
    {
#ifdef HEADLESS
        throw CErrorExit ( "Parameter Error(s), Exiting" );
#endif
    }
}

CSettings::~CSettings()
{
    // Somehow this one is called on an exception
    // but descendant destuctors are not !
}

bool CSettings::Load()
{
    bSettingsLoaded = false;

    // prepare file name for loading initialization data from XML file and read
    // data from file if possible
    QDomDocument IniXMLDocument;
    ReadFromFile ( strFileName, IniXMLDocument );
    QDomNode root = IniXMLDocument.firstChild();

    // read the settings from the given XML file
    bSettingsLoaded = ReadSettingsFromXML ( root );

    if ( CommandlineOptions.store.IsSet() )
    {
        if ( bSettingsLoaded )
        {
            // Save settings with commandline options
            // bSettingsLoaded is set to false on successfull save to Prevent Saving again from destructors !
            bSettingsLoaded = !Save();

            if ( !bSettingsLoaded )
            {
                // Successful save !
                if ( CommandlineArguments.size() )
                {
                    throw ( CInfoExit ( QString ( "Commandline parameters stored: \n" ) + CommandlineArguments.join ( " " ) ) );
                }
                else
                {
                    throw ( CInfoExit ( "Stored Commandline parameters cleared." ) );
                }
            }
        }

        throw ( CErrorExit ( "Commandline parameters NOT stored. Failed to read inifile!" ) );
    }

    // load translation
    if ( !CommandlineOptions.nogui.IsSet() && !CommandlineOptions.notranslation.IsSet() )
    {
        CLocale::LoadTranslation ( strLanguage, QApplication::instance() );
    }

    return bSettingsLoaded;
}

bool CSettings::Save()
{
    if ( !bSettingsLoaded )
    {
        // Do not save invalid settings !
        return false;
    }
    // create XML document for storing initialization parameters
    QDomDocument IniXMLDocument;
    QDomNode     root;
    QDomNode     section;

    if ( strDataSection == strRootSection )
    {
        // New single section document
        // data section to use is new root
        root    = IniXMLDocument.createElement ( strRootSection );
        section = IniXMLDocument.appendChild ( root );
    }
    else
    {
        // multi section document
        // read file first to keep other sections...
        ReadFromFile ( strFileName, IniXMLDocument );
        root = IniXMLDocument.firstChild();
        if ( root.isNull() )
        {
            // Empty document
            // create new root...
            root = IniXMLDocument.createElement ( strRootSection );
            IniXMLDocument.appendChild ( root );
            // use new data section under root
            section = GetSectionForWrite ( root, strDataSection, true );
        }
        else if ( root.nodeName() != strRootSection )
        {
            // old file with a single section ??
            // now creating a new file with multiple sections

            // store old root as a new section
            QDomDocumentFragment oldRoot = IniXMLDocument.createDocumentFragment();
            oldRoot.appendChild ( root );

            // Create new root
            root = IniXMLDocument.createElement ( strRootSection );
            // NOTE: This only orks thanks to a bug in Qt !!
            //       according to the documentation it should not be possible to add a second root element !
            IniXMLDocument.appendChild ( root );

            if ( section.nodeName() != strDataSection )
            {
                // old root section is not mine !
                // keep old root section as new section...
                root.appendChild ( oldRoot );
            }

            section.clear();
        }
        else
        {
            section = root.firstChildElement ( strDataSection );
            if ( !section.isNull() )
            {
                section.clear();
            }
        }

        if ( section.isNull() )
        {
            section = GetSectionForWrite ( root, strDataSection, true );
        }
    }

    if ( section.isNull() )
    {
        return false;
    }

    // write the settings in the XML file
    WriteSettingsToXML ( section );

    // prepare file name for storing initialization data in XML file and store
    // XML data in file
    WriteToFile ( strFileName, IniXMLDocument );

    return true;
}

void CSettings::ReadFromFile ( const QString& strCurFileName, QDomDocument& XMLDocument )
{
    QFile file ( strCurFileName );

    if ( file.open ( QIODevice::ReadOnly ) )
    {
        XMLDocument.setContent ( QTextStream ( &file ).readAll(), false );
        file.close();
    }
}

void CSettings::WriteToFile ( const QString& strCurFileName, const QDomDocument& XMLDocument )
{
    QFile file ( strCurFileName );

    if ( file.open ( QIODevice::WriteOnly ) )
    {
        QTextStream ( &file ) << XMLDocument.toString();
        file.close();
    }
}

bool CSettings::WriteCommandlineArgumentsToXML ( QDomNode& section )
{
    const int argumentCount = CommandlineArguments.size();

    if ( argumentCount > 0 )
    {
        section = GetSectionForWrite ( section, "commandline" );
        SetNumericIniSet ( section, "argumentcount", CommandlineArguments.size() );
        for ( int i = 1; i <= argumentCount; i++ )
        {
            PutIniSetting ( section, QString ( "argument%1" ).arg ( i ), CommandlineArguments[i - 1] );
        }

        return true;
    }

    return false;
}

bool CSettings::ReadCommandlineArgumentsFromXML ( const QDomNode& section )
{
    if ( CommandlineArguments.size() || CommandlineOptions.store.IsSet() )
    {
        // new commandline arguments are set,
        // so don't read the commandline arguments from file
        return false;
    }

    int     argumentCount = 0;
    QString argument;

    const QDomNode readSection = GetSectionForRead ( section, "commandline" );

    if ( GetNumericIniSet ( readSection, "argumentcount", 0, INT_MAX, argumentCount ) )
    {
        for ( int i = 1; i <= argumentCount; i++ )
        {
            argument = GetIniSetting ( readSection, QString ( "argument%1" ).arg ( i ) );
            if ( !argument.isEmpty() )
            {
                CommandlineArguments.append ( argument );
            }
        }
    }

    if ( CommandlineArguments.size() )
    {
        // process the read commandline arguments
        CommandlineOptions.Load ( !CommandlineOptions.server.IsSet(), !CommandlineOptions.nogui.IsSet(), CommandlineArguments, true );
        return true;
    }

    return false;
}

void CSettings::SetFileName ( const QString& sNFiName, const QString& sDefaultFileName )
{
    // return the file name with complete path, take care if given file name is empty
    strFileName = sNFiName;

    if ( strFileName.isEmpty() )
    {
        // we use the Qt default setting file paths for the different OSs by
        // utilizing the QSettings class
        const QString sConfigDir =
            QFileInfo ( QSettings ( QSettings::IniFormat, QSettings::UserScope, APP_NAME, APP_NAME ).fileName() ).absolutePath();

        // make sure the directory exists
        if ( !QFile::exists ( sConfigDir ) )
        {
            QDir().mkpath ( sConfigDir );
        }

        // append the actual file name
        strFileName = sConfigDir + "/" + sDefaultFileName;
    }
}

void CSettings::SetNumericIniSet ( QDomNode& section, const QString& strKey, const int iValue )
{
    // convert input parameter which is an integer to string and store
    PutIniSetting ( section, strKey, QString::number ( iValue ) );
}

bool CSettings::GetNumericIniSet ( const QDomNode& section, const QString& strKey, const int iRangeStart, const int iRangeStop, int& iValue )
{
    // init return value
    bool bReturn = false;

    const QString strGetIni = GetIniSetting ( section, strKey );

    // check if it is a valid parameter
    if ( !strGetIni.isEmpty() )
    {
        // convert string from init file to integer
        iValue = strGetIni.toInt();

        // check range
        if ( ( iValue >= iRangeStart ) && ( iValue <= iRangeStop ) )
        {
            bReturn = true;
        }
    }

    return bReturn;
}

void CSettings::SetFlagIniSet ( QDomNode& section, const QString& strKey, const bool bValue )
{
    // we encode true -> "1" and false -> "0"
    PutIniSetting ( section, strKey, bValue ? "1" : "0" );
}

bool CSettings::GetFlagIniSet ( const QDomNode& section, const QString& strKey, bool& bValue )
{
    // init return value
    bool bReturn = false;

    const QString strGetIni = GetIniSetting ( section, strKey );

    if ( !strGetIni.isEmpty() )
    {
        bValue  = ( strGetIni.toInt() != 0 );
        bReturn = true;
    }

    return bReturn;
}

// Init-file routines using XML ***********************************************

const QDomNode CSettings::GetSectionForRead ( const QDomNode& section, QString strSectionName, bool bForceChild )
{
    if ( !section.isNull() )
    {
        if ( !bForceChild && section.nodeName() == strSectionName )
        {
            return section;
        }

        return section.firstChildElement ( strSectionName );
    }

    return section;
}

QDomNode CSettings::GetSectionForWrite ( QDomNode& section, QString strSectionName, bool bForceChild )
{
    if ( !section.isNull() )
    {
        if ( !bForceChild && section.nodeName() == strSectionName )
        {
            return section;
        }

        QDomNode newSection = section.firstChildElement ( strSectionName );
        if ( newSection.isNull() )
        {
            newSection = section.ownerDocument().createElement ( strSectionName );
            if ( !newSection.isNull() )
            {
                section.appendChild ( newSection );
            }
        }

        return newSection;
    }

    return section;
}

QString CSettings::GetIniSetting ( const QDomNode& section, const QString& sKey, const QString& sDefaultVal )
{
    // init return parameter with default value
    QString sResult ( sDefaultVal );

    if ( !section.isNull() )
    {
        // get key
        QDomElement xmlKey = section.firstChildElement ( sKey );

        if ( !xmlKey.isNull() )
        {
            // get value
            sResult = xmlKey.text();
        }
    }

    return sResult;
}

bool CSettings::PutIniSetting ( QDomNode& section, const QString& sKey, const QString& sValue )
{
    if ( section.isNull() )
    {
        return false;
    }

    // check if key is already there, if not then create it
    QDomElement xmlKey = section.firstChildElement ( sKey );

    if ( xmlKey.isNull() )
    {
        // Add a new text key
        xmlKey = section.ownerDocument().createElement ( sKey );
        if ( section.appendChild ( xmlKey ).isNull() )
        {
            return false;
        }

        // add text data to the key
        QDomText textValue;
        textValue = section.ownerDocument().createTextNode ( sValue );
        return ( !xmlKey.appendChild ( textValue ).isNull() );
    }
    else
    {
        // Child should be a text node !
        xmlKey.firstChild().setNodeValue ( sValue );
        return true;
    }

    return false;
}
