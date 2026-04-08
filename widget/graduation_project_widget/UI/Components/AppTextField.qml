import QtQuick
import QtQuick.Controls
import "../Theme.js" as Theme

TextField {
    id: control

    implicitHeight: 46
    leftPadding: 14
    rightPadding: 14
    topPadding: 12
    bottomPadding: 12
    color: Theme.text
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.primary
    selectedTextColor: "white"
    font.pixelSize: 14

    background: Rectangle {
        radius: 14
        color: control.enabled ? Theme.surface : Theme.surfaceAlt
        border.width: 1
        border.color: control.activeFocus ? Theme.primary : control.hovered ? "#CBD5E1" : Theme.border

        Behavior on border.color {
            ColorAnimation { duration: 130 }
        }
        Behavior on color {
            ColorAnimation { duration: 130 }
        }
    }
}
