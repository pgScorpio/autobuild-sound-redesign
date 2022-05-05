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

#include <QHostAddress>
#include <QHostInfo>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QScopedPointer>

#ifdef USE_OPUS_SHARED_LIB
#    include "opus/opus_custom.h"
#else
#    include "opus_custom.h"
#endif

#include "global.h"
#include "socket.h"
#include "channel.h"
#include "util.h"
#include "buffer.h"
#include "signalhandler.h"
#include "settings.h"
#include "clientrpc.h"

#if defined( _WIN32 ) && !defined( JACK_ON_WINDOWS )
#    include "../windows/sound.h"
#else
#    if ( defined( Q_OS_MACX ) ) && !defined( JACK_REPLACES_COREAUDIO )
#        include "../mac/sound.h"
#    else
#        if defined( Q_OS_IOS )
#            include "../ios/sound.h"
#        else
#            ifdef ANDROID
#                include "../android/sound.h"
#            else
#                include "../linux/sound.h"
#                ifndef JACK_ON_WINDOWS // these headers are not available in Windows OS
#                    include <sched.h>
#                    include <netdb.h>
#                endif
#                include <socket.h>
#            endif
#        endif
#    endif
#endif

/* Definitions ****************************************************************/

// audio in fader range
#define AUD_FADER_IN_MIN    0
#define AUD_FADER_IN_MAX    100
#define AUD_FADER_IN_MIDDLE ( AUD_FADER_IN_MAX / 2 )

// audio reverberation range
#define AUD_REVERB_MAX 100

// default delay period between successive gain updates (ms)
// this will be increased to double the ping time if connected to a distant server
#define DEFAULT_GAIN_DELAY_PERIOD_MS 50

// OPUS number of coded bytes per audio packet
// TODO we have to use new numbers for OPUS to avoid that old CELT packets
// are used in the OPUS decoder (which gives a bad noise output signal).
// Later on when the CELT is completely removed we could set the OPUS
// numbers back to the original CELT values (to reduce network load)

// calculation to get from the number of bytes to the code rate in bps:
// rate [pbs] = Fs / L * N * 8, where
// Fs: sampling rate (SYSTEM_SAMPLE_RATE_HZ)
// L:  number of samples per packet (SYSTEM_FRAME_SIZE_SAMPLES)
// N:  number of bytes per packet (values below)
#define OPUS_NUM_BYTES_MONO_LOW_QUALITY                   12
#define OPUS_NUM_BYTES_MONO_NORMAL_QUALITY                22
#define OPUS_NUM_BYTES_MONO_HIGH_QUALITY                  36
#define OPUS_NUM_BYTES_MONO_LOW_QUALITY_DBLE_FRAMESIZE    25
#define OPUS_NUM_BYTES_MONO_NORMAL_QUALITY_DBLE_FRAMESIZE 45
#define OPUS_NUM_BYTES_MONO_HIGH_QUALITY_DBLE_FRAMESIZE   82

#define OPUS_NUM_BYTES_STEREO_LOW_QUALITY                   24
#define OPUS_NUM_BYTES_STEREO_NORMAL_QUALITY                35
#define OPUS_NUM_BYTES_STEREO_HIGH_QUALITY                  73
#define OPUS_NUM_BYTES_STEREO_LOW_QUALITY_DBLE_FRAMESIZE    47
#define OPUS_NUM_BYTES_STEREO_NORMAL_QUALITY_DBLE_FRAMESIZE 71
#define OPUS_NUM_BYTES_STEREO_HIGH_QUALITY_DBLE_FRAMESIZE   165

#define LEVELMETER_UPDATE_TIME_MS  100  // ms
#define CHECK_AUDIO_DEV_OK_TIME_MS 5000 // ms
#define DETECT_FEEDBACK_TIME_MS    3000 // ms

/* Classes ********************************************************************/

class CAudioDeviceSettings
{
public:
    CAudioDeviceSettings() :
        strName ( "" ),
        iLeftInputChannel ( 0 ),
        iRightInputChannel ( 1 ),
        iLeftOutputChannel ( 0 ),
        iRightOutputChannel ( 1 ),
        iPrefFrameSizeFactor ( 128 ),
        iInputBoost ( 1 )
    {}

public:
    QString strName;
    int     iLeftInputChannel;
    int     iRightInputChannel;
    int     iLeftOutputChannel;
    int     iRightOutputChannel;
    int     iPrefFrameSizeFactor;
    int     iInputBoost;
    // int  iLeftInputBoost;
    // int  iRightInputBoost;
};

