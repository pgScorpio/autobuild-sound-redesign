/******************************************************************************\
 * \Copyright (c) 2004-2022
 * \author    Volker Fischer
 *

\mainpage Jamulus source code documentation

\section intro_sec Introduction

The Jamulus software enables musicians to perform real-time jam sessions over the
internet. There is one server running the Jamulus server software which collects
the audio data from each Jamulus client, mixes the audio data and sends the mix
back to each client.


Prefix definitions for the GUI:

label:        lbl
combo box:    cbx
line edit:    edt
list view:    lvw
check box:    chb
radio button: rbt
button:       but
text view:    txv
slider:       sld
LED:          led
group box:    grb
pixmap label: pxl
LED bar:      lbr

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

#if _WIN32
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <QString>
#include <QEvent>
#include <QDebug>
#include <stdio.h>
#include <math.h>
#include <string>
#ifndef _WIN32
#    include <unistd.h> // solves a compile problem on recent Ubuntu
#endif
#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

/* Definitions ****************************************************************/
// define this macro to get debug output
//#define _DEBUG_
#undef _DEBUG_

// version and application name (use version from qt prject file)
#undef VERSION
#define VERSION  APP_VERSION
#define APP_NAME "Jamulus"

// Sound Definitions
// stereo for input and output on protocol
#define PROT_NUM_IN_CHANNELS  2
#define PROT_NUM_OUT_CHANNELS 2

// minimum driver requirements
#define DRV_MIN_IN_CHANNELS  1
#define DRV_MIN_OUT_CHANNELS 2

// define the maximum number of audio channel for input/output we can store
// channel infos for (and therefore this is the maximum number of entries in
// the channel selection combo box regardless of the actual available number
// of channels by the audio device)
// pgScorpio: No longer a limit in CSoundBase, should also be no limit in CSound, but it still is a limit used in settings....
#define DRV_MAX_NUM_IN_CHANNELS  64
#define DRV_MAX_NUM_OUT_CHANNELS 64

// maximum number of recognized sound cards installed in the system
#define DRV_MAX_NUM_DEVICES 129 // e.g. 16 inputs, 8 outputs + default entry (MacOS)
// pgScorpio: was DRV_MAX_NUM_DEVICES

#define DRV_MAX_INPUT_GAIN 10

// Windows registry key name of auto run entry for the server
#define AUTORUN_SERVER_REG_NAME "Jamulus server"

// default names of the ini-file for client and server
#define DEFAULT_INI_FILE_NAME        "Jamulus.ini"
#define DEFAULT_INI_FILE_NAME_SERVER "Jamulusserver.ini"

// file name for logging file
#define DEFAULT_LOG_FILE_NAME "Jamulussrvlog.txt"

// System block size, this is the block size on which the audio coder works.
// All other block sizes must be a multiple of this size.
// Note that the UpdateAutoSetting() function assumes a value of 128.
#define SYSTEM_FRAME_SIZE_SAMPLES        64
#define DOUBLE_SYSTEM_FRAME_SIZE_SAMPLES ( 2 * SYSTEM_FRAME_SIZE_SAMPLES )

// additional buffer for delay panning
#define MAX_DELAY_PANNING_SAMPLES 64

// default server address and port numbers
#define DEFAULT_QOS_NUMBER            128 // CS4 (Quality of Service)
#define DEFAULT_SERVER_ADDRESS        "anygenre1.jamulus.io"
#define DEFAULT_PORT_NUMBER           22124
#define CENTSERV_ANY_GENRE2           "anygenre2.jamulus.io:22224"
#define CENTSERV_ANY_GENRE3           "anygenre3.jamulus.io:22624"
#define CENTSERV_GENRE_ROCK           "rock.jamulus.io:22424"
#define CENTSERV_GENRE_JAZZ           "jazz.jamulus.io:22324"
#define CENTSERV_GENRE_CLASSICAL_FOLK "classical.jamulus.io:22524"
#define CENTSERV_GENRE_CHORAL         "choral.jamulus.io:22724"

// specify an invalid port to disable the server
#define INVALID_PORT -1

// servers to check for new versions
#define UPDATECHECK1_ADDRESS "updatecheck1.jamulus.io"
#define UPDATECHECK2_ADDRESS "updatecheck2.jamulus.io"

