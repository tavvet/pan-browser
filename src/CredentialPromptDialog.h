#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QCheckBox;

struct CredentialPromptContent {
    QString objectName;
    QString title;
    QString message;
    QStringList details;
    QString suggestedUsername;
    QString privacyHint;
    bool rememberAvailable = false;
    bool rememberInitiallyChecked = false;
    bool retry = false;
    bool savedCredentialRejected = false;
    bool savedCredentialRemoved = false;
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
    [[nodiscard]] bool rememberCredential() const;

private:
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
    QCheckBox *m_rememberCredential = nullptr;
};