class CClientSettings : public CSettings
{
    Q_OBJECT

public:
    CClientSettings ( bool bUseGUI );
    ~CClientSettings();

public:                        // Values without notifiers: (these don't need direct action on change, they are used 'on the fly')
    int iCustomDirectoryIndex; // index of selected custom directory server

    int  iNewClientFaderLevel;
    bool bConnectDlgShowAllMusicians;

    CVector<QString> vecStoredFaderTags;
    CVector<int>     vecStoredFaderLevels;
    CVector<int>     vecStoredPanValues;
    CVector<int>     vecStoredFaderIsSolo;
    CVector<int>     vecStoredFaderIsMute;
    CVector<int>     vecStoredFaderGroupID;
    CVector<QString> vstrIPAddress;

    EChSortType    eChannelSortType;
    EDirectoryType eDirectoryType;

    bool bEnableFeedbackDetection;

public: // window position/state settings
    QByteArray vecWindowPosSettings;
    QByteArray vecWindowPosChat;
    QByteArray vecWindowPosConnect;
    bool       bWindowWasShownSettings;
    bool       bWindowWasShownChat;
    bool       bWindowWasShownConnect;
    int        iSettingsTab;

public:
    // CustomDirectories
    // Special case:
    //     there are many ways vstrDirectoryAddress can be changed,
    //     so, after changing this one, one should always call OnCustomDirectoriesChanged() !!
    CVector<QString> vstrDirectoryAddress;
    void             OnCustomDirectoriesChanged() { emit CustomDirectoriesChanged(); }

protected: // values with notifiers: use Get/Set functions !
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
    CAudioDeviceSettings cAudioDevice;
    //### TODO: END ###//

    EGUIDesign  eGUIDesign;
    EMeterStyle eMeterStyle;

    EAudChanConf  eAudioChannelConfig;
    EAudioQuality eAudioQuality;

    CChannelCoreInfo ChannelInfo;

    int  iClientSockBufNumFrames;
    int  iServerSockBufNumFrames;
    bool bAutoSockBufSize;

    bool bEnableOPUS64;

    int iNumMixerPanelRows;

    int  iAudioInputBalance;
    int  iReverbLevel;
    bool bReverbOnLeftChan;

    bool bOwnFaderFirst;

signals:
    // Settings changed signals

    void CustomDirectoriesChanged();

    void InputBoostChanged();

    void AudioDeviceChanged();
    void InputChannelChanged();
    void OutputChannelChanged();
    void PrefFrameSizeFactorChanged();

    void GUIDesignChanged();
    void MeterStyleChanged();

    void AudioChannelConfigChanged();
    void AudioQualityChanged();

    void ChannelInfoChanged();

    void EnableOPUS64Changed();

    void ClientSockBufNumFramesChanged();
    void ServerSockBufNumFramesChanged();
    void AutoSockBufSizeChanged();

    void NumMixerPanelRowsChanged();

    void AudioInputBalanceChanged();

    void ReverbLevelChanged();
    void ReverbChannelChanged();

    void OwnFaderFirstChanged();

public:
    inline const QString& GetClientName() const { return CommandlineOptions.clientname.Value(); }

    inline QString GetAudioDevice() const { return cAudioDevice.strName; }
    bool           SetAudioDevice ( QString deviceName, bool bReinit = false )
    {
        if ( bReinit || ( cAudioDevice.strName != deviceName ) )
        {
            //### TODO: BEGIN ###//
            // get complete cAudioDevice for this device!
            cAudioDevice.strName = deviceName;
            //### TODO: END ###//
            emit AudioDeviceChanged();

            return true;
        }

        return false;
    }

    inline int GetInputBoost( /* bool bRight */ ) const { return cAudioDevice.iInputBoost; }

    bool SetInputBoost ( /* bool bRight */ int boost )
    {
        if ( cAudioDevice.iInputBoost != boost )
        {
            cAudioDevice.iInputBoost = boost;
            emit InputBoostChanged();

            return true;
        }

        return false;
    }

    int GetInputChannel ( bool bRight ) const
    {
        if ( bRight )
        {
            return cAudioDevice.iRightInputChannel;
        }
        else
        {
            return cAudioDevice.iLeftInputChannel;
        }
    }

