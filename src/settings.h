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
    {
        QObject::connect ( QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &CSettings::OnAboutToQuit );
    }

    void Load ( const QList<QString> CommandLineOptions );
    void Save();

    // common settings
    QByteArray vecWindowPosMain;
    QString    strLanguage;

protected:
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument )                                                  = 0;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& CommandLineOptions ) = 0;

    void ReadFromFile ( const QString& strCurFileName, QDomDocument& XMLDocument );

    void WriteToFile ( const QString& strCurFileName, const QDomDocument& XMLDocument );

    void SetFileName ( QString& strIniFile, const QString& sDefaultFileName );

    // The following functions implement the conversion from the general string
    // to base64 (which should be used for binary data in XML files). This
    // enables arbitrary utf8 characters to be used as the names in the GUI.
    //
    // ATTENTION: The "FromBase64[...]" functions must be used with caution!
    //            The reason is that if the FromBase64ToByteArray() is used to
    //            assign the stored value to a QString, this is incorrect but
    //            will not generate a compile error since there is a default
    //            conversion available for QByteArray to QString.
    static QString    ToBase64 ( const QByteArray strIn ) { return QString::fromLatin1 ( strIn.toBase64() ); }
    static QString    ToBase64 ( const QString strIn ) { return ToBase64 ( strIn.toUtf8() ); }
    static QByteArray FromBase64ToByteArray ( const QString strIn ) { return QByteArray::fromBase64 ( strIn.toLatin1() ); }
    static QString    FromBase64ToString ( const QString strIn ) { return QString::fromUtf8 ( FromBase64ToByteArray ( strIn ) ); }

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

public slots:
    void OnAboutToQuit() { Save(); }
};

#ifndef SERVER_ONLY
class CClientSettings : public CSettings
{
    friend class CClient;
    friend class CClientDlg;
    friend class CClientSettingsDlg;
    friend class CConnectDlg;
    friend class CClientRpc;

public:
    CClientSettings ( QString& strIniFileName,
                      // Commandline options: Should be replace by class
                      quint16  _iPortNumber,
                      quint16  _iQosNumber,
                      QString& _strConnOnStartupAddress,
                      QString& _strMIDISetup,
                      bool     _bNoAutoJackConnect,
                      QString& _strClientName,
                      bool     _bEnableIPv6,
                      bool     _bMuteMeInPersonalMix,
                      bool     _bShowAllServers,
                      bool     _bShowAnalyzerConsole,
                      bool     _bMuteStream ) :
        CSettings(),
        ChannelInfo(),
        iPortNumber ( _iPortNumber ),
        iQosNumber ( _iQosNumber ),
        strConnOnStartupAddress ( _strConnOnStartupAddress ),
        strMIDISetup ( _strMIDISetup ),
        bNoAutoJackConnect ( _bNoAutoJackConnect ),
        strClientName ( _strClientName ),
        bEnableIPv6 ( _bEnableIPv6 ),
        bMuteMeInPersonalMix ( _bMuteMeInPersonalMix ),
        bShowAllServers ( _bShowAllServers ),
        bShowAnalyzerConsole ( _bShowAnalyzerConsole ),
        bMuteStream ( _bMuteStream ),

        vecStoredFaderTags ( MAX_NUM_STORED_FADER_SETTINGS, "" ),
        vecStoredFaderLevels ( MAX_NUM_STORED_FADER_SETTINGS, AUD_MIX_FADER_MAX ),
        vecStoredFaderPanValues ( MAX_NUM_STORED_FADER_SETTINGS, AUD_MIX_PAN_MAX / 2 ),
        vecStoredFaderIsSolo ( MAX_NUM_STORED_FADER_SETTINGS, false ),
        vecStoredFaderIsMute ( MAX_NUM_STORED_FADER_SETTINGS, false ),
        vecStoredFaderGroupID ( MAX_NUM_STORED_FADER_SETTINGS, INVALID_INDEX ),

