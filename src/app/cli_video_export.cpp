#include "app/MainEntrypoints.h"
#include "app/runtime/settings/PreferenceDocumentProvider.h"

#include "app/runtime/Session.h"
#include "app/services/ApplicationServices.h"
#include "export/video_export/VideoExportSnapshot.h"
#include "common/DebugLog.h"
#include "common/OperationLog.h"

#include <QGuiApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <cmath>

namespace {

bool parseCliResolutionToken(const QString& token, int* outputWidth, int* outputHeight)
{
    if (outputWidth == nullptr || outputHeight == nullptr) {
        return false;
    }
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    static const QRegularExpression re(
        QStringLiteral("^\\s*(\\d+)\\s*(?:[xX]\\s*(\\d+)\\s*)?$")
    );
    const QRegularExpressionMatch match = re.match(trimmed);
    if (!match.hasMatch()) {
        return false;
    }
    bool widthOk = false;
    const int parsedWidth = match.captured(1).toInt(&widthOk);
    if (!widthOk || parsedWidth <= 0) {
        return false;
    }
    const QString heightText = match.captured(2).trimmed();
    if (heightText.isEmpty()) {
        *outputWidth = parsedWidth;
        *outputHeight = parsedWidth;
        return true;
    }
    bool heightOk = false;
    const int parsedHeight = heightText.toInt(&heightOk);
    if (!heightOk || parsedHeight <= 0) {
        return false;
    }
    *outputWidth = parsedWidth;
    *outputHeight = parsedHeight;
    return true;
}

}  // namespace

