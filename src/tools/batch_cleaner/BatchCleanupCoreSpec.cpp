#include "batch_cleaner/BatchCleanupCore.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include "core/chart/document/SimaiDocument.h"
#include "core/chart/parser/SimaiParser.h"

namespace {

using namespace miacode::batch_cleaner;

int failures = 0;

void expect(bool condition, const QString& message, const QString& detail = QString())
{
    if (condition) {
        return;
    }
    ++failures;
    QTextStream err(stderr);
    err << "[FAIL] " << message << '\n';
    if (!detail.isEmpty()) {
        err << "  " << detail << '\n';
    }
}

void writeFile(const QString& path, const QByteArray& bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);
    }
}

QByteArray readFile(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

CleanupOptions commentsOnly()
{
    CleanupOptions options;
    options.normalize = false;
    options.unifyDesigner = false;
    return options;
}

void stripsCommentsButKeepsMeter()
{
    const QString chart = QStringLiteral(
        "|| intro\n"
        "(120){4}1,2, || tail note\n"
        "|| 3/4\n"
        "3,4,5,\r\n"
        "  || indented\n"
        "E");
    const CommentStripResult result = stripChartComments(chart);
    expect(result.removedComments == 3, QStringLiteral("three prose comments are removed"),
           QString::number(result.removedComments));
    expect(result.text == QStringLiteral("(120){4}1,2,\n|| 3/4\n3,4,5,\r\nE"),
           QStringLiteral("prose comments go, meter comment and CRLF stay"), result.text);
    expect(stripChartComments(QStringLiteral("1,2,3,\nE")).removedComments == 0,
           QStringLiteral("comment-free chart is untouched"));
    const CommentStripResult noMeter = stripChartComments(chart, true);
    expect(noMeter.removedTimeSignatures == 1 && noMeter.text == QStringLiteral("(120){4}1,2,\n3,4,5,\r\nE"),
           QStringLiteral("meter comment goes when requested"), noMeter.text);
}

void unifiesDesignerOfChartedDifficulties()
{
    SimaiDocument document = SimaiDocument::fromText(QStringLiteral(
        "&title=Song\n&des=Old\n&des_2=A\n&lv_2=5\n&inote_2=(120){4}1,E\n"
        "&lv_3=7\n&des_3=Empty\n&inote_3=\n"
        "&lv_5=13\n&inote_5=(120){4}2,E\n&des_6=Standalone\n"));
    CleanupOptions options = commentsOnly();
    options.unifyDesigner = true;
    options.designer = QStringLiteral(" Mia Team ");
    const DocumentCleanupReport report = cleanupDocument(&document, options);
    expect(document.designer == QStringLiteral("Mia Team"), QStringLiteral("&des is replaced"), document.designer);
    for (int id : {2, 5}) {
        expect(document.designerForSlot(id) == QStringLiteral("Mia Team"),
               QStringLiteral("&des_%1 is replaced").arg(id), document.designerForSlot(id));
    }
    expect(document.designerForSlot(3) == QStringLiteral("Empty"), QStringLiteral("empty chart keeps its designer"));
    expect(document.designerForSlot(6) == QStringLiteral("Standalone"),
           QStringLiteral("chart-less slot keeps its designer"));
    expect(report.designerChanges == 3, QStringLiteral("designer change count"),
           QString::number(report.designerChanges));
}

void stripsTimeSignatures()
{
    SimaiDocument document = SimaiDocument::fromText(QStringLiteral(
        "&title=Song\n&whole_time_signature=3/4\n&lv_5=13\n&inote_5=(120){4}1,2,\n|| 6/8\n3,4,\nE\n"));
    CleanupOptions options = commentsOnly();
    options.stripTimeSignatures = true;
    const DocumentCleanupReport report = cleanupDocument(&document, options);
    const QString text = document.toText();
    expect(report.removedTimeSignatures == 2 && !text.contains(QStringLiteral("time_signature"))
               && !text.contains(QStringLiteral("6/8")),
           QStringLiteral("inline and whole time signatures are removed"), text);
}

// A `{995}` fine offset next to `{12}` needs a common subdivision far above
// the normalizer's cap; it must be rejected promptly and leave the layout.
void rejectsOversizedSubdivision()
{
    const QString fine = QStringLiteral(",").repeated(83);
    const QString chart = QStringLiteral("(193){4}1,2,3,4,\n{8},6/5,7/4,,\n{12}3,\n{995}4") + fine + QStringLiteral("\n{12}3,6,\n{995}5")
        + fine + QStringLiteral("\n{12}6,\n{4}1,2,3,4,\nE");
    SimaiDocument document = SimaiDocument::fromText(QStringLiteral("&lv_4=12\n&inote_4=") + chart);
    const QString before = document.difficulty(4)->chart;
    CleanupOptions options = commentsOnly();
    options.normalize = true;
    QElapsedTimer timer;
    timer.start();
    const DocumentCleanupReport report = cleanupDocument(&document, options);
    expect(timer.elapsed() < 5000, QStringLiteral("oversized subdivision is rejected promptly"),
           QString::number(timer.elapsed()));
    expect(document.difficulty(4)->chart == before && report.warnings.size() == 1,
           QStringLiteral("rejected normalization keeps the layout and warns"), report.warnings.join(QLatin1Char(';')));
}

void normalizeRunsOnStrippedChart()
{
    SimaiDocument document = SimaiDocument::fromText(QStringLiteral(
        "&title=Song\n&lv_5=13\n&inote_5=(120){4}1,2,|| hello\n3,4,\n|| bye\nE\n"));
    CleanupOptions options;
    options.unifyDesigner = false;
    const DocumentCleanupReport report = cleanupDocument(&document, options);
    const QString chart = document.difficulty(5)->chart;
    expect(!chart.contains(QStringLiteral("||")), QStringLiteral("normalized chart has no prose comment"), chart);
    expect(report.difficulties.size() == 1 && report.difficulties.first().normalized,
           QStringLiteral("difficulty is normalized"),
           report.difficulties.isEmpty() ? QString() : report.difficulties.first().normalizeError);
    expect(report.removedComments == 2, QStringLiteral("comment count is reported"),
           QString::number(report.removedComments));
}

// The negative-HS sample makes the normalizer move `<HS*N>` onto other notes;
// the guard must keep the original layout for that difficulty.
void keepsLayoutWhenNormalizationChangesNotes()
{
    const QString original = QString::fromUtf8(readFile(QStringLiteral("samples/negative_hs_demo/maidata.txt")));
    expect(!original.isEmpty(), QStringLiteral("negative HS sample is readable from the source tree"));
    SimaiDocument document = SimaiDocument::fromText(original);
    const QString before = document.difficulty(5)->chart;
    CleanupOptions options;
    options.stripComments = false;
    options.unifyDesigner = false;
    const DocumentCleanupReport report = cleanupDocument(&document, options);
    expect(document.difficulty(5)->chart == before, QStringLiteral("chart layout is kept"));
    expect(report.warnings.size() == 1 && !report.difficulties.first().normalized,
           QStringLiteral("guard reports the rejected normalization"), report.warnings.join(QLatin1Char(';')));
}

void collectsJobsPerSongFolder()
{
    QTemporaryDir temp;
    const QDir root(temp.path());
    writeFile(root.filePath("Songs/A/maidata.txt"), "&title=A\n");
    writeFile(root.filePath("Songs/pack/B/maidata.txt"), "&title=B\n");
    writeFile(root.filePath("Single/maidata.txt"), "&title=S\n");
    writeFile(root.filePath("Songs/out/Old/maidata.txt"), "&title=Old\n");
    const QString output = root.filePath("Songs/out");

    const JobCollection collection = collectJobs(
        {root.filePath("Songs"), root.filePath("Single/maidata.txt"), root.filePath("Single"),
         root.filePath("missing")},
        output);
    expect(collection.jobs.size() == 3, QStringLiteral("three charts, output tree skipped, duplicate merged"),
           QString::number(collection.jobs.size()));
    QStringList outputs;
    for (const ChartJob& job : collection.jobs) {
        outputs.append(QDir(output).relativeFilePath(job.outputDir));
    }
    expect(outputs == QStringList({QStringLiteral("A"), QStringLiteral("pack/B"), QStringLiteral("Single")}),
           QStringLiteral("output folders mirror song folders"), outputs.join(QLatin1Char(',')));
    expect(collection.errors.size() == 1, QStringLiteral("missing input is reported"),
           collection.errors.join(QLatin1Char(';')));

    const JobCollection colliding = collectJobs({root.filePath("Songs/A"), root.filePath("Single")},
                                                root.filePath("Single"));
    expect(colliding.jobs.size() == 1 && !colliding.errors.isEmpty(),
           QStringLiteral("output root equal to a song folder is refused for that song"));

    const JobCollection renamed = collectJobs(
        {root.filePath("Songs/A/maidata.txt"), root.filePath("Songs/pack/B")}, root.filePath("dest"));
    writeFile(root.filePath("Other/A/maidata.txt"), "&title=A2\n");
    const JobCollection twins = collectJobs({root.filePath("Songs/A"), root.filePath("Other/A")},
                                            root.filePath("dest"));
    expect(twins.jobs.size() == 2 && twins.jobs.at(1).outputDir.endsWith(QStringLiteral("A (2)")),
           QStringLiteral("colliding folder names get a suffix"),
           twins.jobs.isEmpty() ? QString() : twins.jobs.last().outputDir);
    expect(renamed.jobs.size() == 2, QStringLiteral("file and folder inputs mix"));
}

void writesCleanCopyWithMedia()
{
    QTemporaryDir temp;
    const QDir root(temp.path());
    const QString song = root.filePath("Songs/A");
    const QByteArray original =
        "\xEF\xBB\xBF&title=A\n&des=Old\n&video=../shared/movie.mp4\n&lv_5=13\n&inote_5=(120){4}1,|| x\nE\n";
    writeFile(song + "/maidata.txt", original);
    writeFile(song + "/track.mp3", "audio");
    writeFile(song + "/bg.png", "image");
    writeFile(song + "/pv.mp4", "video");
    writeFile(song + "/.miacode/project.json", "{}");
    writeFile(song + "/notes.txt", "not media");
    writeFile(root.filePath("Songs/shared/movie.mp4"), "movie");

    const QString output = root.filePath("out");
    const JobCollection collection = collectJobs({song}, output);
    expect(collection.jobs.size() == 1, QStringLiteral("one job"));
    if (collection.jobs.isEmpty()) {
        return;
    }
    CleanupOptions options = commentsOnly();
    options.unifyDesigner = true;
    options.designer = QStringLiteral("Mia");

    const JobResult preview = processJob(collection.jobs.first(), options, false);
    expect(preview.status == JobResult::Status::Changed && !preview.written,
           QStringLiteral("dry run reports a change without writing"));
    expect(!QFileInfo::exists(output), QStringLiteral("dry run creates nothing"));

    const JobResult result = processJob(collection.jobs.first(), options, true);
    expect(result.written, QStringLiteral("job writes"), result.error);
    const QDir target(collection.jobs.first().outputDir);
    for (const char* name : {"track.mp3", "bg.png", "pv.mp4", ".miacode/project.json", "media/movie.mp4"}) {
        expect(QFileInfo(target.filePath(QString::fromLatin1(name))).isFile(),
               QStringLiteral("copied %1").arg(QString::fromLatin1(name)));
    }
    expect(!QFileInfo::exists(target.filePath("notes.txt")), QStringLiteral("non-media files are not copied"));
    const QString written = QString::fromUtf8(readFile(target.filePath("maidata.txt")));
    expect(written.contains(QStringLiteral("&des=Mia")) && written.contains(QStringLiteral("&des_5=Mia"))
               && written.contains(QStringLiteral("&video=media/movie.mp4")) && !written.contains(QStringLiteral("||")),
           QStringLiteral("written chart carries the cleanup"), written);
    expect(readFile(song + "/maidata.txt") == original, QStringLiteral("source chart is untouched"));
}

void diffsLines()
{
    const LineDiff diff = diffLines({"a", "b", "c", "d"}, {"a", "c", "x", "d"});
    expect(diff.removed == QVector<bool>({false, true, false, false}), QStringLiteral("removed lines"));
    expect(diff.added == QVector<bool>({false, false, true, false}), QStringLiteral("added lines"));
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    SimaiParser::setAllowNegativeHsEnabled(true);
    stripsCommentsButKeepsMeter();
    unifiesDesignerOfChartedDifficulties();
    stripsTimeSignatures();
    rejectsOversizedSubdivision();
    normalizeRunsOnStrippedChart();
    keepsLayoutWhenNormalizationChangesNotes();
    collectsJobsPerSongFolder();
    writesCleanCopyWithMedia();
    diffsLines();
    QTextStream out(stdout);
    if (failures > 0) {
        out << failures << " batch cleanup check(s) failed\n";
        return 1;
    }
    out << "batch cleanup spec passed\n";
    return 0;
}
