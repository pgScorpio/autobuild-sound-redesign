/******************************************************************************\
 * Copyright (c) 2004-2022
 *
 * Author(s):
 *  Volker Fischer, revised and maintained by Peter Goderie (pgScorpio)
 *
 * Description:
 * Sound card interface for Windows operating systems
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

#include "sound.h"

//============================================================================
// Helpers for checkNewDeviceCapabilities
//============================================================================

// pointer to our sound object
CSound* pSound;

/******************************************************************************\
* Common                                                                       *
\******************************************************************************/
QString CSound::LoadAndInitializeDriver ( QString strDriverName, bool bOpenDriverSetup )
{
    // find and load driver
    int iDriverIdx = INVALID_INDEX; // initialize with an invalid index

    for ( int i = 0; i < MAX_NUMBER_SOUND_CARDS; i++ )
    {
        if ( strDriverName.compare ( cDriverNames[i] ) == 0 )
        {
            iDriverIdx = i;
        }
    }

    // if the selected driver was not found, return an error message
    if ( iDriverIdx == INVALID_INDEX )
    {
        return tr ( "The selected audio device is no longer present in the system. Please check your audio device." );
    }

    // Save number of channels from last driver
    // Need to save these (but not the driver name) as CheckDeviceCapabilities() overwrites them
    long lNumInChanPrev  = lNumInChan;
    long lNumOutChanPrev = lNumOutChan;

    loadAsioDriver ( cDriverNames[iDriverIdx] );

    // According to the docs, driverInfo.asioVersion and driverInfo.sysRef
    // should be set, but we haven't being doing that and it seems to work
    // okay...
    memset ( &driverInfo, 0, sizeof driverInfo );

    if ( ASIOInit ( &driverInfo ) != ASE_OK )
    {
        // clean up and return error string
        asioDrivers->removeCurrentDriver();
        return tr ( "Couldn't initialise the audio driver. Check if your audio hardware is plugged in and verify your driver settings." );
    }

    // check device capabilities if it fulfills our requirements
    const QString strStat = CheckDeviceCapabilities(); // also sets lNumInChan and lNumOutChan

    // check if device is capable
    if ( strStat.isEmpty() )
    {
        // Reset channel mapping if the sound card name has changed or the number of channels has changed
        if ( ( strCurDevName.compare ( strDriverNames[iDriverIdx] ) != 0 ) || ( lNumInChanPrev != lNumInChan ) || ( lNumOutChanPrev != lNumOutChan ) )
        {
            // In order to fix https://github.com/jamulussoftware/jamulus/issues/796
            // this code runs after a change in the ASIO driver (not when changing the ASIO input selection.)

            // mapping to the defaults (first two available channels)
            ResetChannelMapping();

            // store ID of selected driver if initialization was successful
            strCurDevName = cDriverNames[iDriverIdx];
        }
    }
    else
    {
        // if requested, open ASIO driver setup in case of an error
        if ( bOpenDriverSetup )
        {
            OpenDriverSetup();
            QMessageBox::question ( CMsgBoxes::MainForm(),
                                    CMsgBoxes::MainFormName(),
                                    "Are you done with your ASIO driver settings of " + GetDeviceName ( iDriverIdx ) + "?",
                                    QMessageBox::Yes );
        }

        // driver cannot be used, clean up
        asioDrivers->removeCurrentDriver();
    }

    return strStat;
}

void CSound::UnloadCurrentDriver()
{
    // clean up ASIO stuff
    if ( bRun )
    {
        Stop();
    }
    if ( bufferInfos[0].buffers[0] )
    {
        ASIODisposeBuffers();
        bufferInfos[0].buffers[0] = NULL;
    }
    ASIOExit();
    asioDrivers->removeCurrentDriver();
}

QString CSound::CheckDeviceCapabilities()
{
    // This function checks if our required input/output channel
    // properties are supported by the selected device. If the return
    // string is empty, the device can be used, otherwise the error
    // message is returned.

    // check the sample rate
    const ASIOError CanSaRateReturn = ASIOCanSampleRate ( SYSTEM_SAMPLE_RATE_HZ );

    if ( ( CanSaRateReturn == ASE_NoClock ) || ( CanSaRateReturn == ASE_NotPresent ) )
    {
        // return error string
        return QString ( tr ( "The selected audio device is incompatible "
                              "since it doesn't support a sample rate of %1 Hz. Please select another "
                              "device." ) )
            .arg ( SYSTEM_SAMPLE_RATE_HZ );
    }

    // check if sample rate can be set
    const ASIOError SetSaRateReturn = ASIOSetSampleRate ( SYSTEM_SAMPLE_RATE_HZ );

    if ( ( SetSaRateReturn == ASE_NoClock ) || ( SetSaRateReturn == ASE_InvalidMode ) || ( SetSaRateReturn == ASE_NotPresent ) )
    {
        // return error string
        return QString ( tr ( "The current audio device configuration is incompatible "
                              "because the sample rate couldn't be set to %2 Hz. Please check for a hardware switch or "
                              "driver setting to set the sample rate manually and restart %1." ) )
            .arg ( APP_NAME )
            .arg ( SYSTEM_SAMPLE_RATE_HZ );
    }

    // check the number of available channels
    ASIOGetChannels ( &lNumInChan, &lNumOutChan );

    if ( ( lNumInChan < NUM_IN_OUT_CHANNELS ) || ( lNumOutChan < NUM_IN_OUT_CHANNELS ) )
    {
        // return error string
        return QString ( tr ( "The selected audio device is incompatible since it doesn't support "
                              "%1 in/out channels. Please select another device or configuration." ) )
            .arg ( NUM_IN_OUT_CHANNELS );
    }

    // clip number of input/output channels to our maximum
    if ( lNumInChan > MAX_NUM_IN_OUT_CHANNELS )
    {
        lNumInChan = MAX_NUM_IN_OUT_CHANNELS;
    }
    if ( lNumOutChan > MAX_NUM_IN_OUT_CHANNELS )
    {
        lNumOutChan = MAX_NUM_IN_OUT_CHANNELS;
    }

    // query channel infos for all available input channels
    bool bInputChMixingSupported = true;

    for ( int i = 0; i < lNumInChan; i++ )
    {
        // setup for input channels
        channelInfosInput[i].isInput = ASIOTrue;
        channelInfosInput[i].channel = i;

        ASIOGetChannelInfo ( &channelInfosInput[i] );

        // Check supported sample formats.
        // Actually, it would be enough to have at least two channels which
        // support the required sample format. But since we have support for
        // all known sample types, the following check should always pass and
        // therefore we throw the error message on any channel which does not
        // fulfill the sample format requirement (quick hack solution).
        if ( !CheckSampleTypeSupported ( channelInfosInput[i].type ) )
        {
            // return error string
            return tr ( "The selected audio device is incompatible since "
                        "the required audio sample format isn't available. Please use another device." );
        }

        // store the name of the channel and check if channel mixing is supported
        channelInputName[i] = channelInfosInput[i].name;

        if ( !CheckSampleTypeSupportedForCHMixing ( channelInfosInput[i].type ) )
        {
            bInputChMixingSupported = false;
        }
    }

    // query channel infos for all available output channels
    for ( int i = 0; i < lNumOutChan; i++ )
    {
        // setup for output channels
        channelInfosOutput[i].isInput = ASIOFalse;
        channelInfosOutput[i].channel = i;

        ASIOGetChannelInfo ( &channelInfosOutput[i] );

        // Check supported sample formats.
        // Actually, it would be enough to have at least two channels which
        // support the required sample format. But since we have support for
        // all known sample types, the following check should always pass and
        // therefore we throw the error message on any channel which does not
        // fulfill the sample format requirement (quick hack solution).
        if ( !CheckSampleTypeSupported ( channelInfosOutput[i].type ) )
        {
            // return error string
            return tr ( "The selected audio device is incompatible since "
                        "the required audio sample format isn't available. Please use another device." );
        }
    }

    // special case with 4 input channels: support adding channels
    if ( ( lNumInChan == 4 ) && bInputChMixingSupported )
    {
        // add four mixed channels (i.e. 4 normal, 4 mixed channels)
        lNumInChanPlusAddChan = 8;

        for ( int iCh = 0; iCh < lNumInChanPlusAddChan; iCh++ )
        {
            int iSelCH, iSelAddCH;

            GetSelCHAndAddCH ( iCh, lNumInChan, iSelCH, iSelAddCH );

            if ( iSelAddCH >= 0 )
            {
                // for mixed channels, show both audio channel names to be mixed
                channelInputName[iCh] = channelInputName[iSelCH] + " + " + channelInputName[iSelAddCH];
            }
        }
    }
    else
    {
        // regular case: no mixing input channels used
        lNumInChanPlusAddChan = lNumInChan;
    }

    // everything is ok, return empty string for "no error" case
    return "";
}

