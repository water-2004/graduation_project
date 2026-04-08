#ifndef ABOUT_PAGE_H
#define ABOUT_PAGE_H

#include <QWidget>

#include "global.h"

class QLabel;
class QPushButton;

class AboutPage : public QWidget {
    Q_OBJECT

public:
    explicit AboutPage(QWidget* parent = nullptr);

    void RefreshStatus();
    void SetBusinessState(ClientState state);
    void SetInferenceState(ClientState state);
    void SetCurrentUser(const QString& username);

signals:
    void sig_change_password(const QString& username, const QString& old_pw, const QString& new_pw);

public slots:
    void slot_on_password_changed(const QString& message);
    void slot_on_business_error(const QString& error_text);

private:
    void BuildUi();
    void OnChangePasswordClicked();
    void UpdateStatusIndicator(QLabel* dot_label, QLabel* text_label, ClientState state);

    QString current_username_;

    QLabel* business_dot_ = nullptr;
    QLabel* business_text_ = nullptr;
    QLabel* inference_dot_ = nullptr;
    QLabel* inference_text_ = nullptr;

    ClientState business_state_ = ClientState::Disconnected;
    ClientState inference_state_ = ClientState::Disconnected;
};

#endif // ABOUT_PAGE_H
