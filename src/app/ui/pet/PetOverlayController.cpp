#include "app/ui/pet/PetOverlayController.h"

#include "app/services/PreferenceDocument.h"

#include <QGuiApplication>
#include <QCursor>
#include <QJsonObject>
#include <QRect>
#include <QScreen>

#include <limits>

namespace miacode::ui {
#ifdef Q_OS_MACOS
void attachPetNativeWindow(QWindow* window);
#endif
namespace {

constexpr auto kUiSection = "ui";
constexpr auto kPetVisible = "pet_overlay_visible";
constexpr auto kPetHasPosition = "pet_overlay_has_position";
constexpr auto kPetX = "pet_overlay_x";
constexpr auto kPetY = "pet_overlay_y";
constexpr auto kPetSize = "pet_overlay_size";
constexpr auto kPetCameraMirrored = "pet_overlay_camera_mirrored";
constexpr int kScreenMargin = 8;
constexpr int kEdgeSnapDistance = 24;

QJsonObject loadUiObject()
{
    return PreferenceDocument::loadPreferencesObject()
        .value(QLatin1String(kUiSection)).toObject();
}

void storePetValues(bool visible, bool hasPosition, int x, int y, int size, bool cameraMirrored)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject ui = root.value(QLatin1String(kUiSection)).toObject();
    ui.insert(QLatin1String(kPetVisible), visible);
    ui.insert(QLatin1String(kPetHasPosition), hasPosition);
    ui.insert(QLatin1String(kPetSize), size);
    ui.insert(QLatin1String(kPetCameraMirrored), cameraMirrored);
    ui.remove(QStringLiteral("pet_overlay_camera_angle"));
    if (hasPosition) {
        ui.insert(QLatin1String(kPetX), x);
        ui.insert(QLatin1String(kPetY), y);
    }
    root.insert(QLatin1String(kUiSection), ui);
    PreferenceDocument::savePreferencesObject(root);
}

QScreen* screenForWindowRect(const QRect& windowRect)
{
    QScreen* selected = nullptr;
    qint64 largestIntersection = 0;
    qint64 nearestDistance = std::numeric_limits<qint64>::max();
    const QPoint center = windowRect.center();

    for (QScreen* screen : QGuiApplication::screens()) {
        const QRect available = screen->availableGeometry();
        const QRect intersection = available.intersected(windowRect);
        const qint64 area = static_cast<qint64>(intersection.width()) * intersection.height();
        if (area > largestIntersection) {
            largestIntersection = area;
            selected = screen;
        }
        if (largestIntersection == 0) {
            const QPoint delta = center - available.center();
            const qint64 distance = static_cast<qint64>(delta.x()) * delta.x()
                + static_cast<qint64>(delta.y()) * delta.y();
            if (distance < nearestDistance) {
                nearestDistance = distance;
                selected = screen;
            }
        }
    }
    return selected != nullptr ? selected : QGuiApplication::primaryScreen();
}

int snapAndClamp(int value, int nearEdge, int farEdge, int extent)
{
    const int minimum = nearEdge + kScreenMargin;
    const int maximum = qMax(minimum, farEdge - extent - kScreenMargin + 1);
    if (qAbs(value - minimum) <= kEdgeSnapDistance) {
        return minimum;
    }
    if (qAbs(value - maximum) <= kEdgeSnapDistance) {
        return maximum;
    }
    return qBound(minimum, value, maximum);
}

} // namespace

PetOverlayController::PetOverlayController(QObject* parent)
    : QObject(parent)
{
    const QJsonObject ui = loadUiObject();
    visible_ = ui.value(QLatin1String(kPetVisible)).toBool(false);
    hasStoredPosition_ = ui.value(QLatin1String(kPetHasPosition)).toBool(false);
    storedX_ = ui.value(QLatin1String(kPetX)).toInt();
    storedY_ = ui.value(QLatin1String(kPetY)).toInt();
    size_ = qBound(140, ui.value(QLatin1String(kPetSize)).toInt(220), 440);
    cameraMirrored_ = ui.value(QLatin1String(kPetCameraMirrored)).toBool(false);
}

bool PetOverlayController::visible() const
{
    return visible_;
}

void PetOverlayController::setVisible(bool value)
{
    if (visible_ == value) {
        return;
    }
    visible_ = value;
    storePetValues(visible_, hasStoredPosition_, storedX_, storedY_, size_, cameraMirrored_);
    emit visibleChanged();
}

QVariantMap PetOverlayController::positionMap(int x, int y) const
{
    return QVariantMap{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}

void PetOverlayController::setSize(int value)
{
    value = qBound(140, value, 440);
    if (size_ == value)
        return;
    size_ = value;
    storePetValues(visible_, hasStoredPosition_, storedX_, storedY_, size_, cameraMirrored_);
    emit sizeChanged();
}

void PetOverlayController::prepareWindow(QWindow* window)
{
#ifdef Q_OS_MACOS
    // Qt Cocoa 据此让工具窗口在应用失焦后保持可见。
    window->setProperty("_q_macAlwaysShowToolWindow", true);
    window->setProperty("_q_showWithoutActivating", true);
    window->setFlags((window->flags() & ~Qt::WindowDoesNotAcceptFocus) | Qt::Tool);
    attachPetNativeWindow(window);
#elif defined(Q_OS_WIN)
    window->setProperty("_q_showWithoutActivating", true);
    window->setFlag(Qt::WindowDoesNotAcceptFocus, true);
#else
    Q_UNUSED(window);
#endif
}

void PetOverlayController::setCameraMirrored(bool value)
{
    if (cameraMirrored_ == value)
        return;
    cameraMirrored_ = value;
    storePetValues(visible_, hasStoredPosition_, storedX_, storedY_, size_, cameraMirrored_);
    emit cameraMirroredChanged();
}

QVariantMap PetOverlayController::pointerPosition() const
{
    const QPoint position = QCursor::pos();
    return positionMap(position.x(), position.y());
}

QVariantMap PetOverlayController::initialPosition(int width, int height,
                                                  int anchorX, int anchorY,
                                                  int anchorWidth, int anchorHeight) const
{
    int x = storedX_;
    int y = storedY_;
    if (!hasStoredPosition_) {
        QScreen* screen = screenForWindowRect(QRect(anchorX, anchorY, anchorWidth, anchorHeight));
        const QRect available = screen != nullptr ? screen->availableGeometry() : QRect();
        x = available.right() - width - 48;
        y = available.bottom() - height - 72;
    }
    return correctedPosition(x, y, width, height);
}

QVariantMap PetOverlayController::correctedPosition(int x, int y, int width, int height) const
{
    QScreen* screen = screenForWindowRect(QRect(x, y, width, height));
    if (screen == nullptr) {
        return positionMap(x, y);
    }
    const QRect available = screen->availableGeometry();
    return positionMap(
        snapAndClamp(x, available.left(), available.right(), width),
        snapAndClamp(y, available.top(), available.bottom(), height));
}

void PetOverlayController::savePosition(int x, int y, int width, int height)
{
    const QVariantMap corrected = correctedPosition(x, y, width, height);
    storedX_ = corrected.value(QStringLiteral("x")).toInt();
    storedY_ = corrected.value(QStringLiteral("y")).toInt();
    hasStoredPosition_ = true;
    storePetValues(visible_, hasStoredPosition_, storedX_, storedY_, size_, cameraMirrored_);
}

} // namespace miacode::ui
