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
#include <QFile>
#include <QSettings>
#include <QDir>
#include "global.h"
#include "util.h"

/* Definitions ****************************************************************/
// audio in fader range
#define AUD_FADER_IN_MIN    0
#define AUD_FADER_IN_MAX    100
#define AUD_FADER_IN_MIDDLE ( AUD_FADER_IN_MAX / 2 )

// audio reverberation range
#define AUD_REVERB_MAX 100

/* Classes ********************************************************************/
class CSettings : public QObject
{
    Q_OBJECT

public:
    CSettings() :
        vecWindowPosMain(), // empty array
        strLanguage ( "" ),
        strFileName ( "" )
    {}

    // common settings
    QByteArray vecWindowPosMain;
    QString    strLanguage;

protected:
    void Load();
    void Save();

    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument )        = 0;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument ) = 0;

    void ReadFromFile ( const QString& strCurFileName, QDomDocument& XMLDocument );

    void WriteToFile ( const QString& strCurFileName, const QDomDocument& XMLDocument );

    void SetFileName ( const QString& sNFiName, const QString& sDefaultFileName );

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

    // init file access function for read/write
    void SetNumericIniSet ( QDomDocument& xmlFile, const QString& strSection, const QString& strKey, const int iValue = 0 );

    bool GetNumericIniSet ( const QDomDocument& xmlFile,
                            const QString&      strSection,
                            const QString&      strKey,
                            const int           iRangeStart,
                            const int           iRangeStop,
                            int&                iValue );

    void SetFlagIniSet ( QDomDocument& xmlFile, const QString& strSection, const QString& strKey, const bool bValue = false );

    bool GetFlagIniSet ( const QDomDocument& xmlFile, const QString& strSection, const QString& strKey, bool& bValue );

    // actual working function for init-file access
    QString GetIniSetting ( const QDomDocument& xmlFile, const QString& sSection, const QString& sKey, const QString& sDefaultVal = "" );

    void PutIniSetting ( QDomDocument& xmlFile, const QString& sSection, const QString& sKey, const QString& sValue = "" );

    QString strFileName;
};

#ifndef SERVER_ONLY
class CClientSettings : public CSettings
{
public:
    CClientSettings ( const QString& sNFiName ) :
        CSettings(),
        vecStoredFaderTags ( MAX_NUM_STORED_FADER_SETTINGS, "" ),
        vecStoredFaderLevels ( MAX_NUM_STORED_FADER_SETTINGS, AUD_MIX_FADER_MAX ),
        vecStoredPanValues ( MAX_NUM_STORED_FADER_SETTINGS, AUD_MIX_PAN_MAX / 2 ),
        vecStoredFaderIsSolo ( MAX_NUM_STORED_FADER_SETTINGS, false ),
        vecStoredFaderIsMute ( MAX_NUM_STORED_FADER_SETTINGS, false ),
        vecStoredFaderGroupID ( MAX_NUM_STORED_FADER_SETTINGS, INVALID_INDEX ),
        vstrIPAddress ( MAX_NUM_SERVER_ADDR_ITEMS, "" ),
        iNewClientFaderLevel ( 100 ),
        iInputBoost ( 1 ),
        iSettingsTab ( SETTING_TAB_AUDIONET ),
        bConnectDlgShowAllMusicians ( true ),
        eChannelSortType ( ST_NO_SORT ),
        iNumMixerPanelRows ( 1 ),
        vstrDirectoryAddress ( MAX_NUM_SERVER_ADDR_ITEMS, "" ),
        eDirectoryType ( AT_DEFAULT ),
        bEnableFeedbackDetection ( true ),
        vecWindowPosSettings(), // empty array
        vecWindowPosChat(),     // empty array
        vecWindowPosConnect(),  // empty array
        bWindowWasShownSettings ( false ),
        bWindowWasShownChat ( false ),
        bWindowWasShownConnect ( false ),
        bOwnFaderFirst ( false ),
        ChannelInfo(),
        eAudioQuality ( AQ_NORMAL ),
        eAudioChannelConfig ( CC_MONO ),
        eGUIDesign ( GD_ORIGINAL ),
        eMeterStyle ( MT_LED_STRIPE ),
        bEnableOPUS64 ( false ),
        iAudioInFader ( AUD_FADER_IN_MIDDLE ),
        bReverbOnLeftChan ( false ),
        iReverbLevel ( 0 ),
        iServerSockBufNumFrames ( DEF_NET_BUF_SIZE_NUM_BL ),
        bFraSiFactPrefSupported ( false ),
        bFraSiFactDefSupported ( false ),
        bFraSiFactSafeSupported ( false ),
        bMuteOutStream ( false ),
        bConnectState ( false )
    {
        SetFileName ( sNFiName, DEFAULT_INI_FILE_NAME );
        Load();
        // SelectSoundCard ( strCurrentAudioDevice );
    }