    bool SetInputChannel ( bool bRight, int chNum )
    {
        if ( bRight )
        {
            if ( cAudioDevice.iRightInputChannel != chNum )
            {
                cAudioDevice.iRightInputChannel = chNum;
                emit InputChannelChanged();

                return true;
            }
        }
        else
        {
            if ( cAudioDevice.iLeftInputChannel != chNum )
            {
                cAudioDevice.iLeftInputChannel = chNum;
                emit InputChannelChanged();

                return true;
            }
        }

        return false;
    }

    int GetOutputChannel ( bool bRight ) const
    {
        if ( bRight )
        {
            return cAudioDevice.iRightOutputChannel;
        }
        else
        {
            return cAudioDevice.iLeftOutputChannel;
        }
    }

    bool SetOutputChannel ( bool bRight, int chNum )
    {
        if ( bRight )
        {
            if ( cAudioDevice.iRightOutputChannel != chNum )
            {
                cAudioDevice.iRightOutputChannel = chNum;
                emit OutputChannelChanged();

                return true;
            }
        }
        else
        {
            if ( cAudioDevice.iLeftOutputChannel != chNum )
            {
                cAudioDevice.iLeftOutputChannel = chNum;
                emit OutputChannelChanged();

                return true;
            }
        }

        return false;
    }

    inline int GetSndCrdPrefFrameSizeFactor() const { return cAudioDevice.iPrefFrameSizeFactor; }
    bool       SetSndCrdPrefFrameSizeFactor ( int iSize )
    {
        if ( cAudioDevice.iPrefFrameSizeFactor != iSize )
        {
            cAudioDevice.iPrefFrameSizeFactor = iSize;
            emit PrefFrameSizeFactorChanged();

            return true;
        }

        return false;
    }

    inline EGUIDesign GetGUIDesign() const { return eGUIDesign; }
    bool              SetGUIDesign ( EGUIDesign design )
    {
        if ( eGUIDesign != design )
        {
            eGUIDesign = design;
            emit GUIDesignChanged();

            return true;
        }

        return false;
    }

    inline EMeterStyle GetMeterStyle() const { return eMeterStyle; }
    bool               SetMeterStyle ( EMeterStyle style )
    {
        if ( eMeterStyle != style )
        {
            eMeterStyle = style;
            emit MeterStyleChanged();

            return true;
        }

        return false;
    }

    inline EAudChanConf GetAudioChannelConfig() const { return eAudioChannelConfig; }
    bool                SetAudioChannelConfig ( EAudChanConf config )
    {
        if ( eAudioChannelConfig != config )
        {
            eAudioChannelConfig = config;
            emit AudioChannelConfigChanged();

            return true;
        }

        return false;
    }

    inline EAudioQuality GetAudioQuality() const { return eAudioQuality; }
    bool                 SetAudioQuality ( EAudioQuality quality )
    {
        if ( eAudioQuality != quality )
        {
            eAudioQuality = quality;
            emit AudioQualityChanged();

            return true;
        }

        return false;
    }

    inline CChannelCoreInfo& GetChannelInfo() { return ChannelInfo; };
    bool                     SetChannelInfo ( const CChannelCoreInfo& info )
    {
        ChannelInfo = info;
        emit ChannelInfoChanged();

        return true;
    }

    inline const QString& GetChannelInfoName() const { return ChannelInfo.strName; }
    bool                  SetChannelInfoName ( const QString& name )
    {
        if ( ChannelInfo.strName != name )
        {
            ChannelInfo.strName = name;
            emit ChannelInfoChanged();

            return true;
        }

        return false;
    }

    inline const QLocale::Country GetChannelInfoCountry() const { return ChannelInfo.eCountry; }
    bool                          SetChannelInfoCountry ( const QLocale::Country country )
    {
        if ( ChannelInfo.eCountry != country )
        {
            ChannelInfo.eCountry = country;
            emit ChannelInfoChanged();

            return true;
        }

        return false;
    }

    inline const QString& GetChannelInfoCity() const { return ChannelInfo.strCity; }
    bool                  SetChannelInfoCity ( const QString& city )
    {
        if ( ChannelInfo.strCity != city )
        {
            ChannelInfo.strCity = city;
            emit ChannelInfoChanged();

            return true;
        }

        return false;
    }

