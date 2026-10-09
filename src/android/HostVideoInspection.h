#pragma once
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QVideoSink>
#include <QTimer>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

// Decode the actual Android MP4 for visual QA through the host Qt backend.
// This is a host-only inspection command; it does not participate in exports.
inline int inspectExportVideo(QGuiApplication& app, const QString& input, const QString& output) {
    QMediaPlayer player;
    QVideoSink sink;
    player.setVideoSink(&sink);
    QJsonArray samples;
    QList<qint64> targets{0, 3200, 6400};
    int index = 0;
    bool accepting = false;
    if (!QDir().mkpath(output)) return 40;
    QObject::connect(&player, &QMediaPlayer::errorOccurred, &app, [&app](auto, const QString& error) {
        std::fprintf(stderr, "Video inspection failed: %s\n", qPrintable(error)); app.exit(41);
    });
    const auto seek = [&] {
        accepting = true;
        player.setPosition(targets[index]);
        player.play();
    };
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, &app, [&](auto status) {
        if (status == QMediaPlayer::LoadedMedia && index == 0 && !accepting) {
            const auto duration = player.duration();
            if (duration > 0 && duration <= targets.last())
                targets = {0, duration / 2, qMax<qint64>(0, duration - 200)};
            seek();
        }
    });
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, &app, [&](const QVideoFrame& frame) {
        if (!accepting || !frame.isValid() || frame.startTime() < targets[index] * 1000) return;
        const auto image = frame.toImage();
        if (image.isNull()) { app.exit(42); return; }
        const QString path = output + QStringLiteral("/decoded-%1.png").arg(index);
        if (!image.save(path)) { app.exit(43); return; }
        samples.append(QJsonObject{{"targetMs", targets[index]}, {"ptsUs", frame.startTime()},
            {"width", image.width()}, {"height", image.height()}, {"path", path}});
        accepting = false;
        player.pause();
        if (++index == targets.size()) {
            QSaveFile report(output + "/decoded.json");
            if (!report.open(QIODevice::WriteOnly)) { app.exit(44); return; }
            report.write(QJsonDocument(QJsonObject{{"source", input}, {"samples", samples}}).toJson());
            app.exit(report.commit() ? 0 : 44);
        } else {
            QTimer::singleShot(0, &app, seek);
        }
    });
    QTimer::singleShot(20000, &app, [&app] { app.exit(45); });
    player.setSource(QUrl::fromLocalFile(input));
    return app.exec();
}
