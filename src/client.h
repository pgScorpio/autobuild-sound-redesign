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

/* Classes ********************************************************************/
class CClient : public QObject
{
    Q_OBJECT

public:
    CClient ( CClientSettings& cSettings );

    virtual ~CClient();

    void ApplySettings();

    void Start();
    void Stop();
    bool IsRunning() { return Sound.IsRunning(); }
    bool IsCallbackEntered() const { return Sound.IsCallbackEntered(); }

    double GetLevelForMeterdBLeft() { return SignalLevelMeter.GetLevelForMeterdBLeftOrMono(); }
    double GetLevelForMeterdBRight() { return SignalLevelMeter.GetLevelForMeterdBRight(); }

    bool IsConnected() { return Channel.IsConnected(); }

public:
    // settings
    CClientSettings& Settings;

    bool SetServerAddr ( QString strNAddr );
    void SetRemoteChanGain ( const int iId, const float fGain, const bool bIsMyOwnFader );
    void SetRemoteChanPan ( const int iId, const float fPan ) { Channel.SetRemoteChanPan ( iId, fPan ); }

public:
    bool GetAndResetbJitterBufferOKFlag();
    int  GetUploadRateKbps() { return Channel.GetUploadRateKbps(); }
    int  GetClientSockBufNumFrames()
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

    int GetSndCrdActualMonoBlSize()
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

    int GetSystemMonoBlSize() { return iMonoBlockSizeSam; }

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

    int EstimatedOverallDelay ( const int iPingTimeMs );

public:
    //### TODO: BEGIN ###//
    // These functions are still needed for classes with no direct access to Settings
    // See if there is a better solution
    EGUIDesign GetGUIDesign() const { return Settings.GetGUIDesign(); }

    int GetSockBufNumFrames()
    {
        Settings.SetClientSockBufNumFrames ( Channel.GetSockBufNumFrames() );
        return Settings.GetClientSockBufNumFrames();
    }
    //### TODO: END ###//

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

    bool             bSndCrdConversionBufferRequired;
    int              iSndCardMonoBlockSizeSamConvBuff;
    CBuffer<int16_t> SndCrdConversionBufferIn;
    CBuffer<int16_t> SndCrdConversionBufferOut;
    CVector<int16_t> vecDataConvBuf;
    CVector<int16_t> vecsStereoSndCrdMuteStream;
    CVector<int16_t> vecZeros;

    int iMonoBlockSizeSam;
    int iStereoBlockSizeSam;

    bool   bJitterBufferOK;
    bool   bEnableIPv6;
    bool   bMuteMeInPersonalMix;
    QMutex MutexDriverReinit;

    // for ping measurement
    QElapsedTimer PreciseTime;

    // for gain rate limiting
    QMutex MutexGain;
    QTimer TimerGain;
    int    minGainId;
    int    maxGainId;
    float  oldGain[MAX_NUM_CHANNELS];
    float  newGain[MAX_NUM_CHANNELS];
    int    iCurPingTime;

    CSignalHandler* pSignalHandler;

protected slots:
    void OnHandledSignal ( int sigNum );
    void OnSendProtMessage ( CVector<uint8_t> vecMessage );
    void OnInvalidPacketReceived ( CHostAddress RecHostAddr );

    void OnDetectedCLMessage ( CVector<uint8_t> vecbyMesBodyData, int iRecID, CHostAddress RecHostAddr );

    void OnReqJittBufSize() { CreateServerJitterBufferMessage(); }
    void OnServerJittBufSizeChanged ( int iNewJitBufSize );
    void OnNewConnection();
    void OnCLDisconnection ( CHostAddress InetAddr )
    {
        if ( InetAddr == Channel.GetAddress() )
        {
            emit Disconnected();
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

signals:
    void ConClientListMesReceived ( CVector<CChannelInfo> vecChanInfo );
    void ChatTextReceived ( QString strChatText );
    void ClientIDReceived ( int iChanID );
    void MuteStateHasChangedReceived ( int iChanID, bool bIsMuted );
    void LicenceRequired ( ELicenceType eLicenceType );
    void VersionAndOSReceived ( COSUtil::EOpSystemType eOSType, QString strVersion );
    void PingTimeReceived ( int iPingTime );
    void RecorderStateReceived ( ERecorderState eRecorderState );

    void CLServerListReceived ( CHostAddress InetAddr, CVector<CServerInfo> vecServerInfo );

    void CLRedServerListReceived ( CHostAddress InetAddr, CVector<CServerInfo> vecServerInfo );

    void CLConnClientsListMesReceived ( CHostAddress InetAddr, CVector<CChannelInfo> vecChanInfo );

    void CLPingTimeWithNumClientsReceived ( CHostAddress InetAddr, int iPingTime, int iNumClients );

    void CLVersionAndOSReceived ( CHostAddress InetAddr, COSUtil::EOpSystemType eOSType, QString strVersion );

    void CLChannelLevelListReceived ( CHostAddress InetAddr, CVector<uint16_t> vecLevelList );

    void Disconnected();
    void SoundDeviceChanged ( QString strError );
    void ControllerInFaderLevel ( int iChannelIdx, int iValue );
    void ControllerInPanValue ( int iChannelIdx, int iValue );
    void ControllerInFaderIsSolo ( int iChannelIdx, bool bIsSolo );
    void ControllerInFaderIsMute ( int iChannelIdx, bool bIsMute );
    void ControllerInMuteMyself ( bool bMute );
};
