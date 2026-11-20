import QtQuick

// 首页左侧导航：精选 / 推荐 / 我的 / 退出登录。
Rectangle {
    id: sideBar

    readonly property int sectionFeatured: 0
    readonly property int sectionRecommend: 1
    readonly property int sectionMine: 2

    property int currentSection: sideBar.sectionMine

    signal sectionSelected(int section)
    signal logoutRequested()

    width: 220
    color: "#161823"

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
            active: sideBar.currentSection === sideBar.sectionFeatured
            onClicked: sideBar.sectionSelected(sideBar.sectionFeatured)
        }

        NavItem {
            glyph: "✦"
            label: "推荐"
            active: sideBar.currentSection === sideBar.sectionRecommend
            onClicked: sideBar.sectionSelected(sideBar.sectionRecommend)
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
            active: sideBar.currentSection === sideBar.sectionMine
            onClicked: sideBar.sectionSelected(sideBar.sectionMine)
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
        onClicked: sideBar.logoutRequested()
    }
}