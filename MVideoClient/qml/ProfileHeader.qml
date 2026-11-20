import QtQuick

// 「我的」页顶部资料区：头像 + 昵称 + 关注/粉丝 + 抖音号。
Row {
    id: header

    property string avatarSource: ""
    property string initial: ""
    property real cropScaleX: 0
    property real cropScaleY: 0
    property real cropX: 0
    property real cropY: 0
    property string displayName: ""

    signal avatarClicked()
    signal editClicked()

    spacing: 24

    Item {
        width: 96
        height: 96

        CircleAvatar {
            anchors.fill: parent
            diameter: 96
            initial: header.initial
            imageSource: header.avatarSource
            cropScaleX: header.cropScaleX
            cropScaleY: header.cropScaleY
            cropX: header.cropX
            cropY: header.cropY
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: header.avatarClicked()
        }
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        Row {
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: header.displayName
                color: "#FFFFFF"
                font.pixelSize: 26
                font.bold: true
            }

            Rectangle {
                width: 24
                height: 24
                radius: 12
                anchors.verticalCenter: parent.verticalCenter
                color: penHover.containsMouse ? "#232733" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "✎"
                    color: penHover.containsMouse ? "#FFFFFF" : "#8C93A1"
                    font.pixelSize: 14
                }

                MouseArea {
                    id: penHover

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: header.editClicked()
                }
            }
        }

        Row {
            spacing: 10

            Text { text: "关注 " + UserInfo.follow; color: "#C6CCD6"; font.pixelSize: 13 }
            Text { text: "·"; color: "#4C5460"; font.pixelSize: 13 }
            Text { text: "粉丝 " + UserInfo.fan; color: "#C6CCD6"; font.pixelSize: 13 }
            Text { text: "·"; color: "#4C5460"; font.pixelSize: 13 }
            Text { text: "获赞 0"; color: "#C6CCD6"; font.pixelSize: 13 }
        }

        Text {
            text: "抖音号: " + (UserInfo.account.length > 0 ? UserInfo.account : "—")
            color: "#7A8290"
            font.pixelSize: 12
        }
    }
}