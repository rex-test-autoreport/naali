// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "MumbleFwd.h"

#include "ui_VoiceSettings.h"
#include <QTimer>

namespace MumbleVoip
{
    /// Provide UI for Mumble Voip module settings. The widget is used with Settings service.
    class SettingsWidget : public QWidget, private Ui::VoiceSettings
    {

    Q_OBJECT

    public:
        SettingsWidget(Provider* provider, Settings* settings);
        virtual ~SettingsWidget();

    private:
        virtual void InitializeUI();
        virtual void LoadInitialState();

    private slots:
        virtual void OpenMicrophoneAdjustmentWidget();
        virtual void ApplyEncodeQuality();
        virtual void ApplyPlaybackBufferSize();
        virtual void ApplyMicrophoneLevel();
        virtual void ApplyChanges();
        virtual void UpdateUI();
        virtual void UpdateMicrophoneLevel();
        virtual void OnSessionProviderDestroyed();

    private:
        Settings* settings_;
        Provider* provider_;
        QTimer update_timer_;
    };
}
