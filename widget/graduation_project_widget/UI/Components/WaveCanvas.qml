import QtQuick
import "../Theme.js" as Theme

Item {
    id: root
    property var points: []
    property string alertLevel: "normal"

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = Theme.monitorDark
            ctx.fillRect(0, 0, width, height)

            ctx.strokeStyle = Theme.monitorGrid
            ctx.lineWidth = 1
            for (var gx = 0; gx <= width; gx += 24) {
                ctx.beginPath()
                ctx.moveTo(gx, 0)
                ctx.lineTo(gx, height)
                ctx.stroke()
            }
            for (var gy = 0; gy <= height; gy += 20) {
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(width, gy)
                ctx.stroke()
            }

            if (!root.points || root.points.length === 0) {
                ctx.fillStyle = "#9CA3AF"
                ctx.font = "16px Microsoft YaHei"
                ctx.fillText("等待波形数据...", 20, 28)
                return
            }

            var maxAbs = 0.1
            for (var i = 0; i < root.points.length; ++i) {
                var v = Number(root.points[i])
                maxAbs = Math.max(maxAbs, Math.abs(v))
            }

            var plotColor = Theme.success
            if (root.alertLevel === "critical") {
                plotColor = Theme.critical
            } else if (root.alertLevel === "warning") {
                plotColor = Theme.warning
            }

            var centerY = height / 2
            var scaleY = height * 0.38 / maxAbs
            var stepX = root.points.length > 1 ? width / (root.points.length - 1) : width

            ctx.strokeStyle = plotColor
            ctx.lineWidth = 2.2
            ctx.beginPath()
            for (var idx = 0; idx < root.points.length; ++idx) {
                var x = idx * stepX
                var y = centerY - Number(root.points[idx]) * scaleY
                if (idx === 0) {
                    ctx.moveTo(x, y)
                } else {
                    ctx.lineTo(x, y)
                }
            }
            ctx.stroke()
        }
    }

    onPointsChanged: canvas.requestPaint()
    onAlertLevelChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
}