        vstrIPAddress ( MAX_NUM_SERVER_ADDR_ITEMS, "" ),
        iNewClientFaderLevel ( 100 ),
        iInputBoost ( 1 ),
        iInputChannelLeft ( 0 ),
        iInputChannelRight ( 1 ),
        iOutputChannelLeft ( 0 ),
        iOutputChannelRight ( 1 ),
        iSettingsTab ( SETTING_TAB_AUDIONET ),
        bConnectDlgShowAllMusicians ( true ),
        eChannelSortType ( ST_NO_SORT ),
        iNumMixerPanelRows ( 1 ),
        iCustomDirectoryIndex ( -1 ),
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
        eAudioQuality ( AQ_NORMAL ),
        eAudioChannelConf ( CC_MONO ),
        bAutoSockBufSize ( true ),
        iClientSockBufSize ( DEF_NET_BUF_SIZE_NUM_BL ),
        iServerSockBufSize ( DEF_NET_BUF_SIZE_NUM_BL ),
        eGUIDesign ( GD_ORIGINAL ),
        eMeterStyle ( MT_LED_STRIPE ),
        bEnableOPUS64 ( false ),
        iAudioInFader ( AUD_FADER_IN_MIDDLE ),
        bReverbOnLeftChan ( true ),
        iReverbLevel ( 0 ),
        iPrefFrameSizeFactor ( FRAME_SIZE_FACTOR_DEFAULT ),
        iFrameSizeFactor ( FRAME_SIZE_FACTOR_DEFAULT )

    {
        SetFileName ( strIniFileName, DEFAULT_INI_FILE_NAME );
    }

public:
    // TODO: All settings values should be accessed by Get/Set functions
    //       All Get functions should take commanline overides into account
    //       All Set functions should unset commandline values
    //       All Set functions should emit a signal on value change

    const QString& GetClientName() const { return strClientName; }

    bool GetOwnMixerFirst() const { return bOwnFaderFirst; }
    bool SetOwnMixerFirst ( bool ownFirst )
    {
        if ( bOwnFaderFirst != ownFirst )
        {
            bOwnFaderFirst = ownFirst;
            // emit...
            return true;
        }
        return false;
    };

    QString GetAudioDevice() const { return strAudioDevice; };
    void    SetAudioDevice ( const QString& strDeviceName )
    {
        if ( strAudioDevice != strDeviceName )
        {
            strAudioDevice = strDeviceName;
            // emit...
        }
    }

    int  GetInputChannelLeft() const { return iInputChannelLeft; }
    void SetInputChannelLeft ( int iChan )
    {
        if ( iInputChannelLeft != iChan )
        {
            iInputChannelLeft = iChan;
            // emit...
        }
    }

    int  GetInputChannelRight() const { return iInputChannelRight; }
    void SetInputChannelRight ( int iChan )
    {
        if ( iInputChannelRight != iChan )
        {
            iInputChannelRight = iChan;
            // emit...
        }
    }

    int  GetOutputChannelLeft() const { return iOutputChannelLeft; }
    void SetOutputChannelLeft ( int iChan )
    {
        if ( iOutputChannelLeft != iChan )
        {
            iOutputChannelLeft = iChan;
            // emit...
        }
    }

    int  GetOutputChannelRight() const { return iOutputChannelRight; }
    void SetOutputChannelRight ( int iChan )
    {
        if ( iOutputChannelRight != iChan )
        {
            iOutputChannelRight = iChan;
            // emit...
        }
    }

    int  GetInputBoost() const { return iInputBoost; }
    void SetInputBoost ( int iBoost )
    {
        if ( iInputBoost != iBoost )
        {
            iInputBoost = iBoost;
            // emit....
        }
    }

    EGUIDesign GetGUIDesign() const { return eGUIDesign; }
    void       SetGUIDesign ( const EGUIDesign eNGD ) { eGUIDesign = eNGD; }

    EMeterStyle GetMeterStyle() const { return eMeterStyle; }
    void        SetMeterStyle ( const EMeterStyle eNMT ) { eMeterStyle = eNMT; }

    EAudioQuality GetAudioQuality() const { return eAudioQuality; }
    void          SetAudioQuality ( const EAudioQuality eQuality )
    {
        if ( eAudioQuality != eQuality )
        {
            eAudioQuality = eQuality;
            // emit....
        }
    }

    EAudChanConf GetAudioChannelConfig() const { return eAudioChannelConf; }
    void         SetAudioChannelConfig ( const EAudChanConf eChanConf )
    {
        if ( eAudioChannelConf != eChanConf )
        {
            eAudioChannelConf = eChanConf;
            // emit....
        }
    }

    bool GetAutoSockBufSize() const { return bAutoSockBufSize; }
    void SetAutoSockBufSize ( bool bAuto )
    {
        if ( bAutoSockBufSize != bAuto )
        {
            bAutoSockBufSize = bAuto;
            // emit....
        }
    }

    int  GetClientSockBufSize() { return iServerSockBufSize; }
    void SetClientSockBufSize ( const int iSize )
    {
        if ( iServerSockBufSize != iSize )
        {
            iServerSockBufSize = iSize;
            // emit....
        }
    }

