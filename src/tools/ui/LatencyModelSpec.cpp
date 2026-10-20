#include "app/ui/latency/LatencyModel.h"
#include "app/runtime/latency/LatencySandboxController.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QTextStream>
#include <QTranslator>

#include <memory>

namespace miacode::latency_analysis {
namespace {
BpmDetectionResult nextBpmResult;
OffsetDetectionInputs lastOffsetInputs;
}

DecodedAudio decodeMonoTrack(const QString&, int)
{
    return {{0.25f, 0.5f}, kAnalysisSampleRate, 10.0};
}

Envelope buildOnsetEnvelope(const QVector<float>&, int, const DetectionTuning&)
{
    return {{1.0f}, 0.01};
}

Envelope buildTransientEnvelope(const QVector<float>&, int)
{
    return {{1.0f}, 0.01};
}

BpmDetectionResult detectBpm(const Envelope&, const QString&)
{
    return nextBpmResult;
}

double detectOffset(const Envelope&, const Envelope&, const OffsetDetectionInputs& inputs,
                    const DetectionTuning&)
{
    lastOffsetInputs = inputs;
    return 0.375;
}
}  // namespace miacode::latency_analysis

// The model is exercised with no audition sandbox; these unused controller
// methods only satisfy the independent spec target's linker.
namespace miacode::latency {
LatencySandboxController::LatencySandboxController(Session*, QObject* parent)
    : QObject(parent) {}
LatencySandboxController::~LatencySandboxController() = default;
double LatencySandboxController::documentWholeBpm() const { return 0.0; }
double LatencySandboxController::documentOffsetSeconds() const { return 0.0; }
int LatencySandboxController::documentClockCount() const { return 4; }
quint64 LatencySandboxController::documentOpenGeneration() const { return 0; }
QString LatencySandboxController::trackPath() const { return {}; }
void LatencySandboxController::applyDetectorBpm(double) {}
void LatencySandboxController::applyDetectorOffset(double) {}
void LatencySandboxController::applyDetectorClockCount(int) {}
LatencySandboxController* LatencySandboxController::sandbox() const { return nullptr; }
void LatencySandboxController::exitSandboxIfActive() {}
void LatencySandboxController::setOnPage(bool) {}
void LatencySandboxController::setBpm(double) {}
void LatencySandboxController::setOffsetSeconds(double) {}
void LatencySandboxController::setSubdivision(int) {}
void LatencySandboxController::setSfxVolumePercent(int) {}
}  // namespace miacode::latency

namespace {
class SpecTranslator final : public QTranslator
{
public:
    QString translate(const char*, const char* id, const char*, int) const override
    {
        if (qstrcmp(id, "latency.detected_1") == 0) return QStringLiteral("%1");
        if (qstrcmp(id, "latency.detected_1_s") == 0) return QStringLiteral("%1 s");
        return {};
    }
};

class FakeLatencyEngine final : public miacode::LatencyEngine
{
public:
    double bpm = 100.0;
    double offset = 0.125;
    int clockCount = 4;
    quint64 openGeneration = 1;
    QString audioPath = QStringLiteral("chart-a.wav");
    int bpmWrites = 0;

    double documentWholeBpm() const override { return bpm; }
    double documentOffsetSeconds() const override { return offset; }
    int documentClockCount() const override { return clockCount; }
    quint64 documentOpenGeneration() const override { return openGeneration; }
    QString trackPath() const override { return audioPath; }
    void applyDetectorBpm(double value) override { bpm = value; ++bpmWrites; }
    void applyDetectorOffset(double value) override { offset = value; }
    void applyDetectorClockCount(int value) override { clockCount = value; }
    miacode::latency::LatencySandboxController* sandbox() const override { return nullptr; }
    void exitSandboxIfActive() override {}
};

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) err << "FAIL: " << message << Qt::endl;
    return condition;
}

