#include "app/ui/layout/ChartLibraryModel.h"

#include "app/services/ChartWorkspaceFileService.h"
#include "app/services/PreferenceDocument.h"
#include "core/chart/ChartAssetPaths.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QHash>
#include <QtConcurrentRun>
#include <algorithm>
#include <utility>

namespace miacode::ui {
namespace {
constexpr auto directoryPreference = "chart_library_directory";

struct ScanResult {
    QVariantList songs;
    bool directoryMissing = false;
};

ScanResult scanLibrary(const QString& path)
{
    const QDir root(path);
    if (!root.exists()) return {{}, true};
    ScanResult result;
    for (const QFileInfo& folder : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile chart(QDir(folder.absoluteFilePath()).filePath(QStringLiteral("maidata.txt")));
        if (!chart.open(QIODevice::ReadOnly)) continue;
        const auto decoded = ChartWorkspaceFileService::decodeDocumentText(chart.readAll());
        if (!decoded || chart.error() != QFileDevice::NoError) continue;
        const SimaiDocument document = SimaiDocument::fromText(*decoded);
        const QString chartPath = chart.fileName();
        QVariantList difficulties;
        for (int id : document.difficultyIds()) {
            const SimaiDifficultyData* difficulty = document.difficulty(id);
            difficulties.append(QVariantMap{{QStringLiteral("id"), id},
                {QStringLiteral("shortName"), SimaiDocument::difficultyShortName(id)},
                {QStringLiteral("level"), difficulty->level}});
        }
        QHash<QString, QString> metadata;
        for (const auto& field : document.extraFields)
            metadata.insert(field.key.toLower(), field.value.trimmed());

        QString songId = metadata.value(QStringLiteral("shortid"));
        if (songId.toUInt() > 0)
            songId = songId.rightJustified(6, u'0');
        const QString cover = chart_assets::resolveBackgroundMediaPath(chartPath, /*includeVideoCandidates=*/false);
        result.songs.append(QVariantMap{
            {QStringLiteral("title"), document.title.trimmed().isEmpty() ? folder.fileName() : document.title.trimmed()},
            {QStringLiteral("artist"), document.artist.trimmed()},
            {QStringLiteral("folder"), folder.fileName()},
            {QStringLiteral("songId"), songId},
            {QStringLiteral("kind"), metadata.value(QStringLiteral("cabinet"))},
            {QStringLiteral("chart"), QUrl::fromLocalFile(chartPath)},
            {QStringLiteral("pathKey"), ChartLibraryModel::chartPathKey(chartPath)},
            {QStringLiteral("cover"), chart_assets::displayBackgroundImageUrl(cover)},
            {QStringLiteral("difficulties"), difficulties},
        });
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    const auto title = QStringLiteral("title");
    std::stable_sort(result.songs.begin(), result.songs.end(), [&](const QVariant& a, const QVariant& b) {
        return collator.compare(a.toMap().value(title).toString(), b.toMap().value(title).toString()) < 0;
    });
    const auto songKey = [](const QVariantMap& song) {
        return song.value(QStringLiteral("title")).toString().toCaseFolded()
            + QChar(u'\0') + song.value(QStringLiteral("artist")).toString().toCaseFolded();
    };
    QHash<QString, int> matchingSongs;
    for (const QVariant& entry : std::as_const(result.songs)) ++matchingSongs[songKey(entry.toMap())];
    for (QVariant& entry : result.songs) {
        auto song = entry.toMap();
        if (matchingSongs.value(songKey(song)) > 1) {
            song.insert(QStringLiteral("variant"), song.value(QStringLiteral("folder")));
            entry = std::move(song);
        }
    }
    return result;
}
} // namespace

ChartLibraryModel::ChartLibraryModel(QObject* parent) : QObject(parent)
{
    const auto app = PreferenceDocument::loadPreferencesObject().value(QStringLiteral("app")).toObject();
    const QString path = app.value(QLatin1String(directoryPreference)).toString();
    if (!path.isEmpty()) directory_ = QUrl::fromLocalFile(path);
}

QString ChartLibraryModel::chartPathKey(const QString& path)
{
    if (path.isEmpty()) return {};
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

void ChartLibraryModel::setDirectory(const QUrl& directory)
{
    if (!directory.isLocalFile()) return;
    const QString path = QDir::cleanPath(directory.toLocalFile());
    if (directory_.toLocalFile() == path) {
        refresh();
        return;
    }
    directory_ = QUrl::fromLocalFile(path);
    songs_.clear();
    directoryMissing_ = false;
    auto root = PreferenceDocument::loadPreferencesObject();
    auto app = root.value(QStringLiteral("app")).toObject();
    app.insert(QLatin1String(directoryPreference), path);
    root.insert(QStringLiteral("app"), app);
    PreferenceDocument::savePreferencesObject(root);
    emit directoryChanged();
    emit songsChanged();
    refresh();
}

QString ChartLibraryModel::directoryPath() const
{
    return QDir::toNativeSeparators(directory_.toLocalFile());
}

void ChartLibraryModel::refresh()
{
    if (directory_.isEmpty()) return;
    const quint64 revision = ++scanRevision_;
    if (!scanning_) {
        scanning_ = true;
        emit scanningChanged();
    }
    auto* watcher = new QFutureWatcher<ScanResult>(this);
    connect(watcher, &QFutureWatcher<ScanResult>::finished, this, [this, revision, watcher] {
        watcher->deleteLater();
        if (revision != scanRevision_) return;
        auto result = watcher->result();
        songs_ = std::move(result.songs);
        directoryMissing_ = result.directoryMissing;
        scanning_ = false;
        emit songsChanged();
        emit scanningChanged();
    });
    watcher->setFuture(QtConcurrent::run(scanLibrary, directory_.toLocalFile()));
}

} // namespace miacode::ui
