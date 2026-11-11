#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QTextStream>
#include <QTranslator>

#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "batch_cleaner/BatchCleanerController.h"
#include "batch_cleaner/BatchCleanupCore.h"
#include "core/chart/parser/SimaiParser.h"

namespace {

using namespace miacode::batch_cleaner;

constexpr auto kAppName = "MiaCodeBatchCleaner";

#ifdef Q_OS_WIN
// The executable uses the GUI subsystem; reattach to the launching console so
// command-line runs still print.
void attachParentConsole()
{
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        SetConsoleOutputCP(CP_UTF8);
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
        freopen_s(&stream, "CONOUT$", "w", stderr);
    }
}
#endif

bool wantsCli(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (std::strncmp(arg, "-psn_", 5) == 0) {
            continue;  // macOS Finder process serial number
        }
        if (std::strcmp(arg, "--gui") == 0) {
            return false;
        }
        if (arg[0] != '-' || std::strcmp(arg, "--cli") == 0 || std::strcmp(arg, "--help") == 0
            || std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--version") == 0) {
            return true;
        }
    }
    return false;
}

void installTranslator(QCoreApplication& app, QTranslator& translator)
{
    const QString name = QLocale::system().name();
    QString path = QStringLiteral(":/i18n/en_US.qm");
    if (name.startsWith(QStringLiteral("zh"))) {
        path = QStringLiteral(":/i18n/zh_CN.qm");
    } else if (name.startsWith(QStringLiteral("ja"))) {
        path = QStringLiteral(":/i18n/ja_JP.qm");
    }
    if (translator.load(path)) {
        app.installTranslator(&translator);
    }
}

int runCli(QCoreApplication& app)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "MiaCode 谱面批处理：整理谱面、删除注释、统一谱师，结果连同 track/bg/pv 等资源另存到新目录。"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("inputs"),
                                 QStringLiteral("谱面文件，或递归查找 maidata.txt 的目录。"),
                                 QStringLiteral("<路径...>"));
    const QCommandLineOption cliOption(QStringLiteral("cli"), QStringLiteral("强制使用命令行模式。"));
    const QCommandLineOption outputOption({QStringLiteral("o"), QStringLiteral("output")},
                                          QStringLiteral("输出根目录（默认：第一个输入旁的 <名称>_cleaned）。"),
                                          QStringLiteral("目录"));
    const QCommandLineOption designerOption(QStringLiteral("des"),
                                            QStringLiteral("统一谱师名义：写入 &des 和已有谱面难度的 &des_N。省略则不改谱师。"),
                                            QStringLiteral("名称"));
    const QCommandLineOption keepCommentsOption(QStringLiteral("keep-comments"), QStringLiteral("保留 || 注释。"));
    const QCommandLineOption stripMeterOption(QStringLiteral("strip-time-signatures"),
                                              QStringLiteral("同时删除拍号（|| 3/4 与 &whole_time_signature）。"));
    const QCommandLineOption noNormalizeOption(QStringLiteral("no-normalize"), QStringLiteral("不整理谱面。"));
    const QCommandLineOption gridOption(QStringLiteral("grid384"), QStringLiteral("整理时对齐至 1/384 小节（默认关闭）。"));
    const QCommandLineOption sectionOption(QStringLiteral("section"),
                                           QStringLiteral("空行分组：4、2 或 0（连续排列），默认 4。"),
                                           QStringLiteral("小节数"), QStringLiteral("4"));
    const QCommandLineOption syntaxOption(QStringLiteral("syntax"),
                                          QStringLiteral("输出格式：segment（保留分段，默认）或 compact（紧凑格式）。"),
                                          QStringLiteral("格式"), QStringLiteral("segment"));
    const QCommandLineOption dryRunOption(QStringLiteral("dry-run"), QStringLiteral("只预检，不写入任何文件。"));
    parser.addOptions({cliOption, outputOption, designerOption, keepCommentsOption, stripMeterOption,
                       noNormalizeOption, gridOption, sectionOption, syntaxOption, dryRunOption});
    parser.process(app);

    const QStringList inputs = parser.positionalArguments();
    if (inputs.isEmpty()) {
        err << "缺少输入路径。使用 --help 查看用法。\n";
        return 2;
    }

    CleanupOptions options;
    options.normalize = !parser.isSet(noNormalizeOption);
    options.stripComments = !parser.isSet(keepCommentsOption);
    options.stripTimeSignatures = parser.isSet(stripMeterOption);
    if (options.stripTimeSignatures && !options.stripComments) {
        err << "--strip-time-signatures 不能与 --keep-comments 同时使用。\n";
        return 2;
    }
    options.unifyDesigner = parser.isSet(designerOption);
    options.designer = parser.value(designerOption).trimmed();
    if (options.unifyDesigner && options.designer.isEmpty()) {
        err << "--des 需要非空的谱师名称。\n";
        return 2;
    }
    options.normalization.reduceTo384Grid = parser.isSet(gridOption);
    bool sectionOk = false;
    const int section = parser.value(sectionOption).toInt(&sectionOk);
    if (!sectionOk || (section != 4 && section != 2 && section != 0)) {
        err << "--section 只接受 4、2 或 0。\n";
        return 2;
    }
    options.normalization.sectionMeasureCount = section;
    options.normalization.splitEveryFourMeasures = section == 4;
    const QString syntax = parser.value(syntaxOption).trimmed().toLower();
    if (syntax == QStringLiteral("compact")) {
        options.normalization.syntax = miacode::chart_transform::ChartNormalizationSyntax::CompactSingleLine;
    } else if (syntax != QStringLiteral("segment")) {
        err << "--syntax 只接受 segment 或 compact。\n";
        return 2;
    }

    const bool dryRun = parser.isSet(dryRunOption);
    const QString outputRoot = parser.isSet(outputOption)
        ? QDir::cleanPath(QDir::current().absoluteFilePath(parser.value(outputOption)))
        : defaultOutputRoot(inputs);
    const JobCollection collection = collectJobs(inputs, outputRoot);
    for (const QString& error : collection.errors) {
        err << "[跳过] " << error << '\n';
    }
    if (collection.jobs.isEmpty()) {
        err << "没有找到可处理的谱面。\n";
        return 1;
    }

    out << (dryRun ? "预检" : "输出到") << "：" << QDir::toNativeSeparators(outputRoot) << '\n';
    int failed = 0;
    int changed = 0;
    int warned = 0;
    const int total = collection.jobs.size();
    for (int i = 0; i < total; ++i) {
        const ChartJob& job = collection.jobs.at(i);
        const JobResult result = processJob(job, options, !dryRun);
        out << '[' << (i + 1) << '/' << total << "] ";
        if (result.status == JobResult::Status::Failed) {
            ++failed;
            out << "失败    " << QDir::toNativeSeparators(job.chartPath) << "：" << result.error << '\n';
            continue;
        }
        const bool isChanged = result.status == JobResult::Status::Changed;
        changed += isChanged ? 1 : 0;
        warned += result.report.warnings.isEmpty() ? 0 : 1;
        out << (isChanged ? "修改    " : "无变化  ") << result.title;
        if (!dryRun) {
            out << " -> " << QDir::toNativeSeparators(job.outputDir);
        }
        out << "（注释 −" << result.report.removedComments;
        if (options.stripTimeSignatures) {
            out << "，拍号 −" << result.report.removedTimeSignatures;
        }
        out << "，谱师 " << result.report.designerChanges << " 处）\n";
        for (const QString& warning : result.report.warnings) {
            out << "        警告：" << warning << '\n';
        }
    }
    out << "完成：共 " << total << " 首，修改 " << changed << "，警告 " << warned << "，失败 " << failed
        << (dryRun ? "（未写入文件）" : "") << '\n';
    return failed > 0 ? 1 : 0;
}

