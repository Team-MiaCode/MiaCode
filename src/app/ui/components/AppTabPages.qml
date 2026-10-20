import QtQuick
import QtQuick.Layouts

StackLayout {
    implicitHeight: currentIndex >= 0 && currentIndex < children.length
                    ? children[currentIndex].implicitHeight : 0
}