// getting started and software manual URL
#define CLIENT_GETTING_STARTED_URL "https://jamulus.io/wiki/Getting-Started"
#define SERVER_GETTING_STARTED_URL "https://jamulus.io/wiki/Running-a-Server"
#define SOFTWARE_MANUAL_URL        "https://jamulus.io/wiki/Software-Manual"

// app update message
#define APP_UPGRADE_AVAILABLE_MSG_TEXT \
    QCoreApplication::translate ( \
        "global", \
        "A %1 upgrade is available: <a style='color:red;' href='https://jamulus.io/upgrade?progversion=%2'>go to details and downloads</a>" )

// determining server internal address uses well-known host and port
// We just need a valid, public Internet IP here. We will not send any
// traffic there as we will only set up an UDP socket without sending any
// data.
#define WELL_KNOWN_HOST   "1.1.1.1"              // CloudFlare
#define WELL_KNOWN_HOST6  "2606:4700:4700::1111" // CloudFlare
#define WELL_KNOWN_PORT   DEFAULT_PORT_NUMBER
#define IP_LOOKUP_TIMEOUT 500 // ms

// system sample rate (the sound card and audio coder works on this sample rate)
#define SYSTEM_SAMPLE_RATE_HZ 48000 // Hz

// define the allowed audio frame size factors (since the
// "SYSTEM_FRAME_SIZE_SAMPLES" is quite small, it may be that on some
// computers a larger value is required)
#define FRAME_SIZE_FACTOR_PREFERRED 1 // 64 samples accumulated frame size
#define FRAME_SIZE_FACTOR_DEFAULT   2 // 128 samples accumulated frame size
#define FRAME_SIZE_FACTOR_SAFE      4 // 256 samples accumulated frame size

// define the minimum allowed number of coded bytes for CELT (the encoder
// gets in trouble if the value is too low)
#define CELT_MINIMUM_NUM_BYTES 10

// Maximum block size for network input buffer. It is defined by the longest
// protocol message which is PROTMESSID_CLM_SERVER_LIST: Worst case:
// (2+2+1+2+2)+200*(4+2+2+1+1+2+20+2+32+2+20)=17609
// We add some headroom to that value.
#define MAX_SIZE_BYTES_NETW_BUF 20000

// minimum/maximum network buffer size (which can be chosen by slider)
#define MIN_NET_BUF_SIZE_NUM_BL        1                               // number of blocks
#define MAX_NET_BUF_SIZE_NUM_BL        20                              // number of blocks
#define AUTO_NET_BUF_SIZE_FOR_PROTOCOL ( MAX_NET_BUF_SIZE_NUM_BL + 1 ) // auto set parameter (only used for protocol)

// default network buffer size
#define DEF_NET_BUF_SIZE_NUM_BL 10 // number of blocks

// audio mixer fader and panning maximum value
#define AUD_MIX_FADER_MAX 100
#define AUD_MIX_PAN_MAX   100

// range of audio mixer fader
#define AUD_MIX_FADER_RANGE_DB 35.0f

// coefficient for averaging channel levels for automatic fader adjustment
#define AUTO_FADER_ADJUST_ALPHA 0.2f

// target level for auto fader adjustment in decibels
#define AUTO_FADER_TARGET_LEVEL_DB -30.0f

// threshold in decibels below which the channel is considered as noise
// and not adjusted
#define AUTO_FADER_NOISE_THRESHOLD_DB -40.0f

// maximum number of fader groups (must be consistent to audiomixerboard implementation)
#define MAX_NUM_FADER_GROUPS 8

// maximum number of elemts in the server address combo box
#define MAX_NUM_SERVER_ADDR_ITEMS 12

// maximum number of fader settings to be stored (together with the fader tags)
#define MAX_NUM_STORED_FADER_SETTINGS 250

// range for signal level meter
#define LOW_BOUND_SIG_METER   ( -50.0 ) // dB
#define UPPER_BOUND_SIG_METER ( 0.0 )   // dB

// defines for LED level meter CLevelMeter
#define NUM_STEPS_LED_BAR    8
#define RED_BOUND_LED_BAR    7
#define YELLOW_BOUND_LED_BAR 5

// maximum number of connected clients at the server (must not be larger than 256)
#define MAX_NUM_CHANNELS 150 // max number channels for server

// actual number of used channels in the server
// this parameter can safely be changed from 1 to MAX_NUM_CHANNELS
// without any other changes in the code
#define DEFAULT_USED_NUM_CHANNELS 10 // default used number channels for server