    inline int GetChannelInfoInstrument() const { return ChannelInfo.iInstrument; }
    bool       SetChannelInfoInstrument ( const int instrument )
    {
        if ( ChannelInfo.iInstrument != instrument )
        {
            ChannelInfo.iInstrument = instrument;
            emit ChannelInfoChanged();

            return true;
        }

        return false;
    }

    inline ESkillLevel GetChannelInfoSkillLevel() const { return ChannelInfo.eSkillLevel; }
    bool               SetChannelInfoSkillLevel ( ESkillLevel skillLevel )
    {
        if ( ChannelInfo.eSkillLevel != skillLevel )
        {
            ChannelInfo.eSkillLevel = skillLevel;
            emit ChannelInfoChanged();

            return true;
        }

        return false;
    }

    inline int GetClientSockBufNumFrames() const { return iClientSockBufNumFrames; }
    bool       SetClientSockBufNumFrames ( int numFrames )
    {
        if ( iClientSockBufNumFrames != numFrames )
        {
            iClientSockBufNumFrames = numFrames;
            emit ClientSockBufNumFramesChanged();

            return true;
        }

        return false;
    }

    inline int GetServerSockBufNumFrames() const { return iServerSockBufNumFrames; }
    bool       SetServerSockBufNumFrames ( int numFrames )
    {
        if ( iServerSockBufNumFrames != numFrames )
        {
            iServerSockBufNumFrames = numFrames;
            emit ServerSockBufNumFramesChanged();

            return true;
        }

        return false;
    }

    inline bool GetAutoSockBufSize() const { return bAutoSockBufSize; }
    bool        SetAutoSockBufSize ( bool bAuto )
    {
        if ( bAutoSockBufSize != bAuto )
        {
            bAutoSockBufSize = bAuto;
            emit AutoSockBufSizeChanged();

            return true;
        }

        return false;
    }

    inline bool GetEnableOPUS64() const { return bEnableOPUS64; }
    bool        SetEnableOPUS64 ( bool bEnable )
    {
        if ( bEnableOPUS64 != bEnable )
        {
            bEnableOPUS64 = bEnable;
            emit EnableOPUS64Changed();

            return true;
        }

        return false;
    }

    inline int GetNumMixerPanelRows() const { return iNumMixerPanelRows; }
    bool       SetNumMixerPanelRows ( int rows )
    {
        if ( iNumMixerPanelRows != rows )
        {
            iNumMixerPanelRows = rows;
            emit NumMixerPanelRowsChanged();

            return true;
        }

        return false;
    }

    inline int GetAudioInputBalance() const { return iAudioInputBalance; }
    bool       SetAudioInputBalance ( int iValue )
    {
        if ( iAudioInputBalance != iValue )
        {
            iAudioInputBalance = iValue;
            emit AudioInputBalanceChanged();

            return true;
        }

        return false;
    }

    inline int GetReverbLevel() const { return iReverbLevel; }
    bool       SetReverbLevel ( int iLevel )
    {
        if ( iReverbLevel != iLevel )
        {
            iReverbLevel = iLevel;
            emit ReverbLevelChanged();

            return true;
        }

        return false;
    }

    inline bool GetReverbOnLeftChannel() const { return bReverbOnLeftChan; }
    bool        SetReverbOnLeftChannel ( bool bOnLeftChannel )
    {
        if ( bReverbOnLeftChan != bOnLeftChannel )
        {
            bReverbOnLeftChan = bOnLeftChannel;
            emit ReverbChannelChanged();

            return true;
        }

        return false;
    }

    inline bool GetOwnFaderFirst() const { return bOwnFaderFirst; }
    bool        SetOwnFaderFirst ( bool bOwnFirst )
    {
        if ( bOwnFaderFirst != bOwnFirst )
        {
            bOwnFaderFirst = bOwnFirst;
            emit OwnFaderFirstChanged();

            return true;
        }

        return false;
    }

    /*
    protected:
        void SelectSoundCard ( QString strName );
        void StoreSoundCard ( CSoundCardSettings& sndCardSettings );
    */

public: // Unsaved settings, needed by CClientSettingsDlg
    //### TODO: BEGIN ###//
    // these could be set by CSound on device selection or included in SoundProperties !
    // no more need for separate calls to CSound from CClient.Init() then.
    bool bFraSiFactPrefSupported;
    bool bFraSiFactDefSupported;
    bool bFraSiFactSafeSupported;
    //### TODO: END ###//

    bool bMuteOutStream;

public:
    void LoadFaderSettings ( const QString& strCurFileName );
    void SaveFaderSettings ( const QString& strCurFileName );

