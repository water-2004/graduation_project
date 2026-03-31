#ifndef SIDEBAR_WIDGET_H
#define SIDEBAR_WIDGET_H

#include <QWidget>
#include <QVector>

class QPushButton;
class QVBoxLayout;

class SidebarWidget : public QWidget {
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget* parent = nullptr);

    int AddItem(const QString& text);
    void SetCurrentIndex(int index);
    void SetItemEnabled(int index, bool enabled);

signals:
    void sig_index_changed(int index);

private:
    void OnButtonClicked(int index);

    QVBoxLayout* button_layout_ = nullptr;
    QVector<QPushButton*> buttons_;
    int current_index_ = -1;
};

#endif // SIDEBAR_WIDGET_H