    ~CClientSettings()
    {
        // StoreSoundCard( CurrentSoundCard );
        Save();
    }

    //### TODO: BEGIN ###//
    // new values need to be initialised in constructor and read by CClient

    CChannelCoreInfo ChannelInfo;

    //### TODO: BEGIN ###//
    // After the sound-redesign we should store all Soundcard settings per device (and per device type!),
    // i.e. in a CVector< CSoundCardSettings > SoundCard[],
    // and set a current Device: CSoundCardSettings CurrentSoundCard = SoundCard[n]
    // CurrentSoundCard.strName will be stored in the inifile as 'CurrentSoundCard'
    // and there should be a function Settings.SelectSoundCard ( strName )
    // there should also be a function Settings.StoreSoundCard( CSoundCardSettings& sndCardSettings)
    // Which will store sndCardSettings in the matching SoundCard[x], or add a new device to
    // SoundCard[] when no SoundCard[x] with sndCardSettings.strName already exists.
    //
    QString strCurrentAudioDevice;
    int     iSndCrdLeftInputChannel;
    int     iSndCrdRightInputChannel;
    int     iSndCrdLeftOutputChannel;
    int     iSndCrdRightOutputChannel;
    int     iSndCrdPrefFrameSizeFactor;
    // int  iSndCrdLeftInputBoost;
    // int  iSndCrdRightInputBoost;
    //### TODO: END ###//

    int           iAudioInFader;
    int           iReverbLevel;
    bool          bReverbOnLeftChan;
    bool          bAutoSockBufSize;
    int           iClientSockBufNumFrames;
    int           iServerSockBufNumFrames;
    bool          bEnableOPUS64;
    EGUIDesign    eGUIDesign;
    EMeterStyle   eMeterStyle;
    EAudChanConf  eAudioChannelConfig;
    EAudioQuality eAudioQuality;

    // general settings
    CVector<QString> vecStoredFaderTags;
    CVector<int>     vecStoredFaderLevels;
    CVector<int>     vecStoredPanValues;
    CVector<int>     vecStoredFaderIsSolo;
    CVector<int>     vecStoredFaderIsMute;
    CVector<int>     vecStoredFaderGroupID;
    CVector<QString> vstrIPAddress;
    int              iNewClientFaderLevel;
    int              iInputBoost;
    int              iSettingsTab;
    bool             bConnectDlgShowAllMusicians;
    EChSortType      eChannelSortType;
    int              iNumMixerPanelRows;
    CVector<QString> vstrDirectoryAddress;
    EDirectoryType   eDirectoryType;
    int              iCustomDirectoryIndex; // index of selected custom directory server
    bool             bEnableFeedbackDetection;

    // window position/state settings
    QByteArray vecWindowPosSettings;
    QByteArray vecWindowPosChat;
    QByteArray vecWindowPosConnect;
    bool       bWindowWasShownSettings;
    bool       bWindowWasShownChat;
    bool       bWindowWasShownConnect;
    bool       bOwnFaderFirst;

public:
    // Unsaved settings, needed by CClientSettingsDlg
    //### TODO: BEGIN ###//
    // these could be set by CSound on device selection !
    // no more need for separate calls to CSound from CClient.Init() then.
    bool bFraSiFactPrefSupported;
    bool bFraSiFactDefSupported;
    bool bFraSiFactSafeSupported;
    //### TODO: END ###//

    bool bMuteOutStream;
    bool bConnectState;

public:
    void LoadFaderSettings ( const QString& strCurFileName );
    void SaveFaderSettings ( const QString& strCurFileName );

    // void SelectSoundCard ( QString strName );
    // void StoreSoundCard ( CSoundCardSettings& sndCardSettings );

protected:
    // No CommandLineOptions used when reading Client inifile
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument ) override;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument ) override;

    void ReadFaderSettingsFromXML ( const QDomDocument& IniXMLDocument );
    void WriteFaderSettingsToXML ( QDomDocument& IniXMLDocument );
};
#endif

class CServerSettings : public CSettings
{
public:
    CServerSettings ( const QString& sNFiName ) : CSettings()
    {
        SetFileName ( sNFiName, DEFAULT_INI_FILE_NAME_SERVER );
        Load();
    }

    ~CServerSettings() { Save(); }

    //### TODO: BEGIN ###//
    // CHECK! these new values need to be initialised in constructor and read by CClient
    QString          strServerName;
    QString          strServerCity;
    QLocale::Country eServerCountry;
    bool             bEnableRecording;
    QString          strWelcomeMessage;
    QString          strRecordingDir;
    QString          strDirectoryAddress;
    EDirectoryType   eDirectoryType;
    QString          strServerListFileName;
    bool             bAutoRunMinimized;
    bool             bDelayPan;
    //### TODO: END ###//

protected:
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument ) override;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument ) override;
};