// Same CJK UI font choice as the application. Theme hands this family to
// every shared control; an empty family made Core Text shape and draw with
// mismatched fonts, which garbled their labels.
void installUiFont(QGuiApplication& app)
{
    QStringList candidates;
    const QString language = QLocale::system().name();
    if (language.startsWith(QStringLiteral("ja"))) {
        candidates = {"Yu Gothic UI", "Meiryo UI", "Meiryo", "Hiragino Sans", "Noto Sans CJK JP"};
    } else {
        candidates = {"Microsoft YaHei UI", "Microsoft YaHei", "PingFang SC", "Noto Sans CJK SC"};
    }
    const QStringList available = QFontDatabase::families();
    for (const QString& family : candidates) {
        if (available.contains(family, Qt::CaseInsensitive)) {
            QFont font(family);
            font.setStyleStrategy(QFont::PreferAntialias);
            font.setHintingPreference(QFont::PreferNoHinting);
            app.setFont(font);
            return;
        }
    }
}

int runGui(QGuiApplication& app)
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    BatchCleanerController controller;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("controller"), QVariant::fromValue(&controller)}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("MiaCode.UI", "BatchCleanerWindow");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return app.exec();
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication::setOrganizationName(QStringLiteral("MiaCode"));
    QCoreApplication::setApplicationName(QString::fromLatin1(kAppName));
    QCoreApplication::setApplicationVersion(QStringLiteral(MIACODE_BATCH_CLEANER_VERSION));
    // Same default as the application: charts may use negative `<HS*-N>`.
    SimaiParser::setAllowNegativeHsEnabled(true);

    if (wantsCli(argc, argv)) {
#ifdef Q_OS_WIN
        attachParentConsole();
#endif
        QCoreApplication app(argc, argv);
        return runCli(app);
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(QStringLiteral("MiaCode 谱面批处理"));
    installUiFont(app);
    QTranslator translator;
    installTranslator(app, translator);
    return runGui(app);
}
