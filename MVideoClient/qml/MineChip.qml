import QtQuick

// 作品区的小标签胶囊。
Rectangle {
    id: chip

    property string label: ""
    property bool primary: false

    width: chipText.implicitWidth + 28
    height: 30
    radius: 6
    color: chip.primary ? "#3A1520" : "#1B1F27"
    border.width: chip.primary ? 1 : 0
    border.color: "#FE2C55"

    Text {
        id: chipText

        anchors.centerIn: parent
        text: chip.label
        color: chip.primary ? "#FE2C55" : "#9BA3B0"
        font.pixelSize: 13
    }
}