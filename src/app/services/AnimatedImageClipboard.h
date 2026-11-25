#pragma once

#include <QString>

namespace miacode {

// The UI owns clipboard formats and cached files; the export host owns jobs.
class AnimatedImageClipboard
{
public:
    virtual ~AnimatedImageClipboard() = default;
    virtual QString reserveGifPath(QString* errorMessage) = 0;
    // Published files stay cached; discardGif releases a pending export.
    virtual bool publishGif(const QString& path, QString* errorMessage) = 0;
    virtual void discardGif(const QString& path) = 0;
};

} // namespace miacode
