#pragma once

#include <QQuickRhiItem>
#include <QQmlListProperty>
#include <QQuaternion>
#include <QVector2D>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

namespace miacode::ui {

class PetMesh : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<QVector3D> positions MEMBER positions)
    Q_PROPERTY(QList<QVector2D> uv0s MEMBER uv0s)
    Q_PROPERTY(QList<int> indexes MEMBER indexes)
public:
    explicit PetMesh(QObject* parent = nullptr) : QObject(parent) {}
    QList<QVector3D> positions;
    QList<QVector2D> uv0s;
    QList<int> indexes;
};

class PetModelNode : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_CLASSINFO("DefaultProperty", "nodes")
    Q_PROPERTY(QQmlListProperty<QObject> nodes READ nodes)
    Q_PROPERTY(QVector3D position MEMBER position NOTIFY transformChanged)
    Q_PROPERTY(QQuaternion rotation MEMBER rotation NOTIFY transformChanged)
    Q_PROPERTY(QVector3D eulerRotation MEMBER eulerRotation NOTIFY transformChanged)
    Q_PROPERTY(QVector3D scale MEMBER scale NOTIFY transformChanged)
    Q_PROPERTY(bool visible MEMBER visible NOTIFY transformChanged)
    Q_PROPERTY(PetMesh* geometry MEMBER geometry NOTIFY transformChanged)
public:
    explicit PetModelNode(QObject* parent = nullptr) : QObject(parent) {}
    QQmlListProperty<QObject> nodes() { return {this, &children_}; }
    const QList<QObject*>& childNodes() const { return children_; }
    QVector3D position;
    QQuaternion rotation;
    QVector3D eulerRotation;
    QVector3D scale{1, 1, 1};
    bool visible = true;
    PetMesh* geometry = nullptr;
signals:
    void transformChanged();
private:
    QList<QObject*> children_;
};

class PetScene : public QQuickRhiItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(PetModelNode* model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(bool cameraMirrored READ cameraMirrored WRITE setCameraMirrored NOTIFY cameraMirroredChanged)
public:
    explicit PetScene(QQuickItem* parent = nullptr);
    PetModelNode* model() const { return model_; }
    void setModel(PetModelNode* model);
    bool cameraMirrored() const { return cameraMirrored_; }
    void setCameraMirrored(bool value);
    Q_INVOKABLE void advanceFrame() { update(); }
signals:
    void modelChanged();
    void cameraMirroredChanged();
protected:
    QQuickRhiItemRenderer* createRenderer() override;
private:
    PetModelNode* model_ = nullptr;
    bool cameraMirrored_ = false;
};

} // namespace miacode::ui
