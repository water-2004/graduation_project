import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Rectangle {
    id: root
    implicitHeight: 44
    implicitWidth: label.implicitWidth + 28
    radius: implicitHeight / 2

    property string text: ""
    property string level: "normal"

    color: Theme.levelBackground(level)
    border.width: 1
    border.color: Theme.levelColor(level)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Rectangle {
            width: 10
            height: 10
            radius: 5
            color: Theme.levelColor(root.level)
        }

        Label {
            id: label
            text: root.text
            color: Theme.levelColor(root.level)
            font.pixelSize: 13
            font.bold: true
        }
    }
}
