import QtQuick
import "../Theme.js" as Theme

Item {
    id: root
    property var metrics: []
    property real animationProgress: 0.0

    function restartAnimation() {
        chartAnimation.stop()
        animationProgress = 0.0
        chartAnimation.start()
    }

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            var count = root.metrics ? root.metrics.length : 0
            if (count === 0) {
                return
            }

            var cx = width / 2
            var cy = height / 2
            var radius = Math.min(width, height) * 0.35

            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            for (var ring = 1; ring <= 4; ++ring) {
                ctx.beginPath()
                for (var i = 0; i < count; ++i) {
                    var angle = -Math.PI / 2 + (Math.PI * 2 * i / count)
                    var rx = cx + Math.cos(angle) * radius * ring / 4
                    var ry = cy + Math.sin(angle) * radius * ring / 4
                    if (i === 0) {
                        ctx.moveTo(rx, ry)
                    } else {
                        ctx.lineTo(rx, ry)
                    }
                }
                ctx.closePath()
                ctx.stroke()
            }

            for (var axis = 0; axis < count; ++axis) {
                var axisAngle = -Math.PI / 2 + (Math.PI * 2 * axis / count)
                ctx.beginPath()
                ctx.moveTo(cx, cy)
                ctx.lineTo(cx + Math.cos(axisAngle) * radius, cy + Math.sin(axisAngle) * radius)
                ctx.stroke()
            }

            ctx.beginPath()
            ctx.strokeStyle = Theme.teal
            ctx.fillStyle = Qt.rgba(13 / 255, 148 / 255, 136 / 255, 0.18)
            ctx.lineWidth = 2.5
            for (var idx = 0; idx < count; ++idx) {
                var metric = root.metrics[idx]
                var value = Math.max(0, Math.min(1, Number(metric.value))) * root.animationProgress
                var angle2 = -Math.PI / 2 + (Math.PI * 2 * idx / count)
                var px = cx + Math.cos(angle2) * radius * value
                var py = cy + Math.sin(angle2) * radius * value
                if (idx === 0) {
                    ctx.moveTo(px, py)
                } else {
                    ctx.lineTo(px, py)
                }
            }
            ctx.closePath()
            ctx.fill()
            ctx.stroke()
        }
    }

    Repeater {
        model: root.metrics
        delegate: Text {
            readonly property real angle: -Math.PI / 2 + (Math.PI * 2 * index / root.metrics.length)
            text: modelData.label
            color: Theme.textSecondary
            font.pixelSize: 12
            opacity: root.animationProgress
            x: root.width / 2 + Math.cos(angle) * (Math.min(root.width, root.height) * 0.45) - width / 2
            y: root.height / 2 + Math.sin(angle) * (Math.min(root.width, root.height) * 0.45) - height / 2
        }
    }

    NumberAnimation {
        id: chartAnimation
        target: root
        property: "animationProgress"
        from: 0.0
        to: 1.0
        duration: 950
        easing.type: Easing.OutCubic
    }

    onMetricsChanged: restartAnimation()
    onAnimationProgressChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Component.onCompleted: restartAnimation()
}