    int  GetServerSockBufSize() { return iServerSockBufSize; }
    void SetServerSockBufSize ( const int iSize )
    {
        if ( iServerSockBufSize != iSize )
        {
            iServerSockBufSize = iSize;
            // emit....
        }
    }

    bool GetEnableOPUS64() { return bEnableOPUS64; }
    void SetEnableOPUS64 ( const bool eEnable )
    {
        if ( bEnableOPUS64 != eEnable )
        {
            bEnableOPUS64 = eEnable;
            // emit....
        }
    }

    bool GetOwnFaderFirst() const { return bOwnFaderFirst; }
    void SetOwnFaderFirst ( bool bOwnFirst )
    {
        if ( bOwnFaderFirst != bOwnFirst )
        {
            bOwnFaderFirst = bOwnFirst;
            // emit....
        }
    }
    bool ToggleOwnFaderFirst()
    {
        SetOwnFaderFirst ( !bOwnFaderFirst );

        return bOwnFaderFirst;
    }

    bool GetEnableIPv6() const { return bEnableIPv6; }
    void SetEnableIPv6 ( bool bEnable )
    {
        if ( bEnableIPv6 != bEnable )
        {
            bEnableIPv6 = bEnable;
            // emit....
        }
    }
    bool ToggleEnableIPv6()
    {
        SetEnableIPv6 ( !bEnableIPv6 );
        return bEnableIPv6;
    }

    int  GetNumMixerPanelRows() const { return iNumMixerPanelRows; }
    void SetNumMixerPanelRows ( int numRows )
    {
        if ( iNumMixerPanelRows != numRows )
        {
            iNumMixerPanelRows = numRows;
            // emit....
        }
    }

    bool GetConnectDlgShowAllMusicians() const { return bConnectDlgShowAllMusicians; }
    void SetConnectDlgShowAllMusicians ( bool bShowAll )
    {
        if ( bConnectDlgShowAllMusicians != bShowAll )
        {
            bConnectDlgShowAllMusicians = bShowAll;
            // emit...
        }
    }
    bool ToggleConnectDlgShowAllMusicians()
    {
        SetConnectDlgShowAllMusicians ( !bConnectDlgShowAllMusicians );
        return bConnectDlgShowAllMusicians;
    }

    EChSortType GetChannelSortType() const { return eChannelSortType; }
    void        SetChannelSortType ( EChSortType eSortType )
    {
        if ( eChannelSortType != eSortType )
        {
            eChannelSortType = eSortType;
            // emit....
        }
    }

    bool IsReverbOnLeftChan() const { return bReverbOnLeftChan; }
    void SetReverbOnLeftChan ( bool bOnLeft )
    {
        if ( bReverbOnLeftChan != bOnLeft )
        {
            bReverbOnLeftChan = bOnLeft;
            // emit...
        }
    }
    bool TogleReverbOnLeftChan()
    {
        SetReverbOnLeftChan ( !bReverbOnLeftChan );
        return bReverbOnLeftChan;
    }

    int  GetReverbLevel() const { return iReverbLevel; }
    void SetReverbLevel ( int iLevel )
    {
        if ( iReverbLevel != iLevel )
        {
            iReverbLevel = iLevel;
            // emit....
        }
    }

    int  GetAudioInFader() const { return iAudioInFader; }
    void SetAudioInFader ( int iLevel )
    {
        if ( iAudioInFader != iLevel )
        {
            iAudioInFader = iLevel;
            // emit ??
        }
    }

    bool GetEnableFeedbackDetection() const { return bEnableFeedbackDetection; }
    void SetEnableFeedbackDetection ( bool bEnable )
    {
        if ( bEnableFeedbackDetection != bEnable )
        {
            bEnableFeedbackDetection = bEnable;
            // emit....
        }
    }
    bool ToggleEnableFeedbackDetection()
    {
        SetEnableFeedbackDetection ( !bEnableFeedbackDetection );
        return bEnableFeedbackDetection;
    }

    int GetPrefFrameSizeFactor() const { return iPrefFrameSizeFactor; }
    int SetPrefFrameSizeFactor ( int iFactor )
    {
        // first check new input parameter
        if ( ( iFactor == FRAME_SIZE_FACTOR_PREFERRED ) || ( iFactor == FRAME_SIZE_FACTOR_DEFAULT ) || ( iFactor == FRAME_SIZE_FACTOR_SAFE ) )
        {
            if ( iPrefFrameSizeFactor != iFactor )
            {
                iPrefFrameSizeFactor = iFactor;
                // emit....
            }
        }

        return iPrefFrameSizeFactor;
    }

