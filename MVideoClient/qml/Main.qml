import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Effects

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 980
    minimumHeight: 720
    visible: true
    title: "MVideo"
    color: "#080B10"
    flags: Qt.Window | Qt.FramelessWindowHint

    readonly property string avatarInitial: {
        const name = UserInfo.displayName
        return name.length > 0 ? name.charAt(0).toUpperCase() : "?"
    }

    property bool avatarMenuOpen: false

    readonly property var homePageItem: {
        const item = stack.currentItem
        return item && item.isHome === true ? item : null
    }
    readonly property string topAvatarUrl: homePageItem ? homePageItem.profileAvatarUrl : ""
    readonly property real topAvatarCropScaleX: homePageItem ? homePageItem.profileAvatarCropScaleX : 0
    readonly property real topAvatarCropScaleY: homePageItem ? homePageItem.profileAvatarCropScaleY : 0
    readonly property real topAvatarCropX: homePageItem ? homePageItem.profileAvatarCropX : 0
    readonly property real topAvatarCropY: homePageItem ? homePageItem.profileAvatarCropY : 0
    readonly property string topDisplayName: {
        if (!homePageItem)
            return UserInfo.displayName.length > 0 ? UserInfo.displayName : "未登录"
        if (homePageItem.profileNameOverride.length > 0)
            return homePageItem.profileNameOverride
        return UserInfo.displayName.length > 0 ? UserInfo.displayName : "未登录"
    }
    readonly property string topAvatarInitial: {
        const name = topDisplayName
        return name.length > 0 ? name.charAt(0).toUpperCase() : "?"
    }

    function goToMine() {
        if (!homePageItem)
            return
        homePageItem.currentSection = homePageItem.sectionMine
        avatarMenuOpen = false
    }

    Rectangle {
        id: titleBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48
        color: "#161823"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: root.startSystemMove()
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 10
            spacing: 16

            Row {
                spacing: 16
                visible: stack.currentItem ? stack.currentItem.isHome === true : false

                Text {
                    id: uploadEntry

                    height: 32
                    text: "投稿"
                    color: uploadHover.containsMouse ? "#FFFFFF" : "#A9B4C2"
                    font.pixelSize: 14
                    verticalAlignment: Text.AlignVCenter

                    Behavior on color {
                        ColorAnimation { duration: 120 }
                    }

                    MouseArea {
                        id: uploadHover

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: uploadDialog.openDialog()
                    }
                }

                Item {
                    width: 30
                    height: 30

                    CircleAvatar {
                        anchors.fill: parent
                        diameter: 30
                        imageSource: root.topAvatarUrl
                        initial: root.topAvatarInitial
                        baseColor: "#1B1F27"
                        borderColor: "#2E3642"
                        initialColor: "#E6EDF5"
                        cropScaleX: root.topAvatarCropScaleX
                        cropScaleY: root.topAvatarCropScaleY
                        cropX: root.topAvatarCropX
                        cropY: root.topAvatarCropY
                    }

                    MouseArea {
                        id: avatarHover

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.goToMine()
                        onEntered: {
                            hideAvatarMenuTimer.stop()
                            root.avatarMenuOpen = true
                        }
                        onExited: hideAvatarMenuTimer.restart()
                    }
                }
            }

            Row {
                spacing: 2

                TitleButton {
                    glyph: "—"
                    onClicked: root.showMinimized()
                }

                TitleButton {
                    glyph: "□"
                    onClicked: {
                        if (root.visibility === Window.Maximized)
                            root.showNormal()
                        else
                            root.showMaximized()
                    }
                }

                TitleButton {
                    glyph: "×"
                    hoverColor: "#C42B1C"
                    glyphColor: "#FFFFFF"
                    onClicked: root.close()
                }
            }
        }
    }

    Timer {
        id: hideAvatarMenuTimer

        interval: 140
        repeat: false
        onTriggered: {
            if (!avatarHover.containsMouse && !avatarMenuHover.containsMouse)
                root.avatarMenuOpen = false
        }
    }

    Rectangle {
        id: avatarMenu

        z: 1000
        width: 280
        height: 88
        visible: root.avatarMenuOpen && root.homePageItem !== null
        anchors.top: titleBar.bottom
        anchors.topMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 14
        radius: 12
        color: "#20232B"
        border.width: 1
        border.color: "#303644"

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            spacing: 12

            CircleAvatar {
                diameter: 42
                imageSource: root.topAvatarUrl
                initial: root.topAvatarInitial
                cropScaleX: root.topAvatarCropScaleX
                cropScaleY: root.topAvatarCropScaleY
                cropX: root.topAvatarCropX
                cropY: root.topAvatarCropY
                initialColor: "#E6EDF5"
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text {
                    text: root.topDisplayName
                    color: "#F5F7FA"
                    font.pixelSize: 16
                    font.bold: true
                    elide: Text.ElideRight
                    width: 190
                }

                Row {
                    spacing: 6

                    Text {
                        text: "关注 " + UserInfo.follow
                        color: "#C8CED8"
                        font.pixelSize: 12
                    }

                    Text {
                        text: "|"
                        color: "#59606D"
                        font.pixelSize: 12
                    }

                    Text {
                        text: "粉丝 " + UserInfo.fan
                        color: "#C8CED8"
                        font.pixelSize: 12
                    }
                }
            }
        }

        MouseArea {
            id: avatarMenuHover

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: hideAvatarMenuTimer.stop()
            onExited: hideAvatarMenuTimer.restart()
            onClicked: root.goToMine()
        }
    }
    StackView {
        id: stack

        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        initialItem: loginPage

        pushEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 ; easing.type : Easing.OutCubic}
        }

        pushExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0.35; duration: 160 ; easing.type : Easing.OutCubic}
        }

        popEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0.35; to: 1; duration: 160 ; easing.type : Easing.OutCubic}
        }

        popExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 160 ; easing.type : Easing.OutCubic}
        }

        replaceEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 ; easing.type : Easing.OutCubic}
        }

        replaceExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 ; easing.type : Easing.OutCubic}
        }
    }

    UploadDialog {
        id: uploadDialog

        onPublishRequested: function(fileUrl, title, description, tags) {
            console.log("Publish requested:", fileUrl, "|", title, "|", description, "|", tags)
            AuthServer.uploadVideo(fileUrl, title, description, tags)
        }
    }

    MouseArea {
        width: 6
        height: parent.height
        anchors.left: parent.left
        cursorShape: Qt.SizeHorCursor
        onPressed: root.startSystemResize(Qt.LeftEdge)
    }

    MouseArea {
        width: parent.width
        height: 6
        anchors.top: parent.top
        cursorShape: Qt.SizeVerCursor
        onPressed: root.startSystemResize(Qt.TopEdge)
    }

    MouseArea {
        width: 6
        height: parent.height
        anchors.right: parent.right
        cursorShape: Qt.SizeHorCursor
        onPressed: root.startSystemResize(Qt.RightEdge)
    }

    MouseArea {
        width: parent.width
        height: 6
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeVerCursor
        onPressed: root.startSystemResize(Qt.BottomEdge)
    }

    Component.onCompleted: {
        console.log("AuthServer =", AuthServer)
        console.log("TcpAuth =", TcpAuth)
    }

    Connections {
        target: AuthServer

        function onLoginFinished(error, message) {
            if (error === 0) {
                console.log("TCP: connecting to", UserInfo.chatHost + ":" + UserInfo.chatPort)
                TcpAuth.loginChatServer()
            }
        }
        function onUploadFailed(videoId, error, message) {
            console.log("qml : 文件传输minio失败, id =", videoId, "code =", error, message)
        }

        function onUploadSuccess(videoId, error, message) {
            console.log("qml : 文件传输minio完成, 等待服务器校验完毕, id =", videoId)
        }

        function onQueueChanged(pending, active) {
            console.log("qml : 上传队列, 待传 =", pending, "进行中 =", active)
        }

        function onServerCheckComplete(videoId, ok){
            if(ok) console.log("qml : 服务器认证完成, 解码中", videoId)
            else console.log("qml : 服务器认证失败", videoId)
        }
        function onUploadProgress(videoId, percent) {
            console.log("qml : 上传进度", videoId, percent + "%")
        }
    }

    Connections {
        target: TcpAuth

        function onLoginSucceeded() {
            console.log("TCP: chat login succeeded")
            stack.replace(homePage)
        }

        function onLoginFailed(message) {
            console.log("TCP: chat server connect failed -", message)
            if (stack.currentItem && typeof stack.currentItem.showFeedback === "function")
                stack.currentItem.showFeedback(message, false)
            AuthServer.logout()
        }
    }


    Component {
        id: loginPage

        LoginPage {
            onRegisterRequested: stack.push(registerPage)
            onForgotPasswordRequested: stack.push(forgotPasswordPage)
            onPreviewRequested: stack.replace(homePage)
            onLoginRequested: function(email, password) {
                console.log("Login requested:", email)
                AuthServer.requestLogin(email, password)
            }
        }
    }

    Component {
        id: registerPage
        RegisterPage {
            onBackRequested: stack.pop()
            onRegisterSucceeded: stack.pop()
            onVerifyCodeRequested: function(email) {
                console.log("Verify code requested:", email)
                AuthServer.requestVarityCode(email)
            }
            onRegisterRequested: function(account, password, username, email, phone, code) {
                console.log("Register requested:", account, username, email, phone, code)
                AuthServer.requestRegister(account, password, username, email, phone, code)
            }
        }
    }

    Component {
        id: forgotPasswordPage

        ForgotPasswordPage {
            onBackRequested: stack.pop()
            onResetSucceeded: stack.pop()
            onVerifyCodeRequested: function(email) {
                console.log("Verify code requested:", email)
                AuthServer.requestVarityCode(email)
            }
            onResetRequested: function(email, code, password, confirm) {
                console.log("Password reset requested:", email)
                AuthServer.requestReset(email, code, password, confirm)
            }
        }
    }

    Component {
        id: playerPage

        VideoPlayerPage {
            onBackRequested: stack.pop()
        }
    }

    Component {
        id: homePage

        HomePage {
            readonly property bool isHome: true

            onLogoutRequested: {
                AuthServer.logout()
                stack.replace(loginPage)
            }
            onPlayRequested: function(videoId) {
                stack.push(playerPage, { "videoId": videoId })
            }
        }
    }


}