// Maximum number of servers registered in the server list. If you want to
// change this parameter, you most probably have to adjust MAX_SIZE_BYTES_NETW_BUF.
#define MAX_NUM_SERVERS_IN_SERVER_LIST 150 // reduced to 150 because we now have genre-based server lists

// defines the time interval at which the ping time is updated in the GUI
#define PING_UPDATE_TIME_MS 500 // ms

// defines the time interval at which the ping time is updated for the server list
#define PING_UPDATE_TIME_SERVER_LIST_MS 2500 // ms

// defines the interval between Channel Level updates from the server
#define CHANNEL_LEVEL_UPDATE_INTERVAL 200 // number of frames at 64 samples frame size

// time-out until a registered server is deleted from the server list if no
// new registering was made in minutes
#define SERVLIST_TIME_OUT_MINUTES 33 // minutes (should include 3 UDP registration messages)

// poll time for server list (to check if entries are time-out)
#define SERVLIST_POLL_TIME_MINUTES 1 // minute

// time interval for sending ping messages to servers in the server list
#define SERVLIST_UPDATE_PING_SERVERS_MS 59000 // ms

// time between server registration refreshes
#define SERVLIST_REGIST_INTERV_MINUTES 15 // minutes

// defines the minimum time a server must run to be a permanent server
#define SERVLIST_TIME_PERMSERV_MINUTES 2880 // minutes, 2880 = 60 min * 24 h * 2 d

// registration response timeout
#define REGISTER_SERVER_TIME_OUT_MS 500 // ms

// defines the maximum number of times to retry server registration
// when no response is received within the timeout (before reverting
// to SERVLIST_REGIST_INTERV_MINUTES)
#define REGISTER_SERVER_RETRY_LIMIT 5 // count

// Maximum length of fader tag and text message strings (Since for chat messages
// some HTML code is added, we also have to define a second length which includes
// this additionl HTML code. Right now the length of the HTML code is approx. 66
// characters. Here, we add some headroom to this number)
#define MAX_LEN_FADER_TAG           16
#define MAX_LEN_CHAT_TEXT           1600
#define MAX_LEN_CHAT_TEXT_PLUS_HTML 1800
#define MAX_LEN_SERVER_NAME         20
#define MAX_LEN_IP_ADDRESS          15
#define MAX_LEN_SERVER_CITY         20
#define MAX_LEN_VERSION_TEXT        30

// define Settings tab indexes
#define SETTING_TAB_USER          0
#define SETTING_TAB_AUDIO_NETWORK 1

// common tool tip bottom line text
#define TOOLTIP_COM_END_TEXT \
    "<br><div align=right><font size=-1><i>" + \
        QCoreApplication::translate ( "global", \
                                      "For more information use the \"What's " \
                                      "This\" help (help menu, right mouse button or Shift+F1)" ) + \
        "</i></font></div>"

// server welcome message title (do not change for compatibility!)
#define WELCOME_MESSAGE_PREFIX "<b>Server Welcome Message:</b> "

// mixer settings file name suffix
#define MIX_SETTINGS_FILE_SUFFIX "jch"

// minimum length of JSON-RPC secret string (main.cpp)
#define JSON_RPC_MINIMUM_SECRET_LENGTH 16

// JSON-RPC listen address
#define JSON_RPC_LISTEN_ADDRESS "127.0.0.1"

#define _MAXSHORT     32767
#define _MINSHORT     ( -32768 )
#define INVALID_INDEX -1 // define invalid index as a negative value (a valid index must always be >= 0)

#if HAVE_STDINT_H
#    include <stdint.h>
#elif HAVE_INTTYPES_H
#    include <inttypes.h>
#elif defined( _WIN32 )
typedef __int64          int64_t;
typedef __int32          int32_t;
typedef __int16          int16_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8  uint8_t;
#elif defined( ANDROID )
// don't redfine types for android as these ones below don't work
#else
typedef long long          int64_t;
typedef int                int32_t;
typedef short              int16_t;
typedef unsigned long long uint64_t;
typedef unsigned int       uint32_t;
typedef unsigned short     uint16_t;
typedef unsigned char      uint8_t;
#endif

/* Pseudo enum definitions -------------------------------------------------- */
// definition for custom event
#define MS_PACKET_RECEIVED 0