bool verifyDocumentRefreshDropsDetectionState(QTextStream& err)
{
    FakeLatencyEngine fake;
    miacode::LatencyEngine* engine = &fake;
    miacode::ui::LatencyModel model(engine);
    model.refreshFromDocument();
    miacode::latency_analysis::nextBpmResult.bpm = 132.0;
    miacode::latency_analysis::nextBpmResult.meterId = QStringLiteral("3/4");
    miacode::latency_analysis::nextBpmResult.meterPhaseSeconds = 0.24;
    miacode::latency_analysis::nextBpmResult.meterPhaseValid = true;
    model.detectBpm();
    model.detectOffset();
    bool ok = require(!model.bpmDetectResult().isEmpty() && !model.offsetDetectResult().isEmpty()
                          && miacode::latency_analysis::lastOffsetInputs.hasLastDetectedMeterPhase,
                      QStringLiteral("first chart has visible results and a meter hint"), err);
    model.refreshFromDocument();
    ok &= require(!model.bpmDetectResult().isEmpty() && !model.offsetDetectResult().isEmpty(),
                  QStringLiteral("reopening the same chart preserves its detection results"), err);

    fake.bpm = 90.0;
    fake.offset = -0.25;
    fake.clockCount = 6;
    fake.openGeneration = 2;
    fake.audioPath = QStringLiteral("chart-b.wav");
    model.refreshFromDocument();
    ok &= require(model.bpm() == 90.0 && model.offsetSeconds() == -0.25
                      && model.clockCount() == 6 && model.bpmDetectResult().isEmpty()
                      && model.offsetDetectResult().isEmpty(),
                  QStringLiteral("second chart starts with its own values and no prior results"), err);
    model.detectOffset();
    ok &= require(!miacode::latency_analysis::lastOffsetInputs.hasLastDetectedMeterPhase,
                  QStringLiteral("second chart offset detection ignores the first chart meter phase"), err);
    return ok;
}

bool verifyDefaultBpmDetectionIsWritten(QTextStream& err)
{
    FakeLatencyEngine fake;
    fake.bpm = 120.0;
    miacode::LatencyEngine* engine = &fake;
    miacode::ui::LatencyModel model(engine);
    model.refreshFromDocument();
    model.setBpm(120.0);
    bool ok = require(fake.bpmWrites == 0,
                      QStringLiteral("leaving an unchanged BPM field does not edit the document"), err);
    miacode::latency_analysis::nextBpmResult.bpm = 120.0;
    model.detectBpm();
    ok &= require(fake.bpmWrites == 1,
                  QStringLiteral("a detected default BPM is committed to the document"), err);
    return ok;
}

bool verifyPageFieldsFollowModelAfterEditing(QQmlEngine& qml, QTextStream& err)
{
    FakeLatencyEngine fake;
    miacode::LatencyEngine* engine = &fake;
    miacode::ui::LatencyModel model(engine);
    model.refreshFromDocument();
    qml.rootContext()->setContextProperty(QStringLiteral("latencyModel"), &model);
    QQmlComponent component(&qml);
    component.setData(R"(
import QtQuick
import MiaCode.UI
LatencyPage {
    width: 640
    height: 480
    latency: latencyModel
    previewSession: QtObject { property bool playing: false }
    pages: QtObject {}
}
)", QUrl(QStringLiteral("file:///LatencyModelSpec.qml")));
    if (component.isError()) {
        for (const QQmlError& error : component.errors()) err << error.toString() << Qt::endl;
        return false;
    }
    std::unique_ptr<QObject> page(component.create());
    if (!require(page != nullptr, QStringLiteral("latency page can be created"), err)) return false;

    const struct {
        const char* objectName;
        QString edited;
        QString expected;
    } cases[] = {
        {"latencyBpmField", QStringLiteral("141"), QStringLiteral("175.000")},
        {"latencyOffsetField", QStringLiteral("0.5"), QStringLiteral("-0.250")},
        {"latencyClockCountField", QStringLiteral("5"), QStringLiteral("6")},
    };
    bool ok = true;
    for (const auto& test : cases) {
        QObject* field = page->findChild<QObject*>(QLatin1String(test.objectName));
        if (!require(field != nullptr, QStringLiteral("find %1").arg(QLatin1String(test.objectName)), err)) {
            return false;
        }
        field->setProperty("text", test.edited);
        ok &= require(QMetaObject::invokeMethod(field, "editingFinished"),
                      QStringLiteral("finish editing %1").arg(QLatin1String(test.objectName)), err);
    }
    fake.bpm = 175.0;
    fake.offset = -0.25;
    fake.clockCount = 6;
    model.refreshFromDocument();
    for (const auto& test : cases) {
        QObject* field = page->findChild<QObject*>(QLatin1String(test.objectName));
        ok &= require(field->property("text").toString() == test.expected,
                      QStringLiteral("%1 follows refreshed document value").arg(QLatin1String(test.objectName)), err);
    }
    return ok;
}
}  // namespace

int main(int argc, char** argv)
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication app(argc, argv);
    SpecTranslator translator;
    app.installTranslator(&translator);
    QTextStream err(stderr);
    QQmlEngine qml;
    qml.addImportPath(QStringLiteral(MIACODE_QML_SPEC_IMPORT_ROOT));
    bool ok = verifyDocumentRefreshDropsDetectionState(err);
    ok &= verifyDefaultBpmDetectionIsWritten(err);
    ok &= verifyPageFieldsFollowModelAfterEditing(qml, err);
    return ok ? 0 : 1;
}