    // void SelectSoundCard ( QString strName );
    // void StoreSoundCard ( CSoundCardSettings& sndCardSettings );

protected:
    void ReadFaderSettingsFromXML ( const QDomNode& section );
    void WriteFaderSettingsToXML ( QDomNode& section );

protected:
    virtual bool ReadSettingsFromXML ( const QDomNode& root ) override;
    virtual void WriteSettingsToXML ( QDomNode& root ) override;
};

// CClientStatus ***************************************************************

//### TODO: BEGIN ###//
// Also fader values should go via CClientStatus too !!

class CChannelStatus : public CChannelInfo
{
public:
    CChannelStatus() :
        CChannelInfo(),
        iMixerBoardIndex ( -1 ),
        iLastSendFaderLevel ( iFaderLevel ),
        bActive ( false ),
        bOwnChannel ( false ),
        bMute ( false ),
        bSolo ( false ),
        iFaderLevel ( 0 ),
        iPanValue ( AUD_MIX_PAN_MAX / 2 )
    {}

protected:
    friend class CClientStatus;
    friend class CClient;
    friend class CAudioMixerBoard;

    int iMixerBoardIndex;
    int iLastSendFaderLevel;

public:
    bool bActive;
    bool bOwnChannel;
    bool bMute;
    bool bSolo;
    int  iFaderLevel;
    int  iPanValue;

public:
    int MixerBoardIndex() const { return iMixerBoardIndex; }
};
//### TODO: END ###//

class CClientStatus : public QObject
{
    Q_OBJECT

public:
    CClientStatus() :
        iSndCrdActualBufferSize ( 0 ),
        iMonoBlockSizeSam ( 0 ),
        iSndCardMonoBlockSizeSamConvBuff ( 0 ),
        bSndCrdConversionBufferRequired ( false ),
        strServerAddress(),
        strServerName(),
        bConnectRequested ( false ),
        bDisconnectRequested ( false ),
        bConnectionEnabled ( false ),
        bConnected ( false ),
        iUploadRateKbps ( 0 ),
        iCurPingTimeMs ( 0 ),
        iCurTotalDelayMs ( 0 ),
        bClientJitBufOKFlag ( true ),
        bServerJitBufOKFlag ( true ),
        dSignalLeveldBLeft ( 0 ),
        dSignalLeveldBRight ( 0 )
    {}

protected:
    QString strServerAddress;
    QString strServerName;

    bool bConnectRequested;
    bool bDisconnectRequested;
    bool bConnectionEnabled; // true if we are Connecting or Connected, false if we are Disconnecting or Disconnected
    bool bConnected;

protected:
    friend class CClient;

    int iSndCrdActualBufferSize;
    int iMonoBlockSizeSam; // TODO: rename

    bool bSndCrdConversionBufferRequired;
    int  iSndCardMonoBlockSizeSamConvBuff;

    int iCurPingTimeMs;
    int iCurTotalDelayMs;
    int iUploadRateKbps;

    bool bClientJitBufOKFlag;
    bool bServerJitBufOKFlag;

    double dSignalLeveldBLeft;
    double dSignalLeveldBRight;

    //     CVector<CChannelStatus> Channnel;

signals:
    void Connecting();
    void Disconnecting();

    void Connected();
    void Disconnected();

    // Request signals to CClient
    void ConnectRequested();
    void DisconnectRequested();

    void OpenDriverSetup(); // Just needed for signalling, no related value

public:
    inline int GetSndCrdActualBufferSize() const
    {
        // the actual sound card mono block size depends on whether a
        // sound card conversion buffer is used or not
        if ( bSndCrdConversionBufferRequired )
        {
            return iSndCardMonoBlockSizeSamConvBuff;
        }
        else
        {
            return iMonoBlockSizeSam;
        }
    }

    int GetSndCrdConvBufAdditionalDelayMonoBlSize()
    {
        if ( bSndCrdConversionBufferRequired )
        {
            // by introducing the conversion buffer we also introduce additional
            // delay which equals the "internal" mono buffer size
            return iMonoBlockSizeSam;
        }
        else
        {
            return 0;
        }
    }

