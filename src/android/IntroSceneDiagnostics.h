#pragma once

#include "MobilePreview.h"
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QRunnable>
#include <QSaveFile>
#include <QSGRendererInterface>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QTimer>
#include <memory>

namespace miacode::android {
// Explicit diagnostic launch only. Observe the mounted production scene; do not
// change its images, visibility, frame, rendering backend or playback state.
inline void enableIntroSceneDiagnostics(QQuickWindow& window, MobilePreview& preview, const QString& storageRoot) {
    struct State {
        QHash<QString, QJsonObject> assets;
        QByteArray previous;
        int count = 0;
    };
    const auto state = std::make_shared<State>();
    const QString directory = storageRoot + "/intro-scene-proof";
    if (!QDir().mkpath(directory)) return;
    auto* timer = new QTimer(&window);
    timer->setInterval(600);
    QObject::connect(timer, &QTimer::timeout, &window, [&window, &preview, state, directory, timer] {
        if (!preview.sceneRuntime().introOverlayActive()) return;
        QJsonArray images, transitions;
        QList<QPair<QString, QPointer<QQuickItem>>> textureItems;
        for (auto* item : window.findChildren<QQuickItem*>()) {
            if (item->property("cycleStartFrame").isValid() && item->property("assetsRoot").isValid()) {
                transitions.append(QJsonObject{{"frame", item->property("frame").toInt()},
                    {"cycleStartFrame", item->property("cycleStartFrame").toInt()},
                    {"enterTrimFrames", item->property("enterTrimFrames").toInt()},
                    {"cycleSpanFrames", item->property("cycleSpanFrames").toInt()},
                    {"assetsRoot", item->property("assetsRoot").toUrl().toString()},
                    {"width", item->width()}, {"height", item->height()}, {"visible", item->isVisible()}});
            }
            const auto source = item->property("source").toUrl().toString();
            const bool providerSource = source.startsWith("image://introasset/intro/assets/transition/");
            if (!providerSource && !source.startsWith("qrc:/intro/assets/transition/")) continue;
            textureItems.append({source, item});
            if (!state->assets.contains(source)) {
                const QString resource = ":" + QUrl(source).path();
                QFile bytes(resource);
                const bool opened = bytes.open(QIODevice::ReadOnly);
                QImageReader reader(resource);
                const QImage image = reader.read();
                state->assets.insert(source, QJsonObject{{"opened", opened}, {"decoded", !image.isNull()},
                    {"sha256", QString::fromLatin1(QCryptographicHash::hash(bytes.readAll(), QCryptographicHash::Sha256).toHex())},
                    {"width", image.width()}, {"height", image.height()}, {"format", int(image.format())},
                    {"alpha", image.hasAlphaChannel()},
                    {"centerArgb", QString::number(image.isNull() ? 0 : image.pixel(image.width()/2, image.height()/2), 16)},
                    {"error", image.isNull() ? reader.errorString() : QString()}});
            }
            const auto bounds = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
            QJsonArray ancestors;
            for (auto* parent = item->parentItem(); parent; parent = parent->parentItem()) {
                ancestors.append(QJsonObject{{"type", parent->metaObject()->className()},
                    {"width", parent->width()}, {"height", parent->height()},
                    {"opacity", parent->opacity()}, {"visible", parent->isVisible()},
                    {"scale", parent->scale()}, {"clip", parent->clip()}, {"z", parent->z()}});
            }
            images.append(QJsonObject{{"source", source}, {"status", item->property("status").toInt()},
                {"visible", item->isVisible()}, {"opacity", item->opacity()}, {"scale", item->scale()},
                {"z", item->z()}, {"mipmap", item->property("mipmap").toBool()},
                {"bounds", QJsonArray{bounds.x(), bounds.y(), bounds.width(), bounds.height()}},
                {"asset", state->assets.value(source)}, {"ancestors", ancestors}});
        }
        QJsonObject report{{"frame", preview.sceneRuntime().introOverlayFrame()},
            {"position", preview.positionSeconds()}, {"api", int(window.rendererInterface()->graphicsApi())},
            {"images", images}, {"transitions", transitions}};
        const auto fingerprint = QCryptographicHash::hash(QJsonDocument(report).toJson(QJsonDocument::Compact), QCryptographicHash::Sha256);
        if (state->previous == fingerprint) return;
        state->previous = fingerprint;
        report.insert("pid", double(QCoreApplication::applicationPid()));
        report.insert("utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        const int sequence = state->count++;
        QSaveFile output(directory + QString("/scene-%1.json").arg(sequence, 2, 10, QLatin1Char('0')));
        if (output.open(QIODevice::WriteOnly)) {
            output.write(QJsonDocument(report).toJson()); output.commit();
        }
        window.scheduleRenderJob(QRunnable::create([textureItems, directory, sequence] {
            QJsonArray textures;
            for (const auto& entry : textureItems) {
                const auto item = entry.second;
                if (!item) continue;
                auto* provider = item->isTextureProvider() ? item->textureProvider() : nullptr;
                auto* texture = provider ? provider->texture() : nullptr;
                QJsonObject info{{"source", entry.first}, {"provider", provider != nullptr}, {"texture", texture != nullptr}};
                if (texture) {
                    info.insert("size", QJsonArray{texture->textureSize().width(), texture->textureSize().height()});
                    info.insert("alpha", texture->hasAlphaChannel());
                    info.insert("mipmaps", texture->hasMipmaps());
                    info.insert("filtering", int(texture->filtering()));
                    info.insert("mipmapFiltering", int(texture->mipmapFiltering()));
                    info.insert("atlas", texture->isAtlasTexture());
                    const auto rect = texture->normalizedTextureSubRect();
                    info.insert("subRect", QJsonArray{rect.x(),rect.y(),rect.width(),rect.height()});
                }
                textures.append(info);
            }
            QJsonObject gpu{{"textures", textures}};
            if (auto* context = QOpenGLContext::currentContext()) {
                auto* functions = context->functions();
                GLint maxSize = 0; functions->glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
                gpu.insert("maxTextureSize", maxSize);
                gpu.insert("vendor", QString::fromLatin1(reinterpret_cast<const char*>(functions->glGetString(GL_VENDOR))));
                gpu.insert("renderer", QString::fromLatin1(reinterpret_cast<const char*>(functions->glGetString(GL_RENDERER))));
            }
            QSaveFile gpuOutput(directory + QString("/gpu-%1.json").arg(sequence, 2, 10, QLatin1Char('0')));
            if (gpuOutput.open(QIODevice::WriteOnly)) {
                gpuOutput.write(QJsonDocument(gpu).toJson()); gpuOutput.commit();
            }
        }), QQuickWindow::AfterRenderingStage);
        window.update();
        if (state->count >= 16) timer->stop();
    });
    timer->start();
}
}