void CSound::SetLeftInputChannel ( const int iNewChan )
{
    // apply parameter after input parameter check
    if ( ( iNewChan >= 0 ) && ( iNewChan < lNumInChanPlusAddChan ) )
    {
        vSelectedInputChannels[0] = iNewChan;
    }
}

void CSound::SetRightInputChannel ( const int iNewChan )
{
    // apply parameter after input parameter check
    if ( ( iNewChan >= 0 ) && ( iNewChan < lNumInChanPlusAddChan ) )
    {
        vSelectedInputChannels[1] = iNewChan;
    }
}

void CSound::SetLeftOutputChannel ( const int iNewChan )
{
    // apply parameter after input parameter check
    if ( ( iNewChan >= 0 ) && ( iNewChan < lNumOutChan ) )
    {
        vSelectedOutputChannels[0] = iNewChan;
    }
}

void CSound::SetRightOutputChannel ( const int iNewChan )
{
    // apply parameter after input parameter check
    if ( ( iNewChan >= 0 ) && ( iNewChan < lNumOutChan ) )
    {
        vSelectedOutputChannels[1] = iNewChan;
    }
}

int CSound::GetActualBufferSize ( const int iDesiredBufferSizeMono )
{
    int iActualBufferSizeMono;

    // query the usable buffer sizes
    ASIOGetBufferSize ( &HWBufferInfo.lMinSize, &HWBufferInfo.lMaxSize, &HWBufferInfo.lPreferredSize, &HWBufferInfo.lGranularity );

    // clang-format off
/*
// TEST
CMsgBoxes::ShowInfo( QString("lMinSize: %1, lMaxSize: %2, lPreferredSize: %3, lGranularity: %4").
                             arg(HWBufferInfo.lMinSize).arg(HWBufferInfo.lMaxSize).arg(HWBufferInfo.lPreferredSize).arg(HWBufferInfo.lGranularity) );
_exit(1);
*/
    // clang-format on

    // clang-format off
// TODO see https://github.com/EddieRingle/portaudio/blob/master/src/hostapi/asio/pa_asio.cpp#L1654 (SelectHostBufferSizeForUnspecifiedUserFramesPerBuffer)
    // clang-format on

    // calculate "nearest" buffer size and set internal parameter accordingly
    // first check minimum and maximum values
    if ( iDesiredBufferSizeMono <= HWBufferInfo.lMinSize )
    {
        iActualBufferSizeMono = HWBufferInfo.lMinSize;
    }
    else
    {
        if ( iDesiredBufferSizeMono >= HWBufferInfo.lMaxSize )
        {
            iActualBufferSizeMono = HWBufferInfo.lMaxSize;
        }
        else
        {
            // ASIO SDK 2.2: "Notes: When minimum and maximum buffer size are
            // equal, the preferred buffer size has to be the same value as
            // well; granularity should be 0 in this case."
            if ( HWBufferInfo.lMinSize == HWBufferInfo.lMaxSize )
            {
                iActualBufferSizeMono = HWBufferInfo.lMinSize;
            }
            else
            {
                if ( ( HWBufferInfo.lGranularity < -1 ) || ( HWBufferInfo.lGranularity == 0 ) )
                {
                    // Special case (seen for EMU audio cards): granularity is
                    // zero or less than zero (make sure to exclude the special
                    // case of -1).
                    // There is no definition of this case in the ASIO SDK
                    // document. We assume here that all buffer sizes in between
                    // minimum and maximum buffer sizes are allowed.
                    iActualBufferSizeMono = iDesiredBufferSizeMono;
                }
                else
                {
                    // General case --------------------------------------------
                    // initialization
                    int  iTrialBufSize     = HWBufferInfo.lMinSize;
                    int  iLastTrialBufSize = HWBufferInfo.lMinSize;
                    bool bSizeFound        = false;

                    // test loop
                    while ( ( iTrialBufSize <= HWBufferInfo.lMaxSize ) && ( !bSizeFound ) )
                    {
                        if ( iTrialBufSize >= iDesiredBufferSizeMono )
                        {
                            // test which buffer size fits better: the old one or the
                            // current one
                            if ( ( iTrialBufSize - iDesiredBufferSizeMono ) > ( iDesiredBufferSizeMono - iLastTrialBufSize ) )
                            {
                                iTrialBufSize = iLastTrialBufSize;
                            }

                            // exit while loop
                            bSizeFound = true;
                        }

                        if ( !bSizeFound )
                        {
                            // store old trial buffer size
                            iLastTrialBufSize = iTrialBufSize;

                            // increment trial buffer size (check for special
                            // case first)
                            if ( HWBufferInfo.lGranularity == -1 )
                            {
                                // special case: buffer sizes are a power of 2
                                iTrialBufSize *= 2;
                            }
                            else
                            {
                                iTrialBufSize += HWBufferInfo.lGranularity;
                            }
                        }
                    }

                    // clip trial buffer size (it may happen in the while
                    // routine that "iTrialBufSize" is larger than "lMaxSize" in
                    // case "lMaxSize - lMinSize" is not divisible by the
                    // granularity)
                    if ( iTrialBufSize > HWBufferInfo.lMaxSize )
                    {
                        iTrialBufSize = HWBufferInfo.lMaxSize;
                    }

                    // set ASIO buffer size
                    iActualBufferSizeMono = iTrialBufSize;
                }
            }
        }
    }

    return iActualBufferSizeMono;
}

