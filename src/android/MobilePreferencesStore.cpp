#include "MobilePreferencesStore.h"
#include "MobilePreview.h"
#include "MobileTimeline.h"
#include "app/ui/layout/WorkbenchSettings.h"
#include "app/ui/preferences/PreferenceDocument.h"
#include "common/EditorAppearance.h"
#include "common/PreviewSfxAssets.h"
#include <QGuiApplication>
#include <QScreen>

namespace miacode::android {
namespace {
using Mode = PreviewCanvasFrameRateMode;
Mode fromToken(const QJsonValue& value, Mode fallback) {
    const auto token = value.toString();
    if (token == "30") return Mode::Fps30;
    if (token == "60") return Mode::Fps60;
    if (token == "120") return Mode::Fps120;
    if (token == "display_max") return Mode::DisplayRefresh;
    return fallback;
}
QString token(Mode mode) {
    switch (mode) {
    case Mode::Fps30: return QStringLiteral("30");
    case Mode::Fps120: return QStringLiteral("120");
    case Mode::DisplayRefresh: return QStringLiteral("display_max");
    default: return QStringLiteral("60");
    }
}
bool valid(Mode mode) { return mode >= Mode::Fps30 && mode <= Mode::DisplayRefresh; }
void saveValue(const char* section, const char* key, const QJsonValue& value) {
    auto root = PreferenceDocument::loadPreferencesObject();
    auto child = root.value(QLatin1String(section)).toObject();
    child.insert(QLatin1String(key), value);
    root.insert(QLatin1String(section), child);
    PreferenceDocument::savePreferencesObject(root);
}
}
void MobilePreferencesStore::preparePlatformDefaultsAndDecoder() {
    preview_sfx::setMusicDirectoryOverride(QFileInfo(PreferenceDocument::preferencesFilePath())
        .absoluteDir().filePath(QStringLiteral("music")));
    auto root = PreferenceDocument::loadPreferencesObject();
#ifdef Q_OS_ANDROID
    auto ui = root.value("ui").toObject();
    // A new phone installation must accept its soft keyboard. Explicit user
    // preferences, including external-keyboard IME blocking, are preserved.
    if (!ui.contains("editor_ime_input_disabled")) {
        ui.insert("editor_ime_input_disabled", false);
        root.insert("ui", ui);
        PreferenceDocument::savePreferencesObject(root);
    }
#endif
    if (root.value("preview").toObject().value("video_decode_prefers_software").toBool(false))
        qputenv("QT_FFMPEG_DECODING_HW_DEVICE_TYPES", ",");
}
MobilePreferencesStore::MobilePreferencesStore(ui::WorkbenchSettings& settings, MobilePreview& preview,
    MobileTimeline& timeline, QObject* parent)
    : QObject(parent), settings_(settings), preview_(preview), timeline_(timeline) {
    const auto root = PreferenceDocument::loadPreferencesObject();
    const auto ui = root.value("ui").toObject();
    const auto render = root.value("preview").toObject();
    pointSize_ = settings.codeFont().pointSize();
    spacing_ = runtime::shared::normalizeEditorLineSpacingFactor(ui.value("editor_line_spacing_factor").toDouble(1));
    halfWidth_ = settings.editorHalfWidthInputEnabled();
    completion_ = settings.editorAutoCompletionEnabled();
    imeDisabled_ = settings.editorImeInputDisabled();
    canvasMode_ = fromToken(render.value("canvas_frame_rate_mode"), Mode::Fps60);
    pvMode_ = fromToken(render.value("pv_frame_rate_mode"), Mode::Fps30);
    timelineMode_ = fromToken(render.value("timeline_frame_rate_mode"), canvasMode_);
    software_ = render.value("video_decode_prefers_software").toBool(false);
    swapped_ = ui.value("workspace_panels_swapped").toBool(false);
    applyEditor(false);
    applyFrameRates();
    for (auto* screen : QGuiApplication::screens())
        connect(screen, &QScreen::refreshRateChanged, this, [this] { applyFrameRates(); emit refreshRateChanged(); });
}
void MobilePreferencesStore::applyEditor(bool persist) {
    settings_.setEditorAppearance(pointSize_, spacing_);
    settings_.applyEditorInputPreferences(halfWidth_, completion_, imeDisabled_);
    if (!persist) return;
    auto root = PreferenceDocument::loadPreferencesObject();
    auto ui = root.value("ui").toObject();
    ui.insert("editor_text_font_size", pointSize_);
    ui.insert("editor_line_spacing_factor", spacing_);
    ui.insert("editor_half_width_input", halfWidth_);
    ui.insert("editor_auto_completion", completion_);
    ui.insert("editor_ime_input_disabled", imeDisabled_);
    root.insert("ui", ui);
    PreferenceDocument::savePreferencesObject(root);
}
void MobilePreferencesStore::applyEditorTextFontSize(int value, bool persist) {
    pointSize_ = qBound(runtime::shared::kEditorTextFontSizeMin, value, runtime::shared::kEditorTextFontSizeMax);
    applyEditor(persist);
}
void MobilePreferencesStore::applyEditorLineSpacingFactor(double value, bool persist) {
    spacing_ = runtime::shared::normalizeEditorLineSpacingFactor(value);
    applyEditor(persist);
}
void MobilePreferencesStore::applyEditorHalfWidthInputEnabled(bool value, bool persist) { halfWidth_ = value; applyEditor(persist); }
void MobilePreferencesStore::applyEditorAutoCompletionEnabled(bool value, bool persist) { completion_ = value; applyEditor(persist); }
void MobilePreferencesStore::applyEditorImeInputDisabled(bool value, bool persist) { imeDisabled_ = value; applyEditor(persist); }
double MobilePreferencesStore::previewCanvasRefreshRate() const {
    auto* screen = QGuiApplication::primaryScreen();
    return screen ? screen->refreshRate() : 60;
}
void MobilePreferencesStore::applyFrameRates() {
    const double refresh = previewCanvasRefreshRate();
    preview_.setCanvasFrameRate(canvasMode_, refresh);
    preview_.setStageMediaFrameRate(pvMode_, refresh);
    timeline_.setFrameRate(timelineMode_, refresh);
}
void MobilePreferencesStore::setPreviewCanvasFrameRateMode(Mode mode, bool persist) {
    if (!valid(mode)) return;
    canvasMode_ = mode;
    preview_.setCanvasFrameRate(mode, previewCanvasRefreshRate());
    if (persist) {
        auto root = PreferenceDocument::loadPreferencesObject();
        auto render = root.value("preview").toObject();
        // A missing timeline mode initially inherits the canvas default. Save
        // its current independent value before changing that fallback so a
        // cold start cannot silently change the timeline's frame rate.
        if (!render.contains("timeline_frame_rate_mode"))
            render.insert("timeline_frame_rate_mode", token(timelineMode_));
        render.insert("canvas_frame_rate_mode", token(mode));
        root.insert("preview", render);
        PreferenceDocument::savePreferencesObject(root);
    }
}
void MobilePreferencesStore::setPreviewStageMediaFrameRateMode(Mode mode, bool persist) {
    if (!valid(mode)) return;
    pvMode_ = mode;
    preview_.setStageMediaFrameRate(mode, previewCanvasRefreshRate());
    if (persist) saveValue("preview", "pv_frame_rate_mode", token(mode));
}
void MobilePreferencesStore::setTimelineFrameRateMode(Mode mode, bool persist) {
    if (!valid(mode)) return;
    timelineMode_ = mode;
    timeline_.setFrameRate(mode, previewCanvasRefreshRate());
    if (persist) saveValue("preview", "timeline_frame_rate_mode", token(mode));
}
void MobilePreferencesStore::setVideoDecodePrefersSoftware(bool value, bool persist) {
    software_ = value;
    if (persist) saveValue("preview", "video_decode_prefers_software", value);
    // Qt caches FFmpeg hardware policy at multimedia initialization. This
    // desired policy is applied by preparePlatformDefaultsAndDecoder next run.
}
void MobilePreferencesStore::setWorkspacePanelsSwapped(bool value, bool persist) {
    if (swapped_ == value) return;
    swapped_ = value;
    if (persist) saveValue("ui", "workspace_panels_swapped", value);
    emit panelsSwappedChanged();
}
}
