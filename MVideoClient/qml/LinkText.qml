import QtQuick

Text {
    id: root

    property color normalColor: "#20F1CB"
    property color hoverColor: "#8FFFF0"

    signal clicked()

    color: pointer.containsMouse ? hoverColor : normalColor
    font.pixelSize: 12
    font.underline: pointer.containsMouse

    MouseArea {
        id: pointer

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
