import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

// 编辑资料弹窗：昵称、简介和头像草稿。头像裁剪在 AvatarCropDialog 里完成，
// 这里只保存草稿，点“保存”时把结果整体交给调用方。
Rectangle {
    id: dialog

    property bool opened: false
    property string initialName: ""
    property string initialBio: ""
    property string avatarSource: ""
    property string initial: ""
    property real avatarCropScaleX: 0
    property real avatarCropScaleY: 0
    property real avatarCropX: 0
    property real avatarCropY: 0

    property string draftAvatarUrl: ""
    property real draftCropScaleX: 0
    property real draftCropScaleY: 0
    property real draftCropX: 0
    property real draftCropY: 0
    property bool avatarDirty: false

    signal accepted(string name, string bio, string avatarUrl,
                    real scaleX, real scaleY, real offsetX, real offsetY,
                    bool avatarChanged)
    signal canceled()

    function close() {
        dialog.opened = false
    }

    onOpenedChanged: {
        if (!opened)
            return
        dialog.draftAvatarUrl = dialog.avatarSource
        dialog.draftCropScaleX = dialog.avatarCropScaleX
        dialog.draftCropScaleY = dialog.avatarCropScaleY
        dialog.draftCropX = dialog.avatarCropX
        dialog.draftCropY = dialog.avatarCropY
        dialog.avatarDirty = false
        nameField.text = dialog.initialName
        bioField.text = dialog.initialBio
    }

    anchors.fill: parent
    z: 200
    visible: dialog.opened
    color: "#99000000"

    Shortcut {
        sequence: "Esc"
        enabled: dialog.opened && !cropDialog.opened
        onActivated: dialog.canceled()
    }

    MouseArea {
        anchors.fill: parent
        onClicked: dialog.canceled()
    }

    FileDialog {
        id: fileDialog

        title: "选择头像"
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"]

        onAccepted: cropDialog.open(selectedFile)
    }

    Rectangle {
        id: card

        width: 520
        height: editColumn.height + 56
        anchors.centerIn: parent
        radius: 12
        color: "#1E222B"
        border.width: 1
        border.color: "#2F3542"

        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: editColumn

            x: 32
            y: 28
            width: parent.width - 64
            spacing: 16

            Item {
                width: parent.width
                height: 26

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "编辑资料"
                    color: "#FFFFFF"
                    font.pixelSize: 18
                    font.bold: true
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: "✕"
                    color: "#8C93A1"
                    font.pixelSize: 16

                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -10
                        cursorShape: Qt.PointingHandCursor
                        onClicked: dialog.canceled()
                    }
                }
            }

            Item {
                width: parent.width
                height: 124

                Item {
                    width: 96
                    height: 96
                    anchors.horizontalCenter: parent.horizontalCenter

                    CircleAvatar {
                        anchors.fill: parent
                        diameter: 96
                        initial: dialog.initial
                        imageSource: dialog.draftAvatarUrl
                        cropScaleX: dialog.draftCropScaleX
                        cropScaleY: dialog.draftCropScaleY
                        cropX: dialog.draftCropX
                        cropY: dialog.draftCropY
                    }

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 2
                        anchors.bottomMargin: 2
                        color: "#F2F4F8"

                        Text {
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: -1
                            text: "+"
                            color: "#1E222B"
                            font.pixelSize: 18
                            font.bold: true
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileDialog.open()
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    text: "点击修改头像"
                    color: avatarHintHover.containsMouse ? "#E8EDF5" : "#8C93A1"
                    font.pixelSize: 12

                    MouseArea {
                        id: avatarHintHover

                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: fileDialog.open()
                    }
                }
            }

            Text {
                text: "名字"
                color: "#C6CCD6"
                font.pixelSize: 13
            }

            Item {
                width: parent.width
                height: 40

                TextField {
                    id: nameField

                    anchors.fill: parent
                    leftPadding: 12
                    rightPadding: 56
                    color: "#E8EDF5"
                    font.pixelSize: 14
                    selectByMouse: true
                    maximumLength: 20

                    background: Rectangle {
                        radius: 8
                        color: "#262B36"
                        border.width: 1
                        border.color: nameField.activeFocus ? "#4A5163" : "transparent"
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: nameField.text.length + "/20"
                    color: "#6E7683"
                    font.pixelSize: 12
                }
            }

            Text {
                text: "简介"
                color: "#C6CCD6"
                font.pixelSize: 13
            }

            TextArea {
                id: bioField

                width: parent.width
                height: 132
                leftPadding: 12
                rightPadding: 12
                topPadding: 10
                bottomPadding: 10
                color: "#E8EDF5"
                placeholderText: "介绍一下你自己"
                placeholderTextColor: "#6E7683"
                font.pixelSize: 14
                wrapMode: TextArea.Wrap
                selectByMouse: true

                background: Rectangle {
                    radius: 8
                    color: "#262B36"
                    border.width: 1
                    border.color: bioField.activeFocus ? "#4A5163" : "transparent"
                }
            }

            Item {
                width: parent.width
                height: 44

                Row {
                    anchors.centerIn: parent
                    spacing: 24

                    Rectangle {
                        width: 150
                        height: 44
                        radius: 8
                        color: cancelHover.containsMouse ? "#343A47" : "#2A2F3A"

                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            color: "#E8EDF5"
                            font.pixelSize: 14
                        }

                        MouseArea {
                            id: cancelHover

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.canceled()
                        }
                    }

                    Rectangle {
                        width: 150
                        height: 44
                        radius: 8
                        color: saveHover.containsMouse ? "#D94460" : "#C2334D"

                        Text {
                            anchors.centerIn: parent
                            text: "保存"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: saveHover

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.accepted(nameField.text.trim(), bioField.text,
                                                       dialog.draftAvatarUrl,
                                                       dialog.draftCropScaleX, dialog.draftCropScaleY,
                                                       dialog.draftCropX, dialog.draftCropY,
                                                       dialog.avatarDirty)
                        }
                    }
                }
            }
        }
    }

    AvatarCropDialog {
        id: cropDialog

        initial: dialog.initial

        onAccepted: function(url, scaleX, scaleY, offsetX, offsetY) {
            dialog.draftAvatarUrl = url
            dialog.draftCropScaleX = scaleX
            dialog.draftCropScaleY = scaleY
            dialog.draftCropX = offsetX
            dialog.draftCropY = offsetY
            dialog.avatarDirty = true
            cropDialog.close()
        }

        onCanceled: cropDialog.close()
    }
}