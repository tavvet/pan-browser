#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;

struct CredentialPromptContent {
    QString objectName;
    QString title;
    QString message;
    QStringList details;
    QString suggestedUsername;
    QString privacyHint;
    bool retry = false;
    bool insecureTransport = false;
};

class CredentialPromptDialog final : public QDialog {
public:
    explicit CredentialPromptDialog(
        const CredentialPromptContent &content,
        QWidget *parent = nullptr
    );

    [[nodiscard]] QString username() const;
    [[nodiscard]] QString password() const;

private:
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
};
