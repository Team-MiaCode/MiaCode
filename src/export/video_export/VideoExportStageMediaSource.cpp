#include "export/video_export/VideoExportStageMediaSource.h"

#include <QImageReader>
#include <QTransform>
#include <QtMath>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/display.h>
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <limits>

namespace {
QString decodeError(const char* operation, int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(code, text, sizeof(text));
    return QStringLiteral("%1: %2").arg(QString::fromLatin1(operation), QString::fromUtf8(text));
}

// The display matrix moved from the stream to AVCodecParameters in FFmpeg 6.1.
// Older releases (the 4.4 packaged by Ubuntu 22.04, for instance) expose it only
// on the AVStream.
void displayMatrix(const AVStream* stream, const uint8_t** data, int* size)
{
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(60, 31, 100)
    const AVPacketSideData* side = av_packet_side_data_get(
        stream->codecpar->coded_side_data, stream->codecpar->nb_coded_side_data,
        AV_PKT_DATA_DISPLAYMATRIX);
    *data = side != nullptr ? side->data : nullptr;
    *size = side != nullptr ? side->size : 0;
#else
    int streamSize = 0;
    const uint8_t* streamData = av_stream_get_side_data(
        stream, AV_PKT_DATA_DISPLAYMATRIX, &streamSize);
    *data = streamData;
    *size = streamSize;
#endif
}
}

struct VideoExportStageMediaSource::Data
{
    AVFormatContext* format = nullptr;
    AVCodecContext* codec = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* current = nullptr;
    AVFrame* next = nullptr;
    SwsContext* scaler = nullptr;
    int streamIndex = -1;
    QSize outputSize;
    double startSecond = 0.0;
    double nextSecond = 0.0;
    double currentSecond = -1.0;
    double requestedSecond = -1.0;
    double rotation = 0.0;
    double pixelAspect = 1.0;
    bool draining = false;
    bool nextReady = false;
    bool currentReady = false;
    bool eof = false;
    bool still = false;
    QImage image;
    qint64 imageTimestamp = AV_NOPTS_VALUE;

    ~Data()
    {
        sws_freeContext(scaler);
        av_frame_free(&next);
        av_frame_free(&current);
        av_packet_free(&packet);
        avcodec_free_context(&codec);
        avformat_close_input(&format);
    }

    int decodeNext()
    {
        av_frame_unref(next);
        for (;;) {
            const int received = avcodec_receive_frame(codec, next);
            if (received != AVERROR(EAGAIN)) return received;
            if (draining) return AVERROR_EOF;
            int read = 0;
            do {
                av_packet_unref(packet);
                read = av_read_frame(format, packet);
            } while (read >= 0 && packet->stream_index != streamIndex);
            if (read < 0 && read != AVERROR_EOF) return read;
            draining = read == AVERROR_EOF;
            const int sent = avcodec_send_packet(codec, draining ? nullptr : packet);
            av_packet_unref(packet);
            if (sent < 0 && sent != AVERROR_EOF) return sent;
        }
    }
};

VideoExportStageMediaSource::VideoExportStageMediaSource() : d(std::make_unique<Data>()) {}
VideoExportStageMediaSource::~VideoExportStageMediaSource() = default;

bool VideoExportStageMediaSource::open(const QString& path, const QSize& outputSize, QString* errorMessage)
{
    d = std::make_unique<Data>();
    d->outputSize = outputSize;
    if (path.isEmpty()) return true;
    QImageReader reader(path);
    if (reader.canRead()) {
        reader.setAutoTransform(true);
        d->image = reader.read();
        if (d->image.isNull()) {
            *errorMessage = reader.errorString();
            return false;
        }
        d->still = true;
        return true;
    }
    const QByteArray encodedPath = path.toUtf8();
    int status = avformat_open_input(&d->format, encodedPath.constData(), nullptr, nullptr);
    if (status >= 0) status = avformat_find_stream_info(d->format, nullptr);
    if (status >= 0) status = av_find_best_stream(d->format, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (status < 0) {
        *errorMessage = decodeError("open export video", status);
        return false;
    }
    d->streamIndex = status;
    AVStream* stream = d->format->streams[d->streamIndex];
    const AVCodec* decoder = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!decoder) {
        *errorMessage = QStringLiteral("export video decoder is unavailable");
        return false;
    }
    d->codec = avcodec_alloc_context3(decoder);
    d->packet = av_packet_alloc();
    d->current = av_frame_alloc();
    d->next = av_frame_alloc();
    if (!d->codec || !d->packet || !d->current || !d->next) {
        *errorMessage = QStringLiteral("failed to allocate export video decoder");
        return false;
    }
    status = avcodec_parameters_to_context(d->codec, stream->codecpar);
    d->codec->thread_count = 4;
    if (status >= 0) status = avcodec_open2(d->codec, decoder, nullptr);
    if (status < 0) {
        *errorMessage = decodeError("initialize export video decoder", status);
        return false;
    }
    if (stream->start_time != AV_NOPTS_VALUE) {
        d->startSecond = stream->start_time * av_q2d(stream->time_base);
    }
    const AVRational aspect = av_guess_sample_aspect_ratio(d->format, stream, nullptr);
    if (aspect.num > 0 && aspect.den > 0) d->pixelAspect = av_q2d(aspect);
    const uint8_t* matrixData = nullptr;
    int matrixSize = 0;
    displayMatrix(stream, &matrixData, &matrixSize);
    if (matrixData != nullptr && matrixSize >= 9 * static_cast<int>(sizeof(int32_t))) {
        const double angle = av_display_rotation_get(reinterpret_cast<const int32_t*>(matrixData));
        if (qIsFinite(angle)) d->rotation = -angle;
    }
    return true;
}