/* Classes ********************************************************************/
class CGenErr
{
public:
    CGenErr ( QString strNewErrorMsg, QString strNewErrorType = "" ) : strErrorMsg ( strNewErrorMsg ), strErrorType ( strNewErrorType ) {}

    QString GetErrorText() const
    {
        // return formatted error text
        if ( strErrorType.isEmpty() )
        {
            return strErrorMsg;
        }
        else
        {
            return strErrorType + ": " + strErrorMsg;
        }
    }

protected:
    QString strErrorMsg;
    QString strErrorType;
};

class CCustomEvent : public QEvent
{
public:
    CCustomEvent ( int iNewMeTy, int iNewSt, int iNewChN = 0 ) :
        QEvent ( QEvent::Type ( QEvent::User + 11 ) ),
        iMessType ( iNewMeTy ),
        iStatus ( iNewSt ),
        iChanNum ( iNewChN )
    {}

    int iMessType;
    int iStatus;
    int iChanNum;
};

/* Prototypes for global functions ********************************************/
extern QString UsageArguments ( QString appPath );

//============================================================================
// CMsgBoxes class:
//  Use this static class to show basic Error, Warning and Info messageboxes
//  For own created message boxes you should still use
//    CMsgBoxes::MainForm() and CMsgBoxes::MainFormName()
//============================================================================
#ifndef HEADLESS
#    include <QMessageBox>
#    define tMainform QDialog
#else
#    define tMainform void
#endif

// html text macro's (for use in gui texts)
#define htmlBold( T ) "<b>" + T + "</b>"
#define htmlNewLine() "<br>"

class CMsgBoxes
{
protected:
    static tMainform* pMainForm;
    static QString    strMainFormName;

public:
    static void init ( tMainform* theMainForm, QString theMainFormName )
    {
        pMainForm       = theMainForm;
        strMainFormName = theMainFormName;
    }

    static tMainform*     MainForm() { return pMainForm; }
    static const QString& MainFormName() { return strMainFormName; }

    // Message boxes:
    static void ShowError ( QString strError );
    static void ShowWarning ( QString strWarning );
    static void ShowInfo ( QString strInfo );
};

//============================================================================
// CCommandlineOptions class:
//  Note that passing commandline arguments to classes is no longer required,
//  since an instance of this class can get commandline options anywhere.
//============================================================================

class CCommandlineOptions
{
public:
    CCommandlineOptions() { reset(); }

private:
    friend int main ( int argc, char** argv );

    // Statics assigned from main ()
    static int    appArgc;
    static char** appArgv;

public:
    static QString GetProgramPath() { return QString ( *appArgv ); }

public:
    // sequencial parse functions using the argument index:

    static bool GetFlagArgument ( int& i, const QString& strShortOpt, const QString& strLongOpt );

    static bool GetStringArgument ( int& i, const QString& strShortOpt, const QString& strLongOpt, QString& strArg );

    static bool GetNumericArgument ( int&           i,
                                     const QString& strShortOpt,
                                     const QString& strLongOpt,
                                     double         rRangeStart,
                                     double         rRangeStop,
                                     double&        rValue );

public:
    // find and get a specific argument:

    static bool GetFlagArgument ( const QString& strShortOpt, const QString& strLongOpt )
    {
        for ( int i = 1; i < appArgc; i++ )
        {
            if ( GetFlagArgument ( i, strShortOpt, strLongOpt ) )
            {
                return true;
            }
        }

        return false;
    }

    static bool GetStringArgument ( const QString& strShortOpt, const QString& strLongOpt, QString& strArg )
    {
        for ( int i = 1; i < appArgc; i++ )
        {
            if ( GetStringArgument ( i, strShortOpt, strLongOpt, strArg ) )
            {
                return true;
            }
        }

        return false;
    }

    static bool GetNumericArgument ( const QString& strShortOpt, const QString& strLongOpt, double rRangeStart, double rRangeStop, double& rValue )
    {
        for ( int i = 1; i < appArgc; i++ )
        {
            if ( GetNumericArgument ( i, strShortOpt, strLongOpt, rRangeStart, rRangeStop, rValue ) )
            {
                return true;
            }
        }

        return false;
    }

    //=================================================
    // Non statics to parse bare arguments
    // (These need an instance of CCommandlineOptions)
    //=================================================

protected:
    int    currentIndex;
    char** currentArgv;

