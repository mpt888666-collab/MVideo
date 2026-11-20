import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

// 头像裁剪：拖动/缩放图片，保存后把裁剪参数（相对视口的比例）交给调用方。
Rectangle {
    id: dialog

    property bool opened: false
    property string imageUrl: ""
    property string initial: ""

    readonly property real cropScaleX:
        cropImage.width > 0 ? cropImage.width / cropViewport.width : 1
    readonly property real cropScaleY:
        cropImage.height > 0 ? cropImage.height / cropViewport.height : 1
    readonly property real cropX:
        cropImage.width > 0 ? cropImage.x / cropViewport.width : 0
    readonly property real cropY:
        cropImage.height > 0 ? cropImage.y / cropViewport.height : 0

    signal accepted(string url, real scaleX, real scaleY, real offsetX, real offsetY)
    signal canceled()

    function open(url) {
        dialog.imageUrl = url
        zoomSlider.value = 1.0
        cropViewport.centerCrop()
        dialog.opened = true
    }

    function close() {
        dialog.opened = false
    }

    anchors.fill: parent
    z: 260
    visible: dialog.opened
    color: "#B3000000"

    Shortcut {
        sequence: "Esc"
        enabled: dialog.opened
        onActivated: dialog.canceled()
    }

    FileDialog {
        id: fileDialog

        title: "选择头像"
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.webp *.bmp)"]

        onAccepted: {
            dialog.imageUrl = selectedFile
            zoomSlider.value = 1.0
            cropViewport.centerCrop()
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: dialog.canceled()
    }

    Rectangle {
        id: card

        width: 760
        height: 620
        anchors.centerIn: parent
        radius: 16
        color: "#1F222B"
        border.width: 1
        border.color: "#303644"

        MouseArea {
            anchors.fill: parent
        }

        Text {
            x: 40
            y: 30
            text: "修改头像"
            color: "#FFFFFF"
            font.pixelSize: 22
            font.bold: true
        }

        Text {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 26
            anchors.rightMargin: 28
            text: "✕"
            color: "#AEB5C0"
            font.pixelSize: 22

            MouseArea {
                anchors.fill: parent
                anchors.margins: -10
                cursorShape: Qt.PointingHandCursor
                onClicked: dialog.canceled()
            }
        }

        Row {
            x: 40
            y: 98
            spacing: 36

            Column {
                width: 400
                spacing: 16

                Rectangle {
                    id: cropViewport

                    width: 400
                    height: 400
                    radius: 2
                    color: "#14161C"
                    border.width: 1
                    border.color: "#343945"
                    clip: true

                    function centerCrop() {
                        cropImage.x = (width - cropImage.width) / 2
                        cropImage.y = (height - cropImage.height) / 2
                    }

                    Component.onCompleted: centerCrop()

                    Image {
                        id: cropImage

                        readonly property real coverScale: Math.max(
                            cropViewport.width / Math.max(1, implicitWidth),
                            cropViewport.height / Math.max(1, implicitHeight)
                        )

                        width: implicitWidth * coverScale * zoomSlider.value
                        height: implicitHeight * coverScale * zoomSlider.value
                        source: dialog.imageUrl
                        fillMode: Image.Stretch
                        asynchronous: true
                        cache: false
                        mipmap: true
                        onWidthChanged: cropViewport.centerCrop()
                        onHeightChanged: cropViewport.centerCrop()
                        onStatusChanged: {
                            if (status === Image.Ready)
                                cropViewport.centerCrop()
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        drag.target: cropImage
                        drag.axis: Drag.XAndYAxis
                        drag.minimumX: cropViewport.width - cropImage.width
                        drag.maximumX: 0
                        drag.minimumY: cropViewport.height - cropImage.height
                        drag.maximumY: 0
                        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                    }
                }

                Item {
                    width: 400
                    height: 30

                    Text {
                        id: zoomOutIcon

                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "−"
                        color: "#B7BDC8"
                        font.pixelSize: 20
                    }

                    Text {
                        id: zoomInIcon

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "+"
                        color: "#B7BDC8"
                        font.pixelSize: 20
                    }

                    Slider {
                        id: zoomSlider

                        anchors.left: zoomOutIcon.right
                        anchors.right: zoomInIcon.left
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        from: 1.0
                        to: 2.5
                        value: 1.0
                        leftPadding: 0
                        rightPadding: 0

                        background: Rectangle {
                            x: 0
                            y: (zoomSlider.height - height) / 2
                            width: zoomSlider.availableWidth
                            height: 4
                            radius: 2
                            color: "#414754"

                            Rectangle {
                                width: zoomSlider.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: "#D8DDE6"
                            }
                        }

                        handle: Rectangle {
                            x: zoomSlider.leftPadding
                               + zoomSlider.visualPosition
                               * (zoomSlider.availableWidth - width)
                            y: zoomSlider.topPadding
                               + (zoomSlider.availableHeight - height) / 2
                            width: 18
                            height: 18
                            radius: 9
                            color: "#F2F4F8"
                        }
                    }
                }

                Row {
                    spacing: 16

                    Rectangle {
                        width: 122
                        height: 42
                        radius: 8
                        color: saveHover.containsMouse ? "#FF4D72" : "#FE2C55"

                        Text {
                            anchors.centerIn: parent
                            text: "保存"
                            color: "#FFFFFF"
                            font.pixelSize: 15
                            font.bold: true
                        }

                        MouseArea {
                            id: saveHover

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.accepted(dialog.imageUrl, dialog.cropScaleX,
                                                       dialog.cropScaleY, dialog.cropX, dialog.cropY)
                        }
                    }

                    Rectangle {
                        width: 122
                        height: 42
                        radius: 8
                        color: cancelHover.containsMouse ? "#383E4B" : "#2C313C"

                        Text {
                            anchors.centerIn: parent
                            text: "取消"
                            color: "#E8EDF5"
                            font.pixelSize: 15
                        }

                        MouseArea {
                            id: cancelHover

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialog.canceled()
                        }
                    }
                }
            }

            Column {
                width: 200
                spacing: 10

                CircleAvatar {
                    diameter: 96
                    initial: dialog.initial
                    imageSource: dialog.imageUrl
                    cropScaleX: dialog.cropScaleX
                    cropScaleY: dialog.cropScaleY
                    cropX: dialog.cropX
                    cropY: dialog.cropY
                }

                Text {
                    text: "头像预览"
                    color: "#D4D9E2"
                    font.pixelSize: 14
                }

                Text {
                    text: "审核通过后展示"
                    color: "#7F8794"
                    font.pixelSize: 12
                }

                Item {
                    width: 120
                    height: 28

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "重新上传"
                        color: reuploadHover.containsMouse ? "#FF6B88" : "#FE2C55"
                        font.pixelSize: 14

                        MouseArea {
                            id: reuploadHover

                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: fileDialog.open()
                        }
                    }
                }
            }
        }
    }
}