    inline int    GetUploadRateKbps() const { return iUploadRateKbps; }
    inline int    GetPingTimeMs() const { return iCurPingTimeMs; }
    inline int    GetTotalDelayMs() const { return iCurTotalDelayMs; }
    inline bool   GetJitBufOK() const { return bClientJitBufOKFlag && bServerJitBufOKFlag; }
    inline bool   GetClientJitBufOK() const { return bClientJitBufOKFlag; }
    inline bool   GetServerJitBufOK() const { return bServerJitBufOKFlag; }
    inline double GetSignalLeveldBLeft() const { return dSignalLeveldBLeft; }
    inline double GetSignalLeveldBRight() const { return dSignalLeveldBRight; }

    inline const QString GetServerAddress() const { return strServerAddress; }
    inline const QString GetServerName() const { return strServerName; }
    inline bool          GetConnectionEnabled() const { return bConnectionEnabled || bConnectRequested; }
    bool                 StartConnection ( const QString& serverAddress, const QString& serverName )
    {
        if ( !bConnectionEnabled && !bConnectRequested && !serverAddress.isEmpty() )
        {
            strServerAddress  = serverAddress;
            strServerName     = serverName.isEmpty() ? serverAddress : serverName;
            bConnectRequested = true;
            emit ConnectRequested();

            return true;
        }

        return false;
    }

    bool EndConnection()
    {
        if ( bConnectionEnabled )
        {
            if ( !bDisconnectRequested )
            {
                bDisconnectRequested = true;
                emit DisconnectRequested();
            }

            return true;
        }

        return false;
    }

    void AckConnecting ( bool ack )
    {
        if ( bConnectRequested )
        {
            bConnectRequested  = false;
            bConnectionEnabled = ack;

            if ( ack )
            {
                emit Connecting();
            }
            else
            {
                bConnected = false;
            }
        }
    }

    void AckDisconnecting ( bool ack )
    {
        if ( bDisconnectRequested )
        {
            bDisconnectRequested = false;

            if ( ack )
            {
                emit Disconnecting();
            }
        }
    }

    inline bool GetConnected() const { return bConnected; }
    void        SetConnected ( bool bState = true )
    {
        bState &= bConnectionEnabled; // can't be connected if connection is not enabled !

        if ( bConnected != bState )
        {
            bConnected = bState;
            if ( bConnected )
            {
                emit Connected();
            }
            else
            {
                bConnectionEnabled = false;
                emit Disconnected();
            }
        }
    }

    void RequestDriverSetup() { emit OpenDriverSetup(); }
};

// CClient *********************************************************************

class CClient : public QObject
{
    Q_OBJECT

public:
    CClient ( bool bUseGUI );

    virtual ~CClient();

public:
    CClientSettings            Settings;
    CClientStatus              Status;
    QScopedPointer<CRpcServer> pRpcServer;
    QScopedPointer<CClientRpc> pClientRpc;

protected:
    void ApplySettings();

    bool SetServerAddr ( QString strNAddr );

public:
    bool IsRunning() { return Sound.IsRunning(); }

    bool IsConnected() { return Channel.IsConnected(); }

    void SetRemoteChanGain ( const int iId, const float fGain, const bool bIsMyOwnFader );
    void SetRemoteChanPan ( const int iId, const float fPan ) { Channel.SetRemoteChanPan ( iId, fPan ); }

    int GetClientSockBufNumFrames()
    {
        Settings.SetClientSockBufNumFrames ( Channel.GetSockBufNumFrames() );
        return Settings.GetClientSockBufNumFrames();
    }

    // sound card device selection
    QStringList GetSndCrdDevNames() { return Sound.GetDevNames(); }

    // sound card channel selection
    int     GetSndCrdNumInputChannels() { return Sound.GetNumInputChannels(); }
    QString GetSndCrdInputChannelName ( const int iDiD ) { return Sound.GetInputChannelName ( iDiD ); }

    int     GetSndCrdNumOutputChannels() { return Sound.GetNumOutputChannels(); }
    QString GetSndCrdOutputChannelName ( const int iDiD ) { return Sound.GetOutputChannelName ( iDiD ); }

    int GetSndCrdPrefFrameSizeFactor() { return iSndCrdPrefFrameSizeFactor; }

    int GetSystemMonoBlSize() { return Status.iMonoBlockSizeSam; }

    void GetBufErrorRates ( CVector<double>& vecErrRates, double& dLimit, double& dMaxUpLimit )
    {
        Channel.GetBufErrorRates ( vecErrRates, dLimit, dMaxUpLimit );
    }

protected:
    void OnTimerRemoteChanGain();
    void StartDelayTimer();

public:
    void CreateChatTextMes ( const QString& strChatText ) { Channel.CreateChatTextMes ( strChatText ); }

