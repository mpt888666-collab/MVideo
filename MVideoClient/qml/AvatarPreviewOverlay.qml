import QtQuick

// 点击头像后的放大预览，点任意位置关闭。
Rectangle {
    id: overlay

    property bool opened: false
    property string avatarSource: ""
    property string initial: ""
    property real cropScaleX: 0
    property real cropScaleY: 0
    property real cropX: 0
    property real cropY: 0

    signal dismissed()

    anchors.fill: parent
    z: 150
    visible: opacity > 0
    opacity: overlay.opened ? 1 : 0
    color: "#CC000000"

    Behavior on opacity {
        NumberAnimation { duration: 120 }
    }

    CircleAvatar {
        diameter: 320
        anchors.centerIn: parent
        initial: overlay.initial
        imageSource: overlay.avatarSource
        cropScaleX: overlay.cropScaleX
        cropScaleY: overlay.cropScaleY
        cropX: overlay.cropX
        cropY: overlay.cropY
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: overlay.dismissed()
    }

    Shortcut {
        sequence: "Esc"
        enabled: overlay.opened
        onActivated: overlay.dismissed()
    }
}