import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Popup {
    id: root

    property string fileUrl: ""
    property string fileName: ""
    property string fileHint: ""

    readonly property bool hasFile: fileUrl.length > 0
    readonly property bool canPublish: hasFile && titleInput.text.trim().length > 0

    property var selectedTags: []
    property int maxTags: 5

    readonly property var tagOptions: ["影视", "音乐", "游戏", "美食", "旅行", "搞笑",
                                       "体育", "科技", "数码", "汽车", "时尚", "萌宠"]

    signal publishRequested(string fileUrl, string title, string description, var tags)

    function toggleTag(name) {
        const list = selectedTags.slice()
        const index = list.indexOf(name)

        if (index >= 0)
            list.splice(index, 1)
        else if (list.length < maxTags)
            list.push(name)

        selectedTags = list
    }

    function pickUrl(candidate) {
        const value = candidate.toString()
        if (value.length === 0)
            return

        if (!/\.(mp4|mov|mkv|avi|webm|flv|m4v)$/i.test(value)) {
            fileHint = "只支持 MP4 / MOV / MKV / AVI / WebM 视频文件"
            return
        }

        fileHint = ""
        fileUrl = value
        const parts = value.split("/")
        fileName = decodeURIComponent(parts[parts.length - 1])
    }

    function openDialog() {
        fileUrl = ""
        fileName = ""
        fileHint = ""
        titleInput.text = ""
        descInput.text = ""
        selectedTags = []
        open()
    }

    width: 720
    height: 700
    padding: 0
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay

    background: Rectangle {
        radius: 18
        color: "#12161D"
        border.width: 1
        border.color: "#242C38"
    }

    Overlay.modal: Rectangle {
        color: "#000000"
        opacity: 0.62
    }

    component FlatButton: Rectangle {
        id: flatButton

        property string label: ""
        property int labelSize: 13
        property color baseColor: "#171E27"
        property color hoverColor: "#212A36"
        property color labelColor: "#D6DEE8"

        signal clicked()

        implicitWidth: buttonLabel.implicitWidth + 36
        implicitHeight: 44
        radius: 12
        color: buttonArea.containsMouse ? hoverColor : baseColor

        Text {
            id: buttonLabel

            anchors.centerIn: parent
            text: flatButton.label
            color: flatButton.labelColor
            font.pixelSize: flatButton.labelSize
            font.bold: true
        }

        MouseArea {
            id: buttonArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: flatButton.clicked()
        }
    }

    component TagChip: Rectangle {
        id: tagChip

        property string label: ""
        property bool selected: false

        signal toggled()

        width: chipLabel.implicitWidth + 26
        height: 30
        radius: 15
        color: selected ? "#14312D" : (chipArea.containsMouse ? "#16202B" : "#101821")
        border.width: 1
        border.color: selected ? "#20F1CB" : (chipArea.containsMouse ? "#3A4A5C" : "#2A3644")

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        Text {
            id: chipLabel

            anchors.centerIn: parent
            text: tagChip.label
            color: tagChip.selected ? "#20F1CB" : "#A9B4C2"
            font.pixelSize: 12
        }

        MouseArea {
            id: chipArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tagChip.toggled()
        }
    }

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 74

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.leftMargin: 28
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    Text {
                        text: "发布视频"
                        color: "#F2F6FA"
                        font.pixelSize: 19
                        font.bold: true
                    }

                    Text {
                        text: "分享你的作品，发布后进入审核"
                        color: "#7C8898"
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    id: closeButton

                    width: 32
                    height: 32
                    radius: 9
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    color: closeArea.containsMouse ? "#232B37" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        color: "#A9B4C2"
                        font.pixelSize: 18
                    }

                    MouseArea {
                        id: closeArea

                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: "#1E2530"
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 28
                Layout.rightMargin: 28
                Layout.topMargin: 22
                Layout.bottomMargin: 16
                spacing: 16

                Rectangle {
                    id: dropZone

                    Layout.fillWidth: true
                    Layout.preferredHeight: 172
                    radius: 16
                    color: dropArea.containsDrag ? "#12211F"
                                                     : root.hasFile ? "#0E151D" : "#0C131B"
                    border.width: 1
                    border.color: dropArea.containsDrag ? "#20F1CB"
                                                           : root.hasFile ? "#25323E" : "#2A3644"

                    Behavior on color {
                        ColorAnimation { duration: 140 }
                    }

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 10
                        visible: !root.hasFile

                        Rectangle {
                            Layout.alignment: Qt.AlignHCenter
                            width: 62
                            height: 62
                            radius: 20
                            color: dropArea.containsDrag ? "#15332E" : "#141C26"
                            border.width: 1
                            border.color: dropArea.containsDrag ? "#20F1CB" : "#2E3B4A"

                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: "#20F1CB"
                                font.pixelSize: 30
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: dropArea.containsDrag ? "松手即可添加" : "拖拽视频到这里"
                            color: "#E6EDF5"
                            font.pixelSize: 15
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "或点击选择文件 · 支持 MP4 / MOV / MKV / AVI / WebM"
                            color: "#6E7A8A"
                            font.pixelSize: 12
                        }
                    }

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.margins: 22
                        visible: root.hasFile
                        spacing: 16

                        Rectangle {
                            width: 92
                            height: 92
                            radius: 14
                            color: "#0A1016"
                            border.width: 1
                            border.color: "#25323E"

                            Text {
                                anchors.centerIn: parent
                                text: "▶"
                                color: "#20F1CB"
                                font.pixelSize: 26
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: root.fileName
                                color: "#EAF1F8"
                                font.pixelSize: 14
                                elide: Text.ElideMiddle
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.fileHint.length > 0 ? root.fileHint
                                                                  : "文件已就绪，补个标题就可以发布了"
                                color: root.fileHint.length > 0 ? "#F0A15A" : "#6E7A8A"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }

                        Item {
                            Layout.preferredWidth: 62
                            Layout.preferredHeight: 28

                            Text {
                                anchors.centerIn: parent
                                text: "移除"
                                color: "#8C99A9"
                                font.pixelSize: 12
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.fileUrl = ""
                                    root.fileName = ""
                                    root.fileHint = ""
                                }
                            }
                        }
                    }

                    DropArea {
                        id: dropArea

                        anchors.fill: parent

                        onDropped: function(drop) {
                            drop.accept(Qt.CopyAction)
                            if (drop.hasUrls && drop.urls.length > 0)
                                root.pickUrl(drop.urls[0])
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: !root.hasFile
                        cursorShape: Qt.PointingHandCursor
                        onClicked: filePicker.open()
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "标题"
                            color: "#A7B2C1"
                            font.pixelSize: 12
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: titleInput.text.length + "/50"
                            color: "#5C6877"
                            font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        radius: 12
                        color: "#0C131B"
                        border.width: 1
                        border.color: titleInput.activeFocus ? "#2C8C80" : "#25323E"

                        TextField {
                            id: titleInput

                            anchors.fill: parent
                            anchors.leftMargin: 15
                            anchors.rightMargin: 15
                            color: "#F2F6FA"
                            placeholderText: "给视频起个标题"
                            placeholderTextColor: "#596677"
                            font.pixelSize: 14
                            maximumLength: 50
                            selectByMouse: true
                            selectionColor: "#20F1CB"
                            selectedTextColor: "#04110F"
                            verticalAlignment: TextInput.AlignVCenter
                            background: null
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "描述"
                            color: "#A7B2C1"
                            font.pixelSize: 12
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: descInput.length + "/200"
                            color: "#5C6877"
                            font.pixelSize: 11
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 92
                        radius: 12
                        color: "#0C131B"
                        border.width: 1
                        border.color: descInput.activeFocus ? "#2C8C80" : "#25323E"

                        TextArea {
                            id: descInput

                            anchors.fill: parent
                            anchors.margins: 13
                            color: "#F2F6FA"
                            placeholderText: "介绍一下这个视频（选填）"
                            placeholderTextColor: "#596677"
                            font.pixelSize: 13
                            wrapMode: TextArea.Wrap
                            selectByMouse: true
                            selectionColor: "#20F1CB"
                            selectedTextColor: "#04110F"
                            background: null
                            onTextChanged: {
                                if (descInput.length > 200)
                                    descInput.remove(200, descInput.length - 200)
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true

                        Text {
                            text: "标签"
                            color: "#A7B2C1"
                            font.pixelSize: 12
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.selectedTags.length + "/" + root.maxTags
                            color: "#5C6877"
                            font.pixelSize: 11
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        Layout.preferredHeight: childrenRect.height
                        spacing: 8

                        Repeater {
                            model: root.tagOptions

                            delegate: TagChip {
                                label: modelData
                                selected: root.selectedTags.indexOf(modelData) >= 0
                                onToggled: root.toggleTag(modelData)
                            }
                        }
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 78

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: "#1E2530"
                }

                RowLayout {
                    anchors.right: parent.right
                    anchors.rightMargin: 28
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12

                    FlatButton {
                        label: "取消"
                        onClicked: root.close()
                    }

                    Rectangle {
                        width: 132
                        height: 44
                        radius: 12
                        color: !root.canPublish ? "#1A2830"
                                                 : publishArea.containsMouse ? "#39F5D5" : "#20F1CB"

                        Text {
                            anchors.centerIn: parent
                            text: "发布"
                            color: root.canPublish ? "#04110F" : "#5E7280"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: publishArea

                            anchors.fill: parent
                            enabled: root.canPublish
                            hoverEnabled: true
                            cursorShape: root.canPublish ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                root.publishRequested(root.fileUrl, titleInput.text.trim(),
                                                      descInput.text.trim(), root.selectedTags)
                                root.close()
                            }
                        }
                    }
                }
            }
        }
    }

    FileDialog {
        id: filePicker

        title: "选择视频"
        nameFilters: ["视频文件 (*.mp4 *.mov *.mkv *.avi *.webm *.flv *.m4v)", "所有文件 (*)"]
        onAccepted: root.pickUrl(selectedFile)
    }
}
