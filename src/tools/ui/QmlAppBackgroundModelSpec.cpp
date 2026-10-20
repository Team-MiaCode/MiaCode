#include "app/ui/preferences/AppBackgroundModel.h"
#include "common/DebugLog.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

bool require(bool condition, const QString& message, QTextStream& out)
{
    if (!condition) {
        out << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

bool writeFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("background") == 10;
}

bool testImageStatesAndMerge(QTextStream& out)
{
    QTemporaryDir temp;
    if (!require(temp.isValid(), QStringLiteral("temporary background directory"), out)) {
        return false;
    }
    const QString imagePath = temp.filePath(QStringLiteral("background.png"));
    if (!require(writeFile(imagePath), QStringLiteral("create readable image fixture"), out)) {
        return false;
    }

    QJsonObject root{
        {QStringLiteral("ui"), QJsonObject{
            {QStringLiteral("keep"), QStringLiteral("untouched")},
            {QStringLiteral("app_background"), QJsonObject{
                {QStringLiteral("enabled"), true},
                {QStringLiteral("image_path"), imagePath},
            }},
        }},
        {QStringLiteral("documents"), QJsonObject{{QStringLiteral("keep"), true}}},
    };
    bool saveCalled = false;
    QJsonObject saved;
    miacode::ui::AppBackgroundModel model(
        nullptr,
        [&root] { return root; },
        [&saveCalled, &saved](const QJsonObject& next) {
            saveCalled = true;
            saved = next;
            return true;
        });

    if (!require(model.imagePath() == imagePath && model.imageReadable() && !model.sourceUrl().isEmpty(),
                 QStringLiteral("readable persisted path has three-state projection"), out)) {
        return false;
    }
    model.setOpacity(0.5);
    const QJsonObject savedUi = saved.value(QStringLiteral("ui")).toObject();
    if (!require(saveCalled && savedUi.value(QStringLiteral("keep")).toString() == QStringLiteral("untouched")
                     && saved.value(QStringLiteral("documents")).toObject().value(QStringLiteral("keep")).toBool(),
                 QStringLiteral("background write preserves unrelated preference keys"), out)) {
        return false;
    }

    model.setImagePath(temp.filePath(QStringLiteral("missing.png")));
    if (!require(model.imagePath() == imagePath && model.imageReadable() && !model.sourceUrl().isEmpty()
                     && !model.errorMessage().isEmpty(),
                 QStringLiteral("new invalid path does not replace valid image state"), out)) {
        return false;
    }

    model.clearImage();
    return require(model.imagePath().isEmpty() && model.sourceUrl().isEmpty() && !model.imageReadable()
                       && model.errorMessage().isEmpty(),
                   QStringLiteral("explicit clear produces empty state"), out);
}

bool testSaveFailure(QTextStream& out)
{
    QJsonObject root;
    miacode::ui::AppBackgroundModel model(
        nullptr,
        [&root] { return root; },
        [&root](const QJsonObject& next) { root = next; return false; });
    int errorChanges = 0;
    QObject::connect(&model, &miacode::ui::AppBackgroundModel::errorChanged,
                     [&errorChanges] { ++errorChanges; });
    model.setOpacity(0.7);
    model.reload();
    return require(model.opacity() == 0.7 && errorChanges == 0 && model.errorMessage().isEmpty(),
                   QStringLiteral("save failure applies the runtime value, survives reload and stays silent"), out);
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir logDirectory;
    miacode::debug_log::setSessionProjectLogDirectory(logDirectory.path());
    QTextStream out(stdout);
    const bool ok = testImageStatesAndMerge(out) && testSaveFailure(out);
    if (ok) {
        out << "qml_app_background_model_spec ok" << Qt::endl;
    }
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}
