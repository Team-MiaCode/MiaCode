#include "app/ui/document/CommandService.h"
#include "app/ui/chrome/ShortcutCommands.h"

#include "app/ui/document/DocumentModel.h"

#include <QDesktopServices>
#include <QFileInfo>

namespace miacode::ui {
CommandService::CommandService(
    DocumentModel& document,
    miacode::DocumentBridge*& bridgeSlot,
    QObject* parent)
    : QObject(parent)
    , bridgeSlot_(&bridgeSlot)
    , document_(&document)
{
}

void CommandService::whenDocumentMayBeLeft(std::function<void()> proceed)
{
    if (bridge() == nullptr) {
        return;
    }
    bridge()->requestLeaveDocument([proceed = std::move(proceed)](bool mayLeave) {
        if (mayLeave && proceed) {
            proceed();
        }
    });
}

void CommandService::openDocument(const QUrl& fileUrl)
{
    whenDocumentMayBeLeft([this, fileUrl]() { document_->openFile(fileUrl); });
}

void CommandService::openRecentDocument(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    whenDocumentMayBeLeft(
        [this, path]() { document_->openFile(QUrl::fromLocalFile(path)); });
}

void CommandService::newDocument()
{
    whenDocumentMayBeLeft([this]() { document_->createDocumentFromPickedAudio(); });
}

void CommandService::restoreBackupDocument(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    whenDocumentMayBeLeft([this, path]() { document_->restoreBackup(path); });
}

void CommandService::closeDocument()
{
    if (document_ == nullptr || !document_->hasDocument()) {
        return;
    }
    whenDocumentMayBeLeft([this]() { document_->closeDocument(); });
}

bool CommandService::saveDocument() { return document_->save(); }
bool CommandService::saveWholeDocument() { return document_->saveWholeDocument(); }
bool CommandService::openCurrentFolder()
{
    const QString path = document_->currentFilePath();
    return !path.isEmpty()
        && QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
bool CommandService::saveDocumentAs(const QUrl& fileUrl) { return document_->saveAs(fileUrl); }
void CommandService::discardDocumentChanges() { document_->discardChanges(); }
void CommandService::selectDifficulty(int id) { document_->selectDifficulty(id); }
bool CommandService::addDifficulty(int id) { return document_->addDifficulty(id); }
bool CommandService::removeDifficulty(int id) { return document_->removeDifficulty(id); }
bool CommandService::applyDesignerSlots(
    const QVariantList& slotValues, bool unified, const QString& canonicalName)
{
    return document_->applyDesignerSlots(slotValues, unified, canonicalName);
}

QStringList CommandService::shortcutCommandIds() const
{
    // Chart transforms only, and the shell binds them to the editor rather than
    // back to MainWindow: a transform acts on the editor's selection, which is
    // the one thing this side does not have. Preview commands bind straight to
    // the preview session instead of coming through here.
    return miacode::ui::shortcutCommandIds();
}

} // namespace miacode::ui
