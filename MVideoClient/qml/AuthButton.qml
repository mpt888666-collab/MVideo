import QtQuick
import QtQuick.Controls

Button {
    id: root

    implicitHeight: 50
    opacity: enabled ? 1.0 : 0.45

    background: Rectangle {
        radius: 12
        color: !root.enabled
               ? "#25333D"
               : root.down
                 ? "#17C9AA"
                 : root.hovered
                   ? "#39F5D5"
                   : "#20F1CB"
    }

    contentItem: Text {
        text: root.text
        color: "#04110F"
        font.pixelSize: 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    HoverHandler {
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
