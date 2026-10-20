#include "app/services/ProjectPreferences.h"
#include "common/DebugLog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    const QString chart = temporary.filePath(QStringLiteral("maidata.txt"));
    QFile chartFile(chart);
    if (!chartFile.open(QIODevice::WriteOnly)) return 1;
    chartFile.write("&inote_5=(120){4}1,E\n");
    chartFile.close();
    using namespace miacode::project_preferences;
    const QString path = projectPreferencesFilePath(chart);
    bool ok = QDir().mkpath(path) && load(chart).isEmpty();
    ok &= !save(chart, {{QStringLiteral("fallback"), true}});
    ok &= QDir().rmdir(path);
    const QJsonObject original{{QStringLiteral("vendor"), 42}, {QStringLiteral("track_volume"), 0.23}};
    ok &= save(chart, original) && load(chart) == original;
    QJsonObject updated = load(chart);
    updated.insert(QStringLiteral("track_volume"), 0.67);
    ok &= save(chart, updated) && load(chart) == updated
        && load(chart).value(QStringLiteral("vendor")).toInt() == 42;
    if (!ok) QTextStream(stderr) << "Project preference IO recovery failed\n";
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}
