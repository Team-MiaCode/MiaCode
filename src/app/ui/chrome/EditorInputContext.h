#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

namespace miacode::ui {

class EditorInputContext : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)

public:
    explicit EditorInputContext(QObject* parent = nullptr) : QObject(parent) {}
    QQuickWindow* window() const { return window_; }
    void setWindow(QQuickWindow* window);
    Q_INVOKABLE void commitPendingEdits(QQuickItem* editorPane);

signals:
    void windowChanged();

private:
    QPointer<QQuickWindow> window_;
};

} // namespace miacode::ui