int CSound::Init ( const int iNewPrefMonoBufferSize )
{
    ASIOMutex.lock(); // get mutex lock
    {
        // get the actual sound card buffer size which is supported
        // by the audio hardware
        iASIOBufferSizeMono = GetActualBufferSize ( iNewPrefMonoBufferSize );

        // init base class
        CSoundBase::Init ( iASIOBufferSizeMono );

        // set internal buffer size value and calculate stereo buffer size
        iASIOBufferSizeStereo = 2 * iASIOBufferSizeMono;

        // set the sample rate
        ASIOSetSampleRate ( SYSTEM_SAMPLE_RATE_HZ );

        // create memory for intermediate audio buffer
        vecsMultChanAudioSndCrd.Init ( iASIOBufferSizeStereo );

        // create and activate ASIO buffers (buffer size in samples),
        // dispose old buffers (if any)
        ASIODisposeBuffers();

        // prepare input channels
        for ( int i = 0; i < lNumInChan; i++ )
        {
            bufferInfos[i].isInput    = ASIOTrue;
            bufferInfos[i].channelNum = i;
            bufferInfos[i].buffers[0] = 0;
            bufferInfos[i].buffers[1] = 0;
        }

        // prepare output channels
        for ( int i = 0; i < lNumOutChan; i++ )
        {
            bufferInfos[lNumInChan + i].isInput    = ASIOFalse;
            bufferInfos[lNumInChan + i].channelNum = i;
            bufferInfos[lNumInChan + i].buffers[0] = 0;
            bufferInfos[lNumInChan + i].buffers[1] = 0;
        }

        ASIOCreateBuffers ( bufferInfos, lNumInChan + lNumOutChan, iASIOBufferSizeMono, &asioCallbacks );

        // query the latency of the driver
        long lInputLatency  = 0;
        long lOutputLatency = 0;

        if ( ASIOGetLatencies ( &lInputLatency, &lOutputLatency ) != ASE_NotPresent )
        {
            // add the input and output latencies (returned in number of
            // samples) and calculate the time in ms
            fInOutLatencyMs = ( static_cast<float> ( lInputLatency ) + lOutputLatency ) * 1000 / SYSTEM_SAMPLE_RATE_HZ;
        }
        else
        {
            // no latency available
            fInOutLatencyMs = 0.0f;
        }

        // check whether the driver requires the ASIOOutputReady() optimization
        // (can be used by the driver to reduce output latency by one block)
        bASIOPostOutput = ( ASIOOutputReady() == ASE_OK );
    }
    ASIOMutex.unlock();

    return iASIOBufferSizeMono;
}

void CSound::Start()
{
    // start audio
    ASIOStart();

    // call base class
    CSoundBase::Start();
}

void CSound::Stop()
{
    // stop audio
    ASIOStop();

    // call base class
    CSoundBase::Stop();

    // make sure the working thread is actually done
    // (by checking the locked state)
    if ( ASIOMutex.tryLock ( 5000 ) )
    {
        ASIOMutex.unlock();
    }
}

CSound::CSound ( void ( *fpNewCallback ) ( CVector<int16_t>& psData, void* arg ),
                 void*          arg,
                 const QString& strMIDISetup,
                 const bool,
                 const QString& ) :
    CSoundBase ( "ASIO", fpNewCallback, arg, strMIDISetup ),
    lNumInChan ( 0 ),
    lNumInChanPlusAddChan ( 0 ),
    lNumOutChan ( 0 ),
    fInOutLatencyMs ( 0.0f ), // "0.0" means that no latency value is available
    vSelectedInputChannels ( NUM_IN_OUT_CHANNELS ),
    vSelectedOutputChannels ( NUM_IN_OUT_CHANNELS )
{
    int i;

    // init pointer to our sound object
    pSound = this;

    // We assume NULL'd pointers in this structure indicate that buffers are not
    // allocated yet (see UnloadCurrentDriver).
    memset ( bufferInfos, 0, sizeof bufferInfos );

    // get available ASIO driver names in system
    for ( i = 0; i < MAX_NUMBER_SOUND_CARDS; i++ )
    {
        // allocate memory for driver names
        cDriverNames[i] = new char[32];
    }

    char cDummyName[] = "dummy";
    loadAsioDriver ( cDummyName ); // to initialize external object
    lNumDevs = asioDrivers->getDriverNames ( cDriverNames, MAX_NUMBER_SOUND_CARDS );

    // in case we do not have a driver available, throw error
    if ( lNumDevs == 0 )
    {
        throw CGenErr ( "<b>" + tr ( "No ASIO audio device driver found." ) + "</b><br><br>" +
                        QString ( tr ( "Please install an ASIO driver before running %1. "
                                       "If you own a device with ASIO support, install its official ASIO driver. "
                                       "If not, you'll need to install a universal driver like ASIO4ALL." ) )
                            .arg ( APP_NAME ) );
    }
    asioDrivers->removeCurrentDriver();

    // copy driver names to base class but internally we still have to use
    // the char* variable because of the ASIO API :-(
    for ( i = 0; i < lNumDevs; i++ )
    {
        strDriverNames[i] = cDriverNames[i];
    }

    // init device index as not initialized (invalid)
    strCurDevName = "";

    // init channel mapping
    ResetChannelMapping();

    // set up the asioCallback structure
    asioCallbacks.bufferSwitch         = &bufferSwitch;
    asioCallbacks.sampleRateDidChange  = &sampleRateChanged;
    asioCallbacks.asioMessage          = &asioMessages;
    asioCallbacks.bufferSwitchTimeInfo = &bufferSwitchTimeInfo;
}

