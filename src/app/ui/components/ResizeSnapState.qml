import QtQuick

QtObject {
    id: root

    required property real minimumSize
    required property real maximumSize
    required property var targets
    property real enterDistance: 8
    property real leaveDistance: 14
    property real alignmentTolerance: 0.5
    property bool active: false
    property real rawSize: 0
    property int selectedIndex: -1
    readonly property bool snapped: active && reachable(selectedIndex)
    readonly property real targetSize: snapped ? targets[selectedIndex].size : rawSize
    readonly property real releaseSize: snapped
        ? (targets[selectedIndex].releaseSize ?? targetSize) : rawSize
    readonly property bool releaseReachable: snapped
        && releaseSize >= minimumSize && releaseSize <= maximumSize

    onTargetsChanged: updateSnap()
    onMinimumSizeChanged: updateSnap()
    onMaximumSizeChanged: updateSnap()

    function reachable(index) {
        return index >= 0 && index < targets.length && isFinite(targets[index].size)
            && targets[index].size >= minimumSize && targets[index].size <= maximumSize
    }

    function begin(size) {
        rawSize = size
        selectedIndex = -1
        active = true
        updateSnap(true)
    }

    function update(size) {
        rawSize = size
        updateSnap(false)
    }

    function updateSnap(atPress = false) {
        if (!active)
            return
        if (reachable(selectedIndex)
                && Math.abs(rawSize - targets[selectedIndex].size)
                    <= (targets[selectedIndex].leaveDistance ?? leaveDistance))
            return
        let closest = -1
        let closestDistance = Infinity
        for (let index = 0; index < targets.length; ++index) {
            if (!reachable(index))
                continue
            const distance = Math.abs(rawSize - targets[index].size)
            const threshold = atPress ? alignmentTolerance
                : (targets[index].enterDistance ?? enterDistance)
            if (distance <= threshold && distance < closestDistance) {
                closest = index
                closestDistance = distance
            }
        }
        selectedIndex = closest
    }

    function end() {
        active = false
        selectedIndex = -1
    }
}
