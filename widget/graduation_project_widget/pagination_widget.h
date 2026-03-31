#ifndef PAGINATION_WIDGET_H
#define PAGINATION_WIDGET_H

#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;

class PaginationWidget : public QWidget {
    Q_OBJECT

public:
    explicit PaginationWidget(QWidget* parent = nullptr);

    void SetTotalItems(int total);
    int pageSize() const;
    int currentPage() const;
    int totalPages() const;
    int offset() const;

signals:
    void sig_page_changed(int page, int offset, int page_size);

private slots:
    void OnPrevClicked();
    void OnNextClicked();
    void OnFirstClicked();
    void OnLastClicked();
    void OnPageSizeChanged(int value);

private:
    void UpdateUi();
    void EmitPageChanged();

    int total_items_ = 0;
    int current_page_ = 1;

    QLabel* info_label_ = nullptr;
    QPushButton* first_button_ = nullptr;
    QPushButton* prev_button_ = nullptr;
    QPushButton* next_button_ = nullptr;
    QPushButton* last_button_ = nullptr;
    QSpinBox* page_size_spin_ = nullptr;
};

#endif // PAGINATION_WIDGET_H