void CSound::ResetChannelMapping()
{
    // init selected channel numbers with defaults: use first available
    // channels for input and output
    vSelectedInputChannels[0]  = 0;
    vSelectedInputChannels[1]  = 1;
    vSelectedOutputChannels[0] = 0;
    vSelectedOutputChannels[1] = 1;
}

// ASIO callbacks -------------------------------------------------------------
ASIOTime* CSound::bufferSwitchTimeInfo ( ASIOTime*, long index, ASIOBool processNow )
{
    bufferSwitch ( index, processNow );
    return 0L;
}

bool CSound::CheckSampleTypeSupported ( const ASIOSampleType SamType )
{
    // check for supported sample types
    return ( ( SamType == ASIOSTInt16LSB ) || ( SamType == ASIOSTInt24LSB ) || ( SamType == ASIOSTInt32LSB ) || ( SamType == ASIOSTFloat32LSB ) ||
             ( SamType == ASIOSTFloat64LSB ) || ( SamType == ASIOSTInt32LSB16 ) || ( SamType == ASIOSTInt32LSB18 ) ||
             ( SamType == ASIOSTInt32LSB20 ) || ( SamType == ASIOSTInt32LSB24 ) || ( SamType == ASIOSTInt16MSB ) || ( SamType == ASIOSTInt24MSB ) ||
             ( SamType == ASIOSTInt32MSB ) || ( SamType == ASIOSTFloat32MSB ) || ( SamType == ASIOSTFloat64MSB ) || ( SamType == ASIOSTInt32MSB16 ) ||
             ( SamType == ASIOSTInt32MSB18 ) || ( SamType == ASIOSTInt32MSB20 ) || ( SamType == ASIOSTInt32MSB24 ) );
}

//============================================================================
// ASIO callback statics
//============================================================================

CSound*       CSound::pSound = NULL;
ASIOCallbacks CSound::asioCallbacks; // Pointers to the asio driver callbacks    (ASIO driver -> Application)

//============================================================================
// CSound:
//============================================================================

CSound::CSound ( void ( *fpProcessCallback ) ( CVector<int16_t>& psData, void* arg ), void* pProcessCallBackParg ) :
    CSoundBase ( "ASIO", fpProcessCallback, pProcessCallBackParg ),
    asioDriversLoaded ( false )
{
    setObjectName ( "CSoundThread" );

    asioDriverData.clear();

    // set up the asioCallback structure
    asioCallbacks.bufferSwitch         = &_onBufferSwitch;
    asioCallbacks.sampleRateDidChange  = &_onSampleRateChanged;
    asioCallbacks.asioMessage          = &_onAsioMessages;
    asioCallbacks.bufferSwitchTimeInfo = &_onBufferSwitchTimeInfo;

    // We assume NULL'd pointers in this structure indicate that buffers are not
    // allocated yet (see UnloadCurrentDriver).
    memset ( bufferInfos, 0, sizeof bufferInfos );

    // Set my properties in CSoundbase:
    soundProperties.bHasSetupDialog = true;
    // Reset default texts according new soundProperties
    soundProperties.setDefaultTexts();
    // Set specific texts according this CSound implementation
    soundProperties.strSetupButtonText    = tr ( "ASIO Device Settings" );
    soundProperties.strSetupButtonToolTip = tr ( "Opens the driver settings when available..." ) + "<br>" +
                                            tr ( "Note: %1 currently only supports devices with a sample rate of %2 Hz. "
                                                 "You may need to re-select the driver before any changed settings will take effect." )
                                                .arg ( APP_NAME )
                                                .arg ( SYSTEM_SAMPLE_RATE_HZ ) +
                                            htmlNewLine() + tr ( "For more help see jamulus.io." );
    soundProperties.strSetupButtonAccessibleName = CSoundBase::tr ( "ASIO Device Settings push button" );

    soundProperties.strAudioDeviceWhatsThis = ( "<b>" + tr ( "Audio Device" ) + ":</b> " +
                                                tr ( "Under the Windows operating system the ASIO driver (sound card) can be "
                                                     "selected using %1. If the selected driver is not valid an error "
                                                     "message will be shown. "
                                                     "Under macOS the input and output hardware can be selected." )
                                                    .arg ( APP_NAME ) +
                                                "<br>" +
                                                tr ( "If the driver is selected during an active connection, the connection "
                                                     "is stopped, the driver is changed and the connection is started again "
                                                     "automatically." ) );

    soundProperties.strAudioDeviceToolTip = tr ( "If the ASIO4ALL driver is used, "
                                                 "please note that this driver usually introduces approx. 10-30 ms of "
                                                 "additional audio delay. Using a sound card with a native ASIO driver "
                                                 "is therefore recommended." ) +
                                            htmlNewLine() +
                                            tr ( "If you are using the kX ASIO "
                                                 "driver, make sure to connect the ASIO inputs in the kX DSP settings "
                                                 "panel." );

    // init pointer to our sound object for the asio callback functions
    pSound = this;
}

//============================================================================
// ASIO callback implementations
//============================================================================

