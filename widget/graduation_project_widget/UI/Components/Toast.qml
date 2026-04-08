import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Item {
    id: root
    anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
    visible: false
    z: 999

    property string message: ""
    property string level: "normal"

    width: 420
    height: bubble.implicitHeight

    function showToast(text, toastLevel) {
        root.message = text
        root.level = toastLevel || "normal"
        root.visible = true
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: 2600
        onTriggered: root.visible = false
    }

    Rectangle {
        id: bubble
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width
        radius: 18
        color: Theme.levelColor(root.level)
        opacity: 0.96
        implicitHeight: messageLabel.implicitHeight + 24

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10

            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: "white"
            }

            Label {
                id: messageLabel
                text: root.message
                color: "white"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.pixelSize: 14
                font.bold: true
            }
        }
    }
}
