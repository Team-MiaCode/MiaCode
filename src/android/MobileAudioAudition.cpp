#include "MobileAudioAudition.h"
#include "common/PreviewSfxAssets.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <utility>

namespace miacode::android {
MobileAudioAudition::MobileAudioAudition(MobilePreview& preview, QObject* parent)
    : QObject(parent), preview_(preview) {
    connect(&preview_, &MobilePreview::playingChanged, this, [this] {
        if (preview_.playing()) release();
    });
}

bool MobileAudioAudition::play(const QString& kind, const QString& sfxDir,
    const PreviewAudioSettings& settings) {
    if (preview_.playing()) return false;
    pending_.clear(); pendingKind_.clear();
    for (auto* sample : samples_) sample->stop();
    const QString source = preview_sfx::assetFilePathForKind(sfxDir, kind);
    auto* effect = samples_.value(source, nullptr);
    if (!effect) {
        QFile file(source);
        if (!file.open(QIODevice::ReadOnly)) {
            emit failed(tr("无法读取试听音效：%1").arg(source)); return false;
        }
        const QByteArray data = file.readAll();
        if (file.error() != QFile::NoError || data.isEmpty()) {
            emit failed(tr("试听音效为空或读取失败：%1").arg(source)); return false;
        }
        // Qt's audio backend receives a regular local file, including when
        // the source lives in the APK's assets:/ virtual filesystem.
        const QString directory = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/audio-audition";
        const QString path = directory + '/' + QString::fromLatin1(
            QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()) + ".wav";
        if (!QFile::exists(path)) {
            QSaveFile output(path);
            if (!QDir().mkpath(directory) || !output.open(QIODevice::WriteOnly)
                || output.write(data) != data.size() || !output.commit()) {
                emit failed(tr("无法准备本机试听音效：%1").arg(output.errorString())); return false;
            }
        }
        effect = new QSoundEffect(this);
        samples_.insert(source, effect);
        kinds_.insert(source, kind);
        connect(effect, &QSoundEffect::statusChanged, this, [this, effect, source] {
            if (effect->status() == QSoundEffect::Ready) playReady(effect);
            else if (effect->status() == QSoundEffect::Error && pending_ == effect) {
                pending_.clear(); pendingKind_.clear();
                emit failed(tr("音频设备无法载入试听音效：%1").arg(source));
            }
        });
        effect->setSource(QUrl::fromLocalFile(path));
    }
    effect->setVolume(previewSfxVolumeForKind(settings, kind));
    pending_ = effect; pendingKind_ = kind;
    if (effect->status() == QSoundEffect::Error) {
        pending_.clear(); pendingKind_.clear();
        emit failed(tr("音频设备无法播放试听音效：%1").arg(source)); return false;
    }
    playReady(effect);
    return true;
}

void MobileAudioAudition::applyLevels(const PreviewAudioSettings& settings) {
    for (auto it = samples_.cbegin(); it != samples_.cend(); ++it)
        it.value()->setVolume(previewSfxVolumeForKind(settings, kinds_.value(it.key())));
}

void MobileAudioAudition::playReady(QSoundEffect* effect) {
    if (pending_ != effect || effect->status() != QSoundEffect::Ready) return;
    const QString kind = std::exchange(pendingKind_, QString());
    pending_.clear();
    if (preview_.playing()) return;
    effect->play();
    emit started(kind, effect->volume());
}

void MobileAudioAudition::release() {
    pending_.clear(); pendingKind_.clear();
    qDeleteAll(samples_); samples_.clear(); kinds_.clear();
}
}