void CSound::onBufferSwitch ( long index, ASIOBool /*processNow*/ )
{
    // pgSorpio: Impoved readability by adding the intermediate pointers pAsioSelInput, pAsioAddInput and pAsioSelOutput.
    //           And so its immediately clear mixing can be supported for ANY supported sample type!
    //           (Just adding another int16_t using pAsioAddInput instead of pAsioSelInput!)
    //
    //           Also added input muting here
    //
    // perform the processing for input and output
    QMutexLocker locker ( &mutexAudioProcessCallback ); // get mutex lock

    // CAPTURE -------------------------------------------------------------
    for ( unsigned int i = 0; i < PROT_NUM_IN_CHANNELS; i++ )
    {
        int iSelCH, iSelAddCH;
        getInputSelAndAddChannels ( selectedInputChannels[i], lNumInChan, lNumAddedInChan, iSelCH, iSelAddCH );

        int iInputGain = inputChannelsGain[i];

        void* pAsioSelInput = bufferInfos[iSelCH].buffers[index];
        void* pAsioAddInput = ( iSelAddCH >= 0 ) ? bufferInfos[iSelAddCH].buffers[index] : NULL;

        // copy new captured block in thread transfer buffer (copy
        // mono data interleaved in stereo buffer)
        switch ( asioDriver.inputChannelInfo ( iSelCH ).type )
        {
        case ASIOSTInt16LSB:
        {
            // no type conversion required, just copy operation
            int16_t* pASIOBuf = static_cast<int16_t*> ( pAsioSelInput );

            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] = pASIOBuf[iCurSample] * iInputGain;
            }

            if ( pAsioAddInput )
            {
                // mix input channels case:
                int16_t* pASIOBufAdd = static_cast<int16_t*> ( pAsioAddInput );

                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] += pASIOBufAdd[iCurSample] * iInputGain;
                }
            }
            break;
        }

        case ASIOSTInt24LSB:
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                int iCurSam = 0;
                memcpy ( &iCurSam, ( (char*) pAsioSelInput ) + iCurSample * 3, 3 );
                iCurSam >>= 8;

                audioBuffer[2 * iCurSample + i] = static_cast<int16_t> ( iCurSam ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                // mix input channels case:
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    int iCurSam = 0;
                    memcpy ( &iCurSam, ( (char*) pAsioAddInput ) + iCurSample * 3, 3 );
                    iCurSam >>= 8;

                    audioBuffer[2 * iCurSample + i] += iCurSam * iInputGain;
                    ;
                }
            }
            break;

        case ASIOSTInt32LSB:
        {
            int32_t* pASIOBuf = static_cast<int32_t*> ( pAsioSelInput );

            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] = static_cast<int16_t> ( pASIOBuf[iCurSample] >> 16 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                // mix input channels case:
                int32_t* pASIOBuf = static_cast<int32_t*> ( pAsioSelInput );

                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] += static_cast<int16_t> ( pASIOBuf[iCurSample] >> 16 ) * iInputGain;
                }
            }
            break;
        }

        case ASIOSTFloat32LSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] = static_cast<int16_t> ( static_cast<float*> ( pAsioSelInput )[iCurSample] * _MAXSHORT ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( static_cast<float*> ( pAsioAddInput )[iCurSample] * _MAXSHORT ) * iInputGain;
                }
            }
            break;

        case ASIOSTFloat64LSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] +=
                    static_cast<int16_t> ( static_cast<float*> ( pAsioSelInput )[iCurSample] * _MAXSHORT ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( static_cast<float*> ( pAsioAddInput )[iCurSample] * _MAXSHORT ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32LSB16: // 32 bit data with 16 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] = static_cast<int16_t> ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] & 0xFFFF ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] & 0xFFFF ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32LSB18: // 32 bit data with 18 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] & 0x3FFFF ) >> 2 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] & 0x3FFFF ) >> 2 ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32LSB20: // 32 bit data with 20 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] & 0xFFFFF ) >> 4 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] & 0xFFFFF ) >> 4 ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32LSB24: // 32 bit data with 24 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] & 0xFFFFFF ) >> 8 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] & 0xFFFFFF ) >> 8 ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt16MSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            // flip bits
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] = Flip16Bits ( ( static_cast<int16_t*> ( pAsioSelInput ) )[iCurSample] ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] += Flip16Bits ( ( static_cast<int16_t*> ( pAsioAddInput ) )[iCurSample] ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt24MSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // because the bits are flipped, we do not have to perform the
                // shift by 8 bits
                int iCurSam = 0;
                memcpy ( &iCurSam, ( (char*) pAsioSelInput ) + iCurSample * 3, 3 );

                audioBuffer[2 * iCurSample + i] = Flip16Bits ( static_cast<int16_t> ( iCurSam ) ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    // because the bits are flipped, we do not have to perform the
                    // shift by 8 bits
                    int iCurSam = 0;
                    memcpy ( &iCurSam, ( (char*) pAsioAddInput ) + iCurSample * 3, 3 );

                    audioBuffer[2 * iCurSample + i] += Flip16Bits ( static_cast<int16_t> ( iCurSam ) ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32MSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // flip bits and convert to 16 bit
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( Flip32Bits ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] ) >> 16 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    // flip bits and convert to 16 bit
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( Flip32Bits ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] ) >> 16 ) * iInputGain;
                }
            }
            break;

        case ASIOSTFloat32MSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( static_cast<float> ( Flip32Bits ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] ) ) * _MAXSHORT ) *
                    iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( static_cast<float> ( Flip32Bits ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] ) ) *
                                               _MAXSHORT ) *
                        iInputGain;
                }
            }
            break;

        case ASIOSTFloat64MSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( static_cast<double> ( Flip64Bits ( static_cast<int64_t*> ( pAsioSelInput )[iCurSample] ) ) * _MAXSHORT ) *
                    iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( static_cast<double> ( Flip64Bits ( static_cast<int64_t*> ( pAsioAddInput )[iCurSample] ) ) *
                                               _MAXSHORT ) *
                        iInputGain;
                }
            }
            break;

        case ASIOSTInt32MSB16: // 32 bit data with 16 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( Flip32Bits ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] ) & 0xFFFF ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( Flip32Bits ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] ) & 0xFFFF ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32MSB18: // 32 bit data with 18 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( ( Flip32Bits ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] ) & 0x3FFFF ) >> 2 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( ( Flip32Bits ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] ) & 0x3FFFF ) >> 2 ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32MSB20: // 32 bit data with 20 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( ( Flip32Bits ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] ) & 0xFFFFF ) >> 4 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( ( Flip32Bits ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] ) & 0xFFFFF ) >> 4 ) * iInputGain;
                }
            }
            break;

        case ASIOSTInt32MSB24: // 32 bit data with 24 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                audioBuffer[2 * iCurSample + i] =
                    static_cast<int16_t> ( ( Flip32Bits ( static_cast<int32_t*> ( pAsioSelInput )[iCurSample] ) & 0xFFFFFF ) >> 8 ) * iInputGain;
            }

            if ( pAsioAddInput )
            {
                for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
                {
                    audioBuffer[2 * iCurSample + i] +=
                        static_cast<int16_t> ( ( Flip32Bits ( static_cast<int32_t*> ( pAsioAddInput )[iCurSample] ) & 0xFFFFFF ) >> 8 ) * iInputGain;
                }
            }
            break;
        }
    }

    // call processing callback function
    processCallback ( audioBuffer );

    // PLAYBACK ------------------------------------------------------------
    for ( unsigned int i = 0; i < PROT_NUM_OUT_CHANNELS; i++ )
    {
        const int iSelCH         = lNumInChan + selectedOutputChannels[i];
        void*     pAsioSelOutput = bufferInfos[iSelCH].buffers[index];

        // copy data from sound card in output buffer (copy
        // interleaved stereo data in mono sound card buffer)
        switch ( asioDriver.outputChannelInfo ( selectedOutputChannels[i] ).type )
        {
        case ASIOSTInt16LSB:
        {
            // no type conversion required, just copy operation
            int16_t* pASIOBuf = static_cast<int16_t*> ( pAsioSelOutput );

            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                pASIOBuf[iCurSample] = audioBuffer[iCurSample * 2 + i];
            }
            break;
        }

        case ASIOSTInt24LSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert current sample in 24 bit format
                int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                iCurSam <<= 8;

                memcpy ( ( (char*) pAsioSelOutput ) + iCurSample * 3, &iCurSam, 3 );
            }
            break;

        case ASIOSTInt32LSB:
        {
            int32_t* pASIOBuf = static_cast<int32_t*> ( pAsioSelOutput );

            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                pASIOBuf[iCurSample] = ( iCurSam << 16 );
            }
            break;
        }

        case ASIOSTFloat32LSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                const float fCurSam = static_cast<float> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<float*> ( pAsioSelOutput )[iCurSample] = fCurSam / _MAXSHORT;
            }
            break;

        case ASIOSTFloat64LSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                const double fCurSam = static_cast<double> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<double*> ( pAsioSelOutput )[iCurSample] = fCurSam / _MAXSHORT;
            }
            break;

        case ASIOSTInt32LSB16: // 32 bit data with 16 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = iCurSam;
            }
            break;

        case ASIOSTInt32LSB18: // 32 bit data with 18 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = ( iCurSam << 2 );
            }
            break;

        case ASIOSTInt32LSB20: // 32 bit data with 20 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = ( iCurSam << 4 );
            }
            break;

        case ASIOSTInt32LSB24: // 32 bit data with 24 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = ( iCurSam << 8 );
            }
            break;

        case ASIOSTInt16MSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            // flip bits
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                ( (int16_t*) pAsioSelOutput )[iCurSample] = Flip16Bits ( audioBuffer[iCurSample * 2 + i] );
            }
            break;

        case ASIOSTInt24MSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // because the bits are flipped, we do not have to perform the
                // shift by 8 bits
                int32_t iCurSam = static_cast<int32_t> ( Flip16Bits ( audioBuffer[iCurSample * 2 + i] ) );

                memcpy ( ( (char*) pAsioSelOutput ) + iCurSample * 3, &iCurSam, 3 );
            }
            break;

        case ASIOSTInt32MSB:
            // clang-format off
