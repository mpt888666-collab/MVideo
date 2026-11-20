import QtQuick
import QtQuick.Controls
import QtMultimedia

// 作品播放页：只负责播放器与播放状态，播放地址由 AuthServer/VideoPlaybackController 下发。
Page {
    id: root

    property var videoId: 0
    property string videoTitle: ""
    property string videoDescription: ""
    property string coverUrl: ""
    property string errorMessage: ""

    property int likeCount: 0
    property int commentCount: 0
    property int favoriteCount: 0
    property int playCount: 0

    property bool loading: true
    property bool controlsVisible: true
    property bool controlsHovered: false
    property bool seekDragging: false
    property real seekValue: 0

    readonly property bool playing: player.playbackState === MediaPlayer.PlayingState
    readonly property bool buffering:
        player.mediaStatus === MediaPlayer.LoadingMedia ||
        player.mediaStatus === MediaPlayer.BufferingMedia
    readonly property bool hasError: root.errorMessage.length > 0

    signal backRequested()

    padding: 0
    focus: true
    background: Rectangle { color: "#000000" }

    function formatTime(ms) {
        const total = Math.max(0, Math.floor((Number(ms) || 0) / 1000))
        const minutes = Math.floor(total / 60)
        const seconds = total % 60
        return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds)
    }

    function formatCount(value) {
        const n = Math.max(0, Math.floor(Number(value) || 0))
        if (n >= 100000000)
            return (n / 100000000).toFixed(1).replace(".0", "") + "亿"
        if (n >= 10000)
            return (n / 10000).toFixed(1).replace(".0", "") + "万"
        return String(n)
    }

    function requestVideo() {
        if (Number(root.videoId) <= 0)
            return
        root.errorMessage = ""
        root.loading = true
        AuthServer.playVideo(root.videoId)
    }

    function wakeControls() {
        root.controlsVisible = true
        hideTimer.restart()
    }

    function togglePlay() {
        if (!player.hasVideo)
            return
        if (root.playing)
            player.pause()
        else
            player.play()
    }

    function stepSeek(deltaMs) {
        if (!player.seekable)
            return
        player.position = Math.max(0, Math.min(player.duration, player.position + deltaMs))
        root.wakeControls()
    }

    Component.onCompleted: {
        root.forceActiveFocus()
        root.requestVideo()
    }

    Connections {
        target: AuthServer

        function onPlayReady(videoId, info) {
            if (Number(videoId) !== Number(root.videoId))
                return
            root.videoTitle = info.title || ""
            root.videoDescription = info.description || ""
            root.coverUrl = info.coverUrl || ""
            root.likeCount = Number(info.likeCount) || 0
            root.commentCount = Number(info.commentCount) || 0
            root.favoriteCount = Number(info.favoriteCount) || 0
            root.playCount = Number(info.playCount) || 0
            root.errorMessage = ""
            root.loading = false
            player.source = info.masterUrl
            player.play()
            root.wakeControls()
        }

        function onPlayFailed(videoId, message) {
            if (Number(videoId) !== Number(root.videoId))
                return
            root.loading = false
            root.errorMessage = message
        }
    }

    MediaPlayer {
        id: player

        videoOutput: videoOutput

        audioOutput: AudioOutput {
            id: audio
            volume: 0.8
        }

        loops: MediaPlayer.Infinite

        onErrorOccurred: function(error, errorString) {
            root.loading = false
            root.errorMessage = errorString
        }

        onPlaybackStateChanged: {
            if (player.playbackState === MediaPlayer.PlayingState) {
                hideTimer.restart()
            } else {
                root.controlsVisible = true
                hideTimer.stop()
            }
        }
    }

    Image {
        anchors.fill: parent
        source: root.coverUrl
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        visible: root.coverUrl.length > 0 && (root.loading || !player.hasVideo)
    }

    VideoOutput {
        id: videoOutput

        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: !root.loading && player.hasVideo && !root.hasError
    }

    // 视频未出画时的兜底底色
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        visible: root.loading || root.hasError || !player.hasVideo
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.togglePlay()
        onPositionChanged: root.wakeControls()
        onEntered: root.wakeControls()
    }

    BusyIndicator {
        anchors.centerIn: parent
        width: 56
        height: 56
        running: (root.loading || root.buffering) && !root.hasError
        visible: running
    }

    Timer {
        id: hideTimer

        interval: 2600
        repeat: false

        onTriggered: {
            if (root.playing && !root.seekDragging && !root.controlsHovered)
                root.controlsVisible = false
        }
    }

    Rectangle {
        id: topBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 96
        opacity: (root.controlsVisible || root.hasError) ? 1 : 0
        visible: opacity > 0
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#B3000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }

        Behavior on opacity { NumberAnimation { duration: 160 } }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: root.controlsHovered = true
            onExited: root.controlsHovered = false
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 20
            width: 38
            height: 38
            radius: 19
            color: backHover.containsMouse ? "#33FFFFFF" : "#1AFFFFFF"

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -2
                text: "‹"
                color: "#FFFFFF"
                font.pixelSize: 26
                font.bold: true
            }

            MouseArea {
                id: backHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.backRequested()
            }
        }
    }

    // 右侧互动数据（当前只做展示，点赞/评论/收藏的写接口还没有）
    Column {
        id: actionRail

        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.bottom: bottomArea.top
        anchors.bottomMargin: 16
        spacing: 18
        visible: !root.loading && !root.hasError
        opacity: root.controlsVisible ? 1 : 0

        Behavior on opacity { NumberAnimation { duration: 160 } }

        Repeater {
            model: [
                { kind: "like", count: root.likeCount, label: "点赞" },
                { kind: "comment", count: root.commentCount, label: "评论" },
                { kind: "favorite", count: root.favoriteCount, label: "收藏" },
                { kind: "play", count: root.playCount, label: "播放" }
            ]

            delegate: Column {
                required property var modelData

                spacing: 3

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 40
                    height: 40
                    radius: 20
                    color: "#1FFFFFFF"
                    border.width: 1
                    border.color: "#33FFFFFF"

                    IconCanvas {
                        anchors.centerIn: parent
                        kind: modelData.kind
                        size: 19
                        iconColor: "#F2F5F9"
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.formatCount(modelData.count)
                    color: "#E8EDF5"
                    font.pixelSize: 11
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.label
                    color: "#98A2B1"
                    font.pixelSize: 10
                }
            }
        }
    }

    Rectangle {
        id: bottomArea

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 196
        visible: !root.hasError
        opacity: root.controlsVisible ? 1 : 0
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 0.5; color: "#8C000000" }
            GradientStop { position: 1.0; color: "#D9000000" }
        }

        Behavior on opacity { NumberAnimation { duration: 160 } }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onEntered: root.controlsHovered = true
            onExited: root.controlsHovered = false
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: controlRow.top
            anchors.leftMargin: 22
            anchors.rightMargin: 96
            anchors.bottomMargin: 14
            spacing: 8

            Text {
                width: parent.width
                text: root.videoTitle
                color: "#FFFFFF"
                font.pixelSize: 18
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                visible: text.length > 0
                text: root.videoDescription
                color: "#C9D1DC"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        Row {
            id: controlRow

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 22
            spacing: 12

            Item {
                width: 26
                height: 26
                anchors.verticalCenter: parent.verticalCenter

                IconCanvas {
                    anchors.centerIn: parent
                    kind: root.playing ? "pause" : "play"
                    size: 16
                    iconColor: "#FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.togglePlay()
                }
            }

            Text {
                width: 44
                anchors.verticalCenter: parent.verticalCenter
                text: root.formatTime(player.position)
                color: "#D6DCE5"
                font.pixelSize: 12
            }

            Slider {
                id: seek

                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(60, controlRow.width - 188)
                height: 20
                from: 0
                to: Math.max(1, player.duration)
                value: root.seekDragging ? root.seekValue : Math.min(player.position, to)
                enabled: player.seekable && player.duration > 0

                onMoved: root.seekValue = value

                onPressedChanged: {
                    if (pressed) {
                        root.seekValue = player.position
                        root.seekDragging = true
                    } else {
                        player.position = root.seekValue
                        root.seekDragging = false
                    }
                }

                background: Rectangle {
                    x: seek.leftPadding
                    y: seek.topPadding + seek.availableHeight / 2 - height / 2
                    width: seek.availableWidth
                    height: 4
                    radius: 2
                    color: "#4A5260"

                    Rectangle {
                        width: seek.visualPosition * parent.width
                        height: parent.height
                        radius: 2
                        color: "#FE2C55"
                    }
                }

                handle: Rectangle {
                    x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                    y: seek.topPadding + seek.availableHeight / 2 - height / 2
                    width: 12
                    height: 12
                    radius: 6
                    color: "#FFFFFF"
                    visible: seek.pressed || seek.hovered || root.seekDragging
                    scale: (seek.pressed || root.seekDragging) ? 1.25 : 1.0

                    Behavior on scale { NumberAnimation { duration: 100 } }
                }
            }

            Text {
                width: 44
                anchors.verticalCenter: parent.verticalCenter
                text: root.formatTime(player.duration)
                color: "#8F98A6"
                font.pixelSize: 12
            }

            Item {
                width: 26
                height: 26
                anchors.verticalCenter: parent.verticalCenter

                IconCanvas {
                    anchors.centerIn: parent
                    kind: audio.muted ? "mute" : "sound"
                    size: 17
                    iconColor: "#FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        audio.muted = !audio.muted
                        root.wakeControls()
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(420, root.width - 80)
        spacing: 16
        visible: root.hasError

        Text {
            width: parent.width
            text: "无法播放"
            color: "#FFFFFF"
            font.pixelSize: 20
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            width: parent.width
            text: root.errorMessage
            color: "#B8C0CC"
            font.pixelSize: 13
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 108
            height: 36
            radius: 18
            color: retryHover.containsMouse ? "#FF4D6D" : "#FE2C55"

            Text {
                anchors.centerIn: parent
                text: "重试"
                color: "#FFFFFF"
                font.pixelSize: 14
            }

            MouseArea {
                id: retryHover
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.requestVideo()
            }
        }
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Space) {
            root.togglePlay()
            event.accepted = true
        } else if (event.key === Qt.Key_Escape) {
            root.backRequested()
            event.accepted = true
        } else if (event.key === Qt.Key_Left) {
            root.stepSeek(-5000)
            event.accepted = true
        } else if (event.key === Qt.Key_Right) {
            root.stepSeek(5000)
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            audio.volume = Math.min(1, audio.volume + 0.05)
            root.wakeControls()
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            audio.volume = Math.max(0, audio.volume - 0.05)
            root.wakeControls()
            event.accepted = true
        } else if (event.key === Qt.Key_M) {
            audio.muted = !audio.muted
            root.wakeControls()
            event.accepted = true
        }
    }

    // 纯代码绘制的图标，避免依赖字体：
    // like / comment / favorite / play / pause / sound / mute
    component IconCanvas: Canvas {
        id: icon

        property string kind: "play"
        property real size: 20
        property color iconColor: "#FFFFFF"

        width: size
        height: size

        onKindChanged: requestPaint()
        onIconColorChanged: requestPaint()
        onSizeChanged: requestPaint()
        Component.onCompleted: requestPaint()

        function roundRectPath(ctx, x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.lineTo(x + w - r, y)
            ctx.arc(x + w - r, y + r, r, -Math.PI / 2, 0)
            ctx.lineTo(x + w, y + h - r)
            ctx.arc(x + w - r, y + h - r, r, 0, Math.PI / 2)
            ctx.lineTo(x + r, y + h)
            ctx.arc(x + r, y + h - r, r, Math.PI / 2, Math.PI)
            ctx.lineTo(x, y + r)
            ctx.arc(x + r, y + r, r, Math.PI, Math.PI * 1.5)
            ctx.closePath()
        }

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.clearRect(0, 0, width, height)

            const cx = width / 2
            const cy = height / 2
            const s = Math.min(width, height) / 2

            ctx.fillStyle = iconColor
            ctx.strokeStyle = iconColor
            ctx.lineWidth = Math.max(1.2, s * 0.15)
            ctx.lineJoin = "round"
            ctx.lineCap = "round"

            if (kind === "like") {
                ctx.beginPath()
                ctx.moveTo(cx, cy + s * 0.78)
                ctx.bezierCurveTo(cx - s * 1.3, cy - s * 0.12, cx - s * 0.66, cy - s * 1.02, cx, cy - s * 0.34)
                ctx.bezierCurveTo(cx + s * 0.66, cy - s * 1.02, cx + s * 1.3, cy - s * 0.12, cx, cy + s * 0.78)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "comment") {
                const w = s * 1.6
                const h = s * 1.24
                roundRectPath(ctx, cx - w / 2, cy - h / 2 - s * 0.1, w, h, s * 0.36)
                ctx.moveTo(cx - s * 0.32, cy + h / 2 - s * 0.1)
                ctx.lineTo(cx - s * 0.42, cy + h / 2 + s * 0.42)
                ctx.lineTo(cx + s * 0.08, cy + h / 2 - s * 0.1)
                ctx.stroke()
            } else if (kind === "favorite") {
                ctx.beginPath()
                for (let i = 0; i < 10; ++i) {
                    const radius = (i % 2 === 0) ? s * 0.98 : s * 0.44
                    const angle = -Math.PI / 2 + i * Math.PI / 5
                    const px = cx + Math.cos(angle) * radius
                    const py = cy + Math.sin(angle) * radius
                    if (i === 0)
                        ctx.moveTo(px, py)
                    else
                        ctx.lineTo(px, py)
                }
                ctx.closePath()
                ctx.fill()
            } else if (kind === "play") {
                ctx.beginPath()
                ctx.moveTo(cx - s * 0.5, cy - s * 0.78)
                ctx.lineTo(cx + s * 0.82, cy)
                ctx.lineTo(cx - s * 0.5, cy + s * 0.78)
                ctx.closePath()
                ctx.fill()
            } else if (kind === "pause") {
                const bw = s * 0.3
                const bh = s * 1.5
                ctx.fillRect(cx - s * 0.62, cy - bh / 2, bw, bh)
                ctx.fillRect(cx + s * 0.32, cy - bh / 2, bw, bh)
            } else if (kind === "sound" || kind === "mute") {
                ctx.beginPath()
                ctx.moveTo(cx - s * 0.95, cy - s * 0.3)
                ctx.lineTo(cx - s * 0.55, cy - s * 0.3)
                ctx.lineTo(cx - s * 0.12, cy - s * 0.8)
                ctx.lineTo(cx - s * 0.12, cy + s * 0.8)
                ctx.lineTo(cx - s * 0.55, cy + s * 0.3)
                ctx.lineTo(cx - s * 0.95, cy + s * 0.3)
                ctx.closePath()
                ctx.fill()

                if (kind === "sound") {
                    ctx.beginPath()
                    ctx.arc(cx - s * 0.05, cy, s * 0.55, -Math.PI / 3, Math.PI / 3)
                    ctx.stroke()
                    ctx.beginPath()
                    ctx.arc(cx - s * 0.05, cy, s * 0.92, -Math.PI / 3, Math.PI / 3)
                    ctx.stroke()
                } else {
                    ctx.beginPath()
                    ctx.moveTo(cx + s * 0.32, cy - s * 0.4)
                    ctx.lineTo(cx + s * 0.95, cy + s * 0.4)
                    ctx.moveTo(cx + s * 0.95, cy - s * 0.4)
                    ctx.lineTo(cx + s * 0.32, cy + s * 0.4)
                    ctx.stroke()
                }
            }
        }
    }
}