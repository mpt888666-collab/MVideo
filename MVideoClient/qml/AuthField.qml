import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property string label: ""
    property string placeholder: ""
    property alias text: input.text
    property alias readOnly: input.readOnly
    property bool passwordMode: false
    property bool showPassword: false
    property string actionText: ""
    property bool actionEnabled: true

    readonly property bool actionAvailable: passwordMode || (actionText.length > 0 && actionEnabled)

    signal actionClicked()
    signal edited()
    signal submitted()

    function forceFocus() {
        input.forceActiveFocus()
    }

    spacing: 7

    Text {
        Layout.fillWidth: true
        text: root.label
        color: "#A7B2C1"
        font.pixelSize: 12
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 50
        radius: 12
        color: "#0C131B"
        border.width: 1
        border.color: input.activeFocus ? "#2C8C80" : "#25323E"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 12
            spacing: 8

            TextField {
                id: input

                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#F2F6FA"
                placeholderText: root.placeholder
                placeholderTextColor: "#596677"
                font.pixelSize: 14
                selectByMouse: true
                selectionColor: "#20F1CB"
                selectedTextColor: "#04110F"
                verticalAlignment: TextInput.AlignVCenter
                echoMode: root.passwordMode && !root.showPassword
                          ? TextInput.Password
                          : TextInput.Normal
                passwordCharacter: "●"
                background: null

                onTextChanged: root.edited()
                onAccepted: root.submitted()
            }

            Item {
                id: actionHit

                visible: root.passwordMode || root.actionText.length > 0
                Layout.preferredWidth: visible ? actionLabel.implicitWidth : 0
                Layout.preferredHeight: 30

                Text {
                    id: actionLabel

                    anchors.centerIn: parent
                    text: root.passwordMode
                          ? (root.showPassword ? "隐藏" : "显示")
                          : root.actionText
                    color: root.actionAvailable ? "#20F1CB" : "#566273"
                    font.pixelSize: 12
                    opacity: root.actionAvailable ? 1.0 : 0.55
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.actionAvailable
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if (root.passwordMode) {
                            root.showPassword = !root.showPassword
                        } else {
                            root.actionClicked()
                        }
                    }
                }
            }
        }
    }
}
