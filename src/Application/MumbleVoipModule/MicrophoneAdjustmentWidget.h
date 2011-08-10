// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include "MumbleFwd.h"
#include "AudioFwd.h"

#include "ui_MicrophoneAdjustment.h"
#include <QTimer>

namespace MumbleVoip
{
    /// Read audio data from SoundService adjust volume and playback locally.
    /// Shows graphical view for audio level.
    class MicrophoneAdjustmentWidget : public QWidget, private Ui::MicrophoneAdjustmentWidget
    {
        Q_OBJECT
    public:
        MicrophoneAdjustmentWidget(Framework* framework, Settings* settings);
        virtual ~MicrophoneAdjustmentWidget();

    private slots:
        virtual void HandleRecordedAudio();
        virtual void PlaybackAudioFrame(PCMAudioFrame* frame);
        virtual void ApplyMicrophoneLevel(PCMAudioFrame* frame);
        virtual void UpdateUI();
        virtual void SaveSettings();
        virtual void ApplySliderValue();
        virtual void StartAudioRecording();
    private:
        static const int AUDIO_RECORDING_BUFFER_MS_ = 500;

        Settings* settings_;
        QTimer read_audio_timer_;
        Framework* framework_;
        SoundChannelPtr sound_id_;
        double voice_activity_level_;
        double microphone_level_;
    };
}
