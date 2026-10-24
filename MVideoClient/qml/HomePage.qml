import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string currentPage: "推荐"

    signal logoutRequested()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0A0E15" }
            GradientStop { position: 0.55; color: "#080B10" }
            GradientStop { position: 1.0; color: "#0C1418" }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 92
            color: "#090D13"

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 26
                anchors.bottomMargin: 24
                spacing: 8

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 16
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#20F1CB" }
                        GradientStop { position: 1.0; color: "#4E7BFF" }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "M"
                        color: "#061014"
                        font.pixelSize: 25
                        font.bold: true
                    }
                }

                Item { Layout.preferredHeight: 24 }

                Repeater {
                    model: [
                        { title: "推荐", glyph: "⌂" },
                        { title: "消息", glyph: "✉" },
                        { title: "我的", glyph: "●" }
                    ]

                    delegate: Rectangle {
                        id: navItem

                        required property var modelData
                        readonly property bool selected: root.currentPage === modelData.title

                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 70
                        Layout.preferredHeight: 70
                        radius: 20
                        color: selected ? "#132520" : "transparent"
                        border.width: selected ? 1 : 0
                        border.color: "#1E5A4F"

                        Column {
                            anchors.centerIn: parent
                            spacing: 4

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: navItem.modelData.glyph
                                color: navItem.selected ? "#20F1CB" : "#7D899A"
                                font.pixelSize: 22
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: navItem.modelData.title
                                color: navItem.selected ? "#EFFFFA" : "#7D899A"
                                font.pixelSize: 12
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.currentPage = navItem.modelData.title
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "0.1"
                    color: "#465162"
                    font.pixelSize: 11
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 32
            spacing: 22

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    spacing: 4

                    Text {
                        text: "MVideo"
                        color: "#F5F7FA"
                        font.pixelSize: 30
                        font.bold: true
                    }

                    Text {
                        text: "首页预览 · 当前页面：" + root.currentPage
                        color: "#7D899A"
                        font.pixelSize: 13
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 104
                    Layout.preferredHeight: 38
                    radius: 19
                    color: "#101820"
                    border.width: 1
                    border.color: "#1D2A36"

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 4
                            color: "#F5B942"
                        }

                        Text {
                            text: "UI 预览"
                            color: "#9AA6B7"
                            font.pixelSize: 12
                        }
                    }
                }

                AuthButton {
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 38
                    text: "退出登录"
                    onClicked: root.logoutRequested()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 28
                clip: true
                border.width: 1
                border.color: "#1A2731"
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#101D23" }
                    GradientStop { position: 0.55; color: "#0C151B" }
                    GradientStop { position: 1.0; color: "#0A0F15" }
                }

                Rectangle {
                    width: 420
                    height: 420
                    radius: 210
                    x: parent.width - width * 0.62
                    y: -height * 0.25
                    color: "#133B39"
                    opacity: 0.45
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 120, 720)
                    spacing: 18

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 116
                        Layout.preferredHeight: 28
                        radius: 14
                        color: "#10241F"
                        border.width: 1
                        border.color: "#26574D"

                        Text {
                            anchors.centerIn: parent
                            text: "AUTH UI READY"
                            color: "#20F1CB"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "登录、注册、找回密码页面已就绪"
                        color: "#F5F7FA"
                        font.pixelSize: 32
                        font.bold: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: 590
                        text: "当前完成的是 Qt Quick 界面与表单交互。下一步把提交动作接到 GetServer 的验证码、注册、登录和重置密码接口。"
                        color: "#93A0B2"
                        font.pixelSize: 14
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 8
                        spacing: 12

                        Repeater {
                            model: ["短视频", "IM 消息", "语音视频"]

                            delegate: Rectangle {
                                required property var modelData

                                Layout.preferredWidth: 110
                                Layout.preferredHeight: 38
                                radius: 12
                                color: "#0E1A21"
                                border.width: 1
                                border.color: "#1D303A"

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: "#A9B5C3"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Qt 6 · Qt Quick"
                    color: "#586475"
                    font.pixelSize: 11
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "C++ backend: gateway / IM / RTC"
                    color: "#586475"
                    font.pixelSize: 11
                }
            }
        }
    }
}
