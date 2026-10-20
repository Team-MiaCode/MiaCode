#include "export/video_export/FontLibrary.h"

#include "core/scene/PreviewHudState.h"
#include "common/DebugLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace miacode::video_export {

QString fontFamilyForFile(const QString& path)
{
    return miacode::preview::scene::previewHudFontFamilyForFile(path);
}

QVector<FontLibraryEntry> fontLibraryEntries(const QString& libraryDirectory, bool includeDefault, const QString& defaultLabel)
{
    const bool validDirectory = !libraryDirectory.isEmpty() && QDir::isAbsolutePath(libraryDirectory);
    QDir dir(libraryDirectory);
    const QFileInfoList files = !validDirectory ? QFileInfoList{} : dir.entryInfoList(
        QStringList{QStringLiteral("*.ttf"), QStringLiteral("*.otf")},
        QDir::Files | QDir::Readable,
        QDir::Name | QDir::IgnoreCase
    );

    // The QML export page asks for the same model from several independent
    // combo-box bindings. Keep the directory enumeration as the cheap change
    // detector, but avoid rebuilding and re-registering the same entry list
    // for every getter call. mtime/size invalidates the cache after an import
    // or an edited library file; the localized default label is part of the
    // key because the application can switch languages without restarting in
    // tests and embedded shells. The entry-list model is used on the GUI thread;
    // font registration itself is shared with rendering and synchronized.
    QString signature = validDirectory ? dir.absolutePath() : QString();
    signature.reserve(files.size() * 48);
    for (const QFileInfo& file : files) {
        signature += file.absoluteFilePath();
        signature += QLatin1Char('\0');
        signature += QString::number(file.lastModified().toMSecsSinceEpoch());
        signature += QLatin1Char(':');
        signature += QString::number(file.size());
        signature += QLatin1Char('\n');
    }
    static QString cachedSignature;
    static QString cachedDefaultLabel;
    static bool cachedIncludeDefault = false;
    static QVector<FontLibraryEntry> cachedEntries;
    if (cachedSignature == signature && cachedIncludeDefault == includeDefault
        && cachedDefaultLabel == defaultLabel) {
        return cachedEntries;
    }

    QVector<FontLibraryEntry> entries;
    if (includeDefault) {
        entries.push_back({
            defaultLabel.isEmpty() ? QStringLiteral("Default font") : defaultLabel,
            QString(),
            QString()
        });
    }
    for (const QFileInfo& file : files) {
        const QString path = file.absoluteFilePath();
        const QString family = fontFamilyForFile(path);
        if (family.isEmpty()) {
            continue;
        }
        entries.push_back({
            QStringLiteral("%1 (%2)").arg(family, file.fileName()),
            path,
            family
        });
    }
    cachedSignature = signature;
    cachedIncludeDefault = includeDefault;
    cachedDefaultLabel = defaultLabel;
    cachedEntries = entries;
    return entries;
}

namespace {

QString uniqueFontLibraryPath(const QFileInfo& sourceInfo, const QString& libraryDirectory)
{
    QDir dir(libraryDirectory);
    if (!dir.mkpath(QStringLiteral("."))) {
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
            QStringLiteral("font_library_import_failed"),
            QStringLiteral("path=%1 reason=create-directory-failed").arg(libraryDirectory),
            true, miacode::debug_log::Level::Warn);
        return {};
    }
    const QString baseName = sourceInfo.completeBaseName().isEmpty()
        ? QStringLiteral("font")
        : sourceInfo.completeBaseName();
    const QString suffix = sourceInfo.suffix().isEmpty() ? QStringLiteral("ttf") : sourceInfo.suffix();
    QString candidate = dir.filePath(baseName + QLatin1Char('.') + suffix);
    int copyIndex = 2;
    while (QFileInfo::exists(candidate)) {
        candidate = dir.filePath(QStringLiteral("%1_%2.%3").arg(baseName).arg(copyIndex).arg(suffix));
        ++copyIndex;
    }
    return QFileInfo(candidate).absoluteFilePath();
}

}  // namespace

FontImportResult importFontFileIntoLibrary(const QString& sourcePath, const QString& libraryDirectory)
{
    if (libraryDirectory.isEmpty() || !QDir::isAbsolutePath(libraryDirectory)) {
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
            QStringLiteral("font_library_directory_unavailable"),
            QStringLiteral("path=%1 reason=absolute-directory-required").arg(libraryDirectory),
            true, miacode::debug_log::Level::Warn);
        return {{}, FontImportFailure::CopyFailed};
    }
    const QFileInfo sourceInfo(sourcePath);
    const QString suffix = sourceInfo.suffix().toLower();
    if (!sourceInfo.isFile() || (suffix != QStringLiteral("ttf") && suffix != QStringLiteral("otf"))) {
        return {{}, FontImportFailure::NotFontFile};
    }
    if (fontFamilyForFile(sourceInfo.absoluteFilePath()).isEmpty()) {
        return {{}, FontImportFailure::InvalidFont};
    }

    const QDir libraryDir(libraryDirectory);
    if (sourceInfo.absoluteDir() == libraryDir) {
        return {sourceInfo.absoluteFilePath(), FontImportFailure::None};
    }

    const QString targetPath = uniqueFontLibraryPath(sourceInfo, libraryDirectory);
    if (targetPath.isEmpty()) return {{}, FontImportFailure::CopyFailed};
    QFile sourceFile(sourceInfo.absoluteFilePath());
    if (!sourceFile.copy(targetPath)) {
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
            QStringLiteral("font_library_import_failed"),
            QStringLiteral("source=%1 path=%2 reason=%3")
                .arg(sourceInfo.absoluteFilePath(), targetPath, sourceFile.errorString()),
            true, miacode::debug_log::Level::Warn);
        return {{}, FontImportFailure::CopyFailed};
    }
    return {targetPath, FontImportFailure::None};
}

void applyBannerFontOverride(QVariantMap& templateMap,
                             const QString& displayPath,
                             const QString& bodyPath)
{
    QVariantMap fonts = templateMap.value(QStringLiteral("fonts")).toMap();
    if (!displayPath.isEmpty() && QFileInfo::exists(displayPath)) {
        fonts.insert(QStringLiteral("display"), QUrl::fromLocalFile(displayPath).toString());
    }
    if (!bodyPath.isEmpty() && QFileInfo::exists(bodyPath)) {
        fonts.insert(QStringLiteral("body"), QUrl::fromLocalFile(bodyPath).toString());
    }
    templateMap.insert(QStringLiteral("fonts"), fonts);
}

}  // namespace miacode::video_export
