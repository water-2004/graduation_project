import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Theme.js" as Theme

Item {
    id: root
    property var segments: []
    property real animationProgress: 0.0

    function restartAnimation() {
        chartAnimation.stop()
        animationProgress = 0.0
        chartAnimation.start()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 20

        Canvas {
            id: canvas
            Layout.preferredWidth: Math.min(root.width * 0.56, 240)
            Layout.fillHeight: true
            antialiasing: true

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                var colors = [Theme.primary, Theme.teal, Theme.warning, Theme.success, Theme.critical]
                var total = 0
                for (var i = 0; i < root.segments.length; ++i) {
                    total += Number(root.segments[i].value)
                }
                if (total <= 0) {
                    return
                }

                var cx = width / 2
                var cy = height / 2
                var radius = Math.min(width, height) * 0.38
                var start = -Math.PI / 2
                for (var idx = 0; idx < root.segments.length; ++idx) {
                    var value = Number(root.segments[idx].value)
                    var span = Math.PI * 2 * value / total * root.animationProgress
                    ctx.beginPath()
                    ctx.strokeStyle = colors[idx % colors.length]
                    ctx.lineWidth = radius * 0.42
                    ctx.arc(cx, cy, radius, start, start + span)
                    ctx.stroke()
                    start += Math.PI * 2 * value / total
                }

                ctx.beginPath()
                ctx.fillStyle = Theme.surface
                ctx.arc(cx, cy, radius * 0.58, 0, Math.PI * 2)
                ctx.fill()

                ctx.fillStyle = Theme.text
                ctx.font = "bold 24px Microsoft YaHei"
                ctx.textAlign = "center"
                ctx.textBaseline = "middle"
                ctx.fillText(String(total), cx, cy - 8)
                ctx.fillStyle = Theme.textMuted
                ctx.font = "12px Microsoft YaHei"
                ctx.fillText("总记录", cx, cy + 18)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Repeater {
                model: root.segments
                delegate: RowLayout {
                    spacing: 8
                    opacity: root.animationProgress
                    readonly property var colors: [Theme.primary, Theme.teal, Theme.warning, Theme.success, Theme.critical]

                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        color: colors[index % colors.length]
                    }
                    Label {
                        text: modelData.label + "：" + modelData.value
                        color: Theme.textSecondary
                        font.pixelSize: 13
                    }
                }
            }
        }
    }

    NumberAnimation {
        id: chartAnimation
        target: root
        property: "animationProgress"
        from: 0.0
        to: 1.0
        duration: 900
        easing.type: Easing.OutCubic
    }

    onSegmentsChanged: restartAnimation()
    onAnimationProgressChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Component.onCompleted: restartAnimation()
}
