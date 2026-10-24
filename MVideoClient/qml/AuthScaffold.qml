import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    default property alias contentData: contentColumn.data
    property string eyebrow: "MVIDEO ACCOUNT"
    property string title: ""
    property string subtitle: ""
    property bool showBack: false

    signal backRequested()

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#080C12" }
            GradientStop { position: 0.55; color: "#0A1018" }
            GradientStop { position: 1.0; color: "#0B151A" }
        }
    }

    Rectangle {
        width: 480
        height: 480
        radius: 240
        x: root.width - 390
        y: -180
        color: "#143A35"
        opacity: 0.55
    }

    Rectangle {
        width: 320
        height: 320
        radius: 160
        x: -130
        y: root.height - 230
        color: "#192C57"
        opacity: 0.48
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 48
        spacing: 64

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                spacing: 14

                RowLayout {
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        radius: 15
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#20F1CB" }
                            GradientStop { position: 1.0; color: "#4E7BFF" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "M"
                            color: "#04110F"
                            font.pixelSize: 24
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        spacing: 1

                        Text {
                            text: "MVideo"
                            color: "#F5F7FA"
                            font.pixelSize: 20
                            font.bold: true
                        }

                        Text {
                            text: "短视频 · IM · RTC"
                            color: "#647184"
                            font.pixelSize: 11
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Text {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 520
                    text: "让每一段视频，\n都值得被看见。"
                    color: "#F5F7FA"
                    font.pixelSize: 44
                    font.bold: true
                    lineHeight: 1.08
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    Layout.maximumWidth: 430
                    text: "一个账号，连接你的内容、消息和实时通话。"
                    color: "#8F9BAD"
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                }

                ColumnLayout {
                    Layout.topMargin: 24
                    spacing: 11

                    Repeater {
                        model: [
                            "沉浸式短视频浏览",
                            "私聊、群聊与消息同步",
                            "后续接入语音视频通话"
                        ]

                        delegate: RowLayout {
                            required property var modelData
                            spacing: 10

                            Rectangle {
                                Layout.preferredWidth: 6
                                Layout.preferredHeight: 6
                                radius: 3
                                color: "#20F1CB"
                            }

                            Text {
                                text: modelData
                                color: "#AAB5C4"
                                font.pixelSize: 13
                            }
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Text {
                    text: "MVideo · Qt Quick Client"
                    color: "#435061"
                    font.pixelSize: 11
                }
            }
        }

        Rectangle {
            id: authCard

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 480
            Layout.preferredHeight: Math.min(root.height - 72, 720)
            radius: 28
            color: "#E60E151E"
            border.width: 1
            border.color: "#293542"
            clip: true

            Flickable {
                id: scroller

                anchors.fill: parent
                contentWidth: width
                contentHeight: contentColumn.implicitHeight + 64
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }

                ColumnLayout {
                    id: contentColumn

                    x: 32
                    y: 32
                    width: scroller.width - 64
                    spacing: 16

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.showBack ? 28 : 0
                        visible: root.showBack

                        RowLayout {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 7

                            Text {
                                text: "←"
                                color: "#20F1CB"
                                font.pixelSize: 17
                            }

                            Text {
                                text: "返回登录"
                                color: "#9EABBC"
                                font.pixelSize: 12
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.backRequested()
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.eyebrow
                        color: "#20F1CB"
                        font.pixelSize: 11
                        font.letterSpacing: 1.4
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.title
                        color: "#F5F7FA"
                        font.pixelSize: 31
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.subtitle
                        color: "#8491A3"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.topMargin: 4
                        Layout.bottomMargin: 2
                        Layout.preferredHeight: 1
                        color: "#22303C"
                    }
                }
            }
        }
    }
}
