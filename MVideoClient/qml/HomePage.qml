import QtQuick
import QtQuick.Controls

Page {
    id: root

    padding: 0

    signal logoutRequested()

    readonly property int sectionFeatured: 0
    readonly property int sectionRecommend: 1
    readonly property int sectionMine: 2

    property int currentSection: root.sectionMine

    readonly property var works: AuthServer.works
    readonly property string avatarInitial: {
        const name = UserInfo.displayName
        return name.length > 0 ? name.charAt(0).toUpperCase() : "?"
    }

    function stateLabel(state) {
        if (state === 0)
            return "上传中"
        if (state === 1)
            return "上传中"
        if (state === 2)
            return "解码中"
        if (state === 3)
            return "上传成功"
        if (state === 4)
            return "上传失败"
        return ""
    }

    background: Rectangle {
        color: "#080B10"
    }

    component NavItem: Rectangle {
        id: nav

        property string glyph: ""
        property string label: ""
        property bool active: false

        signal clicked()

        width: parent ? parent.width : 0
        height: 46
        radius: 8
        color: nav.active ? "#2A2D3A" : (navHover.containsMouse ? "#1E2230" : "transparent")

        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 14
            text: nav.glyph
            color: nav.active ? "#FFFFFF" : "#9BA3B0"
            font.pixelSize: 15
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 44
            text: nav.label
            color: nav.active ? "#FFFFFF" : "#C6CCD6"
            font.pixelSize: 15
        }

        MouseArea {
            id: navHover

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: nav.clicked()
        }
    }

    component Chip: Rectangle {
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

    Rectangle {
        id: sidebar

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 220
        color: "#161823"

        Column {
            id: navColumn

            anchors.top: parent.top
            anchors.topMargin: 18
            anchors.left: parent.left
            anchors.leftMargin: 12
            width: parent.width - 24
            spacing: 4

            NavItem {
                glyph: "▤"
                label: "精选"
                active: root.currentSection === root.sectionFeatured
                onClicked: root.currentSection = root.sectionFeatured
            }

            NavItem {
                glyph: "✦"
                label: "推荐"
                active: root.currentSection === root.sectionRecommend
                onClicked: root.currentSection = root.sectionRecommend
            }

            Item {
                width: 1
                height: 10
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#232733"
            }

            Item {
                width: 1
                height: 10
            }

            NavItem {
                glyph: "◉"
                label: "我的"
                active: root.currentSection === root.sectionMine
                onClicked: root.currentSection = root.sectionMine
            }
        }

        NavItem {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            width: parent.width - 24
            glyph: "⏻"
            label: "退出登录"
            onClicked: root.logoutRequested()
        }
    }

    Item {
        id: contentArea

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: sidebar.right
        anchors.right: parent.right

        Text {
            anchors.centerIn: parent
            visible: root.currentSection !== root.sectionMine
            text: root.currentSection === root.sectionFeatured ? "精选内容开发中" : "推荐内容开发中"
            color: "#4C5460"
            font.pixelSize: 15
        }

        Flickable {
            id: flick

            anchors.fill: parent
            visible: root.currentSection === root.sectionMine
            clip: true
            contentWidth: width
            contentHeight: body.height
            boundsBehavior: Flickable.StopAtBounds

            Item {
                id: body

                x: 48
                y: 36
                width: flick.width - 96
                height: bodyColumn.height

                Column {
                    id: bodyColumn

                    width: parent.width
                    spacing: 22

                    Row {
                        spacing: 24

                        Rectangle {
                            width: 96
                            height: 96
                            radius: 48
                            color: "#33294D"
                            border.width: 1
                            border.color: "#4A3C68"

                            Text {
                                anchors.centerIn: parent
                                text: root.avatarInitial
                                color: "#F2EEFF"
                                font.pixelSize: 34
                                font.bold: true
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 12

                            Text {
                                text: UserInfo.displayName.length > 0 ? UserInfo.displayName : "未登录"
                                color: "#FFFFFF"
                                font.pixelSize: 26
                                font.bold: true
                            }

                            Row {
                                spacing: 10

                                Text { text: "关注 0"; color: "#C6CCD6"; font.pixelSize: 13 }
                                Text { text: "·"; color: "#4C5460"; font.pixelSize: 13 }
                                Text { text: "粉丝 0"; color: "#C6CCD6"; font.pixelSize: 13 }
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

                    Row {
                        id: tabRow

                        height: 44
                        spacing: 30

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 8

                            Text {
                                text: "作品 " + root.works.count
                                color: "#FFFFFF"
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Rectangle {
                                width: 28
                                height: 3
                                radius: 2
                                color: "#FE2C55"
                            }
                        }

                        Repeater {
                            model: ["推荐", "喜欢", "收藏", "观看历史", "稍后再看", "我的预约"]

                            delegate: Text {
                                required property string modelData

                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                color: "#6E7683"
                                font.pixelSize: 15
                            }
                        }
                    }

                    Row {
                        spacing: 10

                        Chip { label: "作品"; primary: true }
                        Chip { label: "私密作品" }
                        Chip { label: "合集" }
                        Chip { label: "短剧" }
                    }

                    Flow {
                        width: parent.width
                        spacing: 16
                        visible: root.works.count > 0

                        Repeater {
                            model: root.works

                            delegate: Rectangle {
                                id: card

                                required property string title
                                required property int workState

                                width: 180
                                height: 240
                                radius: 10
                                clip: true
                                color: "#12151D"
                                border.width: 1
                                border.color: cardHover.containsMouse ? "#3C4353" : "#232733"

                                Rectangle {
                                    anchors.fill: parent
                                    radius: card.radius
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: "#2A2140" }
                                        GradientStop { position: 1.0; color: "#0E1117" }
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: -18
                                    text: "▶"
                                    color: "#FFFFFF"
                                    opacity: 0.35
                                    font.pixelSize: 30
                                }

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: stateText.top
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    anchors.bottomMargin: 6
                                    text: card.title
                                    color: "#E8EDF5"
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }

                                Text {
                                    id: stateText

                                    anchors.left: parent.left
                                    anchors.bottom: parent.bottom
                                    anchors.leftMargin: 12
                                    anchors.bottomMargin: 12
                                    text: root.stateLabel(card.workState)
                                    color: card.workState === 2 ? "#4CC38A" : "#8C93A1"
                                    font.pixelSize: 11
                                }

                                MouseArea {
                                    id: cardHover

                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        topPadding: 110
                        spacing: 18
                        visible: root.works.count === 0

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
            }
        }
    }
}