#pragma once

#include <QObject>
#include <QVariantMap>
#include <QWindow>

namespace miacode::ui {

class PetOverlayController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(int size READ size WRITE setSize NOTIFY sizeChanged)
    Q_PROPERTY(bool cameraMirrored READ cameraMirrored WRITE setCameraMirrored NOTIFY cameraMirroredChanged)

public:
    explicit PetOverlayController(QObject* parent = nullptr);

    bool visible() const;
    void setVisible(bool value);
    int size() const { return size_; }
    void setSize(int value);
    bool cameraMirrored() const { return cameraMirrored_; }
    void setCameraMirrored(bool value);
    Q_INVOKABLE void prepareWindow(QWindow* window);
    Q_INVOKABLE QVariantMap pointerPosition() const;

    Q_INVOKABLE QVariantMap initialPosition(int width, int height,
                                            int anchorX, int anchorY,
                                            int anchorWidth, int anchorHeight) const;
    Q_INVOKABLE QVariantMap correctedPosition(int x, int y, int width, int height) const;
    Q_INVOKABLE void savePosition(int x, int y, int width, int height);

signals:
    void visibleChanged();
    void sizeChanged();
    void cameraMirroredChanged();

private:
    QVariantMap positionMap(int x, int y) const;

    bool visible_ = false;
    int size_ = 220;
    bool cameraMirrored_ = false;
    bool hasStoredPosition_ = false;
    int storedX_ = 0;
    int storedY_ = 0;
};

} // namespace miacode::ui
