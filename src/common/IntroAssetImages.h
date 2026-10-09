#pragma once

#include <QDir>
#include <QImageReader>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QUrl>
#include <QUrlQuery>

namespace miacode::intro {
class IntroAssetTexture final : public QQuickTextureFactory {
public:
    explicit IntroAssetTexture(const QImage& image) : image_(image) {}
    QSGTexture* createTexture(QQuickWindow* window) const override {
        return window->createTextureFromImage(image_);
    }
    QSize textureSize() const override { return image_.size(); }
    int textureByteCount() const override { return int(image_.sizeInBytes()); }
    QImage image() const override { return image_; }

private:
    QImage image_;
};

// Keep the authored pixels and mipmap filtering, but upload intro/cover art
// as RGBA. Some Android GLES implementations accept BGRA uploads and then reject
// generating their mipmaps, leaving the texture incomplete when it is sampled.
class IntroAssetImages final : public QQuickImageProvider {
public:
    IntroAssetImages() : QQuickImageProvider(QQuickImageProvider::Texture) {}

    QQuickTextureFactory* requestTexture(const QString& id, QSize* size, const QSize& requestedSize) override {
        const int queryStart = id.indexOf('?');
        const QString imageId = queryStart < 0 ? id : id.left(queryStart);
        const QString path = QDir::cleanPath(imageId);
        QString file;
        if (path.startsWith("intro/assets/")) {
            file = ":/" + path;
        } else {
            const QUrl url(QUrl::fromPercentEncoding(imageId.toUtf8()));
            if (url.scheme() == "qrc") file = ":" + url.path();
            else if (url.isLocalFile()) file = url.toLocalFile();
            else return nullptr;
        }
        QImageReader reader(file);
        const QSize originalSize = reader.size();
        if (size) *size = originalSize;
        if (requestedSize.isValid() && originalSize.isValid())
            reader.setScaledSize(originalSize.scaled(requestedSize, Qt::KeepAspectRatio));
        if (queryStart >= 0) {
            const auto values = QUrlQuery(id.mid(queryStart + 1)).queryItemValue("clip").split(',');
            if (values.size() == 4) {
                bool valid = true;
                int coordinates[4];
                for (int i = 0; i < 4; ++i) {
                    bool converted = false;
                    coordinates[i] = values[i].toInt(&converted);
                    valid = valid && converted;
                }
                if (valid && coordinates[2] > 0 && coordinates[3] > 0)
                    reader.setScaledClipRect(QRect(coordinates[0], coordinates[1], coordinates[2], coordinates[3]));
            }
        }
        QImage image = reader.read();
        if (image.isNull()) return nullptr;
        if (size && !originalSize.isValid()) *size = image.size();
        if (requestedSize.isValid() && image.size() != requestedSize)
            image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        // Qt's default image factory converts RGBA back to ARGB/BGRA. A texture
        // factory preserves the chosen upload format through to the render thread.
        return new IntroAssetTexture(image.convertToFormat(QImage::Format_RGBA8888_Premultiplied));
    }
};

inline void registerIntroAssetImages(QQmlEngine* engine) {
    if (engine && !engine->imageProvider("introasset"))
        engine->addImageProvider("introasset", new IntroAssetImages);
}
}