    int GetFrameSizeFactor() const { return iFrameSizeFactor; }
    int SetFrameSizeFactor ( int iFactor )
    {
        // first check new input parameter
        if ( ( iFactor == FRAME_SIZE_FACTOR_PREFERRED ) || ( iFactor == FRAME_SIZE_FACTOR_DEFAULT ) || ( iFactor == FRAME_SIZE_FACTOR_SAFE ) )
        {
            if ( iFrameSizeFactor != iFactor )
            {
                iFrameSizeFactor = iFactor;
                // emit....
            }
        }

        return iFrameSizeFactor;
    }

    int  GetNewClientFaderLevel() const { return iNewClientFaderLevel; }
    void SetNewClientFaderLevel ( int iNewLevel )
    {
        if ( iNewClientFaderLevel != iNewLevel )
        {
            iNewClientFaderLevel = iNewLevel;
            // emit....
        }
    }

    void SetChannelInfoChanged()
    {
        // emit ChannelInfoChanged()
        //  update channel info at the server
        //  pClient->SetRemoteInfo();
    }

    void SetChannelInfoName ( const QString& strName )
    {
        if ( ChannelInfo.strName != strName )
        {
            ChannelInfo.strName = strName;
            SetChannelInfoChanged();
        }
    }

    void SetChannelInfoInstrument ( int iInstrument )
    {
        if ( ChannelInfo.iInstrument != iInstrument )
        {
            ChannelInfo.iInstrument = iInstrument;
            SetChannelInfoChanged();
        }
    }

    void SetChannelInfoCountry ( QLocale::Country eCountry )
    {
        if ( ChannelInfo.eCountry != eCountry )
        {
            ChannelInfo.eCountry = eCountry;
            SetChannelInfoChanged();
        }
    }

    void SetChannelInfoCity ( const QString& strCity )
    {
        if ( ChannelInfo.strCity != strCity )
        {
            ChannelInfo.strCity = strCity;
            SetChannelInfoChanged();
        }
    }

    void SetChannelInfoSkillLevel ( ESkillLevel eSkillLevel )
    {
        if ( ChannelInfo.eSkillLevel != eSkillLevel )
        {
            ChannelInfo.eSkillLevel = eSkillLevel;
            SetChannelInfoChanged();
        }
    }

public:
    // Values without Get/Set/signal

    CVector<QString> vecStoredFaderTags;
    CVector<int>     vecStoredFaderLevels;
    CVector<int>     vecStoredFaderPanValues;
    CVector<int>     vecStoredFaderIsSolo;
    CVector<int>     vecStoredFaderIsMute;
    CVector<int>     vecStoredFaderGroupID;
    CVector<QString> vstrIPAddress;

public:
    // window position/state settings
    int        iSettingsTab;
    QByteArray vecWindowPosSettings;
    QByteArray vecWindowPosChat;
    QByteArray vecWindowPosConnect;
    bool       bWindowWasShownSettings;
    bool       bWindowWasShownChat;
    bool       bWindowWasShownConnect;

protected:
    quint16  iPortNumber;
    quint16  iQosNumber;
    QString& strConnOnStartupAddress;
    QString& strMIDISetup;
    bool     bNoAutoJackConnect;
    QString& strClientName;
    bool     bEnableIPv6;
    bool     bMuteMeInPersonalMix;
    bool     bShowAllServers;
    bool     bShowAnalyzerConsole;
    bool     bMuteStream;

    EAudioQuality eAudioQuality;
    EAudChanConf  eAudioChannelConf;

    CChannelCoreInfo ChannelInfo;

    // general settings
    int iNewClientFaderLevel;

    int  iAudioInFader;
    bool bReverbOnLeftChan;
    int  iReverbLevel;

    QString strAudioDevice;
    int     iInputBoost;
    int     iInputChannelLeft;
    int     iInputChannelRight;
    int     iOutputChannelLeft;
    int     iOutputChannelRight;

    int iFrameSizeFactor;
    int iPrefFrameSizeFactor;

    bool bConnectDlgShowAllMusicians;

    EGUIDesign  eGUIDesign;
    EMeterStyle eMeterStyle;
    int         iNumMixerPanelRows;
    bool        bOwnFaderFirst;
    EChSortType eChannelSortType;

    int              iCustomDirectoryIndex; // index of selected custom directory server
    CVector<QString> vstrDirectoryAddress;
    EDirectoryType   eDirectoryType;