namespace miacode::app::entry {

int runCliVideoExport(QGuiApplication& app, QString* errorMessage)
{
    MC_OP("runCliVideoExport");
    // Install the timeline language provider and encoder probe cache.
    // HUD/render settings come from app-created task snapshots.
    miacode::runtime::installPreferenceDocumentProvider();
    try {
    QCommandLineParser parser;
    parser.setApplicationDescription(qtTrId("cli.video_export.description"));
    parser.addHelpOption();
    parser.addVersionOption();
    addSharedCliDebugOption(parser);
    parser.addOption(QCommandLineOption(
        QStringLiteral("export-video"),
        qtTrId("cli.video_export.run")
    ));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("chart"), QStringLiteral("chart-path")},
        qtTrId("cli.video_export.chart_path"),
        QStringLiteral("path")
    ));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("d"), QStringLiteral("difficulty")},
        qtTrId("cli.video_export.difficulty"),
        QStringLiteral("difficulty"),
        QStringLiteral("MAS")
    ));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("r"), QStringLiteral("resolution")},
        qtTrId("cli.video_export.resolution"),
        QStringLiteral("size"),
        QStringLiteral("1080")
    ));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("f"), QStringLiteral("fps")},
        qtTrId("cli.video_export.fps"),
        QStringLiteral("fps"),
        QStringLiteral("60")
    ));
    parser.addOption(QCommandLineOption(
        QStringList{QStringLiteral("o"), QStringLiteral("output")},
        qtTrId("cli.video_export.output"),
        QStringLiteral("path")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("start"),
        qtTrId("cli.video_export.start"),
        QStringLiteral("seconds"),
        QStringLiteral("0")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("duration"),
        qtTrId("cli.video_export.duration"),
        QStringLiteral("seconds")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("hide-timestamp"),
        qtTrId("cli.video_export.hide_timestamp")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("show-object-stats"),
        qtTrId("cli.video_export.object_stats")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("show-chart-info"),
        qtTrId("cli.video_export.chart_info")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("intro"),
        qtTrId("cli.video_export.intro")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("intro-card-shadow"),
        qtTrId("cli.video_export.intro_card_shadow")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("intro-pv-start"),
        qtTrId("cli.video_export.intro_pv_start"),
        QStringLiteral("seconds")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("preview-seconds"),
        qtTrId("cli.video_export.preview_seconds"),
        QStringLiteral("seconds"),
        QStringLiteral("0")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("smooth-brightness"),
        qtTrId("cli.video_export.smooth_brightness")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("brightness-outer"),
        qtTrId("cli.video_export.outer_brightness"),
        QStringLiteral("value"),
        QString::number(miacode::preview_video::kBackgroundBrightnessDefault, 'f', 2)
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("brightness-inner"),
        qtTrId("cli.video_export.inner_brightness"),
        QStringLiteral("value"),
        QString::number(miacode::preview_video::kBackgroundBrightnessInnerDefault, 'f', 2)
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("layout-square-scale"),
        qtTrId("cli.video_export.judge_line_scale"),
        QStringLiteral("value"),
        QString::number(miacode::preview_video::kLayoutSquareScaleDefault, 'f', 2)
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("background-scale"),
        qtTrId("cli.video_export.background_scale"),
        QStringLiteral("mode"),
        QStringLiteral("fill")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("flow-speed"),
        qtTrId("cli.video_export.flow_speed"),
        QStringLiteral("value"),
        QString::number(miacode::preview_gameplay::kPreviewTimingDefaultFlowSpeed, 'f', 2)
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("skin-wait-ms"),
        qtTrId("cli.video_export.skin_wait"),
        QStringLiteral("milliseconds"),
        QStringLiteral("2000")
    ));

    if (!parser.parse(app.arguments())) {
        if (errorMessage != nullptr) {
            *errorMessage = parser.errorText();
        }
        return 2;
    }
    if (!parser.isSet(QStringLiteral("export-video"))) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.dispatch_error");
        }
        return 2;
    }

    const QString chartInput = parser.value(QStringLiteral("chart")).trimmed();
    if (chartInput.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.chart_required");
        }
        return 2;
    }

    int outputWidth = 0;
    int outputHeight = 0;
    const bool resolutionOk = parseCliResolutionToken(
        parser.value(QStringLiteral("resolution")),
        &outputWidth,
        &outputHeight
    );
    bool fpsOk = false;
    const int fps = parser.value(QStringLiteral("fps")).toInt(&fpsOk);
    bool startOk = false;
    const double startSeconds = parser.value(QStringLiteral("start")).toDouble(&startOk);
    bool outerBrightnessOk = false;
    const double outerBrightness = parser.value(QStringLiteral("brightness-outer")).toDouble(&outerBrightnessOk);
    bool innerBrightnessOk = false;
    const double innerBrightness = parser.value(QStringLiteral("brightness-inner")).toDouble(&innerBrightnessOk);
    bool layoutScaleOk = false;
    const double layoutSquareScale = parser.value(QStringLiteral("layout-square-scale")).toDouble(&layoutScaleOk);
    const QString backgroundScaleToken = parser.value(QStringLiteral("background-scale")).trimmed().toLower();
    bool flowSpeedOk = false;
    const double flowSpeed = parser.value(QStringLiteral("flow-speed")).toDouble(&flowSpeedOk);
    bool skinWaitOk = false;
    const int skinWaitMs = parser.value(QStringLiteral("skin-wait-ms")).toInt(&skinWaitOk);

    if (!resolutionOk || outputWidth <= 0 || outputHeight <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.resolution_invalid");
        }
        return 2;
    }
    if (outputWidth < outputHeight) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.resolution_aspect");
        }
        return 2;
    }
    if (!fpsOk || fps <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.fps_invalid");
        }
        return 2;
    }
    if (!startOk || !std::isfinite(startSeconds) || startSeconds < 0.0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.start_invalid");
        }
        return 2;
    }
    if (!skinWaitOk || skinWaitMs < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.skin_wait_invalid");
        }
        return 2;
    }
    if (!outerBrightnessOk || !std::isfinite(outerBrightness) || outerBrightness < 0.0 || outerBrightness > 1.0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.outer_brightness_invalid");
        }
        return 2;
    }
    if (!innerBrightnessOk || !std::isfinite(innerBrightness) || innerBrightness < 0.0 || innerBrightness > 1.0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.inner_brightness_invalid");
        }
        return 2;
    }
    if (!layoutScaleOk || !std::isfinite(layoutSquareScale) || layoutSquareScale <= 0.0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.judge_line_scale_invalid");
        }
        return 2;
    }
    const bool squareFitScaleToken =
        backgroundScaleToken == QStringLiteral("square_fit")
        || backgroundScaleToken == QStringLiteral("square-fit")
        || backgroundScaleToken == QStringLiteral("square_fill")
        || backgroundScaleToken == QStringLiteral("square-fill")
        || backgroundScaleToken == QStringLiteral("square");
    const bool innerCircleFitOuterFillScaleToken =
        backgroundScaleToken == QStringLiteral("inner_circle_fit_outer_fill")
        || backgroundScaleToken == QStringLiteral("inner-circle-fit-outer-fill")
        || backgroundScaleToken == QStringLiteral("inner_fit_outer_fill")
        || backgroundScaleToken == QStringLiteral("inner-fit-outer-fill")
        || backgroundScaleToken == QStringLiteral("circle_fit_outer_fill")
        || backgroundScaleToken == QStringLiteral("circle-fit-outer-fill")
        || backgroundScaleToken == QStringLiteral("inner_circle_fit")
        || backgroundScaleToken == QStringLiteral("inner-circle-fit");
    if (backgroundScaleToken != QStringLiteral("fill")
        && backgroundScaleToken != QStringLiteral("fit")
        && !squareFitScaleToken
        && !innerCircleFitOuterFillScaleToken) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.background_scale_invalid");
        }
        return 2;
    }
    if (!flowSpeedOk || !std::isfinite(flowSpeed) || flowSpeed <= 0.0) {
        if (errorMessage != nullptr) {
            *errorMessage = qtTrId("cli.video_export.flow_speed_invalid");
        }
        return 2;
    }

    double durationSeconds = -1.0;
    if (parser.isSet(QStringLiteral("duration"))) {
        bool durationOk = false;
        durationSeconds = parser.value(QStringLiteral("duration")).toDouble(&durationOk);
        if (!durationOk || !std::isfinite(durationSeconds) || durationSeconds <= 0.0) {
            if (errorMessage != nullptr) {
                *errorMessage = qtTrId("cli.video_export.duration_invalid");
            }
            return 2;
        }
    }

    const bool introPvPreview = parser.isSet(QStringLiteral("intro-pv-start"));
    double introPvStartSeconds = 0.0;
    if (introPvPreview) {
        bool startOk = false;
        introPvStartSeconds = parser.value(QStringLiteral("intro-pv-start")).toDouble(&startOk);
        if (!startOk || !std::isfinite(introPvStartSeconds) || introPvStartSeconds < 0.0) {
            if (errorMessage != nullptr) {
                *errorMessage = qtTrId("cli.video_export.intro_pv_invalid");
            }
            return 2;
        }
    }

    Session::CliVideoExportRequest request;
    request.chartPathOrDirectory = chartInput;
    request.difficulty = parser.value(QStringLiteral("difficulty")).trimmed();
    request.outputPath = parser.value(QStringLiteral("output")).trimmed();
    request.outputWidth = outputWidth;
    request.outputHeight = outputHeight;
    request.fps = fps;
    request.exportStartSeconds = startSeconds;
    request.contentDurationSeconds = durationSeconds;
    request.showTimestamp = !parser.isSet(QStringLiteral("hide-timestamp"));
    request.showObjectStatsHud = parser.isSet(QStringLiteral("show-object-stats"));
    request.showChartInfoHud = parser.isSet(QStringLiteral("show-chart-info"));
    request.addIntro = parser.isSet(QStringLiteral("intro")) || introPvPreview;
    request.introCardShadow = parser.isSet(QStringLiteral("intro-card-shadow"));
    request.introPvPreview = introPvPreview;
    request.introPvStartSeconds = introPvStartSeconds;
    {
        bool previewOk = false;
        const double previewSeconds = parser.value(QStringLiteral("preview-seconds")).toDouble(&previewOk);
        request.previewMaxOutputSeconds = (previewOk && previewSeconds > 0.0) ? previewSeconds : 0.0;
    }
    request.smoothBrightness = parser.isSet(QStringLiteral("smooth-brightness"));
    request.backgroundBrightnessOuter = outerBrightness;
    request.backgroundBrightnessInner = innerBrightness;
    request.layoutSquareScale = layoutSquareScale;
    request.backgroundScaleMode = innerCircleFitOuterFillScaleToken
        ? PreviewBackgroundScaleMode::InnerCircleFitOuterFill
        : (squareFitScaleToken
               ? PreviewBackgroundScaleMode::SquareFitContain
               : (backgroundScaleToken == QStringLiteral("fit")
                      ? PreviewBackgroundScaleMode::FitContain
                      : PreviewBackgroundScaleMode::FillCrop));
    request.noteFlowSpeed = miacode::preview_gameplay::normalizePreviewTimingFlowSpeed(flowSpeed);
    request.touchFlowSpeed = request.noteFlowSpeed;
    request.skinLoadWaitMs = skinWaitMs;

    // The CLI export path builds the same application services the shell does:
    // they own the document domain and the job/UI boundaries, and the window
    // only borrows them (stage 3.5 item 1).
    miacode::ApplicationServices applicationServices;
    Session window(applicationServices);
    QString resolvedOutputPath;
    QString exportError;
    QString exportDetails;
    if (!window.exportPreviewVideoFromCli(request, &resolvedOutputPath, &exportError, &exportDetails)) {
        QTextStream(stderr) << qtTrId("cli.export_failed").arg(exportError) << "\n";
        if (!exportDetails.trimmed().isEmpty()) {
            QTextStream(stderr) << exportDetails << "\n";
        }
        return 1;
    }

    QTextStream(stdout) << qtTrId("cli.export_success").arg(QDir::toNativeSeparators(resolvedOutputPath)) << "\n";
    if (!exportDetails.trimmed().isEmpty()) {
        QTextStream(stdout) << exportDetails << "\n";
    }
    return 0;
    } catch (...) {
        const QString detail = currentExceptionDetail();
        const QString message = qtTrId("cli.video_export.exception").arg(detail);
        miacode::debug_log::appendFatalMessage(QStringLiteral("export/cli_exception"), message);
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return 1;
    }
}

}  // namespace miacode::app::entry
