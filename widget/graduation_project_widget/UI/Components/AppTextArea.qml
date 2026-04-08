import QtQuick
import QtQuick.Controls
import "../Theme.js" as Theme

TextArea {
    id: control

    leftPadding: 14
    rightPadding: 14
    topPadding: 14
    bottomPadding: 14
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.primary
    selectedTextColor: "white"
    wrapMode: TextEdit.Wrap
    font.pixelSize: 14

    background: Rectangle {
        radius: 16
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: 1
        border.color: control.activeFocus ? Theme.primary : control.hovered ? "#CBD5E1" : Theme.border

        Behavior on border.color {
            ColorAnimation { duration: 130 }
        }
    }
}