// NOT YET TESTED
            // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit and flip bits
                int iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = Flip32Bits ( iCurSam << 16 );
            }
            break;

        case ASIOSTFloat32MSB: // IEEE 754 32 bit float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                const float fCurSam = static_cast<float> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<float*> ( pAsioSelOutput )[iCurSample] =
                    static_cast<float> ( Flip32Bits ( static_cast<int32_t> ( fCurSam / _MAXSHORT ) ) );
            }
            break;

        case ASIOSTFloat64MSB: // IEEE 754 64 bit double float, as found on Intel x86 architecture
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                const double fCurSam = static_cast<double> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<float*> ( pAsioSelOutput )[iCurSample] =
                    static_cast<double> ( Flip64Bits ( static_cast<int64_t> ( fCurSam / _MAXSHORT ) ) );
            }
            break;

        case ASIOSTInt32MSB16: // 32 bit data with 16 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = Flip32Bits ( iCurSam );
            }
            break;

        case ASIOSTInt32MSB18: // 32 bit data with 18 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = Flip32Bits ( iCurSam << 2 );
            }
            break;

        case ASIOSTInt32MSB20: // 32 bit data with 20 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = Flip32Bits ( iCurSam << 4 );
            }
            break;

        case ASIOSTInt32MSB24: // 32 bit data with 24 bit alignment
                               // clang-format off
// NOT YET TESTED
                               // clang-format on
            for ( unsigned int iCurSample = 0; iCurSample < iDeviceBufferSize; iCurSample++ )
            {
                // convert to 32 bit
                const int32_t iCurSam = static_cast<int32_t> ( audioBuffer[iCurSample * 2 + i] );

                static_cast<int32_t*> ( pAsioSelOutput )[iCurSample] = Flip32Bits ( iCurSam << 8 );
            }
            break;
        }
    }

    // Finally if the driver supports the ASIOOutputReady() optimization,
    // do it here, all data are in place now
    if ( bASIOPostOutput )
    {
        asioDriver.outputReady();
    }
}

ASIOTime* CSound::onBufferSwitchTimeInfo ( ASIOTime*, long index, ASIOBool processNow )
{
    onBufferSwitch ( index, processNow );
    return 0L;
}

void CSound::onSampleRateChanged ( ASIOSampleRate sampleRate )
{
    if ( sampleRate != SYSTEM_SAMPLE_RATE_HZ )
    {
        Stop();
        // pgScorpio: implement error ???
        //            how to notify CClient ??
    }
}

long CSound::onAsioMessages ( long selector, long, void*, double* )
{
    long ret = 0;

    switch ( selector )
    {
    case kAsioEngineVersion:
        // return the supported ASIO version of the host application
        ret = 2L; // Host ASIO implementation version, 2 or higher
        break;

    // both messages might be send if the buffer size changes
    case kAsioBufferSizeChange:
        emit reinitRequest ( tSndCrdResetType::RS_ONLY_RESTART_AND_INIT );
        ret = 1L; // 1L if request is accepted or 0 otherwise
        break;

    case kAsioResetRequest:
        emit reinitRequest ( tSndCrdResetType::RS_RELOAD_RESTART_AND_INIT );
        ret = 1L; // 1L if request is accepted or 0 otherwise
        break;
    }

    return ret;
}

//============================================================================
// CSound Internal
//============================================================================

//====================================
// ASIO Stuff
//====================================

void CSound::closeAllAsioDrivers()
{
    newAsioDriver.Close();
    closeCurrentDevice(); // Closes asioDriver and clears DeviceInfo

    // Just make sure all other driver in the list are closed too...
    for ( long i = 0; i < lNumDevices; i++ )
    {
        CloseAsioDriver ( &asioDriverData[i] );
    }
}

