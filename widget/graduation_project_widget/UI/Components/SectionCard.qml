import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Rectangle {
    id: root
    radius: 22
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    property string title: ""
    property string subtitle: ""
    property int bodyPadding: 20
    default property alias contentData: body.data

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.bodyPadding
        spacing: 16

        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            visible: root.title.length > 0 || root.subtitle.length > 0

            Label {
                text: root.title
                color: Theme.text
                font.pixelSize: 18
                font.bold: true
                visible: root.title.length > 0
            }

            Label {
                text: root.subtitle
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: root.subtitle.length > 0
            }
        }

        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
