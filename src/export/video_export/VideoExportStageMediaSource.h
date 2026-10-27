#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

// Sequential video decoding with timestamp selection and cached still images.
class VideoExportStageMediaSource
{
public:
    VideoExportStageMediaSource();
    ~VideoExportStageMediaSource();
    VideoExportStageMediaSource(const VideoExportStageMediaSource&) = delete;
    VideoExportStageMediaSource& operator=(const VideoExportStageMediaSource&) = delete;

    bool open(const QString& path, const QSize& outputSize, QString* errorMessage);
    QImage frameAt(double seconds, QString* errorMessage);
    bool isStaticImage() const;

private:
    struct Data;
    std::unique_ptr<Data> d;
};
