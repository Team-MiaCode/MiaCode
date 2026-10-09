#pragma once
#include "MobileExportComposition.h"
#include "common/ProjectPreferences.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
#include <cstdio>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace miacode::android {
// Runs against a dedicated application name and copied, media-free charts.
// Separate seed/restart processes exercise the actual owners and v2 models.
inline void preparePreferencesSmoke(const QString& mode) {
    QSettings preferences;
    if (mode == "seed") { preferences.remove("mobile"); preferences.sync(); }
    if (mode == "invalid") {
        const QJsonObject render{{"brightnessOuter", 225}, {"brightnessInner", -40},
            {"scaleMode", 999}, {"centerDisplay", 999}, {"tapJudgeTextDistance", -1},
            {"tapFlowSpeed", -70}, {"touchFlowSpeed", 99}};
        preferences.setValue("mobile/preview", QJsonDocument(QJsonObject{{"render", render},
            {"skin", "missing-skin"}, {"outline", 999}, {"judgeEffectStyle", 999},
            {"customOutline", "missing.png"}}).toJson(QJsonDocument::Compact));
        preferences.sync();
    }
}

inline bool runPreferencesSmoke(const QString& mode, const QString& root, const QString& fixture,
    AndroidDocumentSession& document, MobilePreview& preview, MobileExportComposition& composition) {
    QJsonArray checks;
    const auto check = [&](bool ok, const char* name) {
        checks.append(QJsonObject{{"name", name}, {"passed", ok}});
        if (!ok) std::fprintf(stderr, "Preferences failure: %s\n", name);
        return ok;
    };
    const auto read = [](const QString& path) {
        QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    };
    const auto write = [](const QString& path, const QByteArray& data) {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
    };
    auto* audio = qobject_cast<ui::AudioSettingsModel*>(composition.audioSettingsModel());
    auto* render = qobject_cast<ui::PreviewSettingsModel*>(composition.settings());
    const auto open = [&](const QString& name) {
        const QString path = root + '/' + name + "/maidata.txt";
        const auto source = read(path);
        return !source.isEmpty() && document.workspace().openSource(QString::fromUtf8(source), path).accepted;
    };
    const auto setMix = [&](const QList<int>& levels) {
        const auto channels = audio->channels();
        for (int i = 0; i < channels.size(); ++i)
            audio->setChannelPercent(channels[i].toMap().value("key").toString(), levels[i]);
        audio->releaseAudition();
    };
    const auto expectMix = [&](const QList<int>& expected, int muted, const char* name) {
        const auto channels = audio->channels();
        bool ok = channels.size() == expected.size();
        for (int i = 0; ok && i < channels.size(); ++i)
            ok = channels[i].toMap().value("percent").toInt() == (i == muted ? 0 : expected[i]);
        return check(ok && !audio->breakSlideTailCheerMuted(), name);
    };
    const QList<int> mixA{45, 82, 63, 67, 31, 38, 49, 57, 29, 17};
    const QList<int> mixB{54, 60, 42, 22, 71, 32, 33, 34, 35, 36};
    auto mixDefault = mixA; mixDefault[3] = 76;
    const QVariantMap expectedRender{{"brightnessOuter", 37}, {"brightnessInner", 59},
        {"layoutSquareScale", 75}, {"scaleMode", 2}, {"smoothBrightness", false},
        {"showTimestamp", true}, {"showDebugInfo", true}, {"tapFlowSpeed", 6}, {"touchFlowSpeed", 5},
        {"judgeEffectSlide", true}, {"judgeEffectTap", true}, {"judgeEffectBreak", true},
        {"judgeEffectTouch", true}, {"forceLabeledJudgeLineWhenPaused", false},
        {"slideEarlierOnTop", true}, {"centerDisplay", 4}, {"tapJudgeTextDistance", 1},
        {"touchPadAuthoringShortcut", true}};
    const auto expectRender = [&] {
        const auto actual = composition.renderSettings();
        bool ok = true;
        for (auto it = expectedRender.cbegin(); it != expectedRender.cend(); ++it) {
            const bool matches = it.value().metaType().id() == QMetaType::Bool
                ? actual.value(it.key()).toBool() == it.value().toBool()
                : qAbs(actual.value(it.key()).toDouble() - it.value().toDouble()) < 0.001;
            if (!matches) std::fprintf(stderr, "Render mismatch: %s=%s expected=%s\n",
                qPrintable(it.key()), qPrintable(actual.value(it.key()).toString()), qPrintable(it.value().toString()));
            ok &= matches;
        }
        const auto task = composition.buildSeedTask(document.activeDifficulty());
        const auto& live = preview.sceneRuntime().frameState().render;
        return check(ok && qAbs(task.backgroundBrightnessOuter - 0.37) < 0.001
            && qAbs(task.tapFlowSpeed - 6) < 0.001 && task.showTimestamp
            && task.showObjectStatsHud && !task.showChartInfoHud && task.fixHudTextLayout
            && live.showObjectStatsHud && !live.showChartInfoHud && live.fixHudTextLayout
            && QFileInfo(composition.resolveSkinDir()).fileName() == "skinSD"
            && render->skinJudgeEffectIndex() == 1 && composition.currentCustomOutlineFileName().isEmpty(),
            "application render, appearance, preview and export task agree");
    };
    bool ready = check(audio && render && !root.isEmpty(), "real v2 settings models and owned storage available");
    if (ready && mode == "seed") {
        const auto source = read(fixture);
        ready = check(!source.isEmpty(), "source chart available");
        for (const auto* name : {"A", "B", "C", "D"})
            ready &= check(write(root + '/' + name + "/maidata.txt", source), "media-free project copy written");
        if (ready) {
            QSettings preferences;
            preferences.setValue("mobile/preview", QJsonDocument(QJsonObject{{"future", "preserve"},
                {"render", QJsonObject{{"futureRender", 73}}}}).toJson(QJsonDocument::Compact));
            check(project_preferences::save(root + "/A/maidata.txt", QJsonObject{{"futureProject", 91}}), "foreign project settings prepared");
            check(open("A"), "switch to project A"); setMix(mixA);
            audio->toggleChannelMuted("ex"); audio->setBreakSlideTailCheerMuted(true);
            audio->saveAsSoftwareDefault();
            check(open("B"), "switch to project B");
            check(composition.audioSettings().tapPercent() == 67 && composition.audioSettings().exMuted()
                && audio->breakSlideTailCheerMuted(), "new project uses explicit local default");
            setMix(mixB); audio->toggleChannelMuted("touch"); audio->setBreakSlideTailCheerMuted(false);
            check(open("A"), "return to project A"); expectMix(mixA, 4, "A restores full mix; app tail flag overrides sidecar");
            audio->setChannelPercent("tap", 76); audio->saveAsSoftwareDefault();
            audio->setChannelPercent("tap", 67); audio->releaseAudition();
            check(open("B"), "return to project B"); expectMix(mixB, 8, "changing defaults leaves B mix intact");
            check(open("C"), "switch to new project C"); expectMix(mixDefault, 4, "new C uses updated default");
            check(open("A"), "return to A before app settings edit");
            for (auto it = expectedRender.cbegin(); it != expectedRender.cend(); ++it) render->setValue(it.key(), it.value());
            render->setValue("brightnessOuter", 37.36); render->setValue("tapFlowSpeed", 6.12);
            render->setValue("touchFlowSpeed", 4.88);
            composition.setRenderSetting("showObjectStatsHud", true);
            composition.setRenderSetting("showChartInfoHud", false); composition.setRenderSetting("fixHudTextLayout", true);
            render->setSkinIndex(composition.availableSkinDirectoryNames().indexOf("skinSD"));
            render->setSkinJudgeEffectIndex(1); composition.applyOutlineVariant(PreviewOutlineVariant::Line, false, true);
            expectRender();
            const auto before = preferences.value("mobile/preview").toByteArray();
            composition.applyOutlineVariant(PreviewOutlineVariant::Point, false, false);
            check(preferences.value("mobile/preview").toByteArray() == before, "transient outline does not persist");
            composition.applyOutlineVariant(PreviewOutlineVariant::Line, false, true);
            const auto json = QJsonDocument::fromJson(preferences.value("mobile/preview").toByteArray()).object();
            check(json.value("future").toString() == "preserve" && json.value("render").toObject().value("futureRender").toInt() == 73
                && project_preferences::load(document.currentFilePath()).value("futureProject").toInt() == 91,
                "unowned app and project keys survive changes");
        }
    } else if (ready && mode == "restart") {
        expectRender(); check(open("A"), "cold process opens A"); expectMix(mixA, 4, "cold process restores A mix");
        audio->toggleChannelMuted("ex"); check(composition.audioSettings().exPercent() == 31, "A mute restore volume survives restart");
        audio->toggleChannelMuted("ex");
        check(open("B"), "cold process opens B"); expectMix(mixB, 8, "cold process restores B mix");
        audio->toggleChannelMuted("touch"); check(composition.audioSettings().touchPercent() == 35, "B mute restore volume survives restart");
        audio->toggleChannelMuted("touch");
        audio->restoreSoftwareDefault(); expectMix(mixDefault, 4, "explicit restore uses default while retaining app tail flag");
        setMix(mixB); audio->toggleChannelMuted("touch"); audio->releaseAudition();
        check(write(root + "/D/.miacode/preferences.json", "{malformed"), "malformed project preferences prepared");
        check(open("D"), "open malformed project"); expectMix(mixDefault, 4, "malformed project falls back to local default");
        check(open("A"), "return to A after fallback"); expectMix(mixA, 4, "fallback leaves A intact");
        // Exercise an actual failed atomic replacement while the old file exists.
        const QString path = project_preferences::projectPreferencesFilePath(document.currentFilePath());
        const auto before = read(path);
        bool failure = false;
#ifdef Q_OS_WIN
        const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock != INVALID_HANDLE_VALUE) {
            failure = !project_preferences::save(document.currentFilePath(), QJsonObject{{"bad", true}});
            CloseHandle(lock);
        }
#else
        const QString directory = QFileInfo(path).absolutePath();
        const auto permissions = QFileInfo(directory).permissions();
        if (QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::ExeOwner)) {
            failure = !project_preferences::save(document.currentFilePath(), QJsonObject{{"bad", true}});
            QFile::setPermissions(directory, permissions);
        }