    bool bEnableFeedbackDetection;

    bool bAutoSockBufSize;
    int  iClientSockBufSize;
    int  iServerSockBufSize;

    bool bEnableOPUS64;

public:
    void LoadFaderSettings ( const QString& strCurFileName );
    void SaveFaderSettings ( const QString& strCurFileName );

protected:
    void ReadFaderSettingsFromXML ( const QDomDocument& IniXMLDocument );
    void WriteFaderSettingsToXML ( QDomDocument& IniXMLDocument );

protected:
    // No CommandLineOptions used when reading Client inifile
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument ) override;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& ) override;
};
#endif

class CServerSettings : public CSettings
{
    friend class CServer;
    friend class CServerDlg;

public:
    CServerSettings ( QString& sNFiName,
                      // Commandline options: Should be replace by class
                      int          _iMaxNumChan,
                      QString&     _strLoggingFileName,
                      QString&     _strServerBindIP,
                      quint16      _iPortNumber,
                      quint16      _iQosNumber,
                      QString&     _strHTMLStatusFileName,
                      QString&     _strDirectoryServer,
                      QString&     _strServerListFileName,
                      QString&     _strServerInfo,
                      QString&     _strServerListFilter,
                      QString&     _strServerPublicIP,
                      QString&     _strWelcomeMessage,
                      QString&     _strRecordingDirName,
                      bool         _bDisconnectAllClientsOnQuit,
                      bool         _bUseDoubleSystemFrameSize,
                      bool         _bUseMultithreading,
                      bool         _bDisableRecording,
                      bool         _bDelayPan,
                      bool         _bEnableIPv6,
                      bool         _bStartMinimized,
                      ELicenceType _eLicenceType ) :
        CSettings(),
        iMaxNumChan ( _iMaxNumChan ),
        strLoggingFileName ( _strLoggingFileName ),
        strServerBindIP ( _strServerBindIP ),
        iPortNumber ( _iPortNumber ),
        iQosNumber ( _iQosNumber ),
        strHTMLStatusFileName ( _strHTMLStatusFileName ),
        strDirectoryServer ( _strDirectoryServer ),
        strServerListFileName ( _strServerListFileName ),
        strServerInfo ( _strServerInfo ),
        strServerListFilter ( _strServerListFilter ),
        strServerPublicIP ( _strServerPublicIP ),
        strWelcomeMessage ( _strWelcomeMessage ),
        strRecordingDirName ( _strRecordingDirName ),
        bDisconnectAllClientsOnQuit ( _bDisconnectAllClientsOnQuit ),
        bUseDoubleSystemFrameSize ( _bUseDoubleSystemFrameSize ),
        bUseMultithreading ( _bUseMultithreading ),
        bDisableRecording ( _bDisableRecording ),
        bDelayPan ( _bDelayPan ),
        bEnableIPv6 ( _bEnableIPv6 ),
        bStartMinimized ( _bStartMinimized ),
        eLicenceType ( _eLicenceType ),
        eDirectoryType ( AT_NONE )
    {
        SetFileName ( sNFiName, DEFAULT_INI_FILE_NAME_SERVER );

        if ( !strDirectoryServer.isEmpty() )
        {
            eDirectoryType = AT_CUSTOM;
        }
    }

protected:
    friend class CServer;

    QString          strServerName;
    QString          strServerCity;
    QLocale::Country eServerCountry;
    bool             bEnableRecording;
    QString          strWelcomeMessage;
    QString          strRecordingDir;
    QString          strDirectoryAddress;
    EDirectoryType   eDirectoryType;

    int          iMaxNumChan;
    QString&     strLoggingFileName;
    QString&     strServerBindIP;
    quint16      iPortNumber;
    quint16      iQosNumber;
    QString&     strHTMLStatusFileName;
    QString&     strDirectoryServer;
    QString&     strServerListFileName;
    QString&     strServerInfo;
    QString&     strServerListFilter;
    QString&     strServerPublicIP;
    QString&     strRecordingDirName;
    bool         bStartMinimized;
    bool         bDisconnectAllClientsOnQuit;
    bool         bUseDoubleSystemFrameSize;
    bool         bUseMultithreading;
    bool         bDisableRecording;
    bool         bDelayPan;
    bool         bEnableIPv6;
    ELicenceType eLicenceType;

protected:
    virtual void WriteSettingsToXML ( QDomDocument& IniXMLDocument ) override;
    virtual void ReadSettingsFromXML ( const QDomDocument& IniXMLDocument, const QList<QString>& CommandLineOptions ) override;
};
