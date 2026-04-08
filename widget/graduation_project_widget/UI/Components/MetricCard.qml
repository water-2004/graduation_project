import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Rectangle {
    id: root
    radius: 20
    color: Theme.surface
    border.width: 1
    border.color: Theme.border
    implicitHeight: 144
    opacity: 0.0
    scale: 0.985

    property string title: ""
    property string subtitle: ""
    property string accentColor: Theme.primary
    property string watermark: ""
    property bool useAnimatedNumber: false
    property real targetValue: 0
    property real displayedValue: 0
    property int decimals: 0
    property string prefix: ""
    property string suffix: ""
    property string staticValue: "0"
    property int animationDelay: 0

    function formattedAnimatedValue() {
        return prefix + Number(displayedValue).toLocaleString(Qt.locale(), 'f', decimals) + suffix
    }

    function refreshAnimatedValue() {
        if (!useAnimatedNumber) {
            displayedValue = targetValue
            return
        }
        valueAnimation.stop()
        valueAnimation.from = displayedValue
        valueAnimation.to = targetValue
        valueAnimation.start()
    }

    Behavior on border.color {
        ColorAnimation { duration: 140 }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        width: 84
        height: 84
        radius: 42
        color: Qt.rgba(0, 0, 0, 0.03)

        Label {
            anchors.centerIn: parent
            text: root.watermark
            color: Qt.rgba(0, 0, 0, 0.08)
            font.pixelSize: 36
            font.bold: true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 10

        Rectangle {
            width: 42
            height: 6
            radius: 3
            color: root.accentColor
        }

        Label {
            text: root.title
            color: Theme.textSecondary
            font.pixelSize: 13
        }

        Label {
            text: root.useAnimatedNumber ? root.formattedAnimatedValue() : root.staticValue
            color: Theme.text
            font.pixelSize: 32
            font.bold: true
        }

        Label {
            text: root.subtitle
            color: Theme.textMuted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    NumberAnimation {
        id: valueAnimation
        target: root
        property: "displayedValue"
        duration: 900
        easing.type: Easing.OutCubic
    }

    SequentialAnimation {
        id: entryAnimation
        PauseAnimation { duration: root.animationDelay }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 220
            }
            NumberAnimation {
                target: root
                property: "scale"
                from: 0.985
                to: 1.0
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
    }

    onTargetValueChanged: refreshAnimatedValue()

    Component.onCompleted: {
        if (useAnimatedNumber) {
            displayedValue = 0
            refreshAnimatedValue()
        } else {
            displayedValue = targetValue
        }
        entryAnimation.start()
    }
}
