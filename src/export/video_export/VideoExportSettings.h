#pragma once

#include "export/video_export/VideoExportController.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include <array>

namespace miacode::video_export {

struct VideoExportResolutionPreset {
    int width = 1080;
    int height = 1080;
    const char* label = "1080x1080 (1:1)";
    double aspectRatio = 1.0;
};

inline constexpr std::array<VideoExportResolutionPreset, 21> kVideoExportResolutionPresets{{
    {720, 720, "720x720 (1:1)", 1.0},
    {960, 720, "960x720 (4:3)", 4.0 / 3.0},
    {1280, 720, "1280x720 (16:9)", 16.0 / 9.0},
    {1080, 1080, "1080x1080 (1:1)", 1.0},
    {1440, 1080, "1440x1080 (4:3)", 4.0 / 3.0},
    {1920, 1080, "1920x1080 (16:9)", 16.0 / 9.0},
    {1440, 1440, "1440x1440 (1:1)", 1.0},
    {1920, 1440, "1920x1440 (4:3)", 4.0 / 3.0},
    {2560, 1440, "2560x1440 (16:9)", 16.0 / 9.0},
    {360, 360, "360x360 (1:1)", 1.0},
    {480, 360, "480x360 (4:3)", 4.0 / 3.0},
    {640, 360, "640x360 (16:9)", 16.0 / 9.0},
    {480, 480, "480x480 (1:1)", 1.0},
    {640, 480, "640x480 (4:3)", 4.0 / 3.0},
    {854, 480, "854x480 (16:9)", 16.0 / 9.0},
    {540, 540, "540x540 (1:1)", 1.0},
    {720, 540, "720x540 (4:3)", 4.0 / 3.0},
    {960, 540, "960x540 (16:9)", 16.0 / 9.0},
    {2160, 2160, "2160x2160 (1:1)", 1.0},
    {2880, 2160, "2880x2160 (4:3)", 4.0 / 3.0},
    {3840, 2160, "3840x2160 (16:9)", 16.0 / 9.0},
}};

inline constexpr std::array<int, 5> kVideoExportAudioBitrateOptionsKbps{{128, 160, 192, 256, 320}};
inline constexpr std::array<int, 3> kVideoExportFpsOptions{{30, 60, 120}};
inline constexpr double kFullRangeVideoExportStartEpsilonSeconds = 0.01;

int normalizedVideoExportAudioBitrateKbps(int requested);
int normalizedVideoExportFps(int requested);

QString videoExportPreferencePresetToken(VideoExportPreset preset);
VideoExportPreset videoExportPresetFromPreference(
    const QJsonValue& value,
    VideoExportPreset fallback);
VideoExportSizePreset videoExportSizePresetFromPreference(
    const QJsonValue& value,
    VideoExportSizePreset fallback);

void applyVideoExportPreferences(const QJsonObject& settings, VideoExportTask* task);
void appendVideoExportPreferences(QJsonObject* settings, const VideoExportTask& task);

QString sanitizeVideoExportTimestamp(QString text);
QString formatVideoExportTimestamp(double seconds);
bool parseVideoExportTimestamp(const QString& text, double* seconds);
bool isFullRangeVideoExport(double exportStartSeconds);

// Copies only values the user edits in an export UI. Chart paths, parsed data,
// metadata, output naming and range remain owned by the newly seeded chart.
void copyVideoExportUserSettings(const VideoExportTask& source, VideoExportTask* target);

}  // namespace miacode::video_export
