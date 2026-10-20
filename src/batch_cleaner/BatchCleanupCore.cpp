#include "batch_cleaner/BatchCleanupCore.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QStringConverter>

#include <algorithm>
#include <vector>

#include "core/chart/ChartAssetPaths.h"
#include "core/chart/document/SimaiDocument.h"
#include "core/chart/document/SimaiTimingMetadata.h"
#include "core/chart/parser/SimaiCommentScan.h"
#include "core/chart/parser/SimaiParser.h"

namespace miacode::batch_cleaner {
namespace {

const QString kMaidataFileName = QStringLiteral("maidata.txt");

QString trimTrailingSpace(const QString& text)
{
    int end = text.size();
    while (end > 0 && text.at(end - 1).isSpace()) {
        --end;
    }
    return text.left(end);
}

// Canonical form for comparisons. A path that does not exist yet resolves
// through its nearest existing ancestor, so symlinked parents still compare
// equal to the canonical paths of existing charts.
QString identityPath(const QString& path)
{
    QString pending = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    QStringList tail;
    while (true) {
        const QString canonical = QFileInfo(pending).canonicalFilePath();
        if (!canonical.isEmpty()) {
            tail.prepend(canonical);
            return QDir::cleanPath(tail.join(QLatin1Char('/')));
        }
        const int slash = pending.lastIndexOf(QLatin1Char('/'));
        if (slash <= 0) {
            return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        }
        tail.prepend(pending.mid(slash + 1));
        pending = pending.left(slash);
    }
}

bool isSameOrUnder(const QString& path, const QString& root)
{
    const Qt::CaseSensitivity cs =
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
        Qt::CaseInsensitive;
#else
        Qt::CaseSensitive;
#endif
    return path.compare(root, cs) == 0
        || path.startsWith(root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/'), cs);
}

QString collisionKey(const QString& path)
{
    return QDir::cleanPath(path).toLower();
}

bool copyReplacing(const QString& source, const QString& target)
{
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        return false;
    }
    if (QFileInfo::exists(target) && !QFile::remove(target)) {
        return false;
    }
    return QFile::copy(source, target);
}

struct MediaCopy {
    QString source;
    QString relative;
};

// Mirrors ChartWorkspaceFileService's Save As staging so the copy resolves the
// same media as the source folder.
QVector<MediaCopy> planMediaCopies(const QDir& source, SimaiDocument* document)
{
    QVector<MediaCopy> copies;
    const auto add = [&copies](const QString& path, const QString& relative) {
        for (const MediaCopy& copy : copies) {
            if (copy.relative.compare(relative, Qt::CaseInsensitive) == 0) {
                return;
            }
        }
        copies.append({path, relative});
    };

    const QStringList candidates = chart_assets::trackCandidateFileNames()
        + chart_assets::backgroundMediaCandidateFileNames();
    for (const QString& name : candidates) {
        const QString path = source.filePath(name);
        if (QFileInfo(path).isFile()) {
            add(path, name);
        }
    }
    const QDir settings(source.filePath(QStringLiteral(".miacode")));
    for (const QFileInfo& file : settings.entryInfoList(QDir::Files | QDir::Hidden)) {
        add(file.absoluteFilePath(), QStringLiteral(".miacode/") + file.fileName());
    }

    const QString reference = document->videoPath.trimmed();
    if (!reference.isEmpty()) {
        const QString path = QFileInfo(reference).isAbsolute() ? reference : source.absoluteFilePath(reference);
        const QFileInfo media(path);
        if (media.isFile()) {
            QString relative = source.relativeFilePath(media.absoluteFilePath());
            if (relative == QLatin1String("..") || relative.startsWith(QLatin1String("../"))
                || QDir::isAbsolutePath(relative)) {
                relative = QStringLiteral("media/") + media.fileName();
            }
            add(media.absoluteFilePath(), relative);
            document->videoPath = relative;
        }
    }
    return copies;
}

// Normalization rewrites the same rational times through a different path, so
// equal times can differ by float noise; compare within this tolerance.
constexpr double kTimeToleranceSeconds = 1e-6;

bool sameTime(double a, double b)
{
    return qAbs(a - b) <= kTimeToleranceSeconds;
}

// Everything a player sees: timing, lanes, note kinds, modifiers and the HS
// each note was emitted with.
bool sameNote(const TimelineNoteMarker& a, const TimelineNoteMarker& b)
{
    return sameTime(a.second, b.second) && sameTime(a.endSecond, b.endSecond)
        && sameTime(a.slideTraceSecond, b.slideTraceSecond) && a.lane == b.lane && a.endLane == b.endLane
        && a.type == b.type && a.slideTrackKey == b.slideTrackKey && a.touchPad == b.touchPad
        && a.isBreak == b.isBreak && a.isEx == b.isEx && a.isMine == b.isMine && a.isFirework == b.isFirework
        && a.trackBreak == b.trackBreak && a.trackMine == b.trackMine && a.headMine == b.headMine
        && a.hasHeadStar == b.hasHeadStar && a.headlessImmediate == b.headlessImmediate
        && qAbs(a.hsMultiplier - b.hsMultiplier) <= 1e-9;
}

// Order-independent: notes sharing a moment may come out in another order.
bool sameNotes(const QString& before, const QString& after, const miacode::simai::SimaiTimingMetadata& timing)
{
    const SimaiParseResult original = SimaiParser::parseForTimeline(before, timing);
    const SimaiParseResult normalized = SimaiParser::parseForTimeline(after, timing);
    if (original.errors.size() != normalized.errors.size()
        || original.noteMarkers.size() != normalized.noteMarkers.size()) {
        return false;
    }
    const auto bySecond = [](const TimelineNoteMarker& a, const TimelineNoteMarker& b) { return a.second < b.second; };
    QVector<TimelineNoteMarker> left = original.noteMarkers;
    QVector<TimelineNoteMarker> right = normalized.noteMarkers;
    std::sort(left.begin(), left.end(), bySecond);
    std::sort(right.begin(), right.end(), bySecond);
    QVector<bool> used(right.size(), false);
    qsizetype windowStart = 0;
    for (const TimelineNoteMarker& note : std::as_const(left)) {
        while (windowStart < right.size() && right.at(windowStart).second < note.second - kTimeToleranceSeconds) {
            ++windowStart;
        }
        bool matched = false;
        for (qsizetype j = windowStart;
             j < right.size() && right.at(j).second <= note.second + kTimeToleranceSeconds; ++j) {
            if (!used.at(j) && sameNote(note, right.at(j))) {
                used[j] = true;
                matched = true;
                break;
            }
        }
        if (!matched) {
            return false;
        }
    }
    return true;
}

int countComments(const SimaiDocument& document)
{
    int count = 0;
    for (int id : document.difficultyIds()) {
        count += stripChartComments(document.difficulty(id)->chart).removedComments;
    }
    return count;
}

}  // namespace

CommentStripResult stripChartComments(const QString& chart, bool removeTimeSignatures)
{
    CommentStripResult result;
    const QStringList lines = chart.split(QLatin1Char('\n'));
    QStringList kept;
    kept.reserve(lines.size());
    for (const QString& rawLine : lines) {
        const bool hasCr = rawLine.endsWith(QLatin1Char('\r'));
        const QString line = hasCr ? rawLine.chopped(1) : rawLine;
        const int marker = miacode::simai::commentStartForPosition(line, line.size());
        if (marker < 0) {
            kept.append(rawLine);
            continue;
        }
        int numerator = 0;
        int denominator = 0;
        if (miacode::simai::parseInlineTimeSignatureComment(line, marker, &numerator, &denominator)) {
            if (!removeTimeSignatures) {
                kept.append(rawLine);
                continue;
            }
            ++result.removedTimeSignatures;
        } else {
            ++result.removedComments;
        }
        const QString content = trimTrailingSpace(line.left(marker));
        if (content.trimmed().isEmpty()) {
            continue;
        }
        kept.append(hasCr ? content + QLatin1Char('\r') : content);
    }
    result.text = kept.join(QLatin1Char('\n'));
    return result;
}

DocumentCleanupReport cleanupDocument(SimaiDocument* document, const CleanupOptions& options)
{
    DocumentCleanupReport report;
    const bool removeTimeSignatures = options.stripComments && options.stripTimeSignatures;
    if (removeTimeSignatures) {
        const auto before = document->extraFields.size();
        document->extraFields.erase(
            std::remove_if(document->extraFields.begin(), document->extraFields.end(),
                           [](const SimaiRawField& field) {
                               return field.key.trimmed().compare(QStringLiteral("whole_time_signature"),
                                                                  Qt::CaseInsensitive) == 0;
                           }),
            document->extraFields.end());
        report.removedTimeSignatures += int(before - document->extraFields.size());
    }
    const miacode::simai::SimaiTimingMetadata timing = miacode::simai::buildTimingMetadata(*document);

    for (int id : document->difficultyIds()) {
        SimaiDifficultyData* difficulty = document->difficulty(id);
        DifficultyReport entry;
        entry.id = id;
        if (options.stripComments) {
            const CommentStripResult stripped = stripChartComments(difficulty->chart, removeTimeSignatures);
            difficulty->chart = stripped.text;
            entry.removedComments = stripped.removedComments;
            report.removedComments += stripped.removedComments;
            report.removedTimeSignatures += stripped.removedTimeSignatures;
        }
        if (options.normalize && !difficulty->chart.trimmed().isEmpty()) {
            // Same call the editor makes for 整理全谱 (an empty selection).
            const QString chart = difficulty->chart;
            const auto normalized = miacode::chart_transform::normalizeChartSelectionText(
                chart, 0, chart.size(), timing, options.normalization);
            QString candidate = normalized.ok
                ? miacode::chart_transform::composeNormalizedSelectionReplacement(
                      chart, 0, chart.size(), normalized.text)
                : QString();
            if (normalized.ok && removeTimeSignatures) {
                // Normalization re-emits meter comments for measures it splits.
                candidate = stripChartComments(candidate, true).text;
            }
            if (normalized.ok && sameNotes(chart, candidate, timing)) {
                difficulty->chart = candidate;
                entry.normalized = true;
            } else {
                entry.normalizeError = normalized.ok
                    ? QStringLiteral("整理结果会改变音符（时间、HS 或属性），已保留原排版")
                    : normalized.errorMessage;
                report.warnings.append(QStringLiteral("%1：%2")
                                           .arg(SimaiDocument::difficultyName(id), entry.normalizeError));
            }
        }
        report.difficulties.append(entry);
    }

    const QString designer = options.designer.trimmed();
    if (options.unifyDesigner && !designer.isEmpty()) {
        if (document->designer != designer) {
            document->designer = designer;
            ++report.designerChanges;
        }
        for (int id : document->difficultyIds()) {
            if (document->difficulty(id)->chart.trimmed().isEmpty()) {
                continue;
            }
            if (document->designerForSlot(id) != designer) {
                document->setDesignerForSlot(id, designer);
                ++report.designerChanges;
            }
        }
    }
    return report;
}

QString decodeChartBytes(const QByteArray& bytes, bool* usedSystemEncoding)
{
    if (usedSystemEncoding != nullptr) {
        *usedSystemEncoding = false;
    }
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(bytes.mid(3));
    }
    QStringDecoder utf8Decoder(QStringConverter::Utf8);
    const QString text = utf8Decoder.decode(bytes);
    if (!utf8Decoder.hasError()) {
        return text;
    }
    if (usedSystemEncoding != nullptr) {
        *usedSystemEncoding = true;
    }
    QStringDecoder systemDecoder(QStringConverter::System);
    return systemDecoder.decode(bytes);
}

