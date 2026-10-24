import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root

    width: 1280
    height: 800
    minimumWidth: 980
    minimumHeight: 720
    visible: true
    title: "MVideo"
    color: "#080B10"

    StackView {
        id: stack

        anchors.fill: parent
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

    Component.onCompleted: {
        console.log("AuthServer =", AuthServer)
    }

    Component {
        id: loginPage

        LoginPage {
            onRegisterRequested: stack.push(registerPage)
            onForgotPasswordRequested: stack.push(forgotPasswordPage)
            onPreviewRequested: stack.replace(homePage)
            onLoginRequested: function(email, password) {
                console.log("Login requested:", email)
                AuthServer.requestLogin(email, password, "")
            }
            onLoginSucceeded: stack.replace(homePage)
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
            onLogoutRequested: stack.replace(loginPage)
        }
    }


}