    void reset()
    {
        currentArgv  = appArgv;
        currentIndex = 0;
    }

public:
    QString GetFirstArgument()
    {
        reset();
        // Skipping program path
        return GetNextArgument();
    }

    QString GetNextArgument()
    {
        if ( currentIndex < appArgc )
        {
            currentArgv++;
            currentIndex++;

            if ( currentIndex < appArgc )
            {
                return QString ( *currentArgv );
            }
        }

        return QString();
    }
};

// defines for commandline options in the style "shortopt", "longopt"
// Name is standard CMDLN_LONGOPTNAME
// These defines can be used for strShortOpt, strLongOpt parameters
// of the CCommandlineOptions functions.

// clang-format off

#define CMDLN_SERVER              "-s",                    "--server"
#define CMDLN_INIFILE             "-i",                    "--inifile"
#define CMDLN_NOGUI               "-n",                    "--nogui"
#define CMDLN_PORT                "-p",                    "--port"
#define CMDLN_JSONRPCPORT         "--jsonrpcport",         "--jsonrpcport"
#define CMDLN_JSONRPCSECRETFILE   "--jsonrpcsecretfile",   "--jsonrpcsecretfile"
#define CMDLN_QOS                 "-Q",                    "--qos"
#define CMDLN_NOTRANSLATION       "-t",                    "--notranslation"
#define CMDLN_ENABLEIPV6          "-6",                    "--enableipv6"
#define CMDLN_DISCONONQUIT        "-d",                    "--discononquit"
#define CMDLN_DIRECTORYSERVER     "-e",                    "--directoryserver"
#define CMDLN_DIRECTORYFILE       "--directoryfile",       "--directoryfile"
#define CMDLN_LISTFILTER          "-f",                    "--listfilter"
#define CMDLN_FASTUPDATE          "-F",                    "--fastupdate"
#define CMDLN_LOG                 "-l",                    "--log"
#define CMDLN_LICENCE             "-L",                    "--licence"
#define CMDLN_HTMLSTATUS          "-m",                    "--htmlstatus"
#define CMDLN_SERVERINFO          "-o",                    "--serverinfo"
#define CMDLN_SERVERPUBLICIP      "--serverpublicip",      "--serverpublicip"
#define CMDLN_DELAYPAN            "-P",                    "--delaypan"
#define CMDLN_RECORDING           "-R",                    "--recording"
#define CMDLN_NORECORD            "--norecord",            "--norecord"
#define CMDLN_SERVERBINDIP        "--serverbindip",        "--serverbindip"
#define CMDLN_MULTITHREADING      "-T",                    "--multithreading"
#define CMDLN_NUMCHANNELS         "-u",                    "--numchannels"
#define CMDLN_WELCOMEMESSAGE      "-w",                    "--welcomemessage"
#define CMDLN_STARTMINIMIZED      "-z",                    "--startminimized"
#define CMDLN_CONNECT             "-c",                    "--connect"
#define CMDLN_NOJACKCONNECT       "-j",                    "--nojackconnect"
#define CMDLN_MUTESTREAM          "-M",                    "--mutestream"
#define CMDLN_MUTEMYOWN           "--mutemyown",           "--mutemyown"
#define CMDLN_CLIENTNAME          "--clientname",          "--clientname"
#define CMDLN_CTRLMIDICH          "--ctrlmidich",          "--ctrlmidich"
#define CMDLN_JSONRPCPORT         "--jsonrpcport",         "--jsonrpcport"
#define CMDLN_JSONRPCSECRETFILE   "--jsonrpcsecretfile",   "--jsonrpcsecretfile"
// Backwards compatibilyty:
#define CMDLN_CENTRALSERVER       "--centralserver",       "--centralserver"
// Debug options: (not in help)
#define CMDLN_SHOWALLSERVERS      "--showallservers",      "--showallservers"
#define CMDLN_SHOWANALYZERCONSOLE "--showanalyzerconsole", "--showanalyzerconsole"
// CMDLN_SPECIAL: Used for debugging, should NOT be in help, nor documented elsewhere!
// any option after --special is accepted
#define CMDLN_SPECIAL             "--special",             "--special"
// Special options for sound-redesign testing
#define CMDLN_JACKINPUTS          "--jackinputs",          "--jackinputs"

// clang-format on
