import QtQuick
import QtQuick.Effects

// 圆形头像。没有裁剪参数时按中心裁切，有裁剪参数时按参数平移/缩放绘制。
Item {
    id: avatar

    property string imageSource: ""
    property string initial: ""
    property int diameter: 96
    property real cropScaleX: 0
    property real cropScaleY: 0
    property real cropX: 0
    property real cropY: 0

    property color baseColor: "#33294D"
    property color borderColor: "#4A3C68"
    property color initialColor: "#F2EEFF"

    readonly property bool hasCrop: cropScaleX > 0 && cropScaleY > 0

    width: diameter
    height: diameter

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: avatar.baseColor
        border.width: 1
        border.color: avatar.borderColor

        Text {
            anchors.centerIn: parent
            visible: avatar.imageSource === ""
            text: avatar.initial
            color: avatar.initialColor
            font.pixelSize: Math.round(avatar.diameter * 0.36)
            font.bold: true
        }
    }

    Item {
        id: croppedFrame

        anchors.fill: parent
        anchors.margins: 1
        clip: true
        visible: false

        Image {
            width: croppedFrame.width * avatar.cropScaleX
            height: croppedFrame.height * avatar.cropScaleY
            x: croppedFrame.width * avatar.cropX
            y: croppedFrame.height * avatar.cropY
            source: avatar.imageSource
            fillMode: Image.Stretch
            asynchronous: true
            cache: true
            mipmap: true
            smooth: true
        }
    }

    Image {
        id: avatarImage

        anchors.fill: parent
        anchors.margins: 1
        source: avatar.imageSource
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        anchors.margins: 1
        source: avatarImage
        visible: avatar.imageSource !== "" && !avatar.hasCrop
        maskEnabled: true
        maskSource: avatarMask
    }

    MultiEffect {
        anchors.fill: parent
        anchors.margins: 1
        source: croppedFrame
        visible: avatar.imageSource !== "" && avatar.hasCrop
        maskEnabled: true
        maskSource: avatarMask
    }

    Item {
        id: avatarMask

        anchors.fill: parent
        anchors.margins: 1
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: "black"
        }
    }
}