JobCollection collectJobs(const QStringList& inputs, const QString& outputRoot)
{
    JobCollection collection;
    const QString root = identityPath(outputRoot);
    const QString outputBase = QDir::cleanPath(QFileInfo(outputRoot).absoluteFilePath());
    QSet<QString> seenCharts;
    QSet<QString> usedOutputs;

    const auto addChart = [&](const QString& chartPath, const QString& relativeOutput) {
        const QString chart = identityPath(chartPath);
        if (seenCharts.contains(collisionKey(chart))) {
            return;
        }
        seenCharts.insert(collisionKey(chart));
        const QString sourceDir = QFileInfo(chart).absolutePath();
        if (collisionKey(sourceDir) == collisionKey(root)) {
            collection.errors.append(QStringLiteral("输出目录与源目录相同，已跳过：%1").arg(chart));
            return;
        }
        if (isSameOrUnder(chart, root)) {
            return;  // a previous run's output inside an input folder
        }
        const QString base = QDir(outputBase).filePath(relativeOutput);
        QString outputDir = base;
        for (int suffix = 2; usedOutputs.contains(collisionKey(outputDir)); ++suffix) {
            outputDir = QStringLiteral("%1 (%2)").arg(base).arg(suffix);
        }
        if (collisionKey(identityPath(outputDir)) == collisionKey(sourceDir)) {
            collection.errors.append(QStringLiteral("输出目录与源目录相同，已跳过：%1").arg(chart));
            return;
        }
        usedOutputs.insert(collisionKey(outputDir));
        collection.jobs.append({chart, sourceDir, QDir::cleanPath(outputDir)});
    };

    for (const QString& input : inputs) {
        const QFileInfo info(input);
        if (!info.exists()) {
            collection.errors.append(QStringLiteral("路径不存在：%1").arg(input));
            continue;
        }
        if (info.isFile()) {
            if (!chart_assets::isChartDocumentPath(info.absoluteFilePath())) {
                collection.errors.append(QStringLiteral("不是谱面文件：%1").arg(input));
                continue;
            }
            addChart(info.absoluteFilePath(), info.absoluteDir().dirName());
            continue;
        }
        const QDir directory(identityPath(info.absoluteFilePath()));
        QStringList charts;
        QDirIterator it(directory.absolutePath(), {kMaidataFileName}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            charts.append(it.next());
        }
        std::sort(charts.begin(), charts.end());
        const int before = collection.jobs.size();
        for (const QString& chart : charts) {
            const QString relative = directory.relativeFilePath(QFileInfo(chart).absolutePath());
            addChart(chart, relative == QLatin1String(".") ? directory.dirName() : relative);
        }
        if (collection.jobs.size() == before && charts.isEmpty()) {
            collection.errors.append(QStringLiteral("目录中没有 maidata.txt：%1").arg(input));
        }
    }
    return collection;
}

