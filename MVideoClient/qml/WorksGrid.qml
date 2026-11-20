import QtQuick

// 作品列表：有作品时平铺卡片，没有作品时显示空态。
Column {
    id: grid

    property var works: null

    signal playRequested(var videoId)

    width: parent ? parent.width : implicitWidth
    spacing: 0

    Flow {
        id: cardFlow

        width: parent.width
        spacing: 16
        visible: grid.works !== null && grid.works.count > 0

        Repeater {
            model: grid.works

            delegate: WorkCard {
                onPlayRequested: function(videoId) { grid.playRequested(videoId) }
            }
        }
    }

    Column {
        width: parent.width
        topPadding: 110
        spacing: 18
        visible: grid.works === null || grid.works.count === 0

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 96
            height: 62
            radius: 8
            color: "#1B1F27"
            border.width: 1
            border.color: "#232733"

            Text {
                anchors.centerIn: parent
                text: "▶"
                color: "#5A6270"
                font.pixelSize: 22
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "暂无内容"
            color: "#FFFFFF"
            font.pixelSize: 18
            font.bold: true
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "该账号还未发布过作品"
            color: "#7A8290"
            font.pixelSize: 13
        }
    }
}