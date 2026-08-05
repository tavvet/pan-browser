#pragma once

#include <QWidget>

class QLabel;

class PermissionPrompt final : public QWidget {
    Q_OBJECT

public:
    explicit PermissionPrompt(QWidget *parent = nullptr);

    void showRequest(const QString &origin, const QString &title, const QString &description);
    void hideRequest();

signals:
    void allowRequested();
    void blockRequested();

private:
    QLabel *m_title = nullptr;
    QLabel *m_description = nullptr;
};
