import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
AuthScaffold {
    id: root

    title: "欢迎回来"
    subtitle: "登录后继续浏览视频、消息和通话内容"

    property string feedbackMessage: ""
    property bool feedbackSuccess: false

    signal loginRequested(string email, string password)
    signal registerRequested()
    signal forgotPasswordRequested()
    signal previewRequested()
    signal loginSucceeded()

    function showFeedback(message, success) {
        root.feedbackMessage = message
        root.feedbackSuccess = success
    }

    function clearFeedback() {
        root.feedbackMessage = ""
    }

    Connections {
        target: AuthServer

        function onLoginFinished(error, message, token, host, port, userId) {
            if (error === 0) {
                root.showFeedback(message.length > 0 ? message : "登录成功", true)
                root.loginSucceeded()
            } else {
                root.showFeedback(message, false)
            }
        }
    }

    function isValidEmail(value) {
        const email = value.trim()
        return email.length <= 254
                && /^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$/.test(email)
    }

    function utf8Length(value) {
        return encodeURIComponent(value).replace(/%[A-Fa-f0-9]{2}/g, "x").length
    }

    function submit() {
        const email = emailField.text.trim()
        if (email.length === 0) {
            root.showFeedback("请输入邮箱地址", false)
            emailField.forceFocus()
            return
        }
        if (!root.isValidEmail(email)) {
            root.showFeedback("邮箱格式不正确", false)
            emailField.forceFocus()
            return
        }
        if (passwordField.text.length === 0) {
            root.showFeedback("请输入密码", false)
            passwordField.forceFocus()
            return
        }
        if (root.utf8Length(passwordField.text) > 128) {
            root.showFeedback("密码不能超过 128 字节", false)
            passwordField.forceFocus()
            return
        }

        root.showFeedback("正在登录...", true)
        root.loginRequested(email, passwordField.text)
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 14

        AuthField {
            id: emailField

            Layout.fillWidth: true
            label: "邮箱"
            placeholder: "name@example.com"

            onEdited: root.clearFeedback()
            onSubmitted: root.submit()
        }

        AuthField {
            id: passwordField

            Layout.fillWidth: true
            label: "密码"
            placeholder: "请输入密码"
            passwordMode: true

            onEdited: root.clearFeedback()
            onSubmitted: root.submit()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: -2
            spacing: 8

            CheckBox {
                id: rememberBox

                text: "记住我"
                checked: true
                font.pixelSize: 12
                palette.windowText: "#A3AFBE"
                palette.text: "#A3AFBE"
                palette.highlight: "#20F1CB"
                palette.highlightedText: "#04110F"
            }

            Item { Layout.fillWidth: true }

            LinkText {
                text: "忘记密码？"
                onClicked: root.forgotPasswordRequested()
            }
        }

        AuthButton {
            Layout.fillWidth: true
            Layout.topMargin: 2
            text: "登录"
            onClicked: root.submit()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 52 : 0
            visible: root.feedbackMessage.length > 0
            radius: 10
            color: root.feedbackSuccess ? "#11251F" : "#2A171B"
            border.width: 1
            border.color: root.feedbackSuccess ? "#245A4F" : "#61303B"

            Text {
                anchors.fill: parent
                anchors.margins: 10
                text: root.feedbackMessage
                color: root.feedbackSuccess ? "#83E8D5" : "#FF9AAA"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.preferredHeight: 1
            color: "#202D38"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "还没有账号？"
                color: "#7D899A"
                font.pixelSize: 12
            }

            LinkText {
                text: "立即注册"
                onClicked: root.registerRequested()
            }

            Item { Layout.fillWidth: true }

            LinkText {
                text: "仅预览首页 →"
                normalColor: "#6F7B8C"
                hoverColor: "#B7C2D0"
                onClicked: root.previewRequested()
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: "登录即表示同意《用户协议》和《隐私政策》"
            color: "#4E5A6A"
            font.pixelSize: 10
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }
}
