/*********************************************************
 * Copyright (C) 2020, Val Doroshchuk <valbok@gmail.com> *
 *                                                       *
 * This file is part of QtAVPlayer.                      *
 * Free Qt Media Player based on FFmpeg.                 *
 *********************************************************/

#include "qavhwdevice_videotoolbox_p.h"
#include "qavvideobuffer_gpu_p.h"

#import <CoreVideo/CoreVideo.h>
#if defined(Q_OS_MACOS)
#import <IOSurface/IOSurface.h>
#else
#import <IOSurface/IOSurfaceRef.h>
#endif
#import <Metal/Metal.h>

#include <QDebug>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QVariant>
#include <rhi/qrhi_platform.h>

#include <array>

QT_BEGIN_NAMESPACE

AVPixelFormat QAVHWDevice_VideoToolbox::format() const
{
    return AV_PIX_FMT_VIDEOTOOLBOX;
}

AVHWDeviceType QAVHWDevice_VideoToolbox::type() const
{
    return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
}

class VideoBuffer_MTL : public QAVVideoBuffer_GPU
{
public:
    explicit VideoBuffer_MTL(const QAVVideoFrame &frame)
        : QAVVideoBuffer_GPU(frame)
        , m_pixelBuffer(frame.frame() ? reinterpret_cast<CVPixelBufferRef>(frame.frame()->data[3]) : nullptr)
    {
        if (m_pixelBuffer)
            CVPixelBufferRetain(m_pixelBuffer);
    }

    ~VideoBuffer_MTL() override
    {
        for (const auto &planes : m_textures) {
            for (id<MTLTexture> texture : planes)
                [texture release];
        }
        if (m_pixelBuffer)
            CVPixelBufferRelease(m_pixelBuffer);
    }

    QAVVideoFrame::MapData map() override
    {
        QMutexLocker locker(&m_mutex);
        return QAVVideoBuffer_GPU::map();
    }

    bool isMapped() const override
    {
        QMutexLocker locker(&m_mutex);
        return QAVVideoBuffer_GPU::isMapped();
    }

    QAVVideoFrame::HandleType handleType() const override
    {
        return QAVVideoFrame::MTLTextureHandle;
    }

    QVariant handle(QRhi *rhi) const override
    {
        if (!rhi || rhi->backend() != QRhi::Metal)
            return {};
        const auto *handles = static_cast<const QRhiMetalNativeHandles *>(rhi->nativeHandles());
        id<MTLDevice> device = handles ? reinterpret_cast<id<MTLDevice>>(handles->dev) : nil;
        if (!device || !m_pixelBuffer)
            return {};

        QMutexLocker locker(&m_mutex);
        const auto existing = m_textures.constFind(device);
        if (existing != m_textures.cend())
            return textureHandles(existing.value());

        const OSType format = CVPixelBufferGetPixelFormatType(m_pixelBuffer);
        bool tenBit = false;
        switch (format) {
        case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
            break;
        case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
        case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
            tenBit = true;
            break;
        default:
            qWarning() << "Unsupported VideoToolbox pixel format" << format;
            return {};
        }

        IOSurfaceRef surface = CVPixelBufferGetIOSurface(m_pixelBuffer);
        if (!surface || CVPixelBufferGetPlaneCount(m_pixelBuffer) != 2) {
            qWarning() << "Invalid VideoToolbox IOSurface planes";
            return {};
        }

        TexturePlanes planes{};
        @autoreleasepool {
            for (size_t plane = 0; plane < planes.size(); ++plane) {
                const MTLPixelFormat pixelFormat = tenBit
                    ? (plane ? MTLPixelFormatRG16Unorm : MTLPixelFormatR16Unorm)
                    : (plane ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm);
                MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixelFormat
                    width:IOSurfaceGetWidthOfPlane(surface, plane)
                    height:IOSurfaceGetHeightOfPlane(surface, plane)
                    mipmapped:NO];
                planes[plane] = [device newTextureWithDescriptor:desc iosurface:surface plane:plane];
            }
        }
        if (!planes[0] || !planes[1]) {
            for (id<MTLTexture> texture : planes)
                [texture release];
            return {};
        }
        // The textures retain their device and stay alive with this video frame.
        m_textures.insert(device, planes);
        return textureHandles(planes);
    }

private:
    using TexturePlanes = std::array<id<MTLTexture>, 2>;

    static QVariant textureHandles(const TexturePlanes &planes)
    {
        return QList<QVariant>{quint64(planes[0]), quint64(planes[1])};
    }

    const CVPixelBufferRef m_pixelBuffer;
    mutable QMutex m_mutex;
    mutable QHash<id<MTLDevice>, TexturePlanes> m_textures;
};

QAVVideoBuffer *QAVHWDevice_VideoToolbox::videoBuffer(const QAVVideoFrame &frame) const
{
    return new VideoBuffer_MTL(frame);
}

QT_END_NAMESPACE
