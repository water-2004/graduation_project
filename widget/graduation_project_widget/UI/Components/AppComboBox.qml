pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../Theme.js" as Theme

ComboBox {
    id: control

    property string placeholderText: "请选择"

    implicitHeight: 46
    leftPadding: 14
    rightPadding: 36
    font.pixelSize: 14

    delegate: ItemDelegate {
        id: delegateItem
        required property int index
        width: control.width
        padding: 12
        text: control.textAt(index)
        contentItem: Label {
            text: delegateItem.text
            color: Theme.text
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 14
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: delegateItem.hovered ? Theme.primarySoft : "transparent"
        }
    }

    indicator: Canvas {
        x: control.width - width - 14
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 12
        height: 8
        contextType: "2d"

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.moveTo(0, 0)
            ctx.lineTo(width, 0)
            ctx.lineTo(width / 2, height)
            ctx.closePath()
            ctx.fillStyle = Theme.textMuted
            ctx.fill()
        }
    }

    contentItem: Label {
        leftPadding: control.leftPadding
        rightPadding: control.indicator.width + control.spacing
        text: control.currentIndex >= 0 ? control.currentText : control.placeholderText
        color: control.currentIndex >= 0 ? Theme.text : Theme.textMuted
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: 14
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 14
        color: Theme.surface
        border.width: 1
        border.color: control.activeFocus ? Theme.primary : control.hovered ? "#CBD5E1" : Theme.border

        Behavior on border.color {
            ColorAnimation { duration: 130 }
        }
    }

    popup: Popup {
        y: control.height + 6
        width: control.width
        padding: 8
        implicitHeight: contentItem.implicitHeight + 16
        background: Rectangle {
            radius: 16
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            spacing: 4
        }
    }
}
