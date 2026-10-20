#include "app/MainEntrypoints.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QString>

#include <exception>
#include <new>

namespace miacode::app::entry {

void addSharedCliDebugOption(QCommandLineParser& parser)
{
    // main() already enables debug mode before CLI dispatch. We still declare
    // this option here so subcommand parsers accept forwarded "--debug".
    parser.addOption(QCommandLineOption(
        QStringLiteral("debug"),
        qtTrId("cli.debug")
    ));
    // P3 — hidden internal GPU device-policy overrides. Declared so the CLI
    // export / export-worker parsers accept them without erroring; the values
    // are consumed by miacode::gpu (raw-arg scan), not read back off the parser.
    parser.addOption(QCommandLineOption(
        QStringLiteral("gpu-policy"),
        qtTrId("cli.gpu_policy"),
        QStringLiteral("policy")
    ));
    parser.addOption(QCommandLineOption(
        QStringLiteral("gpu-adapter-luid"),
        qtTrId("cli.gpu_adapter"),
        QStringLiteral("luid")
    ));
}

QString currentExceptionDetail()
{
    try {
        throw;
    } catch (const std::bad_alloc&) {
        return QStringLiteral("std::bad_alloc");
    } catch (const std::exception& ex) {
        const QString what = QString::fromUtf8(ex.what()).trimmed();
        return what.isEmpty() ? QStringLiteral("std::exception") : what;
    } catch (...) {
        return qtTrId("cli.exception_unknown");
    }
}

}  // namespace miacode::app::entry
