import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Popup {
    id: root

    property string titleText: "确认操作"
    property string messageText: "是否继续？"
    property string confirmText: "确认"
    property string cancelText: "取消"
    property string tone: "danger"
    signal confirmed()

    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    width: 420
    padding: 0

    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 }
            NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: 160; easing.type: Easing.OutCubic }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.98; duration: 110 }
        }
    }

    background: Rectangle {
        radius: 22
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
    }

    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        Label {
            text: root.titleText
            color: Theme.text
            font.pixelSize: 24
            font.bold: true
        }

        Label {
            text: root.messageText
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            AppButton {
                Layout.fillWidth: true
                tone: "secondary"
                text: root.cancelText
                onClicked: root.close()
            }

            AppButton {
                Layout.fillWidth: true
                tone: root.tone
                text: root.confirmText
                onClicked: {
                    root.confirmed()
                    root.close()
                }
            }
        }
    }
}
