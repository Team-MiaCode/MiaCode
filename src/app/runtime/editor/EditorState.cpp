#include "app/runtime/editor/EditorHost.h"
#include "app/runtime/Shared.h"
#include "app/runtime/document/DocumentSessionHost.h"

#include <QtCore>

QString miacode::runtime::EditorHost::resolveProjectRenderStateFilePath() const
{
    const QString projectDataDirectoryPath =
        miacode::runtime::shared::resolveProjectDataDirectoryPath(state_.currentFilePath_);
    if (projectDataDirectoryPath.isEmpty()) {
        return QString();
    }
    return QDir(projectDataDirectoryPath).filePath(QStringLiteral("miacode_settings.json"));
}

void miacode::runtime::EditorHost::loadProjectRenderState()
{
    state_.projectLastOpenedDifficultyId_ = 0;

    const QString path = resolveProjectRenderStateFilePath();
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonParseError parseError;
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                const QJsonObject root = doc.object();
                const int savedDifficultyId = root.value("last_opened_difficulty").toInt(0);
                if (SimaiDocument::isDifficultyId(savedDifficultyId)) {
                    state_.projectLastOpenedDifficultyId_ = savedDifficultyId;
                }
            }
        }
    }
}

void miacode::runtime::EditorHost::saveProjectRenderState() const
{
    const QString path = resolveProjectRenderStateFilePath();
    if (path.isEmpty()) {
        return;
    }

    const QFileInfo pathInfo(path);
    if (!QDir().mkpath(pathInfo.absolutePath())) {
        return;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }

    QJsonObject root;
    root.insert("last_opened_difficulty", state_.projectLastOpenedDifficultyId_);
    root.insert("schema", "miacode_settings_v1");
    const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size()) {
        return;
    }
    if (!file.commit()) {
        return;
    }

}

void miacode::runtime::EditorHost::removeProjectRenderState() const
{
    const QString path = resolveProjectRenderStateFilePath();
    if (path.isEmpty()) {
        return;
    }
    QFile::remove(path);
}

QString Session::resolveProjectRenderStateFilePath() const
{
    return editor_->resolveProjectRenderStateFilePath();
}

void Session::loadProjectRenderState()
{
    editor_->loadProjectRenderState();
}

void Session::saveProjectRenderState() const
{
    editor_->saveProjectRenderState();
}

void Session::removeProjectRenderState() const
{
    editor_->removeProjectRenderState();
}
