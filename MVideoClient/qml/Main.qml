import QtQuick
import QtQuick.Controls
import QtQuick.Window

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

    component TitleButton: Rectangle {
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

                Rectangle {
                    id: avatar

                    width: 32
                    height: 32
                    radius: width / 2
                    color: "#1B1F27"
                    border.width: 1
                    border.color: "#2E3642"
                    clip: true

                    Text {
                        anchors.centerIn: parent
                        text: root.avatarInitial
                        color: "#E6EDF5"
                        font.pixelSize: 13
                        font.bold: true
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
        id: homePage

        HomePage {
            readonly property bool isHome: true

            onLogoutRequested: {
                AuthServer.logout()
                stack.replace(loginPage)
            }
        }
    }


}
