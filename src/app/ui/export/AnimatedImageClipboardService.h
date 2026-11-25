#pragma once

#include "app/services/AnimatedImageClipboard.h"

#include <QObject>
#include <memory>

class QClipboard;
class QLockFile;

namespace miacode::ui {

class AnimatedImageClipboardService final : public QObject, public AnimatedImageClipboard
{
public:
    explicit AnimatedImageClipboardService(QObject* parent = nullptr);
    ~AnimatedImageClipboardService() override;

    QString reserveGifPath(QString* errorMessage) override;
    bool publishGif(const QString& path, QString* errorMessage) override;
    void discardGif(const QString& path) override;

private:
    void pruneCache();

    QClipboard* clipboard_ = nullptr;
    QString cacheDirectory_;
    QString pendingPath_;
    std::unique_ptr<QLockFile> pendingFileLock_;
};

} // namespace miacode::ui
