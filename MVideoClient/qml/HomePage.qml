import QtQuick
import QtQuick.Controls

// 首页容器：只负责分区切换、资料页状态和弹窗编排，
// 具体 UI 拆在 MineSideBar / ProfileHeader / WorksGrid 等组件里。
Page {
    id: root

    padding: 0

    signal logoutRequested()
    signal playRequested(var videoId)

    readonly property int sectionFeatured: 0
    readonly property int sectionRecommend: 1
    readonly property int sectionMine: 2

    property int currentSection: root.sectionMine

    property bool avatarPreviewOpen: false
    property bool editProfileOpen: false

    property string profileNameOverride: ""
    property string profileBio: ""
    property string profileAvatarUrl: ""
    property real profileAvatarCropScaleX: 0
    property real profileAvatarCropScaleY: 0
    property real profileAvatarCropX: 0
    property real profileAvatarCropY: 0

    readonly property var works: AuthServer.works

    readonly property string avatarInitial: {
        const name = UserInfo.displayName
        return name.length > 0 ? name.charAt(0).toUpperCase() : "?"
    }
    readonly property string profileNameForEdit:
        profileNameOverride.length > 0 ? profileNameOverride : UserInfo.displayName
    readonly property string profileDisplayName:
        profileNameForEdit.length > 0 ? profileNameForEdit : "未登录"

    Component.onCompleted: {
        root.profileAvatarUrl = AuthServer.avatarUrl
    }

    Connections {
        target: AuthServer

        function onAvatarUrlChanged() {
            root.profileAvatarUrl = AuthServer.avatarUrl
            root.profileAvatarCropScaleX = 0
            root.profileAvatarCropScaleY = 0
            root.profileAvatarCropX = 0
            root.profileAvatarCropY = 0
        }

        function onAvatarUploadFinished(ok, message) {
            if (typeof root.showFeedback === "function")
                root.showFeedback(message, ok)
        }
    }

    background: Rectangle {
        color: "#080B10"
    }

    MineSideBar {
        id: sidebar

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        currentSection: root.currentSection
        onSectionSelected: function(section) { root.currentSection = section }
        onLogoutRequested: root.logoutRequested()
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

                    ProfileHeader {
                        width: parent.width
                        displayName: root.profileDisplayName
                        initial: root.avatarInitial
                        avatarSource: root.profileAvatarUrl
                        cropScaleX: root.profileAvatarCropScaleX
                        cropScaleY: root.profileAvatarCropScaleY
                        cropX: root.profileAvatarCropX
                        cropY: root.profileAvatarCropY

                        onAvatarClicked: root.avatarPreviewOpen = true
                        onEditClicked: root.editProfileOpen = true
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

                        MineChip { label: "作品"; primary: true }
                        MineChip { label: "私密作品" }
                        MineChip { label: "合集" }
                        MineChip { label: "短剧" }
                    }

                    WorksGrid {
                        works: root.works

                        onPlayRequested: function(videoId) { root.playRequested(videoId) }
                    }
                }
            }
        }
    }

    AvatarPreviewOverlay {
        opened: root.avatarPreviewOpen
        initial: root.avatarInitial
        avatarSource: root.profileAvatarUrl
        cropScaleX: root.profileAvatarCropScaleX
        cropScaleY: root.profileAvatarCropScaleY
        cropX: root.profileAvatarCropX
        cropY: root.profileAvatarCropY

        onDismissed: root.avatarPreviewOpen = false
    }

    EditProfileDialog {
        opened: root.editProfileOpen
        initialName: root.profileNameForEdit
        initialBio: root.profileBio
        initial: root.avatarInitial
        avatarSource: root.profileAvatarUrl
        avatarCropScaleX: root.profileAvatarCropScaleX
        avatarCropScaleY: root.profileAvatarCropScaleY
        avatarCropX: root.profileAvatarCropX
        avatarCropY: root.profileAvatarCropY

        onCanceled: root.editProfileOpen = false

        onAccepted: function(name, bio, avatarUrl, scaleX, scaleY, offsetX, offsetY, avatarChanged) {
            if (name.length > 0)
                root.profileNameOverride = name
            root.profileBio = bio
            if (avatarChanged && avatarUrl.length > 0) {
                AuthServer.uploadAvatar(avatarUrl, scaleX, scaleY, offsetX, offsetY)
            } else {
                root.profileAvatarUrl = avatarUrl
                root.profileAvatarCropScaleX = scaleX
                root.profileAvatarCropScaleY = scaleY
                root.profileAvatarCropX = offsetX
                root.profileAvatarCropY = offsetY
            }
            root.editProfileOpen = false
        }
    }
}