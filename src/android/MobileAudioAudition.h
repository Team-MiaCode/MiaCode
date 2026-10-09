#pragma once

#include "MobilePreview.h"
#include <QHash>
#include <QSoundEffect>

namespace miacode::android {

// The settings panel's short WAV audition, separate from chart playback.
// Closing the panel or starting preview invalidates any pending sample load.
class MobileAudioAudition final : public QObject {
    Q_OBJECT
public:
    explicit MobileAudioAudition(MobilePreview&, QObject* parent = nullptr);
    bool play(const QString& kind, const QString& sfxDir, const PreviewAudioSettings&);
    void applyLevels(const PreviewAudioSettings&);
    void release();
signals:
    void failed(const QString& message);
    void started(const QString& kind, double volume);
private:
    void playReady(QSoundEffect* effect);
    MobilePreview& preview_;
    QHash<QString, QSoundEffect*> samples_;
    QHash<QString, QString> kinds_;
    QPointer<QSoundEffect> pending_;
    QString pendingKind_;
};
}
