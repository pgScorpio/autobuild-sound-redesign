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

#include <QLabel>
#include <QString>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QWhatsThis>
#include <QTimer>
#include <QSlider>
#include <QRadioButton>
#include <QMenuBar>
#include <QLayout>
#include <QButtonGroup>
#include <QMessageBox>
#include "global.h"
#include "util.h"
#include "client.h"
#include "settings.h"
#include "multicolorled.h"
#include "ui_clientsettingsdlgbase.h"

/* Definitions ****************************************************************/
// update time for GUI controls
#define DISPLAY_UPDATE_TIME 1000 // ms

/* Classes ********************************************************************/
class CClientSettingsDlg : public CBaseDlg, private Ui_CClientSettingsDlgBase
{
    Q_OBJECT

public:
    CClientSettingsDlg ( CClient& cClient, QWidget* parent = nullptr );

    void UpdateUploadRate();
    void UpdateDisplay();
    void UpdateSoundDeviceChannelSelectionFrame();

    void SetEnableFeedbackDetection ( bool enable );

protected:
    void    UpdateJitterBufferFrame();
    void    UpdateBufferDelayFrame();
    void    UpdateDirectoryServerComboBox();
    void    UpdateAudioFaderSlider();
    QString GenSndCrdBufferDelayString ( const int iFrameSize, const QString strAddText = "" );

    virtual void showEvent ( QShowEvent* );

    CClient&         Client;
    CClientSettings& Settings;
    CClientStatus&   Status;

    QTimer       TimerStatus;
    QButtonGroup SndCrdBufferDelayButtonGroup;

public slots:
    void OnTimerStatus() { UpdateDisplay(); }

    void OnTabSelection ( int iTabIdx );
    void OnTabChanged();

    void OnClientJitBufSliderChanged ( int value );
    void OnServerJitBufSliderChanged ( int value );
    void OnAutoJitBufStateChanged ( int value );
    void OnEnableOPUS64StateChanged ( int value );
    void OnFeedbackDetectionChanged ( int value );
    void OnCustomDirectoriesEditingFinished();
    void OnNewClientLevelEditingFinished() { Settings.iNewClientFaderLevel = edtNewClientLevel->text().toInt(); }
    void OnInputBoostChanged();
    void OnBufferDelaySelection ( QAbstractButton* button );
    void OnSoundcardSelection ( int iSndDevIdx );
    void OnLeftInputSelection ( int iChanIdx );
    void OnRightInputSelection ( int iChanIdx );
    void OnLeftOutputSelection ( int iChanIdx );
    void OnRightOutputSelection ( int iChanIdx );
    void OnAudioChannelConfigSelection ( int iChanIdx );
    void OnAudioQualitySelection ( int iQualityIdx );
    void OnSkinSelection ( int iDesignIdx );
    void OnMeterStyleSelection ( int iMeterStyleIdx );
    void OnLanguageSelection ( QString strLanguage ) { Settings.strLanguage = strLanguage; }
    void OnAliasTextChanged ( const QString& strNewName );
    void OnInstrumentSelection ( int iCntryListItem );
    void OnCountrySelection ( int iCntryListItem );
    void OnCityTextChanged ( const QString& strNewName );
    void OnSkillSelection ( int iCntryListItem );
    void OnAudioPanValueChanged ( int value );
    void OnInputBalanceChanged();
    void OnNumMixerPanelRowsChanged();
    void OnDriverSetupClicked();
};