bool CSound::prepareAsio ( bool bStartAsio )
{
    bool ok = true;

    // dispose old buffers (if any)
    asioDriver.disposeBuffers(); // implies ASIOStop() !

    // create memory for intermediate audio buffer
    audioBuffer.Init ( PROT_NUM_IN_CHANNELS * iDeviceBufferSize );

    long numInputChannels  = asioDriver.numInputChannels();
    long numOutputChannels = asioDriver.numOutputChannels();

    long bufferIndex = 0;

    // prepare input channels
    for ( int i = 0; i < numInputChannels; i++ )
    {
        bufferInfos[bufferIndex].isInput    = ASIO_INPUT;
        bufferInfos[bufferIndex].channelNum = i;
        bufferInfos[bufferIndex].buffers[0] = 0;
        bufferInfos[bufferIndex].buffers[1] = 0;
        bufferIndex++;
    }

    // prepare output channels
    for ( int i = 0; i < numOutputChannels; i++ )
    {
        bufferInfos[bufferIndex].isInput    = ASIO_OUTPUT;
        bufferInfos[bufferIndex].channelNum = i;
        bufferInfos[bufferIndex].buffers[0] = 0;
        bufferInfos[bufferIndex].buffers[1] = 0;
        bufferIndex++;
    }

    // create and activate ASIO buffers (buffer size in samples),
    if ( !asioDriver.createBuffers ( bufferInfos, bufferIndex, iDeviceBufferSize, &asioCallbacks ) )
    {
        ok = false;
    }

    // check whether the driver requires the ASIOOutputReady() optimization
    // (can be used by the driver to reduce output latency by one block)
    bASIOPostOutput = ( asioDriver.outputReady() == ASE_OK );

    // set the sample rate (make sure we get the correct latency)
    if ( !asioDriver.setSampleRate ( SYSTEM_SAMPLE_RATE_HZ ) )
    {
        ok = false;
    }

    // Query the latency of the driver
    long lInputLatency  = 0;
    long lOutputLatency = 0;
    // Latency is returned in number of samples
    if ( !asioDriver.getLatencies ( &lInputLatency, &lOutputLatency ) )
    {
        // No latency available
        // Assume just the buffer delay
        lInputLatency = lOutputLatency = iDeviceBufferSize;
    }

    // Calculate total latency in ms
    {
        float fLatency;

        fLatency = lInputLatency;
        fLatency += lOutputLatency;
        fLatency *= 1000;
        fLatency /= SYSTEM_SAMPLE_RATE_HZ;

        fInOutLatencyMs = fLatency;
    }

    if ( ok && bStartAsio )
    {
        return asioDriver.start();
    }

    return ok;
}

unsigned int CSound::getDeviceBufferSize ( unsigned int iDesiredBufferSize )
{
    int iActualBufferSize = iDesiredBufferSize;

    // Limits to be obtained from the asio driver.
    // Init all to zero, just in case the asioDriver.getBufferSize call fails.
    long lMinSize       = 0;
    long lMaxSize       = 0;
    long lPreferredSize = 0;
    long lGranularity   = 0;

    // query the asio driver
    asioDriver.getBufferSize ( &lMinSize, &lMaxSize, &lPreferredSize, &lGranularity );

    // calculate "nearest" buffer size within limits
    //
    // first check minimum and maximum limits
    if ( iActualBufferSize < lMinSize )
    {
        iActualBufferSize = lMinSize;
    }

    if ( iActualBufferSize > lMaxSize )
    {
        iActualBufferSize = lMaxSize;
    }

    if ( iActualBufferSize == lPreferredSize )
    {
        // No need to check any further
        return iActualBufferSize;
    }

    if ( lMaxSize > lMinSize ) // Do we have any choice ??
    {
        if ( lGranularity == -1 )
        {
            // Size must be a power of 2
            // So lMinSize and lMaxSize are expected be powers of 2 too!
            // Start with lMinSize...
            long lTrialBufSize = lMinSize;
            do
            {
                // And double this size...
                lTrialBufSize *= 2;
                // Until we get above the maximum size or above the current actual (requested) size...
            } while ( ( lTrialBufSize < lMaxSize ) && ( lTrialBufSize < iActualBufferSize ) );

            // Now take the previous power of two...
            iActualBufferSize = ( lTrialBufSize / 2 );
        }
        else if ( lGranularity > 1 )
        {
            // Size must be a multiple of lGranularity,
            // Round to a lGranularity multiple by determining any remainder
            long remainder = ( iActualBufferSize % lGranularity );
            if ( remainder )
            {
                // Round down by subtracting the remainder...
                iActualBufferSize -= remainder;

                // Use rounded down size if this is actually the preferred buffer size
                if ( iActualBufferSize == lPreferredSize )
                {
                    return iActualBufferSize;
                }

                // Otherwise see if we should, and could, have rounded up...
                if ( ( remainder >= ( lGranularity / 2 ) ) &&             // Should we have rounded up ?
                     ( iActualBufferSize <= ( lMaxSize - lGranularity ) ) // And can we round up ?
                )
                {
                    // Change rounded down size to rounded up size by adding 1 granularity...
                    iActualBufferSize += lGranularity;
                }
            }
        }
        // There are no definitions for other values of granularity in the ASIO SDK !
        // Assume everything else should never occur and is OK as long as the size is within min/max limits.
    }

    return iActualBufferSize;
}

