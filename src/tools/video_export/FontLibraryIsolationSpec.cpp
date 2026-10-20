#include "export/video_export/FontLibrary.h"
#include "core/scene/PreviewHudState.h"
#include "common/DebugLog.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QTextStream>

#include <atomic>
#include <thread>
#include <vector>

namespace {
bool require(bool condition, const char* label, QTextStream& err)
{
    if (!condition) err << "FAIL: " << label << Qt::endl;
    return condition;
}
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    using namespace miacode::video_export;
    using namespace miacode::preview::scene;
    const QString fontA = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/XiaolaiMono-Regular.subset.ttf");
    const QString fontB = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf");
    const QString previousCwd = QDir::currentPath();
    QDir::setCurrent(temporary.path());
    bool ok = require(QFile::copy(fontA, temporary.filePath(QStringLiteral("cwd.ttf"))), "copy cwd fixture", err);
    const auto noDirectory = fontLibraryEntries({}, true, QStringLiteral("Default"));
    ok &= require(noDirectory.size() == 1 && noDirectory.first().path.isEmpty(), "empty directory never scans cwd", err);
    ok &= require(importFontFileIntoLibrary(fontA, {}).failure == FontImportFailure::CopyFailed
        && !QFile::exists(temporary.filePath(QStringLiteral("fonts"))), "empty directory import never creates cwd fonts", err);
    QDir().mkpath(QStringLiteral("relative"));
    QFile::copy(fontA, temporary.filePath(QStringLiteral("relative/fixture.ttf")));
    ok &= require(fontLibraryEntries(QStringLiteral("relative")).isEmpty(), "relative directory never scans cwd", err);
    ok &= require(importFontFileIntoLibrary(fontB, QStringLiteral("relative")).failure == FontImportFailure::CopyFailed
        && QDir(QStringLiteral("relative")).entryList(QDir::Files).size() == 1, "relative directory import never writes", err);

    const QString dirA = temporary.filePath(QStringLiteral("library-a"));
    const QString dirB = temporary.filePath(QStringLiteral("library-b"));
    const auto importedA = importFontFileIntoLibrary(fontA, dirA);
    const auto importedB = importFontFileIntoLibrary(fontB, dirB);
    ok &= require(importedA.failure == FontImportFailure::None && importedB.failure == FontImportFailure::None, "explicit libraries import successfully", err);
    const auto entriesA = fontLibraryEntries(dirA);
    const auto entriesB = fontLibraryEntries(dirB);
    ok &= require(entriesA.size() == 1 && entriesB.size() == 1
        && entriesA.first().path == importedA.path && entriesB.first().path == importedB.path,
        "different directory caches remain isolated", err);

    const QString changing = temporary.filePath(QStringLiteral("changing.ttf"));
    ok &= require(QFile::copy(fontA, changing), "copy changing font", err);
    const QString familyA = previewHudFontFamilyForFile(changing);
    ok &= require(!familyA.isEmpty() && fontFamilyForFile(changing) == familyA, "HUD and library share resolver", err);
    QFile::remove(changing);
    ok &= require(previewHudFontFamilyForFile(changing).isEmpty(), "removed file invalidates cached family", err);
    QFile::copy(fontB, changing);
    const QString familyB = previewHudFontFamilyForFile(changing);
    ok &= require(!familyB.isEmpty() && familyB != familyA, "replacement reloads new family after a cached failure", err);
    QFile::remove(changing);
    QFile invalid(changing);
    invalid.open(QIODevice::WriteOnly);
    invalid.write("not a font");
    invalid.close();
    ok &= require(fontFamilyForFile(changing).isEmpty(), "invalid replacement cannot reuse cached family", err);
    QFile::remove(changing);
    QFile::copy(fontA, changing);
    ok &= require(fontFamilyForFile(changing) == familyA, "valid replacement recovers after failed parsing", err);

    std::atomic<bool> consistent{true};
    std::vector<std::thread> readers;
    for (int thread = 0; thread < 4; ++thread) readers.emplace_back([&] {
        for (int repeat = 0; repeat < 100; ++repeat) {
            if (previewHudFontFamilyForFile(changing) != familyA) consistent.store(false);
        }
    });
    for (auto& reader : readers) reader.join();
    ok &= require(consistent.load(), "concurrent UI/render cache reads remain consistent", err);
    QDir::setCurrent(previousCwd);
    miacode::debug_log::shutdownAsyncLogWriter();
    if (ok) QTextStream(stdout) << "font_library_isolation_spec ok" << Qt::endl;
    return ok ? 0 : 1;
}
