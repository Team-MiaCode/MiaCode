#include "app/ui/editor/ScintillaDocumentAdapter.h"

namespace miacode::ui {
ScintillaDocumentAdapter::ScintillaDocumentAdapter(ScintillaQuick_item& editor) : editor_(editor)
{
    editor_.send(SCI_ALLOCATELINECHARACTERINDEX, SC_LINECHARACTERINDEX_UTF16);
    refresh();
}
ScintillaDocumentAdapter::~ScintillaDocumentAdapter()
{
    for (const auto& document : std::as_const(documents_))
        editor_.send(SCI_RELEASEDOCUMENT, 0, document.pointer);
}

void ScintillaDocumentAdapter::saveViewport()
{
    auto it = documents_.find(scope_);
    if (it == documents_.end()) return;
    it->anchor = editor_.send(SCI_GETANCHOR);
    it->caret = editor_.send(SCI_GETCURRENTPOS);
    it->xOffset = editor_.send(SCI_GETXOFFSET);
}

void ScintillaDocumentAdapter::captureViewport()
{
    auto it = documents_.find(scope_);
    if (it == documents_.end()) return;
    int x = editor_.send(SCI_GETMARGINLEFT);
    for (int margin = 0; margin < editor_.send(SCI_GETMARGINS); ++margin)
        x += editor_.send(SCI_GETMARGINWIDTHN, margin);
    it->topPosition = editor_.send(SCI_POSITIONFROMPOINT, x, 0);
    saveViewport();
}

bool ScintillaDocumentAdapter::restoreViewport()
{
    if (!viewportPending_) return false;
    viewportPending_ = false;
    const auto& document = documents_[scope_];
    const int position = qBound(0, document.topPosition, int(editor_.send(SCI_GETLENGTH)));
    editor_.send(SCI_ENSUREVISIBLE, editor_.send(SCI_LINEFROMPOSITION, position));
    const int row = editor_.send(SCI_GETFIRSTVISIBLELINE)
        + editor_.send(SCI_POINTYFROMPOSITION, 0, position) / editor_.send(SCI_TEXTHEIGHT, 0);
    editor_.scrollVertical(row);
    editor_.send(SCI_SETXOFFSET, document.xOffset);
    return true;
}

void ScintillaDocumentAdapter::activate(const QString& scope, const QString& text)
{
    if (scope_ != scope || !documents_.contains(scope)) {
        saveViewport();
        if (!documents_.contains(scope)) {
            Document document;
            document.pointer = editor_.send(SCI_CREATEDOCUMENT, 0, SC_DOCUMENTOPTION_DEFAULT);
            documents_.insert(scope, document);
        }
        scope_ = scope;
        const auto& document = documents_[scope];
        editor_.send(SCI_SETDOCPOINTER, 0, document.pointer);
        editor_.send(SCI_SETCODEPAGE, SC_CP_UTF8);
        if (!(editor_.send(SCI_GETLINECHARACTERINDEX) & SC_LINECHARACTERINDEX_UTF16))
            editor_.send(SCI_ALLOCATELINECHARACTERINDEX, SC_LINECHARACTERINDEX_UTF16);
        refresh();
        if (text_ != text) {
            editor_.sends(SCI_SETTEXT, 0, text.toUtf8().constData());
            editor_.send(SCI_EMPTYUNDOBUFFER);
        }
        const int length = editor_.send(SCI_GETLENGTH);
        editor_.send(SCI_SETSELECTION, qBound(0, document.caret, length), qBound(0, document.anchor, length));
        viewportPending_ = true;
    } else if (text_ != text) {
        saveViewport();
        // External DSL transforms and source replacements join native history.
        editor_.send(SCI_BEGINUNDOACTION);
        editor_.send(SCI_SETTARGETSTART, 0);
        editor_.send(SCI_SETTARGETEND, editor_.send(SCI_GETLENGTH));
        const QByteArray replacement = text.toUtf8();
        editor_.sends(SCI_REPLACETARGET, replacement.size(), replacement.constData());
        editor_.send(SCI_ENDUNDOACTION);
        viewportPending_ = true;
    }
}

void ScintillaDocumentAdapter::drop(const QString& scope)
{
    auto it = documents_.find(scope);
    if (it == documents_.end()) return;
    editor_.send(SCI_RELEASEDOCUMENT, 0, it->pointer);
    documents_.erase(it);
    if (scope_ == scope) {
        editor_.send(SCI_SETDOCPOINTER, 0, 0);
        editor_.send(SCI_SETCODEPAGE, SC_CP_UTF8);
        editor_.send(SCI_ALLOCATELINECHARACTERINDEX, SC_LINECHARACTERINDEX_UTF16);
        text_.clear();
        scope_.clear();
        viewportPending_ = false;
    }
}

void ScintillaDocumentAdapter::clear()
{
    editor_.send(SCI_SETDOCPOINTER, 0, 0);
    editor_.send(SCI_SETCODEPAGE, SC_CP_UTF8);
    editor_.send(SCI_ALLOCATELINECHARACTERINDEX, SC_LINECHARACTERINDEX_UTF16);
    text_.clear();
    for (const auto& document : std::as_const(documents_))
        editor_.send(SCI_RELEASEDOCUMENT, 0, document.pointer);
    documents_.clear();
    scope_.clear();
    viewportPending_ = false;
}

void ScintillaDocumentAdapter::refresh()
{
    const int length = editor_.send(SCI_GETLENGTH);
    QByteArray buffer(length + 1, Qt::Uninitialized);
    editor_.send(SCI_GETTEXT, buffer.size(), reinterpret_cast<sptr_t>(buffer.data()));
    text_ = QString::fromUtf8(buffer.constData(), length);
}
int ScintillaDocumentAdapter::lineAt(int position) const
{
    return editor_.send(SCI_LINEFROMINDEXPOSITION, position, SC_LINECHARACTERINDEX_UTF16);
}
void ScintillaDocumentAdapter::applyChange(int position, int removedUtf16, const QByteArray& inserted)
{
    text_.replace(utf16Position(position), removedUtf16, QString::fromUtf8(inserted));
}
int ScintillaDocumentAdapter::bytePosition(int utf16) const
{
    utf16 = qBound(0, utf16, int(text_.size()));
    const int line = lineAt(utf16);
    return editor_.send(SCI_POSITIONRELATIVECODEUNITS, editor_.send(SCI_POSITIONFROMLINE, line), utf16 - lineStart(line));
}
int ScintillaDocumentAdapter::utf16Position(int byte) const
{
    byte = qBound(0, byte, int(editor_.send(SCI_GETLENGTH)));
    const int line = editor_.send(SCI_LINEFROMPOSITION, byte);
    return lineStart(line) + editor_.send(SCI_COUNTCODEUNITS, editor_.send(SCI_POSITIONFROMLINE, line), byte);
}
}
