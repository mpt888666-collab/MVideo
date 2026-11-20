import QtQuick
import QtQuick.Effects

// 单个作品卡片，作为 WorksGrid 的 delegate 使用（title/workState/coverUrl 来自模型）。
Rectangle {
    id: card

    required property string title
    required property int workState
    required property string coverUrl
    required property int uploadPercent
    required property var videoId

    readonly property bool uploading: workState <= 1

    signal playRequested(var videoId)

    width: 180
    height: 240
    radius: 10
    clip: true
    color: "#12151D"
    border.width: 1
    border.color: cardHover.containsMouse ? "#3C4353" : "#232733"

    function stateLabel(state) {
        if (state === 0)
            return "上传中"
        if (state === 1)
            return "上传中"
        if (state === 2)
            return "解码中"
        if (state === 3)
            return "已发布"
        if (state === 4)
            return "发布失败"
        return ""
    }

    Rectangle {
        anchors.fill: parent
        radius: card.radius
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#2A2140" }
            GradientStop { position: 1.0; color: "#0E1117" }
        }
    }

    Image {
        id: cover

        anchors.fill: parent
        source: card.coverUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: cover
        maskEnabled: true
        maskSource: coverMask
        visible: card.coverUrl !== ""
        blurEnabled: card.uploading
        blur: 0.85
        blurMax: 32
    }

    Item {
        id: coverMask

        anchors.fill: parent
        visible: false
        layer.enabled: true

        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "black"
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: card.radius
        color: "#99000000"
        visible: card.uploading
    }

    Canvas {
        id: progressRing

        width: 64
        height: 64
        anchors.centerIn: parent
        visible: card.uploading

        property int percent: card.uploadPercent

        onPercentChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            const cx = width / 2
            const cy = height / 2
            const radius = 24

            ctx.lineWidth = 4
            ctx.lineCap = "round"

            ctx.beginPath()
            ctx.strokeStyle = "rgba(255,255,255,0.22)"
            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
            ctx.stroke()

            const ratio = Math.max(0.02, Math.min(1, percent / 100))
            ctx.beginPath()
            ctx.strokeStyle = "#FE2C55"
            ctx.arc(cx, cy, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * ratio)
            ctx.stroke()
        }
    }

    Text {
        anchors.centerIn: progressRing
        text: card.uploadPercent + "%"
        color: "#FFFFFF"
        font.pixelSize: 12
        font.bold: true
        visible: card.uploading
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -18
        text: "▶"
        color: "#FFFFFF"
        opacity: 0.35
        font.pixelSize: 30
        visible: !card.uploading
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: stateText.top
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 6
        text: card.title
        color: "#E8EDF5"
        font.pixelSize: 13
        elide: Text.ElideRight
    }

    Text {
        id: stateText

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 12
        anchors.bottomMargin: 12
        text: card.stateLabel(card.workState)
        color: card.workState === 3 ? "#4CC38A" : (card.workState === 2 ? "#E8B339" : "#8C93A1")
        font.pixelSize: 11
    }

    MouseArea {
        id: cardHover

        anchors.fill: parent
        hoverEnabled: true
        enabled: card.workState === 3 && Number(card.videoId) > 0
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: card.playRequested(card.videoId)
    }
}