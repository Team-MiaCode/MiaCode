#include "app/StartupArguments.h"

#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << Qt::endl; ok = false; }
    };
    using miacode::app::entry::startupOpenTargetFromArguments;
    const QString cwd = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/launch dir"));
    const QString japanese = QStringLiteral("/charts/いとをかし/maidata.txt");

    expect(startupOpenTargetFromArguments({"MiaCode"}, cwd).isEmpty(), "no target means no startup document");
    expect(startupOpenTargetFromArguments({"MiaCode", japanese}, cwd) == japanese,
           "a non-ASCII absolute path is kept verbatim");
    expect(startupOpenTargetFromArguments({"MiaCode", "charts/song/maidata.txt"}, cwd)
               == cwd + QStringLiteral("/charts/song/maidata.txt"),
           "a relative path resolves against the launch directory");
    expect(startupOpenTargetFromArguments({"MiaCode", "./song/../song"}, cwd) == cwd + QStringLiteral("/song"),
           "a relative folder is cleaned");
    expect(startupOpenTargetFromArguments({"MiaCode", "--debug", "--rhi", "opengl", "song"}, cwd)
               == cwd + QStringLiteral("/song"),
           "flags and the --rhi value are skipped");
    expect(startupOpenTargetFromArguments({"MiaCode", "--", "-dash song"}, cwd) == cwd + QStringLiteral("/-dash song"),
           "-- passes a dash-prefixed name through");
    expect(startupOpenTargetFromArguments({"MiaCode", "--debug"}, cwd).isEmpty(), "flags alone name no target");
    return ok ? 0 : 1;
}