QString defaultOutputRoot(const QStringList& inputs)
{
    for (const QString& input : inputs) {
        const QFileInfo info(input);
        if (!info.exists()) {
            continue;
        }
        const QDir folder = info.isDir() ? QDir(info.absoluteFilePath()) : info.absoluteDir();
        QDir parent = folder;
        parent.cdUp();
        return QDir::cleanPath(parent.filePath(folder.dirName() + QStringLiteral("_cleaned")));
    }
    return QDir::cleanPath(QDir::current().filePath(QStringLiteral("cleaned")));
}

LineDiff diffLines(const QStringList& left, const QStringList& right)
{
    LineDiff diff;
    const qsizetype n = left.size();
    const qsizetype m = right.size();
    diff.removed.fill(true, n);
    diff.added.fill(true, m);

    // Trim the common head and tail so the table only covers the edited span.
    qsizetype head = 0;
    while (head < n && head < m && left.at(head) == right.at(head)) {
        diff.removed[head] = diff.added[head] = false;
        ++head;
    }
    qsizetype tail = 0;
    while (tail < n - head && tail < m - head && left.at(n - 1 - tail) == right.at(m - 1 - tail)) {
        diff.removed[n - 1 - tail] = diff.added[m - 1 - tail] = false;
        ++tail;
    }
    const qsizetype rows = n - head - tail;
    const qsizetype cols = m - head - tail;
    if (rows == 0 || cols == 0) {
        return diff;
    }
    constexpr qsizetype kMaxCells = 16 * 1024 * 1024;
    if (rows * cols > kMaxCells) {
        for (qsizetype i = 0; i < qMin(rows, cols); ++i) {
            const bool same = left.at(head + i) == right.at(head + i);
            diff.removed[head + i] = diff.added[head + i] = !same;
        }
        return diff;
    }

    std::vector<quint32> table(static_cast<size_t>((rows + 1) * (cols + 1)), 0);
    const auto at = [&](qsizetype i, qsizetype j) -> quint32& {
        return table[static_cast<size_t>(i * (cols + 1) + j)];
    };
    for (qsizetype i = rows - 1; i >= 0; --i) {
        for (qsizetype j = cols - 1; j >= 0; --j) {
            at(i, j) = left.at(head + i) == right.at(head + j) ? at(i + 1, j + 1) + 1
                                                               : qMax(at(i + 1, j), at(i, j + 1));
        }
    }
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < rows && j < cols) {
        if (left.at(head + i) == right.at(head + j)) {
            diff.removed[head + i] = diff.added[head + j] = false;
            ++i;
            ++j;
        } else if (at(i + 1, j) >= at(i, j + 1)) {
            ++i;
        } else {
            ++j;
        }
    }
    return diff;
}