bool VideoExportStageMediaSource::isStaticImage() const { return d->still; }

QImage VideoExportStageMediaSource::frameAt(double seconds, QString* errorMessage)
{
    errorMessage->clear();
    if (d->still || !d->format) return d->image;
    if (seconds < 0.0) return {};
    AVStream* stream = d->format->streams[d->streamIndex];
    const double timeBase = av_q2d(stream->time_base);
    const bool seek = (d->requestedSecond < 0.0 && seconds > 1.0)
        || (d->requestedSecond >= 0.0 && (seconds < d->requestedSecond - 0.000001
                                         || seconds > d->requestedSecond + 2.0));
    if (seek) {
        const qint64 timestamp = qRound64((seconds + d->startSecond) / timeBase);
        const int status = av_seek_frame(d->format, d->streamIndex, timestamp, AVSEEK_FLAG_BACKWARD);
        if (status < 0) {
            *errorMessage = decodeError("seek export video", status);
            return {};
        }
        avcodec_flush_buffers(d->codec);
        av_frame_unref(d->current);
        av_frame_unref(d->next);
        d->draining = d->eof = d->currentReady = d->nextReady = false;
        d->imageTimestamp = AV_NOPTS_VALUE;
    }
    d->requestedSecond = seconds;
    while (!d->eof) {
        if (!d->nextReady) {
            const int status = d->decodeNext();
            if (status == AVERROR_EOF) {
                d->eof = true;
                break;
            }
            if (status < 0) {
                *errorMessage = decodeError("decode export video frame", status);
                return {};
            }
            d->nextReady = true;
            d->nextSecond = d->next->best_effort_timestamp == AV_NOPTS_VALUE
                ? std::max(0.0, d->currentSecond + av_q2d(av_inv_q(av_guess_frame_rate(d->format, stream, nullptr))))
                : d->next->best_effort_timestamp * timeBase - d->startSecond;
        }
        if (d->currentReady && d->nextSecond > seconds + 0.000001) break;
        av_frame_unref(d->current);
        std::swap(d->current, d->next);
        d->currentSecond = d->nextSecond;
        d->currentReady = true;
        d->nextReady = false;
    }
    if (!d->currentReady) {
        *errorMessage = QStringLiteral("export video contains no decoded frames");
        return {};
    }
    const qint64 imageTimestamp = qRound64(d->currentSecond * 1000000.0);
    if (imageTimestamp == d->imageTimestamp) return d->image;
    const AVFrame* frame = d->current;
    QSize displaySize(qMax(1, qRound(frame->width * d->pixelAspect)), frame->height);
    const bool rotated = qAbs(qSin(qDegreesToRadians(d->rotation))) > 0.5;
    const QSize target = rotated ? d->outputSize.transposed() : d->outputSize;
    const double scale = std::min(1.0, std::max(double(target.width()) / displaySize.width(),
                                             double(target.height()) / displaySize.height()));
    const QSize size(qMax(1, qRound(displaySize.width() * scale)),
                     qMax(1, qRound(displaySize.height() * scale)));
    d->scaler = sws_getCachedContext(d->scaler, frame->width, frame->height,
        static_cast<AVPixelFormat>(frame->format), size.width(), size.height(), AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!d->scaler) {
        *errorMessage = QStringLiteral("failed to create export video color converter");
        return {};
    }
    const int colorSpace = frame->colorspace == AVCOL_SPC_BT709 ? SWS_CS_ITU709
        : (frame->colorspace == AVCOL_SPC_BT2020_NCL ? SWS_CS_BT2020 : SWS_CS_DEFAULT);
    const int* coefficients = sws_getCoefficients(colorSpace);
    sws_setColorspaceDetails(d->scaler, coefficients, frame->color_range == AVCOL_RANGE_JPEG,
                            coefficients, 1, 0, 1 << 16, 1 << 16);
    QImage image(size, QImage::Format_RGBA8888);
    if (image.isNull()) {
        *errorMessage = QStringLiteral("failed to allocate export video frame");
        return {};
    }
    uint8_t* planes[] = {image.bits(), nullptr, nullptr, nullptr};
    const int strides[] = {static_cast<int>(image.bytesPerLine()), 0, 0, 0};
    const int rows = sws_scale(d->scaler, frame->data, frame->linesize, 0, frame->height, planes, strides);
    if (rows < 0) {
        *errorMessage = decodeError("convert export video frame", rows);
        return {};
    }
    if (!qFuzzyIsNull(d->rotation)) image = image.transformed(QTransform().rotate(d->rotation));
    d->image = std::move(image);
    d->imageTimestamp = imageTimestamp;
    return d->image;
}