bool CSound::checkNewDeviceCapabilities()
{
    // This function checks if our required input/output channel
    // properties are supported by the selected device. If the return
    // string is empty, the device can be used, otherwise the error
    // message is returned.
    bool ok = true;

    if ( !newAsioDriver.IsOpen() )
    {
        strErrorList.append ( QString ( "Coding Error: Calling CheckDeviceCapabilities() with no newAsioDriver open! " ) );
        return false;
    }

    // check the sample rate
    if ( !newAsioDriver.canSampleRate ( SYSTEM_SAMPLE_RATE_HZ ) )
    {
        ok = false;
        // return error string
        strErrorList.append ( tr ( "The selected audio device does not support a sample rate of %1 Hz. " ).arg ( SYSTEM_SAMPLE_RATE_HZ ) );
    }
    else
    {
        // check if sample rate can be set
        if ( !newAsioDriver.setSampleRate ( SYSTEM_SAMPLE_RATE_HZ ) )
        {
            ok = false;
            strErrorList.append ( tr ( "The audio devices sample rate could not be set to %2 Hz. " ).arg ( SYSTEM_SAMPLE_RATE_HZ ) );
        }
    }

    if ( newAsioDriver.numInputChannels() < DRV_MIN_IN_CHANNELS )
    {
        ok = false;
        strErrorList.append ( tr ( "The selected audio device doesn not support at least"
                                   "%1 %2 channel(s)." )
                                  .arg ( DRV_MIN_IN_CHANNELS )
                                  .arg ( tr ( "input" ) ) );
    }

    if ( newAsioDriver.numOutputChannels() < DRV_MIN_OUT_CHANNELS )
    {
        ok = false;
        strErrorList.append ( tr ( "The selected audio device doesn not support at least"
                                   "%1 %2 channel(s)." )
                                  .arg ( DRV_MIN_OUT_CHANNELS )
                                  .arg ( tr ( "output" ) ) );
    }

    // query channel infos for all available input channels
    bool channelOk = true;
    for ( int i = 0; i < newAsioDriver.numInputChannels(); i++ )
    {
        const ASIOChannelInfo& channel = newAsioDriver.inputChannelInfo ( i );

        if ( !SampleTypeSupported ( channel.type ) )
        {
            // return error string
            channelOk = false;
        }
    }

    if ( !channelOk )
    {
        ok = false;
        strErrorList.append ( tr ( "The selected audio device is incompatible since "
                                   "the required %1 audio sample format isn't available." )
                                  .arg ( tr ( "input)" ) ) );
    }

    // query channel infos for all available output channels
    channelOk = true;
    for ( int i = 0; i < newAsioDriver.numOutputChannels(); i++ )
    {
        const ASIOChannelInfo& channel = newAsioDriver.outputChannelInfo ( i );

        // Check supported sample formats.
        // Actually, it would be enough to have at least two channels which
        // support the required sample format. But since we have support for
        // all known sample types, the following check should always pass and
        // therefore we throw the error message on any channel which does not
        // fulfill the sample format requirement (quick hack solution).
        if ( !SampleTypeSupported ( channel.type ) )
        {
            channelOk = false;
        }
    }

    if ( !channelOk )
    {
        ok = false;
        strErrorList.append ( tr ( "The selected audio device is incompatible since "
                                   "the required %1 audio sample format isn't available." )
                                  .arg ( tr ( "output)" ) ) );
    }

    // special case with 4 input channels: support adding channels
    // pgScorpio: What the hack is this for ??? Only used for Jack?? But mixing can be done in Jack! KISS!
    if ( ( newAsioDriver.numInputChannels() > 2 ) )
    {
        // Total name size for AddedChannelData
        const long addednamesize = ( sizeof ( ASIOAddedChannelInfo::ChannelData.name ) + sizeof ( ASIOAddedChannelInfo::AddedName ) );

        // add mixed channels (i.e. 4 normal, 4 mixed channels)
        const long            numInputChan = newAsioDriver.numInputChannels();
        const long            numAddedChan = getNumInputChannelsToAdd ( numInputChan );
        ASIOAddedChannelInfo* addedChannel = newAsioDriver.setNumAddedInputChannels ( numAddedChan );

        for ( long i = 0; i < numAddedChan; i++ )
        {
            int iSelChan, iAddChan;

            getInputSelAndAddChannels ( numInputChan + i, numInputChan, numAddedChan, iSelChan, iAddChan );

            char*       combinedName = addedChannel[i].ChannelData.name;
            const char* firstName    = newAsioDriver.inputChannelInfo ( iSelChan ).name;
            const char* secondName   = newAsioDriver.inputChannelInfo ( iAddChan ).name;

            if ( ( iSelChan >= 0 ) && ( iAddChan >= 0 ) )
            {
                strncpy_s ( combinedName, addednamesize, firstName, sizeof ( ASIOChannelInfo::name ) );

                strcat_s ( combinedName, addednamesize, " + " );

                strncat_s ( combinedName, addednamesize, secondName, sizeof ( ASIOChannelInfo::name ) );
            }
        }
    }

    newAsioDriver.openData.capabilitiesOk = ok;

    return ok;
}

//============================================================================
// CSoundBase internal
//============================================================================

long CSound::createDeviceList ( bool bRescan )
{

    if ( bRescan && asioDriversLoaded )
    {
        closeAllAsioDrivers();
        asioDriversLoaded = false;
    }

    if ( !asioDriversLoaded )
    {
        strDeviceNames.clear();
        lNumDevices = GetAsioDriverDataList ( asioDriverData );
        for ( long i = 0; i < lNumDevices; i++ )
        {
            strDeviceNames.append ( asioDriverData[i].drvname );
        }

        asioDriversLoaded = ( lNumDevices != 0 );

        return lNumDevices;
    }

    return lNumDevices;
}

bool CSound::checkDeviceChange ( tDeviceChangeCheck mode, int iDriverIndex )
{
    if ( ( iDriverIndex < 0 ) || ( iDriverIndex >= lNumDevices ) )
    {
        // Invalid index !
        return false;
    }

    if ( ( mode != CSoundBase::tDeviceChangeCheck::CheckOpen ) && ( !newAsioDriver.IsOpen() ) )
    {
        // All other modes are only valid if a newDriver is successfully opened first.
        return false;
    }

    switch ( mode )
    {
    case tDeviceChangeCheck::Abort: // Not changing to newDRiver !
        return newAsioDriver.Close();

    case tDeviceChangeCheck::CheckOpen: // Open the device we are gona check
        newAsioDriver.AssignFrom ( &asioDriverData[iDriverIndex] );
        return newAsioDriver.Open();

    case tDeviceChangeCheck::Activate: // Actually changing current device !
        if ( asioDriver.AssignFrom ( &newAsioDriver ) )
        {
            clearDeviceInfo();

            lNumInChan      = asioDriver.openData.lNumInChan;
            lNumAddedInChan = asioDriver.openData.lNumAddedInChan;

            lNumOutChan = asioDriver.openData.lNumOutChan;

            asioDriver.GetInputChannelNames ( strInputChannelNames );
            asioDriver.GetOutputChannelNames ( strOutputChannelNames );

            iCurrentDevice = asioDriver.index;
            resetChannelMapping();
            // pgScorpio TODO: read ChannelMapping from ini settings !

            return true;
        }
        return false;

    case tDeviceChangeCheck::CheckCapabilities:
        return checkNewDeviceCapabilities();

    default:
        return false;
    }
}

void CSound::closeCurrentDevice()
{
    if ( IsStarted() )
    {
        Stop();
    }

    asioDriver.disposeBuffers();
    asioDriver.Close();

    clearDeviceInfo();
    memset ( bufferInfos, 0, sizeof ( bufferInfos ) );
}

//============================================================================
// CSoundBase Interface to CClient:
//============================================================================

bool CSound::start()
{
    if ( !IsStarted() )
    {
        // prepare start and try to start asio
        if ( prepareAsio ( true ) )
        {
            strErrorList.clear();

            return true;
        }
        else
        {
            // We failed!, asio is stopped !
            // notify base class
            strErrorList.clear();
            strErrorList.append ( htmlBold ( tr ( "Failed to start your audio device!." ) ) );
            strErrorList.append ( tr ( "Please check your device settings..." ) );

            return false;
        }
    }

    return IsStarted();
}

bool CSound::stop()
{
    if ( IsStarted() )
    {
        // stop audio
        asioDriver.stop();

        return true;
    }

    return !IsStarted();
}
