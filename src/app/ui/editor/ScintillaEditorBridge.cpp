#include <QPointer>
#include <QTimer>
#include "app/ui/editor/ScintillaEditorBridge.h"
#include "editor/SimaiCompletionCatalog.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTouchEvent>
#include <QScopedValueRollback>
#include <QStyleHints>
#include <cmath>

namespace miacode::ui {
using miacode::editor::normalizeSimaiInput;
ScintillaEditorBridge::ScintillaEditorBridge(QQuickItem* parent)
    : ScintillaQuick_item(parent), document_(*this), styler_(*this, document_)
{
#ifdef Q_OS_IOS
    setAcceptTouchEvents(false);
    setProperty("inputMethodHints", QVariant::fromValue(Qt::InputMethodHints(Qt::ImhNoPredictiveText | Qt::ImhNoAutoUppercase | Qt::ImhNoTextHandles | Qt::ImhNoEditMenu)));
#endif
    send(SCI_SETCODEPAGE, SC_CP_UTF8);
    send(SCI_USEPOPUP, SC_POPUP_NEVER);
    send(SCI_SETHSCROLLBAR, false);
    for (int key : {SCK_ADD, SCK_SUBTRACT, SCK_DIVIDE})
        send(SCI_CLEARCMDKEY, key | (SCMOD_CTRL << 16));
    connect(this, &ScintillaQuick_item::textChanged, this, &ScintillaEditorBridge::textMutated);
    connect(this, &ScintillaQuick_item::cursorPositionChanged, this, [this] {
        refreshSelection(!programmatic_);
    });
    connect(this, &ScintillaQuick_item::updateUi, this, [this](Scintilla::Update) {
        refreshSelection(!programmatic_);
        emit availabilityChanged();
        // 选区使用选择高亮；插入光标所在行使用当前行背景。
        const bool showLine = send(SCI_GETSELECTIONEMPTY);
        if (bool(send(SCI_GETCARETLINEVISIBLE)) != showLine) {
            if (showLine) {
                const QColor fill = palette_.value(QStringLiteral("currentLine")).value<QColor>();
                send(SCI_SETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK,
                     quint32(scintillaquick::rgb_from_color(fill)) | (quint32(fill.alpha()) << 24));
            } else send(SCI_RESETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK);
        }
    });
    connect(this, &QQuickItem::activeFocusChanged, this, [this] { publishContext(false); emit followVisualChanged(); });
    connect(this, &ScintillaQuick_item::fontChanged, this, [this] {
        preserveViewport();
        styler_.setAppearance(property("font").value<QFont>(), palette_);
    });
    connect(this, &ScintillaQuick_item::notificationReceived, this, [this](const ScintillaQuick_notification& notification) {
        const int flags = int(notification.modificationType);
        if (notification.code == Scintilla::Notification::Modified && (flags & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT))) {
            const int position = document_.utf16Position(notification.position);
            const int line = document_.lineAt(position);
            const int removed = flags & SC_MOD_DELETETEXT
                ? QString::fromUtf8(notification.text).size() : 0;
            const int added = flags & SC_MOD_INSERTTEXT ? QString::fromUtf8(notification.text).size() : 0;
            if (styler_.following() && followCaretPosition_ >= position)
                followCaretPosition_ = qMax(position, followCaretPosition_ - removed) + added;
            document_.applyChange(notification.position, removed,
                                  flags & SC_MOD_INSERTTEXT ? notification.text : QByteArray{});
            styler_.invalidate(line, notification.linesAdded);
        }
        if ((int(notification.modificationType) & SC_MOD_CONTAINER) && touchUndoAnchors_.contains(notification.token))
            pendingTouchAnchor_ = touchUndoAnchors_.value(notification.token).position;
    });
    connect(this, &ScintillaQuick_item::styleNeeded, this, [this](Scintilla::Position) { if (ready_) styler_.style(); });
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ScintillaEditorBridge::availabilityChanged);
    connect(this, &ScintillaQuick_item::marginClicked, this, [this](Scintilla::Position position, Scintilla::KeyMod, int margin) {
        if (margin != 1) return;
        beginUserInteraction();
        const int line = send(SCI_LINEFROMPOSITION, position) + 1;
        const QRectF rect = positionToRectangle(document_.utf16Position(position));
        emit bookmarkMenuRequested(line, 16, rect.y());
    });
    trackScenePosition();
}
ScintillaEditorBridge::~ScintillaEditorBridge()
{
    if (syncController_) syncController_->setEditorReadiness(-1, 0, false);
}
void ScintillaEditorBridge::componentComplete()
{
    ScintillaQuick_item::componentComplete();
    ready_ = true;
    refreshSettings();
    synchronizeDocument();
}
void ScintillaEditorBridge::updatePolish()
{
    prepareLayout();
    if (document_.restoreViewport()) {
        viewportToRestore_.reset();
    } else if (viewportToRestore_) {
        const auto anchor = *viewportToRestore_;
        viewportToRestore_.reset();
        const int position = qBound(0, anchor.position, int(send(SCI_GETLENGTH)));
        send(SCI_ENSUREVISIBLE, send(SCI_LINEFROMPOSITION, position));
        const int lineHeight = send(SCI_TEXTHEIGHT, 0);
        const qreal targetY = qBound(qreal(0), anchor.y, qMax(qreal(0), height() - lineHeight));
        const qreal positionY = send(SCI_POINTYFROMPOSITION, 0, position);
        scrollVertical(int(send(SCI_GETFIRSTVISIBLELINE)) + qRound((positionY - targetY) / lineHeight));
    }

    const auto navigation = pendingNavigation_;
    pendingNavigation_.reset();
    bool applied = false;
    if (navigation) {
        const auto& request = *navigation;
        applied = navigationVisible_ && documentSession_ && !imeComposing_
            && request.difficulty == documentSession_->currentDifficultyId()
            && request.revision == documentSession_->documentRevision()
            && request.generation == documentSession_->documentOpenGeneration()
            && request.start >= 0 && request.end >= request.start && request.end <= document_.text().size();
        if (applied) {
            QScopedValueRollback guard(programmatic_, true);
            select(request.start, request.end);
            if (request.focus) forceActiveFocus();
            if (request.reveal) revealPosition(request.end, true);
            publishContext(false);
        }
    }
    captureFrame();
    publishLayout();
    if (cursorRectangle_.top() >= 0 && cursorRectangle_.bottom() <= height()) {
        viewportAnchor_ = {int(send(SCI_GETCURRENTPOS)), cursorRectangle_.y()};
    } else {
        int textLeft = send(SCI_GETMARGINLEFT);
        for (int margin = 0; margin < send(SCI_GETMARGINS); ++margin)
            textLeft += send(SCI_GETMARGINWIDTHN, margin);
        const int position = send(SCI_POSITIONFROMPOINT, textLeft, 0);
        viewportAnchor_ = {position, qreal(send(SCI_POINTYFROMPOSITION, 0, position))};
    }
    document_.captureViewport();
    if (navigation && syncController_)
        syncController_->acknowledgeNavigation(navigation->sequence, applied);
}