    void CreateCLPingMes() { ConnLessProtocol.CreateCLPingMes ( Channel.GetAddress(), PreparePingMessage() ); }

    void CreateCLServerListPingMes ( const CHostAddress& InetAddr )
    {
        ConnLessProtocol.CreateCLPingWithNumClientsMes ( InetAddr, PreparePingMessage(), 0 /* dummy */ );
    }

    void CreateCLServerListReqVerAndOSMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqVersionAndOSMes ( InetAddr ); }

    void CreateCLServerListReqConnClientsListMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqConnClientsListMes ( InetAddr ); }

    void CreateCLReqServerListMes ( const CHostAddress& InetAddr ) { ConnLessProtocol.CreateCLReqServerListMes ( InetAddr ); }

    int EstimatedOverallDelay();

protected:
    // callback function must be static, otherwise it does not work
    static void AudioCallback ( CVector<short>& psData, void* arg );

    void Init();
    void ProcessSndCrdAudioData ( CVector<short>& vecsStereoSndCrd );
    void ProcessAudioDataIntern ( CVector<short>& vecsStereoSndCrd );

    int  PreparePingMessage();
    int  EvaluatePingMessage ( const int iMs );
    void CreateServerJitterBufferMessage();

    // only one channel is needed for client application
    CChannel  Channel;
    CProtocol ConnLessProtocol;

    // audio encoder/decoder
    OpusCustomMode*        Opus64Mode;
    OpusCustomEncoder*     Opus64EncoderMono;
    OpusCustomDecoder*     Opus64DecoderMono;
    OpusCustomEncoder*     Opus64EncoderStereo;
    OpusCustomDecoder*     Opus64DecoderStereo;
    OpusCustomMode*        OpusMode;
    OpusCustomEncoder*     OpusEncoderMono;
    OpusCustomDecoder*     OpusDecoderMono;
    OpusCustomEncoder*     OpusEncoderStereo;
    OpusCustomDecoder*     OpusDecoderStereo;
    OpusCustomEncoder*     CurOpusEncoder;
    OpusCustomDecoder*     CurOpusDecoder;
    EAudComprType          eAudioCompressionType;
    int                    iCeltNumCodedBytes;
    int                    iOPUSFrameSizeSamples;
    int                    iNumAudioChannels;
    bool                   bIsInitializationPhase;
    float                  fMuteOutStreamGain;
    CVector<unsigned char> vecCeltData;

    CHighPrioSocket         Socket;
    CSound                  Sound;
    CStereoSignalLevelMeter SignalLevelMeter;

    CVector<uint8_t> vecbyNetwData;

    CAudioReverb AudioReverb;

    int iSndCrdPrefFrameSizeFactor;
    int iSndCrdFrameSizeFactor;

    CBuffer<int16_t> SndCrdConversionBufferIn;
    CBuffer<int16_t> SndCrdConversionBufferOut;
    CVector<int16_t> vecDataConvBuf;
    CVector<int16_t> vecsStereoSndCrdMuteStream;
    CVector<int16_t> vecZeros;

    int iStereoBlockSizeSam;

    bool   bJitterBufferOK;
    QMutex MutexDriverReinit;

    // for ping measurement
    QElapsedTimer PreciseTime;

    // for gain rate limiting
    QMutex MutexGain;

    // Timers
    QTimer TimerGain;
    QTimer TimerStatus;
    QTimer TimerSigMet;
    QTimer TimerCheckAudioDeviceOk;
    QTimer TimerDetectFeedback;

    int   minGainId;
    int   maxGainId;
    float oldGain[MAX_NUM_CHANNELS];
    float newGain[MAX_NUM_CHANNELS];

    CSignalHandler* pSignalHandler;

