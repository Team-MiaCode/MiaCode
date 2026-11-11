#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/chart/transform/ChartNormalization.h"

class SimaiDocument;

namespace miacode::batch_cleaner {

struct CleanupOptions {
    bool normalize = true;
    miacode::chart_transform::ChartNormalizationOptions normalization = defaultNormalization();
    bool stripComments = true;
    // Only together with stripComments: also drops inline meter comments
    // (`|| 3/4`) and the `&whole_time_signature` field.
    bool stripTimeSignatures = false;
    bool unifyDesigner = true;
    QString designer;

    static miacode::chart_transform::ChartNormalizationOptions defaultNormalization()
    {
        miacode::chart_transform::ChartNormalizationOptions options;
        options.reduceTo384Grid = false;
        return options;
    }
};

struct CommentStripResult {
    QString text;
    int removedComments = 0;
    int removedTimeSignatures = 0;
};

// Drops `||` comments from one difficulty's chart text. Inline meter comments
// (`|| 3/4`) set measure boundaries for the parser and normalization, so they
// stay unless `removeTimeSignatures`. A line that held only a comment is
// removed together with its break.
CommentStripResult stripChartComments(const QString& chart, bool removeTimeSignatures = false);

struct DifficultyReport {
    int id = 0;
    int removedComments = 0;
    bool normalized = false;
    QString normalizeError;
};

struct DocumentCleanupReport {
    QVector<DifficultyReport> difficulties;
    int removedComments = 0;
    int removedTimeSignatures = 0;
    int designerChanges = 0;
    QStringList warnings;
};

// Applies the enabled steps to `document` in order: strip comments (and time
// signatures), normalize, unify designer. Normalization is only kept when the
// parsed notes (timing, lanes, modifiers, HS) are unchanged; otherwise, or when
// it fails, the difficulty keeps its stripped text and reports a warning.
// The designer goes to `&des` and to `&des_N` of difficulties whose chart is
// not empty; chart-less slots keep whatever they had.
DocumentCleanupReport cleanupDocument(SimaiDocument* document, const CleanupOptions& options);

// Same decoding as the main application's chart open: UTF-8 (BOM stripped),
// falling back to the system codec when the bytes are not valid UTF-8.
QString decodeChartBytes(const QByteArray& bytes, bool* usedSystemEncoding = nullptr);

struct ChartJob {
    QString chartPath;
    QString sourceDir;
    QString outputDir;
};

struct JobCollection {
    QVector<ChartJob> jobs;
    QStringList errors;
};

// Expands inputs (chart files, or directories searched recursively for
// maidata.txt) into one job per chart. Each song folder maps to
// `outputRoot/<path relative to the input directory>`; a chart directly in an
// input directory or given as a file maps to `outputRoot/<folder name>`.
// Charts already under `outputRoot` are skipped and colliding targets get a
// numeric suffix.
JobCollection collectJobs(const QStringList& inputs, const QString& outputRoot);

// Default output root next to the first input.
QString defaultOutputRoot(const QStringList& inputs);

struct JobResult {
    enum class Status {
        Unchanged,
        Changed,
        Failed,
    };
    Status status = Status::Failed;
    QString title;
    QString error;
    QString originalText;
    QString cleanedText;
    int difficultyCount = 0;
    int sourceCommentCount = 0;
    DocumentCleanupReport report;
    QStringList copiedFiles;
    bool written = false;
};

// Line-level diff for the preview: `removed[i]` marks left lines absent from
// the right, `added[j]` marks right lines absent from the left. Inputs too
// large for the LCS table fall back to a positional comparison.
struct LineDiff {
    QVector<bool> removed;
    QVector<bool> added;
};
LineDiff diffLines(const QStringList& left, const QStringList& right);

// Reads and cleans one chart. With `write`, creates the output folder, copies
// the song's media (track, bg/pv, &video target, .miacode settings) the way the
// main application's Save As does, then writes the cleaned maidata.txt.
JobResult processJob(const ChartJob& job, const CleanupOptions& options, bool write);

}  // namespace miacode::batch_cleaner