void ScintillaEditorBridge::preserveViewport()
{
    if (ready_ && !viewportToRestore_) viewportToRestore_ = viewportAnchor_;
}

void ScintillaEditorBridge::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    if (newGeometry.size() != oldGeometry.size()) preserveViewport();
    ScintillaQuick_item::geometryChange(newGeometry, oldGeometry);
}

void ScintillaEditorBridge::trackScenePosition()
{
    for (const auto& connection : sceneConnections_) disconnect(connection);
    sceneConnections_.clear();
    for (QQuickItem* item = this; item; item = item->parentItem()) {
        sceneConnections_.append(connect(item, &QQuickItem::xChanged, this, &ScintillaEditorBridge::scenePositionChanged));
        sceneConnections_.append(connect(item, &QQuickItem::yChanged, this, &ScintillaEditorBridge::scenePositionChanged));
        sceneConnections_.append(connect(item, &QQuickItem::rotationChanged, this, &ScintillaEditorBridge::scenePositionChanged));
        sceneConnections_.append(connect(item, &QQuickItem::scaleChanged, this, &ScintillaEditorBridge::scenePositionChanged));
        sceneConnections_.append(connect(item, &QQuickItem::transformOriginChanged, this, &ScintillaEditorBridge::scenePositionChanged));
        sceneConnections_.append(connect(item, &QQuickItem::parentChanged, this, &ScintillaEditorBridge::trackScenePosition));
    }
    emit scenePositionChanged();
}
void ScintillaEditorBridge::publishLayout()
{
    const QFont font = property("font").value<QFont>();
    const int height = send(SCI_TEXTHEIGHT, 0);
    const QSizeF size(width(), this->height());
    const bool metricsChanged = effectiveFont_ != font || lineHeight_ != height || layoutSize_ != size;
    effectiveFont_ = font;
    lineHeight_ = height;
    layoutSize_ = size;

    const QRectF cursor = positionToRectangle(cursorPosition());
    const QRectF follow = styler_.following() ? positionToRectangle(followCaretPosition_) : QRectF{};
    const bool cursorChanged = cursorRectangle_ != cursor;
    const bool followChanged = followCursorRectangle_ != follow;
    cursorRectangle_ = cursor;
    followCursorRectangle_ = follow;
    if (metricsChanged) emit layoutChanged();
    if (cursorChanged) emit cursorRectangleChanged();
    if (followChanged) emit followVisualChanged();
}
void ScintillaEditorBridge::setDocumentSession(DocumentModel* value)
{
    if (documentSession_ == value) return;
    if (documentSession_) disconnect(documentSession_, nullptr, this, nullptr);
    documentSession_ = value;
    if (value) {
        connect(value, &DocumentModel::chartTextChanged, this, &ScintillaEditorBridge::synchronizeDocument);
        connect(value, &DocumentModel::documentStateChanged, this, &ScintillaEditorBridge::synchronizeDocument);
        connect(value, &DocumentModel::documentReplaced, this, &ScintillaEditorBridge::synchronizeDocument);
        connect(value, &DocumentModel::syntaxIssuesChanged, this, &ScintillaEditorBridge::refreshDiagnostics);
    }
    synchronizeDocument();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setController(EditorController* value)
{
    if (controller_ == value) return;
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = value;
    if (value) connect(value, &EditorController::settingsChanged, this, &ScintillaEditorBridge::refreshSettings);
    refreshSettings();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setSyncController(miacode::EditorSyncController* value)
{
    if (syncController_ == value) return;
    if (syncController_) {
        syncController_->setEditorReadiness(-1, 0, false);
        disconnect(syncController_, nullptr, this, nullptr);
    }
    syncController_ = value;
    if (value) {
        connect(value, &miacode::EditorSyncController::navigationRequested, this, &ScintillaEditorBridge::navigate);
        connect(value, &miacode::EditorSyncController::followChanged, this, [this] { applyFollow(true); });
        connect(value, &miacode::EditorSyncController::touchPadAuthoringRequested, this, &ScintillaEditorBridge::touchAuthoring);
    }
    publishContext(false);
    emit bindingsChanged();
}
void ScintillaEditorBridge::setAnalysisSession(AnalysisModel* value)
{
    if (analysisSession_ == value) return;
    if (analysisSession_) disconnect(analysisSession_, nullptr, this, nullptr);
    analysisSession_ = value;
    if (value) connect(value, &AnalysisModel::changed, this, &ScintillaEditorBridge::refreshDiagnostics);
    refreshDiagnostics();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setNavigationVisible(bool value)
{
    if (navigationVisible_ == value) return;
    preserveViewport();
    navigationVisible_ = value;
    if (value) synchronizeDocument();
    if (!value) {
        pendingNavigation_.reset();
        if (controller_) controller_->closeCompletion();
    }
    publishContext(false);
    applyFollow();
    request_scene_graph_update(true, true, false);
    emit bindingsChanged();
}
void ScintillaEditorBridge::setEditorColors(const QVariantMap& value)
{
    if (palette_ == value) return;
    palette_ = value;
    styler_.setAppearance(property("font").value<QFont>(), value);
    emit editorColorsChanged();
}
void ScintillaEditorBridge::setBlockSpacing(int value)
{
    preserveViewport();
    blockSpacing_ = qMax(0, value);
    send(SCI_SETEXTRAASCENT, blockSpacing_ / 2);
    send(SCI_SETEXTRADESCENT, blockSpacing_ - blockSpacing_ / 2);
    emit appearanceChanged();
}
void ScintillaEditorBridge::setAutoWrap(bool value)
{
    if (autoWrap_ == value) return;
    preserveViewport();
    autoWrap_ = value;
    send(SCI_SETWRAPMODE, value ? SC_WRAP_WORD : SC_WRAP_NONE);
    send(SCI_SETHSCROLLBAR, !value);
    send(SCI_SETSCROLLWIDTHTRACKING, !value);
    if (value) send(SCI_SETXOFFSET, 0);
    emit appearanceChanged();
}
void ScintillaEditorBridge::setScrollPastEnd(bool value)
{
    preserveViewport();
    scrollPastEnd_ = value;
    send(SCI_SETENDATLASTLINE, !value);
    emit appearanceChanged();
}
int ScintillaEditorBridge::cursorPosition() const { return document_.utf16Position(send(SCI_GETCURRENTPOS)); }
void ScintillaEditorBridge::setCursorPosition(int value) { select(value, value); }
int ScintillaEditorBridge::selectionStart() const { return document_.utf16Position(send(SCI_GETSELECTIONSTART)); }
int ScintillaEditorBridge::selectionEnd() const { return document_.utf16Position(send(SCI_GETSELECTIONEND)); }
QString ScintillaEditorBridge::selectedText() const { return document_.text().mid(selectionStart(), selectionEnd() - selectionStart()); }
QRectF ScintillaEditorBridge::positionToRectangle(int position) const
{
    const int byte = document_.bytePosition(position);
    return QRectF(send(SCI_POINTXFROMPOSITION, 0, byte), send(SCI_POINTYFROMPOSITION, 0, byte), 2,
                  send(SCI_TEXTHEIGHT, send(SCI_LINEFROMPOSITION, byte)));
}
QRectF ScintillaEditorBridge::cursorRectangle() const { return cursorRectangle_; }
int ScintillaEditorBridge::positionAt(qreal x, qreal y) const
{
    return document_.utf16Position(send(SCI_POSITIONFROMPOINT, qRound(x), qRound(y)));
}
QRectF ScintillaEditorBridge::textPositionRectangle(int position) const
{
    return positionToRectangle(position);
}
QRectF ScintillaEditorBridge::lineRectangle(int line) const
{
    return positionToRectangle(document_.lineStart(qBound(0, line - 1, document_.lineCount() - 1)));
}
int ScintillaEditorBridge::lineAtPosition(int position) const
{
    return document_.lineAt(qBound(0, position, int(document_.text().size()))) + 1;
}
QRectF ScintillaEditorBridge::followCursorRectangle() const
{
    return followCursorRectangle_;
}
bool ScintillaEditorBridge::followCaretVisible() const
{
    return documentSession_ && syncController_ && syncController_->followActive()
        && syncController_->followDifficultyId() == documentSession_->currentDifficultyId()
        && styler_.following()
        && (syncController_->followPlaybackActive() || !hasActiveFocus());
}
int ScintillaEditorBridge::cursorLine() const { return send(SCI_LINEFROMPOSITION, send(SCI_GETCURRENTPOS)) + 1; }
int ScintillaEditorBridge::cursorColumn() const
{
    return cursorPosition() - document_.utf16Position(send(SCI_POSITIONFROMLINE, cursorLine() - 1)) + 1;
}
void ScintillaEditorBridge::synchronizeDocument()
{
    if (!ready_ || !documentSession_ || synchronizing_ || handlingIme_) return;
    QScopedValueRollback guard(synchronizing_, true);
    const auto generation = documentSession_->documentOpenGeneration();
    const QString scope = QStringLiteral("difficulty:%1").arg(documentSession_->currentDifficultyId());
    const bool identityChanged = generation_ != generation || scope != document_.scope();
    if (!identityChanged && synchronizedRevision_ == documentSession_->documentRevision()) return;
    if (controller_ && (identityChanged || document_.text() != documentSession_->chartText()))
        controller_->closeCompletion();
    if (imeComposing_ && !identityChanged) { refreshDiagnostics(); return; }
    if (identityChanged && imeComposing_) {
        QScopedValueRollback imeGuard(handlingIme_, true);
        QGuiApplication::inputMethod()->reset();
        imeComposing_ = false;
        emit imeComposingChanged();
    }
    if (generation_ != generation) {
        QGuiApplication::inputMethod()->reset();
        imeComposing_ = false;
        emit imeComposingChanged();
        document_.clear();
        touchUndoAnchors_.clear();
        generation_ = generation;
    }
    if (identityChanged) {
        reportedAnchor_ = reportedCaret_ = -1;
        wheelRemainder_ = 0;
    }
    document_.activate(scope, documentSession_->chartText());
    synchronizedRevision_ = documentSession_->documentRevision();
    if (identityChanged) styler_.reset();
    if (controller_) controller_->setDifficulty(documentSession_->currentDifficultyId());
    refreshDecorations();
    publishContext(false);
    emit selectionChanged();
    emit availabilityChanged();
}
void ScintillaEditorBridge::textMutated()
{
    if (!ready_ || synchronizing_ || handlingIme_) return;
    if (documentSession_) {
        QScopedValueRollback guard(synchronizing_, true);
        documentSession_->setChartText(document_.text());
        synchronizedRevision_ = documentSession_->documentRevision();
        if (controller_) controller_->setDifficulty(documentSession_->currentDifficultyId());
    }
    refreshDecorations();
    publishContext(!programmatic_);
    if (controller_) {
        controller_->updateCompletion(document_.text(), cursorPosition());
    }
    emit selectionChanged();
    emit availabilityChanged();
    if (hasActiveFocus()) updateInputMethod(Qt::ImQueryInput);
}
void ScintillaEditorBridge::refreshDecorations()
{
    styler_.style();
    const auto bookmarks = styler_.bookmarks();
    if (documentSession_) documentSession_->setEditorBookmarks(documentSession_->currentDifficultyId(), bookmarks);
    if (bookmarks_ != bookmarks) {
        bookmarks_ = bookmarks;
        emit bookmarksChanged();
    }
    refreshDiagnostics();
    applyFollow();
}
void ScintillaEditorBridge::refreshDiagnostics()
{
    if (documentSession_ && !documentSession_->validationPending()
        && documentSession_->validationRevision() == documentSession_->documentRevision())
        styler_.diagnostics(documentSession_->syntaxIssues(), false);
    if (!analysisSession_) styler_.diagnostics({}, true);
    else if (!analysisSession_->pending()) {
        if (documentSession_ && analysisSession_->available()
            && analysisSession_->difficultyId() == documentSession_->currentDifficultyId()
            && analysisSession_->revision() == documentSession_->documentRevision())
            styler_.diagnostics(analysisSession_->muriRows(), true);
        else if (!analysisSession_->available()) styler_.diagnostics({}, true);
    }
}
void ScintillaEditorBridge::refreshSettings()
{
    if (!controller_) return;
    send(SCI_SETOVERTYPE, controller_->overwriteMode());
#ifdef Q_OS_IOS
    setFlag(ItemAcceptsInputMethod, true);
#else
    setFlag(ItemAcceptsInputMethod, !controller_->imeInputDisabled());
#endif
}
void ScintillaEditorBridge::publishContext(bool userCaret)
{
    if (!ready_ || !documentSession_ || !syncController_) return;
    const int difficulty = documentSession_->currentDifficultyId();
    const auto revision = documentSession_->documentRevision();
    syncController_->setEditorReadiness(difficulty, revision, navigationVisible_);
    syncController_->setEditorContext(difficulty, revision, document_.utf16Position(send(SCI_GETANCHOR)), cursorPosition(),
                                      hasActiveFocus(), imeComposing_, cursorLine(), cursorColumn(),
                                      userCaret && !imeComposing_ && !programmatic_ && !synchronizing_);
}
void ScintillaEditorBridge::refreshSelection(bool userCaret)
{
    if (handlingIme_) return;
    const int anchor = send(SCI_GETANCHOR);
    const int caret = send(SCI_GETCURRENTPOS);
    if (anchor == reportedAnchor_ && caret == reportedCaret_) return;
    reportedAnchor_ = anchor;
    reportedCaret_ = caret;
    emit selectionChanged();
    if (!synchronizing_ && !handlingIme_) {
        publishContext(userCaret);
        if (controller_ && userCaret) {
            if (anchor == caret)
                controller_->updateCompletion(document_.text(), cursorPosition());
            else
                controller_->closeCompletion();
        }
        if (hasActiveFocus()) updateInputMethod(Qt::ImQueryInput);
    }
}
void ScintillaEditorBridge::revealPosition(int utf16, bool center)
{
    send(SCI_ENSUREVISIBLE, send(SCI_LINEFROMPOSITION, document_.bytePosition(utf16)));
    // Scintilla 的文本坐标包含换行后的子行，用该坐标定位目标显示行。
    const QRectF rect = positionToRectangle(utf16);
    if (!center && rect.top() >= 0 && rect.bottom() <= height()) return;
    const int first = send(SCI_GETFIRSTVISIBLELINE);
    const qreal displacement = rect.center().y() - height() / 2;
    scrollVertical(first + qRound(displacement / qMax(1.0, rect.height())));
}
void ScintillaEditorBridge::applyFollow(bool reveal)
{
    if (styler_.following() && documentSession_ && syncController_ && syncController_->followActive()
        && syncController_->followDifficultyId() == documentSession_->currentDifficultyId()
        && syncController_->followRevision() < documentSession_->documentRevision()) return;
    const bool active = documentSession_ && syncController_ && syncController_->followActive()
        && syncController_->followDifficultyId() == documentSession_->currentDifficultyId()
        && syncController_->followRevision() == documentSession_->documentRevision();
    if (active) followCaretPosition_ = syncController_->followCaret();
    styler_.follow(active, active ? syncController_->followStart() : 0,
                  active ? syncController_->followEnd() : 0);
    const int caretStyle = active && syncController_->followPlaybackActive() ? CARETSTYLE_INVISIBLE : CARETSTYLE_LINE;
    if (send(SCI_GETCARETSTYLE) != caretStyle) send(SCI_SETCARETSTYLE, caretStyle);
    emit followVisualChanged();
    if (reveal && active && navigationVisible_ && syncController_->followReveal())
        revealPosition(syncController_->followCaret(), syncController_->followPlaybackActive());
}
void ScintillaEditorBridge::navigate(qulonglong sequence, int difficulty, qulonglong revision, int start, int end, bool focus, bool reveal)
{
    pendingNavigation_ = NavigationRequest{sequence, difficulty, revision,
        documentSession_->documentOpenGeneration(), start, end, focus, reveal};
    request_scene_graph_update(true, true, false);
}
void ScintillaEditorBridge::touchAuthoring(const QString& pad, QChar separator, int difficulty, qulonglong revision, int anchor, int position)
{
    if (!documentSession_ || !controller_ || imeComposing_ || difficulty != documentSession_->currentDifficultyId()
        || revision != documentSession_->documentRevision()) return;
    QScopedValueRollback guard(programmatic_, true);
    const auto tx = controller_->touchPadAuthoring(document_.text(), anchor, position, pad, separator);
    if (applyEditorTransaction(tx)) {
        forceActiveFocus();
        publishTouchAnchor(tx.transaction.touchTokenStart);
    }
}
void ScintillaEditorBridge::undo()
{
    QGuiApplication::inputMethod()->commit();
    if (controller_) controller_->closeCompletion();
    send(SCI_UNDO);
    publishTouchUndoAnchor();
}
void ScintillaEditorBridge::redo()
{
    QGuiApplication::inputMethod()->commit();
    if (controller_) controller_->closeCompletion();
    send(SCI_REDO);
    publishTouchUndoAnchor();
}
void ScintillaEditorBridge::publishTouchAnchor(int position)
{
    const int bounded = qBound(0, position, int(document_.text().size()));
    const int line = document_.lineAt(bounded);
    syncController_->setTouchPadPreviewAnchor(documentSession_->currentDifficultyId(),
        documentSession_->documentRevision(), line + 1, bounded - document_.lineStart(line) + 1);
}
void ScintillaEditorBridge::publishTouchUndoAnchor()
{
    if (pendingTouchAnchor_ >= 0 && documentSession_ && syncController_)
        publishTouchAnchor(pendingTouchAnchor_);
    pendingTouchAnchor_ = -1;
}
void ScintillaEditorBridge::cut() { send(SCI_CUT); }
void ScintillaEditorBridge::copy() { send(SCI_COPY); }
void ScintillaEditorBridge::paste() { send(SCI_PASTE); }
void ScintillaEditorBridge::selectAll() { send(SCI_SELECTALL); refreshSelection(true); }
void ScintillaEditorBridge::select(int anchor, int position)
{
    send(SCI_SETSELECTION, document_.bytePosition(position), document_.bytePosition(anchor));
    refreshSelection(!programmatic_);
}
void ScintillaEditorBridge::selectCurrentLine()
{
    const int line = cursorLine() - 1;
    select(document_.utf16Position(send(SCI_POSITIONFROMLINE, line)), document_.utf16Position(send(SCI_GETLINEENDPOSITION, line)));
}
void ScintillaEditorBridge::jumpToLine(int line)
{
    const int position = document_.utf16Position(send(SCI_POSITIONFROMLINE, qBound(0, line - 1, int(send(SCI_GETLINECOUNT)) - 1)));
    setCursorPosition(position);
    centerCursorInView();
    forceActiveFocus();
}
void ScintillaEditorBridge::centerCursorInView() { revealPosition(cursorPosition(), true); }
bool ScintillaEditorBridge::applyEditorTransaction(const miacode::editor::SimaiTextEditResult& result)
{
    if (!result.error.isEmpty() && documentSession_) {
        qWarning().noquote() << "MiaCode editor:" << result.error;
    }
    if (!result.consumed) return false;
    const auto& tx = result.transaction;
    const bool caretChanged = tx.anchor != document_.utf16Position(send(SCI_GETANCHOR))
        || tx.position != cursorPosition();
    if (tx.hasEdit || caretChanged) {
        QScopedValueRollback guard(synchronizing_, true);
        if (tx.hasEdit) {
            send(SCI_BEGINUNDOACTION);
            send(SCI_SETTARGETSTART, document_.bytePosition(tx.replacementStart));
            send(SCI_SETTARGETEND, document_.bytePosition(tx.replacementEnd));
            const QByteArray replacement = tx.replacementText.toUtf8();
            sends(SCI_REPLACETARGET, replacement.size(), replacement.constData());
            if (tx.touchTokenStart >= 0) {
                const int token = nextTouchToken_++;
                touchUndoAnchors_.insert(token, {document_.scope(), tx.touchTokenStart});
                send(SCI_ADDUNDOACTION, token, 0);
            }
            send(SCI_ENDUNDOACTION);
        }
        select(tx.anchor, tx.position);
        send(SCI_SCROLLCARET);
    }
    if (tx.hasEdit) textMutated();
    else if (caretChanged) publishContext(!programmatic_);
    return true;
}
void ScintillaEditorBridge::acceptCompletionFromPopup()
{
    if (!controller_) return;
    applyEditorTransaction(controller_->acceptCompletion(document_.text(), document_.utf16Position(send(SCI_GETANCHOR)), cursorPosition()));
    forceActiveFocus();
}
void ScintillaEditorBridge::dropDocument(const QString& key)
{
    QScopedValueRollback guard(synchronizing_, true);
    const bool current = document_.scope() == key;
    document_.drop(key);
    touchUndoAnchors_.removeIf([&key](auto it) { return it.value().scope == key; });
    if (current) {
        styler_.reset();
        bookmarks_.clear();
        emit bookmarksChanged();
        emit selectionChanged();
        emit availabilityChanged();
    }
}
void ScintillaEditorBridge::configureSearch(const QString& query, const QString& replacement, bool matchCase, bool wholeWord)
{
    setFindText(query); setReplacementText(replacement);
    setFindOptions((matchCase ? SCFIND_MATCHCASE : 0) | (wholeWord ? SCFIND_WHOLEWORD : 0));
}
void ScintillaEditorBridge::seekPreviewToCaret()
{
    if (syncController_ && documentSession_ && selectionStart() == selectionEnd())
        syncController_->seekPreviewToEditorLocation(documentSession_->currentDifficultyId(), documentSession_->documentRevision(), cursorLine(), cursorColumn());
}
void ScintillaEditorBridge::exportSelectionRange()
{
    if (syncController_ && documentSession_ && selectionStart() != selectionEnd())
        syncController_->requestSelectionRangeExport(documentSession_->currentDifficultyId(), documentSession_->documentRevision(), selectionStart(), selectionEnd());
}
bool ScintillaEditorBridge::applyNormalization(const QVariantMap& options)
{
    return documentSession_ && applyEditorTransaction(documentSession_->normalizeChartSelection(document_.text(), selectionStart(), selectionEnd(), options));
}
bool ScintillaEditorBridge::applyChartTransform(const QString& operation)
{
    return documentSession_ && applyEditorTransaction(documentSession_->transformChartSelection(document_.text(), selectionStart(), selectionEnd(), operation));
}
bool ScintillaEditorBridge::createBookmarkAtLine(int line, const QString& title)
{
    if (!controller_ || line < 1 || line > document_.lineCount()) return false;
    const int start = document_.lineStart(line - 1);
    const int end = document_.utf16Position(send(SCI_GETLINEENDPOSITION, line - 1));
    return applyEditorTransaction(controller_->createBookmark(document_.text().mid(start, end - start), start, title));
}
bool ScintillaEditorBridge::renameBookmarkAtLine(int line, const QString& title)
{
    if (!controller_ || line < 1 || line > document_.lineCount()) return false;
    const int start = document_.lineStart(line - 1);
    const int end = document_.utf16Position(send(SCI_GETLINEENDPOSITION, line - 1));
    return applyEditorTransaction(controller_->renameBookmark(document_.text().mid(start, end - start), start, title));
}
bool ScintillaEditorBridge::deleteBookmarkAtLine(int line)
{
    if (!controller_ || line < 1 || line > document_.lineCount()) return false;
    const int start = document_.lineStart(line - 1);
    const int end = document_.utf16Position(send(SCI_GETLINEENDPOSITION, line - 1));
    return applyEditorTransaction(controller_->deleteBookmark(document_.text().mid(start, end - start), start));
}
void ScintillaEditorBridge::beginUserInteraction()
{
    viewportToRestore_.reset();
    if (pendingNavigation_) {
        const auto sequence = pendingNavigation_->sequence;
        pendingNavigation_.reset();
        syncController_->acknowledgeNavigation(sequence, false);
    }
    if (syncController_ && documentSession_)
        syncController_->beginPointerInteraction(documentSession_->currentDifficultyId(), documentSession_->documentRevision());
}
void ScintillaEditorBridge::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Find)) { emit findRequested(); event->accept(); return; }
    if (event->key() == Qt::Key_Menu || (event->key() == Qt::Key_F10 && event->modifiers().testFlag(Qt::ShiftModifier))) {
        updatePolish();
        const auto rect = cursorRectangle(); emit contextMenuRequested(rect.x(), rect.bottom()); event->accept(); return;
    }
    if (event->matches(QKeySequence::Undo)) { undo(); event->accept(); return; }
    if (event->matches(QKeySequence::Redo)) { redo(); event->accept(); return; }
    if (event->matches(QKeySequence::Cut)) { cut(); event->accept(); return; }
    if (event->matches(QKeySequence::Copy)) { copy(); event->accept(); return; }
    if (event->matches(QKeySequence::Paste)) { paste(); event->accept(); return; }
    if (event->matches(QKeySequence::SelectAll)) { selectAll(); event->accept(); return; }
    if (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown) beginUserInteraction();
    if (controller_) {
        const QString input = event->text();
        // Only DSL pairing, completion and half-width conversion intercept
        // typing. Scintilla executes ordinary editing and clipboard commands.
        const bool smartGlyph = controller_->autoCompletionEnabled() &&
            (QStringLiteral("(){}[]h").contains(input) && input.size() == 1);
        const bool convert = controller_->halfWidthInputEnabled() && normalizeSimaiInput(input) != input;
        const bool completionKey = controller_->completionActive() &&
            (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down || event->key() == Qt::Key_Escape
             || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Tab);
        if (completionKey || smartGlyph || convert || event->key() == Qt::Key_Backspace) {
            const auto tx = controller_->processKey(document_.text(), document_.utf16Position(send(SCI_GETANCHOR)), cursorPosition(), input, event->key(), int(event->modifiers()));
            if (applyEditorTransaction(tx) || tx.suppressFallbackInsert) { event->accept(); return; }
        }
    }
    ScintillaQuick_item::keyPressEvent(event);
    refreshSelection(true);
}
void ScintillaEditorBridge::mousePressEvent(QMouseEvent* event)
{
    beginUserInteraction();
#ifdef Q_OS_IOS
    if (event->source() == Qt::MouseEventSynthesizedByQt && event->button() == Qt::LeftButton) {
        if (controller_) controller_->closeCompletion();
        touchMouseDown_ = true;
        touchSelecting_ = lastTouchTapTime_ > 0
            && event->timestamp() - lastTouchTapTime_ <= 350
            && (event->position() - lastTouchTapPosition_).manhattanLength() <= 12;
        lastTouchTapTime_ = 0;
        touchScrolling_ = false;
        touchOrigin_ = touchPrevious_ = event->position();
        touchScrollRemainder_ = 0;
        setKeepMouseGrab(true);
        if (touchSelecting_) {
            ScintillaQuick_item::mousePressEvent(event);
            refreshSelection(true);
        }
        event->accept();
        return;
    }
#endif
    if (event->button() == Qt::RightButton) {
        int gutterWidth = 0;
        for (int margin = 0; margin < send(SCI_GETMARGINS); ++margin)
            gutterWidth += send(SCI_GETMARGINWIDTHN, margin);
        if (event->position().x() >= 0 && event->position().x() < gutterWidth) {
            const int position = send(SCI_POSITIONFROMPOINT, gutterWidth + send(SCI_GETMARGINLEFT), qRound(event->position().y()));
            const int line = send(SCI_LINEFROMPOSITION, position) + 1;
            forceActiveFocus();
            emit bookmarkMenuRequested(line, event->position().x(), event->position().y());
            event->accept();
            return;
        }
        forceActiveFocus();
        const int position = positionAt(event->position().x(), event->position().y());
        if (position < selectionStart() || position > selectionEnd()) setCursorPosition(position);
        updatePolish();
        emit contextMenuRequested(event->position().x(), event->position().y());
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) setKeepMouseGrab(true);
    ScintillaQuick_item::mousePressEvent(event);
    refreshSelection(!programmatic_);
}
void ScintillaEditorBridge::mouseReleaseEvent(QMouseEvent* event)
{
#ifdef Q_OS_IOS
    if (event->source() == Qt::MouseEventSynthesizedByQt && event->button() == Qt::LeftButton) {
        touchMouseDown_ = false;
        if (touchSelecting_) {
            ScintillaQuick_item::mouseReleaseEvent(event);
            refreshSelection(true);
            emit contextMenuRequested(event->position().x(), event->position().y());
        } else if (!touchScrolling_) {
            QMouseEvent press(QEvent::MouseButtonPress, event->position(), event->globalPosition(),
                Qt::LeftButton, Qt::LeftButton, event->modifiers());
            ScintillaQuick_item::mousePressEvent(&press);
            ScintillaQuick_item::mouseReleaseEvent(event);
            refreshSelection(true);
            lastTouchTapTime_ = event->timestamp();
            lastTouchTapPosition_ = event->position();
            if (selectionStart() != selectionEnd())
                emit contextMenuRequested(event->position().x(), event->position().y());
        }
        touchSelecting_ = touchScrolling_ = false;
        setKeepMouseGrab(false);
        event->accept();
        return;
    }
#endif
    if (event->button() == Qt::RightButton) { event->accept(); return; }
    ScintillaQuick_item::mouseReleaseEvent(event);
    if (event->button() == Qt::LeftButton) setKeepMouseGrab(false);
    refreshSelection(!programmatic_);
    if (event->button() == Qt::LeftButton && (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) seekPreviewToCaret();
}
void ScintillaEditorBridge::mouseMoveEvent(QMouseEvent* event)
{
#ifdef Q_OS_IOS
    if (touchMouseDown_ && event->source() == Qt::MouseEventSynthesizedByQt) {
        const QPointF point = event->position();
        if (touchSelecting_) {
            ScintillaQuick_item::mouseMoveEvent(event);
            refreshSelection(true);
        } else {
            if (!touchScrolling_ && (point - touchOrigin_).manhattanLength() > 12) {
                touchScrolling_ = true;
            }
            if (touchScrolling_) {
                const QPointF delta = touchPrevious_ - point;
                touchScrollRemainder_ += delta.y() / qMax(1, int(send(SCI_TEXTHEIGHT, 0)));
                const int rows = int(touchScrollRemainder_);
                if (rows) {
                    scrollVertical(int(send(SCI_GETFIRSTVISIBLELINE)) + rows);
                    touchScrollRemainder_ -= rows;
                }
                if (!autoWrap_)
                    scrollHorizontal(int(send(SCI_GETXOFFSET)) + qRound(delta.x()));
                else if (send(SCI_GETXOFFSET) != 0) scrollHorizontal(0);
            }
        }
        touchPrevious_ = point;
        event->accept();
        return;
    }
#endif
    ScintillaQuick_item::mouseMoveEvent(event);
    refreshSelection(!programmatic_);
}
void ScintillaEditorBridge::mouseDoubleClickEvent(QMouseEvent* event)
{
#ifdef Q_OS_IOS
    if (event->source() == Qt::MouseEventSynthesizedByQt) {
        if (!touchSelecting_) {
            touchSelecting_ = touchMouseDown_ = true;
            touchScrolling_ = false;
            setKeepMouseGrab(true);
            ScintillaQuick_item::mousePressEvent(event);
            refreshSelection(true);
        }
        event->accept();
        return;
    }
#endif
    ScintillaQuick_item::mouseDoubleClickEvent(event);
}
void ScintillaEditorBridge::mouseUngrabEvent()
{
    if (touchMouseDown_) lastTouchTapTime_ = 0;
    if (touchMouseDown_ && touchSelecting_) {
        QMouseEvent release(QEvent::MouseButtonRelease, touchPrevious_, mapToGlobal(touchPrevious_),
            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        ScintillaQuick_item::mouseReleaseEvent(&release);
        refreshSelection(true);
    }
    touchMouseDown_ = touchSelecting_ = touchScrolling_ = false;
    setKeepMouseGrab(false);
    ScintillaQuick_item::mouseUngrabEvent();
}
void ScintillaEditorBridge::wheelEvent(QWheelEvent* event)
{
    beginUserInteraction();
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    if (!autoWrap_ && (shift || std::abs(event->pixelDelta().x()) > std::abs(event->pixelDelta().y())
        || std::abs(event->angleDelta().x()) > std::abs(event->angleDelta().y()))) {
        const int pixels = shift ? event->pixelDelta().y() : event->pixelDelta().x();
        const int angle = shift ? event->angleDelta().y() : event->angleDelta().x();
        const qreal delta = pixels ? pixels : qreal(angle) / 120
            * send(SCI_TEXTWIDTH, STYLE_DEFAULT, reinterpret_cast<sptr_t>("M"))
            * QGuiApplication::styleHints()->wheelScrollLines();
        scrollHorizontal(int(send(SCI_GETXOFFSET)) - qRound(delta));
        event->accept();
        return;
    }
    if (event->phase() == Qt::ScrollBegin) wheelRemainder_ = 0;
    if (!event->pixelDelta().isNull()
        && (event->phase() != Qt::NoScrollPhase || event->angleDelta().isNull())) {
        wheelRemainder_ -= qreal(event->pixelDelta().y()) / qMax(1, int(send(SCI_TEXTHEIGHT, 0)));
    } else {
        wheelRemainder_ -= qreal(event->angleDelta().y()) / 120 * QGuiApplication::styleHints()->wheelScrollLines();
    }
    const int rows = int(std::trunc(wheelRemainder_));
    wheelRemainder_ -= rows;
    if (rows != 0) scrollVertical(int(send(SCI_GETFIRSTVISIBLELINE)) + rows);
    if (event->phase() == Qt::ScrollEnd) wheelRemainder_ = 0;
    event->accept();
}

void ScintillaEditorBridge::inputMethodEvent(QInputMethodEvent* event)
{
#ifndef Q_OS_IOS
    if (controller_ && controller_->imeInputDisabled()) { event->accept(); return; }
#endif
    const bool wasComposing = imeComposing_;
    const bool changesText = wasComposing || !event->preeditString().isEmpty()
        || !event->commitString().isEmpty() || event->replacementLength() != 0;
    {
        QScopedValueRollback guard(handlingIme_, true);
        imeComposing_ = !event->preeditString().isEmpty();
        auto attributes = event->attributes();
#ifdef Q_OS_IOS
        if (event->replacementStart() != 0 || event->replacementLength() != 0)
            attributes.removeIf([](const auto& attribute) { return attribute.type == QInputMethodEvent::Selection; });
#endif
        QInputMethodEvent normalized(event->preeditString(), attributes);
        const QString commit = controller_ && controller_->halfWidthInputEnabled() ? normalizeSimaiInput(event->commitString()) : event->commitString();
        normalized.setCommitString(commit, event->replacementStart(), event->replacementLength());
        const bool committed = !commit.isEmpty() || event->replacementLength() != 0;
        if (committed) send(SCI_BEGINUNDOACTION);
#ifdef Q_OS_IOS
        if (event->replacementStart() != 0 || event->replacementLength() != 0) {
            if (wasComposing) {
                QInputMethodEvent cancel;
                ScintillaQuick_item::inputMethodEvent(&cancel);
            }
            const int start = qBound(0, cursorPosition() + event->replacementStart(), int(document_.text().size()));
            const int end = qBound(start, start + event->replacementLength(), int(document_.text().size()));
            select(start, end);
            normalized.setCommitString(commit);
            if (commit.isEmpty() && event->replacementLength() != 0)
                sends(SCI_REPLACESEL, 0, "");
        }
#endif
        ScintillaQuick_item::inputMethodEvent(&normalized);
        if (controller_ && committed && commit.size() == 1 && !imeComposing_
            && event->replacementStart() == 0 && event->replacementLength() == 0
            && controller_->autoCompletionEnabled()
            && !controller_->overwriteMode() && !controller_->completionActive()) {
            const QChar glyph = commit.front();
            const QChar closing = miacode::editor::closingBracketFor(glyph);
            bool closingPresent = false;
            if (!closing.isNull()) {
                const int caret = cursorPosition();
                if (caret >= document_.text().size() || document_.text().at(caret) != closing) {
                    const QByteArray closingText = QString(closing).toUtf8();
                    sends(SCI_INSERTTEXT, send(SCI_GETCURRENTPOS), closingText.constData());
                }
                closingPresent = true;
            }
            controller_->triggerCompletion(glyph, document_.text(), cursorPosition(), closingPresent);
        }
        if (committed) send(SCI_ENDUNDOACTION);
        event->setAccepted(normalized.isAccepted());
    }
    emit imeComposingChanged();
    if (imeComposing_) {
        if (syncController_) syncController_->setTouchPadControlHold(false);
        publishContext(false);
        refreshSelection(false);
    } else if (changesText) textMutated();
    else refreshSelection(false);
}
}
