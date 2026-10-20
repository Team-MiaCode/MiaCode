#include "app/services/PreferenceJsonFile.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool put(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray bytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) {
            err << "FAIL: " << message << '\n';
            ok = false;
        }
    };
    using namespace miacode::preference_json_file;
    expect(temporary.isValid(), "temporary directory exists");
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    const QDir directory(temporary.path());
    const QString path = directory.filePath(QStringLiteral("preferences.json"));
    const QJsonObject defaults{{QStringLiteral("default"), true}};
    expect(read(path).status == ReadStatus::Missing, "missing file is distinct");
    expect(load(path, defaults) == defaults && !QFile::exists(path), "missing reads do not create files");
    expect(put(path, "{}"), "create valid empty object");
    expect(read(path).status == ReadStatus::Valid && load(path, defaults).isEmpty(),
           "valid empty object is authoritative");

    for (const QByteArray& corrupt : {QByteArray("{\r\n\"setting\":1,"), QByteArray("[]"), QByteArray()}) {
        expect(put(path, corrupt), "create damaged preferences");
        const QStringList before = directory.entryList({QStringLiteral("*.corrupt-*")}, QDir::Files);
        expect(read(path).status == ReadStatus::Corrupt, "invalid root is distinct");
        expect(load(path, defaults) == defaults, "corruption falls back to defaults");
        expect(read(path).object == defaults, "corrupt file is rebuilt");
        const QStringList after = directory.entryList({QStringLiteral("*.corrupt-*")}, QDir::Files);
        QStringList created = after;
        for (const QString& name : before) {
            created.removeAll(name);
        }
        expect(created.size() == 1, "recovery makes one backup");
        if (created.size() == 1) {
            expect(bytes(directory.filePath(created.front())) == corrupt, "backup preserves exact bytes");
        }
        load(path, defaults);
        expect(directory.entryList({QStringLiteral("*.corrupt-*")}, QDir::Files) == after,
               "successful recovery does not repeat backups");
    }

    expect(put(path, "{broken"), "create corruption for direct save");
    const QJsonObject changed{{QStringLiteral("changed"), 42}};
    expect(write(path, changed) && read(path).object == changed, "direct save safely replaces corruption");
    const QString obstacle = directory.filePath(QStringLiteral("obstacle"));
    expect(QDir().mkpath(obstacle), "create unreadable-as-file directory");
    expect(read(obstacle).status == ReadStatus::ReadError, "IO error is distinct from corruption");
    expect(load(obstacle, defaults) == defaults && !write(obstacle, changed) && QFileInfo(obstacle).isDir(),
           "IO failures preserve the original path");
    const QString parentFile = directory.filePath(QStringLiteral("parent-file"));
    expect(put(parentFile, "keep"), "create parent obstacle");
    expect(!write(QDir(parentFile).filePath(QStringLiteral("preferences.json")), changed)
               && bytes(parentFile) == "keep", "failed atomic save preserves parent obstacle");
    expect(write(path, changed) && read(path).object == changed, "normal atomic save succeeds");
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}
