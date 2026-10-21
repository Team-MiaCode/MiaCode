#pragma once

#include <QDir>
#include <QString>
#include <QStringList>

namespace miacode::app::entry {

// The chart file or chart folder named on the command line ("Open with", a
// chart dropped onto the executable, or a shell). Relative paths are resolved
// against the launch directory here, so the document, its recent-files entry
// and the remembered folder never depend on where a later run starts.
inline QString startupOpenTargetFromArguments(const QStringList& arguments,
                                              const QString& workingDirectory = QDir::currentPath())
{
    QString target;
    for (int index = 1; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index).trimmed();
        if (argument.isEmpty()) {
            continue;
        }
        if (argument == QStringLiteral("--")) {
            if (index + 1 < arguments.size()) {
                target = arguments.at(index + 1);
            }
            break;
        }
        if (argument == QStringLiteral("--rhi")) {
            ++index;
            continue;
        }
        if (argument.startsWith('-')) {
            continue;
        }
        target = arguments.at(index);
        break;
    }
    target = QDir::fromNativeSeparators(target.trimmed());
    return target.isEmpty() ? QString()
                            : QDir::cleanPath(QDir(workingDirectory).absoluteFilePath(target));
}

} // namespace miacode::app::entry
