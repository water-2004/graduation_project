#ifndef ECG_WAVE_WIDGET_H
#define ECG_WAVE_WIDGET_H

#include <QColor>
#include <QTimer>
#include <QSizePolicy>
#include <QVector>
#include <QWidget>

// 心电波形控件：负责把 187 点单拍心电画成曲线，并做简单回放动画。
class EcgWaveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EcgWaveWidget(QWidget *parent = nullptr);

    void setBeat(const QVector<double>& values, const QString& alert_level);
    void setAlertLevel(const QString& alert_level);
    void clearWave();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onFrameTick();

private:
    QColor borderColor() const;
    QColor waveColor() const;

    QVector<double> full_values_;
    QString alert_level_ = "normal";
    QTimer timer_;
    int visible_count_ = 0;
};

#endif // ECG_WAVE_WIDGET_H