#endif
        check(failure && read(path) == before, "failed atomic replacement preserves existing preferences");
    } else if (ready && mode == "invalid") {
        const auto actual = composition.renderSettings();
        check(actual.value("brightnessOuter").toDouble() == 100 && actual.value("brightnessInner").toDouble() == 0
            && actual.value("scaleMode").toInt() == 3 && actual.value("centerDisplay").toInt() == 7
            && actual.value("tapJudgeTextDistance").toInt() == 0 && actual.value("tapFlowSpeed").toDouble() == 1
            && actual.value("touchFlowSpeed").toDouble() == 10 && render->skinJudgeEffectIndex() == 1
            && composition.buildSeedTask(document.activeDifficulty()).outlineVariant == static_cast<PreviewOutlineVariant>(3)
            && composition.currentCustomOutlineFileName().isEmpty(), "invalid saved ranges, enums and missing assets normalize safely");
        check(open("A"), "invalid app settings still allow A"); expectMix(mixA, 4, "invalid app settings preserve project mix");
    } else check(false, "recognized diagnostic mode");
    if (audio) audio->releaseAudition();
    QSettings().sync();
    bool passed = true;
    for (const auto& entry : checks) passed &= entry.toObject().value("passed").toBool();
    passed &= write(root + "/preferences-" + mode + ".json", QJsonDocument(QJsonObject{
        {"mode", mode}, {"passed", passed}, {"checks", checks}, {"P5Accepted", false},
        {"pairedV2FidelityVerified", false}}).toJson(QJsonDocument::Indented));
    std::fprintf(stderr, "Preferences smoke: %s; mode=%s; checks=%d\n", passed ? "passed" : "failed", qPrintable(mode), int(checks.size()));
    return passed;
}
}
