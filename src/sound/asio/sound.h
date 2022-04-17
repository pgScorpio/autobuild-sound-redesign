/******************************************************************************\
 * Copyright (c) 2004-2022
 *
 * Author(s):
 *  Volker Fischer, revised and maintained by Peter Goderie (pgScorpio)
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

#include <QMutex>
#include <QMessageBox>
#include "../../util.h"
#include "../../global.h"
#include "../soundbase.h"

// The following includes require the ASIO SDK to be placed in
// libs/ASIOSDK2 during build.
// Important:
// - Do not copy parts of ASIO SDK into the Jamulus source tree without
//   further consideration as it would make the license situation more
//   complicated.
// - When building yourself, read and understand the
//   Steinberg ASIO SDK Licensing Agreement and verify whether you might be
//   obliged to sign it as well, especially when considering distribution
//   of Jamulus Windows binaries with ASIO support.
#include "asiosys.h"
#include "asiodriver.h"

//============================================================================
// CSound
//============================================================================

class CSound : public CSoundBase
{
    Q_OBJECT

public:
    CSound ( void ( *theProcessCallback ) ( CVector<int16_t>& psData, void* arg ), void* theProcessCallbackArg );

#ifdef OLD_SOUND_COMPATIBILITY
    // Backwards compatibility constructor
    CSound ( void ( *theProcessCallback ) ( CVector<int16_t>& psData, void* pCallbackArg ),
             void*   theProcessCallbackArg,
             QString strMIDISetup,
             bool    bNoAutoJackConnect,
             QString strNClientName );
#endif

    virtual ~CSound()
    {
        closeCurrentDevice();
        closeAllAsioDrivers();
    }

protected:
    // ASIO data
    bool bASIOPostOutput;

    bool              asioDriversLoaded;
    tVAsioDrvDataList asioDriverData;

    CAsioDriver asioDriver;    // The current selected asio driver
    CAsioDriver newAsioDriver; // the new asio driver opened by checkDeviceChange(CheckOpen, ...),
                               // to be checked by checkDeviceChange(CheckCapabilities, ...)
                               // and set as asioDriver by checkDeviceChange(Activate, ...) or closed by checkDeviceChange(Abort, ...)

    ASIOBufferInfo bufferInfos[DRV_MAX_IN_CHANNELS + DRV_MAX_OUT_CHANNELS]; // TODO: Use CVector<ASIOBufferInfo> ?

protected:
    // ASIO callback implementations
    void      onBufferSwitch ( long index, ASIOBool processNow );
    ASIOTime* onBufferSwitchTimeInfo ( ASIOTime* timeInfo, long index, ASIOBool processNow );
    void      onSampleRateChanged ( ASIOSampleRate sampleRate );
    long      onAsioMessages ( long selector, long value, void* message, double* opt );

protected:
    // callbacks
    static inline CSound& instance() { return *static_cast<CSound*> ( pSound ); }

    static ASIOCallbacks asioCallbacks;

    static void _onBufferSwitch ( long index, ASIOBool processNow ) { instance().onBufferSwitch ( index, processNow ); }

    static ASIOTime* _onBufferSwitchTimeInfo ( ASIOTime* timeInfo, long index, ASIOBool processNow )
    {
        return instance().onBufferSwitchTimeInfo ( timeInfo, index, processNow );
    }

    static void _onSampleRateChanged ( ASIOSampleRate sampleRate ) { instance().onSampleRateChanged ( sampleRate ); }

    static long _onAsioMessages ( long selector, long value, void* message, double* opt )
    {
        return instance().onAsioMessages ( selector, value, message, opt );
    }

protected:
    // CSound Internal

    void closeAllAsioDrivers();
    bool prepareAsio ( bool bStartAsio ); // Called before starting

    bool checkNewDeviceCapabilities(); // used by checkDeviceChange( checkCapabilities, iDriverIndex)

    //============================================================================
    // Virtual interface to CSoundBase:
    //============================================================================
protected:
    // CSoundBase internal

    virtual long         createDeviceList ( bool bRescan = false ); // Fills strDeviceList. Returns number of devices found
    virtual bool         checkDeviceChange ( CSoundBase::tDeviceChangeCheck mode, int iDriverIndex ); // Open device sequence handling....
    virtual unsigned int getDeviceBufferSize ( unsigned int iDesiredBufferSize );

    virtual void closeCurrentDevice(); // Closes the current driver and Clears Device Info
    virtual bool openDeviceSetup() { return asioDriver.controlPanel(); }

    virtual bool start();
    virtual bool stop();
};