protected slots:
    void OnTimerUpdateStatus();
    void OnTimerSigMet();
    void OnTimerCheckAudioDeviceOk();

    void OnApplicationStartup();

    void OnHandledSignal ( int sigNum );
    void OnSendProtMessage ( CVector<uint8_t> vecMessage );
    void OnInvalidPacketReceived ( CHostAddress RecHostAddr );

    void OnDetectedCLMessage ( CVector<uint8_t> vecbyMesBodyData, int iRecID, CHostAddress RecHostAddr );

    void OnReqJittBufSize() { CreateServerJitterBufferMessage(); }
    void OnServerJittBufSizeChanged ( int iNewJitBufSize );
    void OnConnected();    // From Channel
    void OnDisconnected(); // From Channel

    void OnCLDisconnection ( CHostAddress InetAddr )
    {
        if ( InetAddr == Channel.GetAddress() )
        {
            Status.EndConnection();
            Status.SetConnected ( false );
        }
    }

    void OnCLPingReceived ( CHostAddress InetAddr, int iMs );

    void OnSendCLProtMessage ( CHostAddress InetAddr, CVector<uint8_t> vecMessage );

    void OnCLPingWithNumClientsReceived ( CHostAddress InetAddr, int iMs, int iNumClients );

    void OnSndCrdReinitRequest ( int iSndCrdResetType );
    void OnControllerInFaderLevel ( int iChannelIdx, int iValue );
    void OnControllerInPanValue ( int iChannelIdx, int iValue );
    void OnControllerInFaderIsSolo ( int iChannelIdx, bool bIsSolo );
    void OnControllerInFaderIsMute ( int iChannelIdx, bool bIsMute );
    void OnControllerInMuteMyself ( bool bMute );
    void OnClientIDReceived ( int iChanID );
    void OnAboutToQuit();

    void OnAudioDeviceChanged();
    void OnInputChannelChanged();
    void OnOutputChannelChanged();
    void OnPrefFrameSizeFactorChanged();

public slots:
    void OnConnectRequest();
    void OnDisconnectRequest();

    void OnReInitRequest();
    void OnReverbChannelChanged();
    void OnChannelInfoChanged();

    void OnClientSockBufNumFramesChanged() { Channel.SetSockBufNumFrames ( Settings.GetClientSockBufNumFrames() ); }

    void OnServerSockBufNumFramesChanged()
    {
        // if auto setting is disabled, inform the server about the new size
        if ( !Settings.GetAutoSockBufSize() )
        {
            Channel.CreateJitBufMes ( Settings.GetServerSockBufNumFrames() );
        }
    };

    void OnAutoSockBufSizeChanged()
    {
        // first, set new value in the channel object
        Channel.SetDoAutoSockBufSize ( Settings.GetAutoSockBufSize() );
        // inform the server about the change
        CreateServerJitterBufferMessage();
    }

    void OnDriverSetup()
    {
#if defined( _WIN32 ) && !defined( WITH_JACK )
        Sound.OpenDriverSetup();
#endif
    }

    void OnFeedbackDetectionChanged ( int state ) { emit AudioFeedbackStateChange ( state ); }

signals:
    void ApplicationStartup();
    void AudioFeedbackDetected();
    void AudioFeedbackStateChange ( int state );

    void StatusUpdated();
    void SignalLeveldbUpdated();

    void ConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo );
    void ChatTextReceived ( QString strChatText );
    void ClientIDReceived ( int iChanID );
    void MuteStateHasChangedReceived ( int iChanID, bool bIsMuted );
    void LicenceRequired ( ELicenceType eLicenceType );
    void VersionAndOSReceived ( COSUtil::EOpSystemType eOSType, QString strVersion );
    void PingTimeReceived();
    void RecorderStateReceived ( ERecorderState eRecorderState );

    void CLServerListReceived ( CHostAddress InetAddr, CVector<CServerInfo> vecServerInfo );

    void CLRedServerListReceived ( CHostAddress InetAddr, CVector<CServerInfo> vecServerInfo );

    void CLConnClientsListMesReceived ( CHostAddress InetAddr, CVector<CChannelInfo> vecChanInfo );

    void CLPingTimeWithNumClientsReceived ( CHostAddress InetAddr, int iPingTime, int iNumClients );

    void CLVersionAndOSReceived ( CHostAddress InetAddr, COSUtil::EOpSystemType eOSType, QString strVersion );

    void CLChannelLevelListReceived ( CHostAddress InetAddr, CVector<uint16_t> vecLevelList );

    //  void Disconnected();
    void SoundDeviceChanged ( QString strError );
    void ControllerInFaderLevel ( int iChannelIdx, int iValue );
    void ControllerInPanValue ( int iChannelIdx, int iValue );
    void ControllerInFaderIsSolo ( int iChannelIdx, bool bIsSolo );
    void ControllerInFaderIsMute ( int iChannelIdx, bool bIsMute );
    void ControllerInMuteMyself ( bool bMute );
};
