import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

AuthScaffold {
    id: root

    title: "重置密码"
    subtitle: "验证邮箱后设置一个新密码"
    showBack: true

    property int codeCountdown: 0
    property string feedbackMessage: ""
    property bool feedbackSuccess: false

    signal resetRequested(string email, string code, string password, string confirm)
    signal verifyCodeRequested(string email)
    signal resetSucceeded()

    function showFeedback(message, success) {
        root.feedbackMessage = message
        root.feedbackSuccess = success
    }

    function clearFeedback() {
        root.feedbackMessage = ""
    }

    Connections {
        target: AuthServer

        function onVerifyCodeFinished(error, message) {
            if (error === 0) {
                root.codeCountdown = 60
                codeTimer.restart()
                root.showFeedback(message, true)
            } else {
                root.codeCountdown = 0
                codeTimer.stop()
                root.showFeedback(message, false)
            }
        }

        function onResetPasswordFinished(error, message) {
            if (error === 0) {
                root.showFeedback(message, true)
                root.resetSucceeded()
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

    function isValidCode(value) {
        return /^[A-Za-z0-9]{4,8}$/.test(value)
    }

    function utf8Length(value) {
        return encodeURIComponent(value).replace(/%[A-Fa-f0-9]{2}/g, "x").length
    }

    function requestCode() {
        const email = emailField.text.trim()
        if (!root.isValidEmail(email)) {
            root.showFeedback("请先填写正确的邮箱地址", false)
            emailField.forceFocus()
            return
        }
        root.showFeedback("正在发送验证码...", true)
        root.verifyCodeRequested(email)
    }

    function submit() {
        const email = emailField.text.trim()
        const code = codeField.text.trim()
        const password = passwordField.text
        const confirm = confirmField.text

        if (!root.isValidEmail(email)) {
            root.showFeedback("请输入正确的邮箱地址", false)
            emailField.forceFocus()
            return
        }
        if (!root.isValidCode(code)) {
            root.showFeedback("验证码需为 4-8 位字母或数字", false)
            codeField.forceFocus()
            return
        }
        if (root.utf8Length(password) < 8 || root.utf8Length(password) > 128) {
            root.showFeedback("新密码长度需为 8-128 字节", false)
            passwordField.forceFocus()
            return
        }
        if (password !== confirm) {
            root.showFeedback("两次输入的密码不一致", false)
            confirmField.forceFocus()
            return
        }

        root.showFeedback("正在重置密码...", true)
        root.resetRequested(email, code, password, confirm)
    }

    Timer {
        id: codeTimer

        interval: 1000
        repeat: true

        onTriggered: {
            root.codeCountdown -= 1
            if (root.codeCountdown <= 0) {
                root.codeCountdown = 0
                stop()
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 13

        AuthField {
            id: emailField

            Layout.fillWidth: true
            label: "注册邮箱"
            placeholder: "name@example.com"
            onEdited: root.clearFeedback()
        }

        AuthField {
            id: codeField

            Layout.fillWidth: true
            label: "验证码"
            placeholder: "4-8 位字母或数字"
            actionText: root.codeCountdown > 0
                        ? root.codeCountdown + "s 后重试"
                        : "获取验证码"
            actionEnabled: root.codeCountdown === 0
            onActionClicked: root.requestCode()
            onEdited: root.clearFeedback()
        }

        AuthField {
            id: passwordField

            Layout.fillWidth: true
            label: "新密码"
            placeholder: "8-128 字节"
            passwordMode: true
            onEdited: root.clearFeedback()
        }

        AuthField {
            id: confirmField

            Layout.fillWidth: true
            label: "确认新密码"
            placeholder: "再次输入新密码"
            passwordMode: true
            onEdited: root.clearFeedback()
            onSubmitted: root.submit()
        }

        AuthButton {
            Layout.fillWidth: true
            Layout.topMargin: 2
            text: "重置密码"
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

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2
            spacing: 6

            Text {
                text: "想起来了？"
                color: "#7D899A"
                font.pixelSize: 12
            }

            LinkText {
                text: "返回登录"
                onClicked: root.backRequested()
            }

            Item { Layout.fillWidth: true }
        }
    }
}
