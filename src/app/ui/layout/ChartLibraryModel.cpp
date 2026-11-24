#include "app/ui/layout/ChartLibraryModel.h"

#include "app/services/ChartWorkspaceFileService.h"
#include "app/services/PreferenceDocument.h"
#include "core/chart/ChartAssetPaths.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QtConcurrentRun>
#include <algorithm>
#include <utility>

namespace miacode::ui {
namespace {
constexpr auto directoryPreference = "chart_library_directory";

class ChartLibraryCoverProvider final : public QQuickImageProvider
{
public:
    ChartLibraryCoverProvider() : QQuickImageProvider(Image) {}

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const qsizetype separator = id.indexOf(u'/');
        if (separator < 0) return {};

        bool radiusValid = false;
        const qreal radiusRatio = id.left(separator).toDouble(&radiusValid);
        const auto path = QByteArray::fromBase64(id.mid(separator + 1).toLatin1(), QByteArray::Base64UrlEncoding);
        if (!radiusValid || !(radiusRatio >= 0 && radiusRatio <= 0.5) || path.isEmpty()) return {};

        const QSize targetSize = !requestedSize.isEmpty() ? requestedSize : QSize(96, 96);
        QImageReader reader(QString::fromUtf8(path));
        const QSize originalSize = reader.size();
        if (!originalSize.isEmpty()) {
            const int side = std::min(originalSize.width(), originalSize.height());
            reader.setClipRect(QRect((originalSize.width() - side) / 2, (originalSize.height() - side) / 2, side, side));
            reader.setScaledSize(targetSize);
        }
        QImage cover = reader.read();
        if (cover.isNull()) return {};
        if (cover.size() != targetSize) {
            const int side = std::min(cover.width(), cover.height());
            cover = cover.copy((cover.width() - side) / 2, (cover.height() - side) / 2, side, side)
                .scaled(targetSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }

        QImage rounded(targetSize, QImage::Format_ARGB32_Premultiplied);
        rounded.fill(Qt::transparent);
        QPainter painter(&rounded);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(cover));
        const qreal radius = radiusRatio * targetSize.width();
        painter.drawRoundedRect(QRectF(rounded.rect()), radius, radius);
        painter.end();
        if (size) *size = rounded.size();
        return rounded;
    }
};

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
        QString songId;
        for (const auto& field : document.extraFields) {
            if (field.key.compare(QLatin1String("shortid"), Qt::CaseInsensitive) == 0)
                songId = field.value.trimmed();
        }
        if (songId.toUInt() > 0)
            songId = songId.rightJustified(6, u'0');
        const QString cover = chart_assets::resolveBackgroundMediaPath(chartPath, /*includeVideoCandidates=*/false);
        result.songs.append(QVariantMap{
            {QStringLiteral("title"), document.title.trimmed().isEmpty() ? folder.fileName() : document.title.trimmed()},
            {QStringLiteral("artist"), document.artist.trimmed()},
            {QStringLiteral("folder"), folder.fileName()},
            {QStringLiteral("songId"), songId},
            {QStringLiteral("chart"), QUrl::fromLocalFile(chartPath)},
            {QStringLiteral("pathKey"), ChartLibraryModel::chartPathKey(chartPath)},
            {QStringLiteral("coverKey"), QString::fromLatin1(cover.toUtf8().toBase64(
                QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))},
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

void ChartLibraryModel::registerCoverProvider(QQmlEngine& engine)
{
    engine.addImageProvider(QStringLiteral("chartlibrarycover"), new ChartLibraryCoverProvider);
}

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

QUrl ChartLibraryModel::pickerDirectory(const QString& chartPath) const
{
    if (!directory_.isEmpty()) return directory_;
    const QFileInfo chart(chartPath);
    if (chartPath.isEmpty() || !chart.isAbsolute()) return {};
    QDir parent = chart.absoluteDir();
    parent.cdUp();
    return QUrl::fromLocalFile(parent.absolutePath());
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
