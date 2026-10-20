#include "app/ui/preview/NoteImageProvider.h"

#include "app/ui/preview/PreviewModel.h"
#include "timeline/TimelineNoteAssets.h"

#include <QImage>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickImageProvider>


namespace miacode::ui {
namespace {

QString spriteTypeForKind(const QString& kind)
{
    if (kind == QStringLiteral("break")) {
        return QStringLiteral("tap_break");
    }
    return kind;
}

QImage visibleNoteImage(const QImage& source)
{
    const QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int left = image.width();
    int top = image.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < image.height(); ++y) {
        const auto* pixels = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if (qAlpha(pixels[x]) == 0) continue;
            left = qMin(left, x);
            right = qMax(right, x);
            top = qMin(top, y);
            bottom = qMax(bottom, y);
        }
    }
    return right >= left ? image.copy(left, top, right - left + 1, bottom - top + 1) : QImage();
}

class NoteImageProvider final : public QQuickImageProvider
{
public:
    explicit NoteImageProvider(PreviewModel* model)
        : QQuickImageProvider(QQuickImageProvider::Image)
        , model_(model)
    {
    }

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        QString kind = id;
        const int queryPosition = kind.indexOf(QLatin1Char('?'));
        if (queryPosition >= 0) {
            kind.truncate(queryPosition);
        }

        const QString skinDirectory = model_->currentSkinDirectory();
        if (skinDirectory != cachedSkinDirectory_ || icons_.isEmpty()) {
            cachedSkinDirectory_ = skinDirectory;
            const auto assets = miacode::timeline::loadTimelineNoteAssets(skinDirectory);
            for (const QString& type : {QStringLiteral("tap"), QStringLiteral("hold"),
                     QStringLiteral("slide"), QStringLiteral("touch"), QStringLiteral("tap_break")}) {
                icons_.insert(type, visibleNoteImage(assets.noteIcons.value(type).toImage()));
            }
        }

        QImage image = icons_.value(spriteTypeForKind(kind));
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
        } else if (requestedSize.isValid()) {
            image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        if (size != nullptr) {
            *size = image.size();
        }
        return image;
    }

private:
    QPointer<PreviewModel> model_;
    QString cachedSkinDirectory_;
    QHash<QString, QImage> icons_;
};

} // namespace

void registerNoteImageProvider(QQmlEngine* engine, PreviewModel* model)
{
    engine->addImageProvider(QStringLiteral("noteicon"), new NoteImageProvider(model));
}

} // namespace miacode::ui
