import QtQuick
import QtQuick.Controls
import "../Theme.js" as Theme

Button {
    id: control

    property string tone: "secondary"
    property bool compact: false

    implicitHeight: compact ? 38 : 44
    implicitWidth: compact ? contentItem.implicitWidth + 24 : Math.max(116, contentItem.implicitWidth + 30)
    hoverEnabled: true

    function backgroundColor() {
        if (!enabled) {
            return Theme.border
        }
        if (tone === "primary") {
            return hovered ? Theme.teal : Theme.primary
        }
        if (tone === "danger") {
            return hovered ? Theme.critical : Theme.criticalSoft
        }
        if (tone === "success") {
            return hovered ? Theme.teal : Theme.successSoft
        }
        if (tone === "ghost") {
            return hovered ? Theme.primarySoft : "transparent"
        }
        return hovered ? "#E2E8F0" : Theme.surfaceAlt
    }

    function borderColor() {
        if (!enabled) {
            return Theme.border
        }
        if (tone === "primary") {
            return hovered ? Theme.teal : Theme.primary
        }
        if (tone === "danger") {
            return Theme.critical
        }
        if (tone === "success") {
            return Theme.teal
        }
        if (tone === "ghost") {
            return hovered ? Theme.primary : "transparent"
        }
        return Theme.border
    }

    function textColor() {
        if (!enabled) {
            return Theme.textMuted
        }
        if (tone === "primary") {
            return "white"
        }
        if (tone === "danger") {
            return hovered ? "white" : Theme.critical
        }
        if (tone === "success") {
            return hovered ? "white" : Theme.teal
        }
        if (tone === "ghost") {
            return Theme.primary
        }
        return Theme.text
    }

    background: Rectangle {
        radius: control.compact ? 12 : 14
        color: control.backgroundColor()
        border.width: 1
        border.color: control.borderColor()

        Behavior on color {
            ColorAnimation { duration: 140 }
        }
        Behavior on border.color {
            ColorAnimation { duration: 140 }
        }
    }

    contentItem: Label {
        text: control.text
        color: control.textColor()
        font.pixelSize: control.compact ? 13 : 14
        font.bold: true
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    scale: pressed ? 0.985 : 1.0
    Behavior on scale {
        NumberAnimation { duration: 90 }
    }
}
