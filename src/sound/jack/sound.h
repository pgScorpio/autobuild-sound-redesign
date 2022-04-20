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

#ifndef JACK_ON_WINDOWS // these headers are not available in Windows OS
#    include <unistd.h>
#    include <sys/ioctl.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <QThread>
#include <string.h>
#include "../../util.h"
#include "../soundbase.h"
#include "../../global.h"

#include "jackclient.h"

// Values for bool bInput/bIsInput
#define biINPUT  true
#define biOUTPUT false

//============================================================================
// CSound:
//============================================================================

class CSound : public CSoundBase
{
    Q_OBJECT

protected: // Jack:
    CJackClient jackClient;
    bool        bJackWasShutDown;
    bool        bAutoConnect;
    int         iJackNumInputs;

    bool startJack();
    bool stopJack();
    bool checkCapabilities();
    bool setBaseValues();

public:
    CSound ( void ( *theProcessCallback ) ( CVector<short>& psData, void* arg ), void* theProcessCallbackArg );

#ifdef OLD_SOUND_COMPATIBILITY
    // Backwards compatibility constructor
    CSound ( void ( *fpProcessCallback ) ( CVector<int16_t>& psData, void* arg ),
             void*          theProcessCallbackArg,
             const QString& strMIDISetup,
             const bool,
             const QString& );
#endif

    virtual ~CSound()
    {
        Stop();
        stopJack();
    }

protected:
    static inline CSound& instance() { return *static_cast<CSound*> ( pSound ); }

    // CSound callbacks:
    int onBufferSwitch ( jack_nframes_t nframes, void* arg );

    int onBufferSizeCallback();

    void onShutdownCallback();

protected:
    // static callbacks:
    static int _BufferSwitch ( jack_nframes_t nframes, void* arg ) { return instance().onBufferSwitch ( nframes, arg ); }

    static int _BufferSizeCallback ( jack_nframes_t, void* ) { return instance().onBufferSizeCallback(); }

    static void _ShutdownCallback ( void* ) { instance().onShutdownCallback(); }

    //============================================================================
    // Virtual interface to CSoundBase:
    //============================================================================
protected:
    virtual void onChannelSelectionChanged(){}; // Needed on macOS

    virtual long         createDeviceList ( bool bRescan = false );                       // Fills strDeviceNames returns lNumDevices
    virtual bool         checkDeviceChange ( tDeviceChangeCheck mode, int iDriverIndex ); // Performs the different actions for a device change
    virtual unsigned int getDeviceBufferSize ( unsigned int iDesiredBufferSize ); // returns the nearest possible buffersize of selected device
    virtual void         closeCurrentDevice();                                    // Closes the current driver and Clears Device Info
    virtual bool         openDeviceSetup() { return false; }
    virtual bool         start(); // Should call _onStarted() just before return
    virtual bool         stop();  // Should call _onStopped() just before return
};
