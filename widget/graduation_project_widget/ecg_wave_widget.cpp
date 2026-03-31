#include "ecg_wave_widget.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kFrameIntervalMs = 12;
constexpr int kPointsPerFrame = 3;

}  // namespace

EcgWaveWidget::EcgWaveWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    timer_.setInterval(kFrameIntervalMs);
    connect(&timer_, &QTimer::timeout, this, &EcgWaveWidget::onFrameTick);
}

void EcgWaveWidget::setBeat(const QVector<double>& values, const QString& alert_level) {
    full_values_ = values;
    alert_level_ = alert_level.trimmed().isEmpty() ? QStringLiteral("normal") : alert_level.trimmed();

    if (full_values_.isEmpty()) {
        visible_count_ = 0;
        timer_.stop();
        update();
        return;
    }

    visible_count_ = std::min<int>(2, static_cast<int>(full_values_.size()));
    if (visible_count_ < full_values_.size()) {
        timer_.start();
    } else {
        timer_.stop();
    }
    update();
}

void EcgWaveWidget::setAlertLevel(const QString& alert_level) {
    alert_level_ = alert_level.trimmed().isEmpty() ? QStringLiteral("normal") : alert_level.trimmed();
    update();
}

void EcgWaveWidget::clearWave() {
    timer_.stop();
    full_values_.clear();
    alert_level_ = QStringLiteral("normal");
    visible_count_ = 0;
    update();
}

void EcgWaveWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(248, 252, 250));

    const QRectF frame_rect = rect().adjusted(1.0, 1.0, -1.0, -1.0);
    painter.setPen(QPen(borderColor(), 2.0));
    painter.drawRoundedRect(frame_rect, 8.0, 8.0);

    const QRectF plot_rect = rect().adjusted(14.0, 18.0, -14.0, -18.0);
    painter.setPen(QPen(QColor(226, 236, 232), 1.0));
    for (int i = 1; i < 6; ++i) {
        const qreal y = plot_rect.top() + plot_rect.height() * static_cast<qreal>(i) / 6.0;
        painter.drawLine(QPointF(plot_rect.left(), y), QPointF(plot_rect.right(), y));
    }
    for (int i = 1; i < 12; ++i) {
        const qreal x = plot_rect.left() + plot_rect.width() * static_cast<qreal>(i) / 12.0;
        painter.drawLine(QPointF(x, plot_rect.top()), QPointF(x, plot_rect.bottom()));
    }

    painter.setPen(QPen(QColor(107, 124, 117), 1.0));
    painter.drawText(plot_rect.adjusted(4.0, 0.0, -4.0, 0.0),
                     Qt::AlignTop | Qt::AlignLeft,
                     QStringLiteral("波形点数: %1 / %2").arg(visible_count_).arg(full_values_.size()));
    painter.drawText(plot_rect.adjusted(4.0, 0.0, -4.0, 0.0),
                     Qt::AlignTop | Qt::AlignRight,
                     QStringLiteral("告警: %1").arg(alert_level_));

    if (full_values_.isEmpty() || visible_count_ <= 0) {
        painter.setPen(QPen(QColor(125, 135, 131), 1.0));
        painter.drawText(plot_rect, Qt::AlignCenter, QStringLiteral("暂无波形数据"));
        return;
    }

    const int count = std::min<int>(visible_count_, static_cast<int>(full_values_.size()));
    auto [min_it, max_it] = std::minmax_element(full_values_.cbegin(), full_values_.cbegin() + count);
    double min_value = *min_it;
    double max_value = *max_it;
    if (std::fabs(max_value - min_value) < 1e-6) {
        min_value -= 1.0;
        max_value += 1.0;
    }

    QPainterPath path;
    for (int i = 0; i < count; ++i) {
        const qreal x = plot_rect.left() + plot_rect.width() * static_cast<qreal>(i) / static_cast<qreal>(std::max(count - 1, 1));
        const qreal normalized = (full_values_.at(i) - min_value) / (max_value - min_value);
        const qreal y = plot_rect.bottom() - normalized * plot_rect.height();

        if (i == 0) {
            path.moveTo(x, y);
        } else {
            path.lineTo(x, y);
        }
    }

    painter.setPen(QPen(waveColor(), 2.2));
    painter.drawPath(path);
}

void EcgWaveWidget::onFrameTick() {
    visible_count_ = std::min<int>(visible_count_ + kPointsPerFrame, static_cast<int>(full_values_.size()));
    if (visible_count_ >= full_values_.size()) {
        timer_.stop();
    }
    update();
}

QColor EcgWaveWidget::borderColor() const {
    if (alert_level_ == QStringLiteral("critical")) {
        return QColor(196, 68, 68);
    }
    if (alert_level_ == QStringLiteral("warning")) {
        return QColor(208, 140, 54);
    }
    return QColor(90, 133, 112);
}

QColor EcgWaveWidget::waveColor() const {
    if (alert_level_ == QStringLiteral("critical")) {
        return QColor(214, 45, 45);
    }
    if (alert_level_ == QStringLiteral("warning")) {
        return QColor(221, 149, 36);
    }
    return QColor(45, 138, 90);
}

