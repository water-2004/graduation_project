import QtQuick
import "../Theme.js" as Theme

Item {
    id: root
    property var points: []
    property color lineColor: Theme.primary
    property color fillColor: Theme.primarySoft
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
            ctx.fillStyle = Theme.surfaceAlt
            ctx.fillRect(0, 0, width, height)

            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            for (var gy = 0; gy < height; gy += 34) {
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(width, gy)
                ctx.stroke()
            }

            if (!root.points || root.points.length === 0) {
                return
            }

            var maxValue = 1
            for (var i = 0; i < root.points.length; ++i) {
                maxValue = Math.max(maxValue, Number(root.points[i].value))
            }

            var stepX = root.points.length > 1 ? width / (root.points.length - 1) : width
            var clipWidth = Math.max(1, width * root.animationProgress)

            ctx.save()
            ctx.beginPath()
            ctx.rect(0, 0, clipWidth, height)
            ctx.clip()

            ctx.beginPath()
            for (var idx = 0; idx < root.points.length; ++idx) {
                var item = root.points[idx]
                var x = idx * stepX
                var y = height - (Number(item.value) / maxValue) * (height - 20) - 10
                if (idx === 0) {
                    ctx.moveTo(x, y)
                } else {
                    ctx.lineTo(x, y)
                }
            }
            ctx.lineTo(width, height - 10)
            ctx.lineTo(0, height - 10)
            ctx.closePath()
            ctx.fillStyle = Qt.rgba(0.0, 110 / 255, 216 / 255, 0.08)
            ctx.fill()

            ctx.beginPath()
            for (var idx2 = 0; idx2 < root.points.length; ++idx2) {
                var item2 = root.points[idx2]
                var x2 = idx2 * stepX
                var y2 = height - (Number(item2.value) / maxValue) * (height - 20) - 10
                if (idx2 === 0) {
                    ctx.moveTo(x2, y2)
                } else {
                    ctx.lineTo(x2, y2)
                }
            }
            ctx.strokeStyle = root.lineColor
            ctx.lineWidth = 3
            ctx.stroke()
            ctx.restore()
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

    onPointsChanged: restartAnimation()
    onAnimationProgressChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()

    Component.onCompleted: restartAnimation()
}
