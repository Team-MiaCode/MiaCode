// 桌宠狐狸定稿模型：骨骼、盒式 UV 与动画以官方基岩包为来源。
import QtQuick
import MiaCode.UI

PetModelNode {
    id: fox
    property string clip: ""
    property real phase: 0
    property real lifeTime: 0
    property real sleepTime: 0
    property real wiggleTime: 0
    onClipChanged: {
        if (clip === "animation.fox.sleep") sleepTime = 0
        if (clip === "animation.fox.wiggle") wiggleTime = 0
    }
    onPhaseChanged: {
        if (clip === "animation.fox.sleep") sleepTime = phase
        if (clip === "animation.fox.wiggle") wiggleTime = phase
    }
    property real sittingWeight: clip === "animation.fox.sit" ? 1 : 0
    property real sleepingWeight: clip === "animation.fox.sleep" ? 1 : 0
    eulerRotation.y: -115 * sleepingWeight
    property real wiggleWeight: clip === "animation.fox.wiggle" ? 1 : 0
    readonly property int transitionDuration: clip === "animation.fox.wiggle" ? 150 : 650
    readonly property bool sleeping: sleepingWeight > 0.5
    readonly property real idleMotion: (1 - sleepingWeight) * (1 - wiggleWeight)
    readonly property real breathing: Math.sin(lifeTime * Math.PI / 2) * (1 - wiggleWeight)
    Behavior on sittingWeight { NumberAnimation { duration: fox.transitionDuration; easing.type: Easing.InOutSine } }
    Behavior on sleepingWeight { NumberAnimation { duration: fox.transitionDuration; easing.type: Easing.InOutSine } }
    Behavior on wiggleWeight { NumberAnimation { duration: fox.transitionDuration; easing.type: Easing.InOutSine } }
    function rotationZYX(x, y, z) {
        const rx = -x * Math.PI / 360
        const ry = -y * Math.PI / 360
        const rz = z * Math.PI / 360
        const cx = Math.cos(rx), sx = Math.sin(rx)
        const cy = Math.cos(ry), sy = Math.sin(ry)
        const cz = Math.cos(rz), sz = Math.sin(rz)
        return Qt.quaternion(cx*cy*cz + sx*sy*sz,
                             sx*cy*cz - cx*sy*sz,
                             cx*sy*cz + sx*cy*sz,
                             cx*cy*sz - sx*sy*cz)
    }
    PetModelNode {
        objectName: "world"
        position: Qt.vector3d(-(0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16)
        rotation: fox.rotationZYX((0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))))
        PetModelNode {
            objectName: "root"
            position: Qt.vector3d(-(0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16)
            rotation: fox.rotationZYX((0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))))
            PetModelNode {
                objectName: "body"
                position: Qt.vector3d(-(0 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16, (8 + (0 + fox.sittingWeight * ((0 + 1.0) - (0)) + fox.sleepingWeight * ((0 + -4.8) - (0)) + fox.wiggleWeight * ((0 + -1.8) - (0)))) / 16, (0 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16)
                rotation: fox.rotationZYX(((0 + (-(0))) + fox.sittingWeight * (((0 + (-(0))) + -60.0) - ((0 + (-(0))))) + fox.sleepingWeight * (((0 + (-(0))) + 0.0) - ((0 + (-(0))))) + fox.wiggleWeight * (((0 + (-(0))) + 0.0) - ((0 + (-(0)))))), ((0 + 0.0) + fox.sittingWeight * (((0 + 0.0) + 0.0) - ((0 + 0.0))) + fox.sleepingWeight * (((0 + 0.0) + 0.0) - ((0 + 0.0))) + fox.wiggleWeight * (((0 + 0.0) + ((Math.cos((((fox.lifeTime * 20.0) * 53.7)) * Math.PI / 180) * 5.0) - ((0 + 0.0)))) - ((0 + 0.0)))), ((0 + 0.0) + fox.sittingWeight * (((0 + 0.0) + 0.0) - ((0 + 0.0))) + fox.sleepingWeight * (((0 + 0.0) + -90.0) - ((0 + 0.0))) + fox.wiggleWeight * (((0 + 0.0) + 0.0) - ((0 + 0.0)))))
                PetModelNode {
                    objectName: "body_mesh"

                    scale: Qt.vector3d(1, 1 + fox.breathing * 0.04, 1 + fox.breathing * 0.02)
                    geometry: PetMesh {
                        positions: [
                            Qt.vector3d(0.1875, 0.1875, -0.1875),
                            Qt.vector3d(0.1875, -0.1875, -0.1875),
                            Qt.vector3d(0.1875, 0.1875, 0.5),
                            Qt.vector3d(0.1875, -0.1875, 0.5),
                            Qt.vector3d(-0.1875, -0.1875, -0.1875),
                            Qt.vector3d(-0.1875, 0.1875, -0.1875),
                            Qt.vector3d(-0.1875, -0.1875, 0.5),
                            Qt.vector3d(-0.1875, 0.1875, 0.5),
                            Qt.vector3d(-0.1875, -0.1875, -0.1875),
                            Qt.vector3d(0.1875, -0.1875, -0.1875),
                            Qt.vector3d(-0.1875, 0.1875, -0.1875),
                            Qt.vector3d(0.1875, 0.1875, -0.1875),
                            Qt.vector3d(-0.1875, 0.1875, 0.5),
                            Qt.vector3d(0.1875, 0.1875, 0.5),
                            Qt.vector3d(-0.1875, -0.1875, 0.5),
                            Qt.vector3d(0.1875, -0.1875, 0.5),
                            Qt.vector3d(-0.1875, 0.1875, -0.1875),
                            Qt.vector3d(0.1875, 0.1875, -0.1875),
                            Qt.vector3d(-0.1875, 0.1875, 0.5),
                            Qt.vector3d(0.1875, 0.1875, 0.5),
                            Qt.vector3d(0.1875, -0.1875, -0.1875),
                            Qt.vector3d(-0.1875, -0.1875, -0.1875),
                            Qt.vector3d(0.1875, -0.1875, 0.5),
                            Qt.vector3d(-0.1875, -0.1875, 0.5)
                        ]

                        uv0s: [
                            Qt.vector2d(0.468994141, 0.343261719),
                            Qt.vector2d(0.562255859, 0.343261719),
                            Qt.vector2d(0.468994141, 0.00048828125),
                            Qt.vector2d(0.562255859, 0.00048828125),
                            Qt.vector2d(0.656494141, 0.343261719),
                            Qt.vector2d(0.749755859, 0.343261719),
                            Qt.vector2d(0.656494141, 0.00048828125),
                            Qt.vector2d(0.749755859, 0.00048828125),
                            Qt.vector2d(0.656005859, 0.344238281),
                            Qt.vector2d(0.562744141, 0.344238281),
                            Qt.vector2d(0.656005859, 0.530761719),
                            Qt.vector2d(0.562744141, 0.530761719),
                            Qt.vector2d(0.749755859, 0.530761719),
                            Qt.vector2d(0.656494141, 0.530761719),
                            Qt.vector2d(0.749755859, 0.344238281),
                            Qt.vector2d(0.656494141, 0.344238281),
                            Qt.vector2d(0.750244141, 0.343261719),
                            Qt.vector2d(0.843505859, 0.343261719),
                            Qt.vector2d(0.750244141, 0.00048828125),
                            Qt.vector2d(0.843505859, 0.00048828125),
                            Qt.vector2d(0.562744141, 0.343261719),
                            Qt.vector2d(0.656005859, 0.343261719),
                            Qt.vector2d(0.562744141, 0.00048828125),
                            Qt.vector2d(0.656005859, 0.00048828125)
                        ]
                        indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21]
                    }
                }
                PetModelNode {
                    objectName: "head"
                    position: Qt.vector3d(-(0 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + 1.8) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16, (0 + (0 + fox.sittingWeight * ((0 + 3.0) - (0)) + fox.sleepingWeight * ((0 + -0.4) - (0)) + fox.wiggleWeight * ((0 + -0.8) - (0)))) / 16, (-3 + (0 + fox.sittingWeight * ((0 + -3.0) - (0)) + fox.sleepingWeight * ((0 + -2.0) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16)
                    rotation: fox.rotationZYX((0 + fox.sittingWeight * ((0 + 60.0) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * ((0 + Math.max((-5.0), Math.min(0, ((-5.0) * (fox.wiggleTime / 2.0))))) - (0))) + fox.idleMotion * Math.sin(fox.lifeTime * 1.3) * 2.5, (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + -115.0) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + ((Math.cos(((fox.sleepTime * 160.0)) * Math.PI / 180) + 90) - (0))) - (0)) + fox.wiggleWeight * ((0 + Math.max(0, Math.min(25.0, (25.0 * (fox.wiggleTime / 2.0))))) - (0))) + fox.idleMotion * Math.sin(fox.lifeTime * 0.7) * 1.5)
                    PetModelNode {
                        objectName: "head_mesh"

                        visible: !fox.sleeping
                        geometry: PetMesh {
                            positions: [
                                Qt.vector3d(0.25, 0.125, 0),
                                Qt.vector3d(0.25, 0.125, -0.375),
                                Qt.vector3d(0.25, -0.25, 0),
                                Qt.vector3d(0.25, -0.25, -0.375),
                                Qt.vector3d(-0.25, 0.125, -0.375),
                                Qt.vector3d(-0.25, 0.125, 0),
                                Qt.vector3d(-0.25, -0.25, -0.375),
                                Qt.vector3d(-0.25, -0.25, 0),
                                Qt.vector3d(-0.25, 0.125, -0.375),
                                Qt.vector3d(0.25, 0.125, -0.375),
                                Qt.vector3d(-0.25, 0.125, 0),
                                Qt.vector3d(0.25, 0.125, 0),
                                Qt.vector3d(-0.25, -0.25, 0),
                                Qt.vector3d(0.25, -0.25, 0),
                                Qt.vector3d(-0.25, -0.25, -0.375),
                                Qt.vector3d(0.25, -0.25, -0.375),
                                Qt.vector3d(-0.25, 0.125, 0),
                                Qt.vector3d(0.25, 0.125, 0),
                                Qt.vector3d(-0.25, -0.25, 0),
                                Qt.vector3d(0.25, -0.25, 0),
                                Qt.vector3d(0.25, 0.125, -0.375),
                                Qt.vector3d(-0.25, 0.125, -0.375),
                                Qt.vector3d(0.25, -0.25, -0.375),
                                Qt.vector3d(-0.25, -0.25, -0.375),
                                Qt.vector3d(0.25, 0.25, -0.25),
                                Qt.vector3d(0.25, 0.25, -0.3125),
                                Qt.vector3d(0.25, 0.125, -0.25),
                                Qt.vector3d(0.25, 0.125, -0.3125),
                                Qt.vector3d(0.125, 0.25, -0.3125),
                                Qt.vector3d(0.125, 0.25, -0.25),
                                Qt.vector3d(0.125, 0.125, -0.3125),
                                Qt.vector3d(0.125, 0.125, -0.25),
                                Qt.vector3d(0.125, 0.25, -0.3125),
                                Qt.vector3d(0.25, 0.25, -0.3125),
                                Qt.vector3d(0.125, 0.25, -0.25),
                                Qt.vector3d(0.25, 0.25, -0.25),
                                Qt.vector3d(0.125, 0.125, -0.25),
                                Qt.vector3d(0.25, 0.125, -0.25),
                                Qt.vector3d(0.125, 0.125, -0.3125),
                                Qt.vector3d(0.25, 0.125, -0.3125),
                                Qt.vector3d(0.125, 0.25, -0.25),
                                Qt.vector3d(0.25, 0.25, -0.25),
                                Qt.vector3d(0.125, 0.125, -0.25),
                                Qt.vector3d(0.25, 0.125, -0.25),
                                Qt.vector3d(0.25, 0.25, -0.3125),
                                Qt.vector3d(0.125, 0.25, -0.3125),
                                Qt.vector3d(0.25, 0.125, -0.3125),
                                Qt.vector3d(0.125, 0.125, -0.3125),
                                Qt.vector3d(-0.125, 0.25, -0.25),
                                Qt.vector3d(-0.125, 0.25, -0.3125),
                                Qt.vector3d(-0.125, 0.125, -0.25),
                                Qt.vector3d(-0.125, 0.125, -0.3125),
                                Qt.vector3d(-0.25, 0.25, -0.3125),
                                Qt.vector3d(-0.25, 0.25, -0.25),
                                Qt.vector3d(-0.25, 0.125, -0.3125),
                                Qt.vector3d(-0.25, 0.125, -0.25),
                                Qt.vector3d(-0.25, 0.25, -0.3125),
                                Qt.vector3d(-0.125, 0.25, -0.3125),
                                Qt.vector3d(-0.25, 0.25, -0.25),
                                Qt.vector3d(-0.125, 0.25, -0.25),
                                Qt.vector3d(-0.25, 0.125, -0.25),
                                Qt.vector3d(-0.125, 0.125, -0.25),
                                Qt.vector3d(-0.25, 0.125, -0.3125),
                                Qt.vector3d(-0.125, 0.125, -0.3125),
                                Qt.vector3d(-0.25, 0.25, -0.25),
                                Qt.vector3d(-0.125, 0.25, -0.25),
                                Qt.vector3d(-0.25, 0.125, -0.25),
                                Qt.vector3d(-0.125, 0.125, -0.25),
                                Qt.vector3d(-0.125, 0.25, -0.3125),
                                Qt.vector3d(-0.25, 0.25, -0.3125),
                                Qt.vector3d(-0.125, 0.125, -0.3125),
                                Qt.vector3d(-0.25, 0.125, -0.3125),
                                Qt.vector3d(0.125, -0.125, -0.375),
                                Qt.vector3d(0.125, -0.125, -0.5625),
                                Qt.vector3d(0.125, -0.25, -0.375),
                                Qt.vector3d(0.125, -0.25, -0.5625),
                                Qt.vector3d(-0.125, -0.125, -0.5625),
                                Qt.vector3d(-0.125, -0.125, -0.375),
                                Qt.vector3d(-0.125, -0.25, -0.5625),
                                Qt.vector3d(-0.125, -0.25, -0.375),
                                Qt.vector3d(-0.125, -0.125, -0.5625),
                                Qt.vector3d(0.125, -0.125, -0.5625),
                                Qt.vector3d(-0.125, -0.125, -0.375),
                                Qt.vector3d(0.125, -0.125, -0.375),
                                Qt.vector3d(-0.125, -0.25, -0.375),
                                Qt.vector3d(0.125, -0.25, -0.375),
                                Qt.vector3d(-0.125, -0.25, -0.5625),
                                Qt.vector3d(0.125, -0.25, -0.5625),
                                Qt.vector3d(-0.125, -0.125, -0.375),
                                Qt.vector3d(0.125, -0.125, -0.375),
                                Qt.vector3d(-0.125, -0.25, -0.375),
                                Qt.vector3d(0.125, -0.25, -0.375),
                                Qt.vector3d(0.125, -0.125, -0.5625),
                                Qt.vector3d(-0.125, -0.125, -0.5625),
                                Qt.vector3d(0.125, -0.25, -0.5625),
                                Qt.vector3d(-0.125, -0.25, -0.5625)
                            ]

                            uv0s: [
                                Qt.vector2d(0.000244140625, 0.812011719),
                                Qt.vector2d(0.0935058594, 0.812011719),
                                Qt.vector2d(0.000244140625, 0.625488281),
                                Qt.vector2d(0.0935058594, 0.625488281),
                                Qt.vector2d(0.218994141, 0.812011719),
                                Qt.vector2d(0.312255859, 0.812011719),
                                Qt.vector2d(0.218994141, 0.625488281),
                                Qt.vector2d(0.312255859, 0.625488281),
                                Qt.vector2d(0.218505859, 0.812988281),
                                Qt.vector2d(0.0939941406, 0.812988281),
                                Qt.vector2d(0.218505859, 0.999511719),
                                Qt.vector2d(0.0939941406, 0.999511719),
                                Qt.vector2d(0.343505859, 0.999511719),
                                Qt.vector2d(0.218994141, 0.999511719),
                                Qt.vector2d(0.343505859, 0.812988281),
                                Qt.vector2d(0.218994141, 0.812988281),
                                Qt.vector2d(0.312744141, 0.812011719),
                                Qt.vector2d(0.437255859, 0.812011719),
                                Qt.vector2d(0.312744141, 0.625488281),
                                Qt.vector2d(0.437255859, 0.625488281),
                                Qt.vector2d(0.0939941406, 0.812011719),
                                Qt.vector2d(0.218505859, 0.812011719),
                                Qt.vector2d(0.0939941406, 0.625488281),
                                Qt.vector2d(0.218505859, 0.625488281),
                                Qt.vector2d(0.000244140625, 0.968261719),
                                Qt.vector2d(0.0153808594, 0.968261719),
                                Qt.vector2d(0.000244140625, 0.906738281),
                                Qt.vector2d(0.0153808594, 0.906738281),
                                Qt.vector2d(0.0471191406, 0.968261719),
                                Qt.vector2d(0.0622558594, 0.968261719),
                                Qt.vector2d(0.0471191406, 0.906738281),
                                Qt.vector2d(0.0622558594, 0.906738281),
                                Qt.vector2d(0.0466308594, 0.969238281),
                                Qt.vector2d(0.0158691406, 0.969238281),
                                Qt.vector2d(0.0466308594, 0.999511719),
                                Qt.vector2d(0.0158691406, 0.999511719),
                                Qt.vector2d(0.0778808594, 0.999511719),
                                Qt.vector2d(0.0471191406, 0.999511719),
                                Qt.vector2d(0.0778808594, 0.969238281),
                                Qt.vector2d(0.0471191406, 0.969238281),
                                Qt.vector2d(0.0627441406, 0.968261719),
                                Qt.vector2d(0.0935058594, 0.968261719),
                                Qt.vector2d(0.0627441406, 0.906738281),
                                Qt.vector2d(0.0935058594, 0.906738281),
                                Qt.vector2d(0.0158691406, 0.968261719),
                                Qt.vector2d(0.0466308594, 0.968261719),
                                Qt.vector2d(0.0158691406, 0.906738281),
                                Qt.vector2d(0.0466308594, 0.906738281),
                                Qt.vector2d(0.343994141, 0.968261719),
                                Qt.vector2d(0.359130859, 0.968261719),
                                Qt.vector2d(0.343994141, 0.906738281),
                                Qt.vector2d(0.359130859, 0.906738281),
                                Qt.vector2d(0.390869141, 0.968261719),
                                Qt.vector2d(0.406005859, 0.968261719),
                                Qt.vector2d(0.390869141, 0.906738281),
                                Qt.vector2d(0.406005859, 0.906738281),
                                Qt.vector2d(0.390380859, 0.969238281),
                                Qt.vector2d(0.359619141, 0.969238281),
                                Qt.vector2d(0.390380859, 0.999511719),
                                Qt.vector2d(0.359619141, 0.999511719),
                                Qt.vector2d(0.421630859, 0.999511719),
                                Qt.vector2d(0.390869141, 0.999511719),
                                Qt.vector2d(0.421630859, 0.969238281),
                                Qt.vector2d(0.390869141, 0.969238281),
                                Qt.vector2d(0.406494141, 0.968261719),
                                Qt.vector2d(0.437255859, 0.968261719),
                                Qt.vector2d(0.406494141, 0.906738281),
                                Qt.vector2d(0.437255859, 0.906738281),
                                Qt.vector2d(0.359619141, 0.968261719),
                                Qt.vector2d(0.390380859, 0.968261719),
                                Qt.vector2d(0.359619141, 0.906738281),
                                Qt.vector2d(0.390380859, 0.906738281),
                                Qt.vector2d(0.000244140625, 0.155761719),
                                Qt.vector2d(0.0466308594, 0.155761719),
                                Qt.vector2d(0.000244140625, 0.0942382812),
                                Qt.vector2d(0.0466308594, 0.0942382812),
                                Qt.vector2d(0.109619141, 0.155761719),
                                Qt.vector2d(0.156005859, 0.155761719),
                                Qt.vector2d(0.109619141, 0.0942382812),
                                Qt.vector2d(0.156005859, 0.0942382812),
                                Qt.vector2d(0.109130859, 0.156738281),
                                Qt.vector2d(0.0471191406, 0.156738281),
                                Qt.vector2d(0.109130859, 0.249511719),
                                Qt.vector2d(0.0471191406, 0.249511719),
                                Qt.vector2d(0.171630859, 0.249511719),
                                Qt.vector2d(0.109619141, 0.249511719),
                                Qt.vector2d(0.171630859, 0.156738281),
                                Qt.vector2d(0.109619141, 0.156738281),
                                Qt.vector2d(0.156494141, 0.155761719),
                                Qt.vector2d(0.218505859, 0.155761719),
                                Qt.vector2d(0.156494141, 0.0942382812),
                                Qt.vector2d(0.218505859, 0.0942382812),
                                Qt.vector2d(0.0471191406, 0.155761719),
                                Qt.vector2d(0.109130859, 0.155761719),
                                Qt.vector2d(0.0471191406, 0.0942382812),
                                Qt.vector2d(0.109130859, 0.0942382812)
                            ]
                            indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21, 24, 26, 25, 26, 27, 25, 28, 30, 29, 30, 31, 29, 32, 34, 33, 34, 35, 33, 36, 38, 37, 38, 39, 37, 40, 42, 41, 42, 43, 41, 44, 46, 45, 46, 47, 45, 48, 50, 49, 50, 51, 49, 52, 54, 53, 54, 55, 53, 56, 58, 57, 58, 59, 57, 60, 62, 61, 62, 63, 61, 64, 66, 65, 66, 67, 65, 68, 70, 69, 70, 71, 69, 72, 74, 73, 74, 75, 73, 76, 78, 77, 78, 79, 77, 80, 82, 81, 82, 83, 81, 84, 86, 85, 86, 87, 85, 88, 90, 89, 90, 91, 89, 92, 94, 93, 94, 95, 93]
                        }
                    }
                    PetModelNode {
                        objectName: "head_sleeping"
                        position: Qt.vector3d(-(0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16)
                        rotation: fox.rotationZYX((0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))))
                        PetModelNode {
                            objectName: "head_sleeping_mesh"

                            visible: fox.sleeping
                            geometry: PetMesh {
                                positions: [
                                    Qt.vector3d(0.25, 0.125, 0),
                                    Qt.vector3d(0.25, 0.125, -0.375),
                                    Qt.vector3d(0.25, -0.25, 0),
                                    Qt.vector3d(0.25, -0.25, -0.375),
                                    Qt.vector3d(-0.25, 0.125, -0.375),
                                    Qt.vector3d(-0.25, 0.125, 0),
                                    Qt.vector3d(-0.25, -0.25, -0.375),
                                    Qt.vector3d(-0.25, -0.25, 0),
                                    Qt.vector3d(-0.25, 0.125, -0.375),
                                    Qt.vector3d(0.25, 0.125, -0.375),
                                    Qt.vector3d(-0.25, 0.125, 0),
                                    Qt.vector3d(0.25, 0.125, 0),
                                    Qt.vector3d(-0.25, -0.25, 0),
                                    Qt.vector3d(0.25, -0.25, 0),
                                    Qt.vector3d(-0.25, -0.25, -0.375),
                                    Qt.vector3d(0.25, -0.25, -0.375),
                                    Qt.vector3d(-0.25, 0.125, 0),
                                    Qt.vector3d(0.25, 0.125, 0),
                                    Qt.vector3d(-0.25, -0.25, 0),
                                    Qt.vector3d(0.25, -0.25, 0),
                                    Qt.vector3d(0.25, 0.125, -0.375),
                                    Qt.vector3d(-0.25, 0.125, -0.375),
                                    Qt.vector3d(0.25, -0.25, -0.375),
                                    Qt.vector3d(-0.25, -0.25, -0.375),
                                    Qt.vector3d(0.25, 0.25, -0.25),
                                    Qt.vector3d(0.25, 0.25, -0.3125),
                                    Qt.vector3d(0.25, 0.125, -0.25),
                                    Qt.vector3d(0.25, 0.125, -0.3125),
                                    Qt.vector3d(0.125, 0.25, -0.3125),
                                    Qt.vector3d(0.125, 0.25, -0.25),
                                    Qt.vector3d(0.125, 0.125, -0.3125),
                                    Qt.vector3d(0.125, 0.125, -0.25),
                                    Qt.vector3d(0.125, 0.25, -0.3125),
                                    Qt.vector3d(0.25, 0.25, -0.3125),
                                    Qt.vector3d(0.125, 0.25, -0.25),
                                    Qt.vector3d(0.25, 0.25, -0.25),
                                    Qt.vector3d(0.125, 0.125, -0.25),
                                    Qt.vector3d(0.25, 0.125, -0.25),
                                    Qt.vector3d(0.125, 0.125, -0.3125),
                                    Qt.vector3d(0.25, 0.125, -0.3125),
                                    Qt.vector3d(0.125, 0.25, -0.25),
                                    Qt.vector3d(0.25, 0.25, -0.25),
                                    Qt.vector3d(0.125, 0.125, -0.25),
                                    Qt.vector3d(0.25, 0.125, -0.25),
                                    Qt.vector3d(0.25, 0.25, -0.3125),
                                    Qt.vector3d(0.125, 0.25, -0.3125),
                                    Qt.vector3d(0.25, 0.125, -0.3125),
                                    Qt.vector3d(0.125, 0.125, -0.3125),
                                    Qt.vector3d(-0.125, 0.25, -0.25),
                                    Qt.vector3d(-0.125, 0.25, -0.3125),
                                    Qt.vector3d(-0.125, 0.125, -0.25),
                                    Qt.vector3d(-0.125, 0.125, -0.3125),
                                    Qt.vector3d(-0.25, 0.25, -0.3125),
                                    Qt.vector3d(-0.25, 0.25, -0.25),
                                    Qt.vector3d(-0.25, 0.125, -0.3125),
                                    Qt.vector3d(-0.25, 0.125, -0.25),
                                    Qt.vector3d(-0.25, 0.25, -0.3125),
                                    Qt.vector3d(-0.125, 0.25, -0.3125),
                                    Qt.vector3d(-0.25, 0.25, -0.25),
                                    Qt.vector3d(-0.125, 0.25, -0.25),
                                    Qt.vector3d(-0.25, 0.125, -0.25),
                                    Qt.vector3d(-0.125, 0.125, -0.25),
                                    Qt.vector3d(-0.25, 0.125, -0.3125),
                                    Qt.vector3d(-0.125, 0.125, -0.3125),
                                    Qt.vector3d(-0.25, 0.25, -0.25),
                                    Qt.vector3d(-0.125, 0.25, -0.25),
                                    Qt.vector3d(-0.25, 0.125, -0.25),
                                    Qt.vector3d(-0.125, 0.125, -0.25),
                                    Qt.vector3d(-0.125, 0.25, -0.3125),
                                    Qt.vector3d(-0.25, 0.25, -0.3125),
                                    Qt.vector3d(-0.125, 0.125, -0.3125),
                                    Qt.vector3d(-0.25, 0.125, -0.3125),
                                    Qt.vector3d(0.125, -0.125, -0.375),
                                    Qt.vector3d(0.125, -0.125, -0.5625),
                                    Qt.vector3d(0.125, -0.25, -0.375),
                                    Qt.vector3d(0.125, -0.25, -0.5625),
                                    Qt.vector3d(-0.125, -0.125, -0.5625),
                                    Qt.vector3d(-0.125, -0.125, -0.375),
                                    Qt.vector3d(-0.125, -0.25, -0.5625),
                                    Qt.vector3d(-0.125, -0.25, -0.375),
                                    Qt.vector3d(-0.125, -0.125, -0.5625),
                                    Qt.vector3d(0.125, -0.125, -0.5625),
                                    Qt.vector3d(-0.125, -0.125, -0.375),
                                    Qt.vector3d(0.125, -0.125, -0.375),
                                    Qt.vector3d(-0.125, -0.25, -0.375),
                                    Qt.vector3d(0.125, -0.25, -0.375),
                                    Qt.vector3d(-0.125, -0.25, -0.5625),
                                    Qt.vector3d(0.125, -0.25, -0.5625),
                                    Qt.vector3d(-0.125, -0.125, -0.375),
                                    Qt.vector3d(0.125, -0.125, -0.375),
                                    Qt.vector3d(-0.125, -0.25, -0.375),
                                    Qt.vector3d(0.125, -0.25, -0.375),
                                    Qt.vector3d(0.125, -0.125, -0.5625),
                                    Qt.vector3d(-0.125, -0.125, -0.5625),
                                    Qt.vector3d(0.125, -0.25, -0.5625),
                                    Qt.vector3d(-0.125, -0.25, -0.5625)
                                ]

                                uv0s: [
                                    Qt.vector2d(0.000244140625, 0.437011719),
                                    Qt.vector2d(0.0935058594, 0.437011719),
                                    Qt.vector2d(0.000244140625, 0.250488281),
                                    Qt.vector2d(0.0935058594, 0.250488281),
                                    Qt.vector2d(0.218994141, 0.437011719),
                                    Qt.vector2d(0.312255859, 0.437011719),
                                    Qt.vector2d(0.218994141, 0.250488281),
                                    Qt.vector2d(0.312255859, 0.250488281),
                                    Qt.vector2d(0.218505859, 0.437988281),
                                    Qt.vector2d(0.0939941406, 0.437988281),
                                    Qt.vector2d(0.218505859, 0.624511719),
                                    Qt.vector2d(0.0939941406, 0.624511719),
                                    Qt.vector2d(0.343505859, 0.624511719),
                                    Qt.vector2d(0.218994141, 0.624511719),
                                    Qt.vector2d(0.343505859, 0.437988281),
                                    Qt.vector2d(0.218994141, 0.437988281),
                                    Qt.vector2d(0.312744141, 0.437011719),
                                    Qt.vector2d(0.437255859, 0.437011719),
                                    Qt.vector2d(0.312744141, 0.250488281),
                                    Qt.vector2d(0.437255859, 0.250488281),
                                    Qt.vector2d(0.0939941406, 0.437011719),
                                    Qt.vector2d(0.218505859, 0.437011719),
                                    Qt.vector2d(0.0939941406, 0.250488281),
                                    Qt.vector2d(0.218505859, 0.250488281),
                                    Qt.vector2d(0.000244140625, 0.968261719),
                                    Qt.vector2d(0.0153808594, 0.968261719),
                                    Qt.vector2d(0.000244140625, 0.906738281),
                                    Qt.vector2d(0.0153808594, 0.906738281),
                                    Qt.vector2d(0.0471191406, 0.968261719),
                                    Qt.vector2d(0.0622558594, 0.968261719),
                                    Qt.vector2d(0.0471191406, 0.906738281),
                                    Qt.vector2d(0.0622558594, 0.906738281),
                                    Qt.vector2d(0.0466308594, 0.969238281),
                                    Qt.vector2d(0.0158691406, 0.969238281),
                                    Qt.vector2d(0.0466308594, 0.999511719),
                                    Qt.vector2d(0.0158691406, 0.999511719),
                                    Qt.vector2d(0.0778808594, 0.999511719),
                                    Qt.vector2d(0.0471191406, 0.999511719),
                                    Qt.vector2d(0.0778808594, 0.969238281),
                                    Qt.vector2d(0.0471191406, 0.969238281),
                                    Qt.vector2d(0.0627441406, 0.968261719),
                                    Qt.vector2d(0.0935058594, 0.968261719),
                                    Qt.vector2d(0.0627441406, 0.906738281),
                                    Qt.vector2d(0.0935058594, 0.906738281),
                                    Qt.vector2d(0.0158691406, 0.968261719),
                                    Qt.vector2d(0.0466308594, 0.968261719),
                                    Qt.vector2d(0.0158691406, 0.906738281),
                                    Qt.vector2d(0.0466308594, 0.906738281),
                                    Qt.vector2d(0.343994141, 0.968261719),
                                    Qt.vector2d(0.359130859, 0.968261719),
                                    Qt.vector2d(0.343994141, 0.906738281),
                                    Qt.vector2d(0.359130859, 0.906738281),
                                    Qt.vector2d(0.390869141, 0.968261719),
                                    Qt.vector2d(0.406005859, 0.968261719),
                                    Qt.vector2d(0.390869141, 0.906738281),
                                    Qt.vector2d(0.406005859, 0.906738281),
                                    Qt.vector2d(0.390380859, 0.969238281),
                                    Qt.vector2d(0.359619141, 0.969238281),
                                    Qt.vector2d(0.390380859, 0.999511719),
                                    Qt.vector2d(0.359619141, 0.999511719),
                                    Qt.vector2d(0.421630859, 0.999511719),
                                    Qt.vector2d(0.390869141, 0.999511719),
                                    Qt.vector2d(0.421630859, 0.969238281),
                                    Qt.vector2d(0.390869141, 0.969238281),
                                    Qt.vector2d(0.406494141, 0.968261719),
                                    Qt.vector2d(0.437255859, 0.968261719),
                                    Qt.vector2d(0.406494141, 0.906738281),
                                    Qt.vector2d(0.437255859, 0.906738281),
                                    Qt.vector2d(0.359619141, 0.968261719),
                                    Qt.vector2d(0.390380859, 0.968261719),
                                    Qt.vector2d(0.359619141, 0.906738281),
                                    Qt.vector2d(0.390380859, 0.906738281),
                                    Qt.vector2d(0.000244140625, 0.155761719),
                                    Qt.vector2d(0.0466308594, 0.155761719),
                                    Qt.vector2d(0.000244140625, 0.0942382812),
                                    Qt.vector2d(0.0466308594, 0.0942382812),
                                    Qt.vector2d(0.109619141, 0.155761719),
                                    Qt.vector2d(0.156005859, 0.155761719),
                                    Qt.vector2d(0.109619141, 0.0942382812),
                                    Qt.vector2d(0.156005859, 0.0942382812),
                                    Qt.vector2d(0.109130859, 0.156738281),
                                    Qt.vector2d(0.0471191406, 0.156738281),
                                    Qt.vector2d(0.109130859, 0.249511719),
                                    Qt.vector2d(0.0471191406, 0.249511719),
                                    Qt.vector2d(0.171630859, 0.249511719),
                                    Qt.vector2d(0.109619141, 0.249511719),
                                    Qt.vector2d(0.171630859, 0.156738281),
                                    Qt.vector2d(0.109619141, 0.156738281),
                                    Qt.vector2d(0.156494141, 0.155761719),
                                    Qt.vector2d(0.218505859, 0.155761719),
                                    Qt.vector2d(0.156494141, 0.0942382812),
                                    Qt.vector2d(0.218505859, 0.0942382812),
                                    Qt.vector2d(0.0471191406, 0.155761719),
                                    Qt.vector2d(0.109130859, 0.155761719),
                                    Qt.vector2d(0.0471191406, 0.0942382812),
                                    Qt.vector2d(0.109130859, 0.0942382812)
                                ]
                                indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21, 24, 26, 25, 26, 27, 25, 28, 30, 29, 30, 31, 29, 32, 34, 33, 34, 35, 33, 36, 38, 37, 38, 39, 37, 40, 42, 41, 42, 43, 41, 44, 46, 45, 46, 47, 45, 48, 50, 49, 50, 51, 49, 52, 54, 53, 54, 55, 53, 56, 58, 57, 58, 59, 57, 60, 62, 61, 62, 63, 61, 64, 66, 65, 66, 67, 65, 68, 70, 69, 70, 71, 69, 72, 74, 73, 74, 75, 73, 76, 78, 77, 78, 79, 77, 80, 82, 81, 82, 83, 81, 84, 86, 85, 86, 87, 85, 88, 90, 89, 90, 91, 89, 92, 94, 93, 94, 95, 93]
                            }
                        }
                    }
                    PetModelNode {
                        objectName: "held_item"
                        position: Qt.vector3d(-(-2 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (-4.7 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (-10 + (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0)))) / 16)
                        rotation: fox.rotationZYX((0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))), (0 + fox.sittingWeight * (0 - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * (0 - (0))))
                    }
                }
                PetModelNode {
                    objectName: "leg0"
                    position: Qt.vector3d(-(-3 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16, (-2 + (0 + fox.sittingWeight * ((0 + 4.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 1.6) - (0)))) / 16, (6 + (0 + fox.sittingWeight * ((0 + 2.5) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16)
                    rotation: fox.rotationZYX((0 + fox.sittingWeight * ((0 + -15.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))))
                    PetModelNode {
                        objectName: "leg0_mesh"

                        visible: !fox.sleeping
                        geometry: PetMesh {
                            positions: [
                                Qt.vector3d(0.0003125, 0, 0.0625),
                                Qt.vector3d(0.0003125, 0, -0.0625),
                                Qt.vector3d(0.0003125, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, 0, -0.0625),
                                Qt.vector3d(-0.1246875, 0, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, -0.375, 0.0625),
                                Qt.vector3d(-0.1246875, 0, -0.0625),
                                Qt.vector3d(0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1246875, 0, 0.0625),
                                Qt.vector3d(0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, -0.0625),
                                Qt.vector3d(0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, 0, 0.0625),
                                Qt.vector3d(0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1246875, 0, -0.0625),
                                Qt.vector3d(0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, -0.375, -0.0625)
                            ]

                            uv0s: [
                                Qt.vector2d(0.218994141, 0.187011719),
                                Qt.vector2d(0.249755859, 0.187011719),
                                Qt.vector2d(0.218994141, 0.00048828125),
                                Qt.vector2d(0.249755859, 0.00048828125),
                                Qt.vector2d(0.281494141, 0.187011719),
                                Qt.vector2d(0.312255859, 0.187011719),
                                Qt.vector2d(0.281494141, 0.00048828125),
                                Qt.vector2d(0.312255859, 0.00048828125),
                                Qt.vector2d(0.281005859, 0.187988281),
                                Qt.vector2d(0.250244141, 0.187988281),
                                Qt.vector2d(0.281005859, 0.249511719),
                                Qt.vector2d(0.250244141, 0.249511719),
                                Qt.vector2d(0.312255859, 0.249511719),
                                Qt.vector2d(0.281494141, 0.249511719),
                                Qt.vector2d(0.312255859, 0.187988281),
                                Qt.vector2d(0.281494141, 0.187988281),
                                Qt.vector2d(0.312744141, 0.187011719),
                                Qt.vector2d(0.343505859, 0.187011719),
                                Qt.vector2d(0.312744141, 0.00048828125),
                                Qt.vector2d(0.343505859, 0.00048828125),
                                Qt.vector2d(0.250244141, 0.187011719),
                                Qt.vector2d(0.281005859, 0.187011719),
                                Qt.vector2d(0.250244141, 0.00048828125),
                                Qt.vector2d(0.281005859, 0.00048828125)
                            ]
                            indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21]
                        }
                    }
                }
                PetModelNode {
                    objectName: "leg1"
                    position: Qt.vector3d(-(1 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16, (-2 + (0 + fox.sittingWeight * ((0 + 4.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 1.6) - (0)))) / 16, (6 + (0 + fox.sittingWeight * ((0 + 2.5) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16)
                    rotation: fox.rotationZYX((0 + fox.sittingWeight * ((0 + -15.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))))
                    PetModelNode {
                        objectName: "leg1_mesh"

                        visible: !fox.sleeping
                        geometry: PetMesh {
                            positions: [
                                Qt.vector3d(-0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, 0, -0.0625),
                                Qt.vector3d(-0.1253125, 0, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, -0.375, 0.0625),
                                Qt.vector3d(-0.1253125, 0, -0.0625),
                                Qt.vector3d(-0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1253125, 0, 0.0625),
                                Qt.vector3d(-0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, -0.0625),
                                Qt.vector3d(-0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, 0, 0.0625),
                                Qt.vector3d(-0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1253125, 0, -0.0625),
                                Qt.vector3d(-0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, -0.375, -0.0625)
                            ]

                            uv0s: [
                                Qt.vector2d(0.343994141, 0.187011719),
                                Qt.vector2d(0.374755859, 0.187011719),
                                Qt.vector2d(0.343994141, 0.00048828125),
                                Qt.vector2d(0.374755859, 0.00048828125),
                                Qt.vector2d(0.406494141, 0.187011719),
                                Qt.vector2d(0.437255859, 0.187011719),
                                Qt.vector2d(0.406494141, 0.00048828125),
                                Qt.vector2d(0.437255859, 0.00048828125),
                                Qt.vector2d(0.406005859, 0.187988281),
                                Qt.vector2d(0.375244141, 0.187988281),
                                Qt.vector2d(0.406005859, 0.249511719),
                                Qt.vector2d(0.375244141, 0.249511719),
                                Qt.vector2d(0.437255859, 0.249511719),
                                Qt.vector2d(0.406494141, 0.249511719),
                                Qt.vector2d(0.437255859, 0.187988281),
                                Qt.vector2d(0.406494141, 0.187988281),
                                Qt.vector2d(0.437744141, 0.187011719),
                                Qt.vector2d(0.468505859, 0.187011719),
                                Qt.vector2d(0.437744141, 0.00048828125),
                                Qt.vector2d(0.468505859, 0.00048828125),
                                Qt.vector2d(0.375244141, 0.187011719),
                                Qt.vector2d(0.406005859, 0.187011719),
                                Qt.vector2d(0.375244141, 0.00048828125),
                                Qt.vector2d(0.406005859, 0.00048828125)
                            ]
                            indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21]
                        }
                    }
                }
                PetModelNode {
                    objectName: "leg2"
                    position: Qt.vector3d(-(-3 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16, (-2 + (0 + fox.sittingWeight * ((0 + 0.75) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 1.6) - (0)))) / 16, (-1 + (0 + fox.sittingWeight * ((0 + 3.5) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16)
                    rotation: fox.rotationZYX((0 + fox.sittingWeight * ((0 + 40.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))))
                    PetModelNode {
                        objectName: "leg2_mesh"

                        visible: !fox.sleeping
                        geometry: PetMesh {
                            positions: [
                                Qt.vector3d(0.0003125, 0, 0.0625),
                                Qt.vector3d(0.0003125, 0, -0.0625),
                                Qt.vector3d(0.0003125, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, 0, -0.0625),
                                Qt.vector3d(-0.1246875, 0, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, -0.375, 0.0625),
                                Qt.vector3d(-0.1246875, 0, -0.0625),
                                Qt.vector3d(0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1246875, 0, 0.0625),
                                Qt.vector3d(0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, -0.0625),
                                Qt.vector3d(0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, 0, 0.0625),
                                Qt.vector3d(0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1246875, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, -0.375, 0.0625),
                                Qt.vector3d(0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1246875, 0, -0.0625),
                                Qt.vector3d(0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1246875, -0.375, -0.0625)
                            ]

                            uv0s: [
                                Qt.vector2d(0.218994141, 0.187011719),
                                Qt.vector2d(0.249755859, 0.187011719),
                                Qt.vector2d(0.218994141, 0.00048828125),
                                Qt.vector2d(0.249755859, 0.00048828125),
                                Qt.vector2d(0.281494141, 0.187011719),
                                Qt.vector2d(0.312255859, 0.187011719),
                                Qt.vector2d(0.281494141, 0.00048828125),
                                Qt.vector2d(0.312255859, 0.00048828125),
                                Qt.vector2d(0.281005859, 0.187988281),
                                Qt.vector2d(0.250244141, 0.187988281),
                                Qt.vector2d(0.281005859, 0.249511719),
                                Qt.vector2d(0.250244141, 0.249511719),
                                Qt.vector2d(0.312255859, 0.249511719),
                                Qt.vector2d(0.281494141, 0.249511719),
                                Qt.vector2d(0.312255859, 0.187988281),
                                Qt.vector2d(0.281494141, 0.187988281),
                                Qt.vector2d(0.312744141, 0.187011719),
                                Qt.vector2d(0.343505859, 0.187011719),
                                Qt.vector2d(0.312744141, 0.00048828125),
                                Qt.vector2d(0.343505859, 0.00048828125),
                                Qt.vector2d(0.250244141, 0.187011719),
                                Qt.vector2d(0.281005859, 0.187011719),
                                Qt.vector2d(0.250244141, 0.00048828125),
                                Qt.vector2d(0.281005859, 0.00048828125)
                            ]
                            indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21]
                        }
                    }
                }
                PetModelNode {
                    objectName: "leg3"
                    position: Qt.vector3d(-(1 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16, (-2 + (0 + fox.sittingWeight * ((0 + 0.75) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 1.6) - (0)))) / 16, (-1 + (0 + fox.sittingWeight * ((0 + 3.5) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0)))) / 16)
                    rotation: fox.rotationZYX((0 + fox.sittingWeight * ((0 + 40.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * (0 - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))))
                    PetModelNode {
                        objectName: "leg3_mesh"

                        visible: !fox.sleeping
                        geometry: PetMesh {
                            positions: [
                                Qt.vector3d(-0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, 0, -0.0625),
                                Qt.vector3d(-0.1253125, 0, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, -0.375, 0.0625),
                                Qt.vector3d(-0.1253125, 0, -0.0625),
                                Qt.vector3d(-0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1253125, 0, 0.0625),
                                Qt.vector3d(-0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, -0.0625),
                                Qt.vector3d(-0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, 0, 0.0625),
                                Qt.vector3d(-0.0003125, 0, 0.0625),
                                Qt.vector3d(-0.1253125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, -0.375, 0.0625),
                                Qt.vector3d(-0.0003125, 0, -0.0625),
                                Qt.vector3d(-0.1253125, 0, -0.0625),
                                Qt.vector3d(-0.0003125, -0.375, -0.0625),
                                Qt.vector3d(-0.1253125, -0.375, -0.0625)
                            ]

                            uv0s: [
                                Qt.vector2d(0.343994141, 0.187011719),
                                Qt.vector2d(0.374755859, 0.187011719),
                                Qt.vector2d(0.343994141, 0.00048828125),
                                Qt.vector2d(0.374755859, 0.00048828125),
                                Qt.vector2d(0.406494141, 0.187011719),
                                Qt.vector2d(0.437255859, 0.187011719),
                                Qt.vector2d(0.406494141, 0.00048828125),
                                Qt.vector2d(0.437255859, 0.00048828125),
                                Qt.vector2d(0.406005859, 0.187988281),
                                Qt.vector2d(0.375244141, 0.187988281),
                                Qt.vector2d(0.406005859, 0.249511719),
                                Qt.vector2d(0.375244141, 0.249511719),
                                Qt.vector2d(0.437255859, 0.249511719),
                                Qt.vector2d(0.406494141, 0.249511719),
                                Qt.vector2d(0.437255859, 0.187988281),
                                Qt.vector2d(0.406494141, 0.187988281),
                                Qt.vector2d(0.437744141, 0.187011719),
                                Qt.vector2d(0.468505859, 0.187011719),
                                Qt.vector2d(0.437744141, 0.00048828125),
                                Qt.vector2d(0.468505859, 0.00048828125),
                                Qt.vector2d(0.375244141, 0.187011719),
                                Qt.vector2d(0.406005859, 0.187011719),
                                Qt.vector2d(0.375244141, 0.00048828125),
                                Qt.vector2d(0.406005859, 0.00048828125)
                            ]
                            indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21]
                        }
                    }
                }
                PetModelNode {
                    objectName: "tail"
                    position: Qt.vector3d(-(0 + (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (0 + (0 + fox.sittingWeight * ((0 + -0.075) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * (0 - (0)))) / 16, (7 + (0 + fox.sittingWeight * ((0 + -0.15) - (0)) + fox.sleepingWeight * ((0 + 1.5) - (0)) + fox.wiggleWeight * (0 - (0)))) / 16)
                    rotation: fox.rotationZYX((0 + fox.sittingWeight * ((0 + 60.0) - (0)) + fox.sleepingWeight * ((0 + -125.0) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))), (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * ((0 + ((Math.cos((((fox.lifeTime * 20.0) * 53.7)) * Math.PI / 180) * 10.0) - (0))) - (0))) + fox.idleMotion * Math.sin(fox.lifeTime * 1.1) * 6, (0 + fox.sittingWeight * ((0 + 0.0) - (0)) + fox.sleepingWeight * ((0 + 0.0) - (0)) + fox.wiggleWeight * ((0 + 0.0) - (0))))
                    PetModelNode {
                        objectName: "tail_mesh"

                        geometry: PetMesh {
                            positions: [
                                Qt.vector3d(0.125, 0.158410821, 0.0913962651),
                                Qt.vector3d(0.125, -0.149341601, 0.0371312096),
                                Qt.vector3d(0.125, 0.0607337215, 0.645350626),
                                Qt.vector3d(0.125, -0.247018701, 0.591085571),
                                Qt.vector3d(-0.125, -0.149341601, 0.0371312096),
                                Qt.vector3d(-0.125, 0.158410821, 0.0913962651),
                                Qt.vector3d(-0.125, -0.247018701, 0.591085571),
                                Qt.vector3d(-0.125, 0.0607337215, 0.645350626),
                                Qt.vector3d(-0.125, -0.149341601, 0.0371312096),
                                Qt.vector3d(0.125, -0.149341601, 0.0371312096),
                                Qt.vector3d(-0.125, 0.158410821, 0.0913962651),
                                Qt.vector3d(0.125, 0.158410821, 0.0913962651),
                                Qt.vector3d(-0.125, 0.0607337215, 0.645350626),
                                Qt.vector3d(0.125, 0.0607337215, 0.645350626),
                                Qt.vector3d(-0.125, -0.247018701, 0.591085571),
                                Qt.vector3d(0.125, -0.247018701, 0.591085571),
                                Qt.vector3d(-0.125, 0.158410821, 0.0913962651),
                                Qt.vector3d(0.125, 0.158410821, 0.0913962651),
                                Qt.vector3d(-0.125, 0.0607337215, 0.645350626),
                                Qt.vector3d(0.125, 0.0607337215, 0.645350626),
                                Qt.vector3d(0.125, -0.149341601, 0.0371312096),
                                Qt.vector3d(-0.125, -0.149341601, 0.0371312096),
                                Qt.vector3d(0.125, -0.247018701, 0.591085571),
                                Qt.vector3d(-0.125, -0.247018701, 0.591085571)
                            ]

                            uv0s: [
                                Qt.vector2d(0.437744141, 0.843261719),
                                Qt.vector2d(0.515380859, 0.843261719),
                                Qt.vector2d(0.437744141, 0.562988281),
                                Qt.vector2d(0.515380859, 0.562988281),
                                Qt.vector2d(0.578369141, 0.843261719),
                                Qt.vector2d(0.656005859, 0.843261719),
                                Qt.vector2d(0.578369141, 0.562988281),
                                Qt.vector2d(0.656005859, 0.562988281),
                                Qt.vector2d(0.577880859, 0.844238281),
                                Qt.vector2d(0.515869141, 0.844238281),
                                Qt.vector2d(0.577880859, 0.999511719),
                                Qt.vector2d(0.515869141, 0.999511719),
                                Qt.vector2d(0.640380859, 0.999511719),
                                Qt.vector2d(0.578369141, 0.999511719),
                                Qt.vector2d(0.640380859, 0.844238281),
                                Qt.vector2d(0.578369141, 0.844238281),
                                Qt.vector2d(0.656494141, 0.843261719),
                                Qt.vector2d(0.718505859, 0.843261719),
                                Qt.vector2d(0.656494141, 0.562988281),
                                Qt.vector2d(0.718505859, 0.562988281),
                                Qt.vector2d(0.515869141, 0.843261719),
                                Qt.vector2d(0.577880859, 0.843261719),
                                Qt.vector2d(0.515869141, 0.562988281),
                                Qt.vector2d(0.577880859, 0.562988281)
                            ]
                            indexes: [0, 2, 1, 2, 3, 1, 4, 6, 5, 6, 7, 5, 8, 10, 9, 10, 11, 9, 12, 14, 13, 14, 15, 13, 16, 18, 17, 18, 19, 17, 20, 22, 21, 22, 23, 21]
                        }
                    }
                }
            }
        }
    }
}
