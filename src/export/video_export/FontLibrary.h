#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

namespace miacode::video_export {

// Portable user font library — the `<preferences dir>/fonts` directory that
// holds imported .ttf/.otf files. Its API is UI-neutral so every QML surface
// can expose the same entries without a QWidget adapter.

struct FontLibraryEntry {
    QString label;   // "Family (file.ttf)"; the default entry uses defaultLabel.
    QString path;    // absolute path in the library; empty == default/none.
    QString family;  // resolved font family; empty for the default entry.
};

// Result of importing one file into the portable font library.  The QML shell
// owns user interaction, so this data-layer operation deliberately has no
// dialog or message-box dependency.
enum class FontImportFailure {
    None,
    NotFontFile,
    InvalidFont,
    CopyFailed,
};

struct FontImportResult {
    QString path;
    FontImportFailure failure = FontImportFailure::None;
};

// The host supplies an absolute library directory. Empty/relative directories
// expose only the default entry and imports fail without touching the cwd.
// Every .ttf/.otf in the library, family-resolved (unreadable files skipped),
// sorted by filename. When `includeDefault` is set a leading
// {defaultLabel, "", ""} entry is prepended (the "use the bundled default"
// choice).
QVector<FontLibraryEntry> fontLibraryEntries(const QString& libraryDirectory,
                                             bool includeDefault = false,
                                             const QString& defaultLabel = QString());

// Resolve/register a font using the synchronized scene cache. Unchanged files
// reuse registrations; replacement/deletion invalidates by mtime and size.
// Requires a live QGuiApplication. Empty on failure.
QString fontFamilyForFile(const QString& path);

// Validate a local .ttf/.otf and copy it into the portable library.  A file
// already in that library is returned unchanged.  Callers present any error
// through their own UI boundary.
FontImportResult importFontFileIntoLibrary(const QString& sourcePath, const QString& libraryDirectory);

// Overlay the user's difficulty-card font choice onto a parsed banner template's
// `fonts` block (keys `display` / `body`). An absolute path is injected as a
// file:// URL string, which MaimaiBannerCard.qml treats as a literal FontLoader
// source; a bare filename (the untouched default) keeps the qrc `fontsRoot`
// behaviour. A missing file is IGNORED (the bundled default stays), so a
// composition carrying a font path that is absent on another machine still
// renders its text instead of going blank.
void applyBannerFontOverride(QVariantMap& templateMap,
                             const QString& displayPath,
                             const QString& bodyPath);

}  // namespace miacode::video_export
