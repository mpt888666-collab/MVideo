import QtQuick

// 无边框窗口右上角的最小化/最大化/关闭按钮。
Rectangle {
    id: titleButton

    property string glyph: ""
    property color hoverColor: "#232734"
    property color glyphColor: "#D6DCE6"

    signal clicked()

    width: 44
    height: 32
    radius: 4
    color: buttonHover.containsMouse ? hoverColor : "transparent"

    Text {
        anchors.centerIn: parent
        text: titleButton.glyph
        color: titleButton.glyphColor
        font.pixelSize: 14
    }

    MouseArea {
        id: buttonHover

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: titleButton.clicked()
    }
}