JobResult processJob(const ChartJob& job, const CleanupOptions& options, bool write)
{
    JobResult result;
    QFile file(job.chartPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.error = QStringLiteral("无法读取谱面：%1").arg(file.errorString());
        return result;
    }
    bool usedSystemEncoding = false;
    result.originalText = decodeChartBytes(file.readAll(), &usedSystemEncoding);
    file.close();

    SimaiDocument document = SimaiDocument::fromText(result.originalText);
    result.title = document.title.trimmed().isEmpty() ? QFileInfo(job.sourceDir).fileName() : document.title.trimmed();
    result.difficultyCount = document.difficultyIds().size();
    result.sourceCommentCount = countComments(document);
    result.report = cleanupDocument(&document, options);
    if (usedSystemEncoding) {
        result.report.warnings.prepend(QStringLiteral("源文件不是 UTF-8，已按系统编码读取并以 UTF-8 写出"));
    }
    const QVector<MediaCopy> copies = planMediaCopies(QDir(job.sourceDir), &document);
    result.cleanedText = document.toText();
    result.status = result.cleanedText == result.originalText ? JobResult::Status::Unchanged
                                                              : JobResult::Status::Changed;
    if (!write) {
        return result;
    }

    const QDir target(job.outputDir);
    if (!QDir().mkpath(target.absolutePath())) {
        result.status = JobResult::Status::Failed;
        result.error = QStringLiteral("无法创建输出目录：%1").arg(job.outputDir);
        return result;
    }
    for (const MediaCopy& copy : copies) {
        if (!copyReplacing(copy.source, target.filePath(copy.relative))) {
            result.status = JobResult::Status::Failed;
            result.error = QStringLiteral("复制资源失败：%1").arg(copy.relative);
            return result;
        }
        result.copiedFiles.append(copy.relative);
    }
    QSaveFile output(target.filePath(QFileInfo(job.chartPath).fileName()));
    const QByteArray payload = QStringEncoder(QStringConverter::Utf8).encode(result.cleanedText);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text) || output.write(payload) != payload.size()
        || !output.commit()) {
        result.status = JobResult::Status::Failed;
        result.error = QStringLiteral("写入谱面失败：%1").arg(output.errorString());
        return result;
    }
    result.written = true;
    return result;
}

}  // namespace miacode::batch_cleaner
