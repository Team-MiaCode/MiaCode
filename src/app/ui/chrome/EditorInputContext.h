#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>
#include <QVector>

class QKeyEvent;

namespace miacode::ui {

class EditorInputContext : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    // The chart editor owns typing keys: it types them before single-key shortcuts,
    // and typing left unclaimed in neutral workbench areas returns to it.
    Q_PROPERTY(QQuickItem* typingTarget READ typingTarget WRITE setTypingTarget NOTIFY typingTargetChanged)
    Q_PROPERTY(bool workbenchCommandsAllowed READ workbenchCommandsAllowed NOTIFY contextChanged)
    Q_PROPERTY(QObject* textInput READ textInput NOTIFY contextChanged)
    Q_PROPERTY(bool modalBlocked READ modalBlocked NOTIFY contextChanged)
    Q_PROPERTY(bool editingCommandsAllowed READ editingCommandsAllowed NOTIFY contextChanged)
    Q_PROPERTY(bool nonTextCommandsAllowed READ nonTextCommandsAllowed NOTIFY contextChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY contextChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY contextChanged)
    Q_PROPERTY(bool canCut READ canCut NOTIFY contextChanged)
    Q_PROPERTY(bool canCopy READ canCopy NOTIFY contextChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY contextChanged)
    Q_PROPERTY(bool canSelectAll READ canSelectAll NOTIFY contextChanged)

public:
    explicit EditorInputContext(QObject* parent = nullptr);
    QQuickWindow* window() const { return window_; }
    void setWindow(QQuickWindow* window);
    QQuickItem* typingTarget() const { return typingTarget_; }
    void setTypingTarget(QQuickItem* target);
    Q_INVOKABLE void commitPendingEdits(QQuickItem* editorPane);
    bool workbenchCommandsAllowed() const;
    QObject* textInput() const { return textInput_; }
    bool modalBlocked() const { return modalBlocked_; }
    bool editingCommandsAllowed() const { return !modalBlocked_ && !composing_; }
    // Workbench commands that would otherwise swallow typing or menu navigation, such as plain Space.
    bool nonTextCommandsAllowed() const;
    bool canUndo() const { return canEdit("undo"); }
    bool canRedo() const { return canEdit("redo"); }
    bool canCut() const { return canEdit("cut"); }
    bool canCopy() const { return canEdit("copy"); }
    bool canPaste() const { return canEdit("paste"); }
    bool canSelectAll() const { return canEdit("selectAll"); }
    Q_INVOKABLE bool performEdit(const QString& operation);

signals:
    void windowChanged();
    void typingTargetChanged();
    void contextChanged();

private:
    bool canEdit(const QString& operation) const;
    bool typingTargetWritable() const;
    bool redirectTyping(QKeyEvent* event);
    bool eventFilter(QObject* object, QEvent* event) override;
    void watch(QObject* object, const QList<QByteArray>& signalSignatures,
               QVector<QMetaObject::Connection>& connections);
    QPointer<QQuickWindow> window_;
    QPointer<QQuickItem> typingTarget_;
    QPointer<QObject> textInput_;
    QPointer<QObject> focusedItem_;
    QMetaObject::Connection windowFocusConnection_;
    QVector<QMetaObject::Connection> inputConnections_;
    QVector<QMetaObject::Connection> focusConnections_;
    bool modalBlocked_ = false;
    bool composing_ = false;

private slots:
    void refresh();
};

} // namespace miacode::ui
