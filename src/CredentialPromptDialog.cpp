#include "CredentialPromptDialog.h"

#include <QCoreApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString uiText(const char *source)
{
    return QCoreApplication::translate("CredentialPromptDialog", source);
}

QLabel *plainTextLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}

} // namespace

CredentialPromptDialog::CredentialPromptDialog(
    const CredentialPromptContent &content,
    QWidget *parent
)
    : QDialog(parent)
{
    setObjectName(content.objectName);
    setWindowTitle(content.title);
    setModal(true);
    setMinimumWidth(500);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(14);

    auto *title = plainTextLabel(windowTitle(), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);

    if (content.retry) {
        auto *retryMessage = plainTextLabel(
            content.savedCredentialRejected
                ? (content.savedCredentialRemoved
                    ? uiText(QT_TRANSLATE_NOOP(
                        "CredentialPromptDialog",
                        "The saved credentials were not accepted and were removed. Enter new credentials to continue."
                    ))
                    : uiText(QT_TRANSLATE_NOOP(
                        "CredentialPromptDialog",
                        "The saved credentials were not accepted. Check them and try again."
                    )))
                : uiText(QT_TRANSLATE_NOOP(
                    "CredentialPromptDialog",
                    "Authentication failed. Check the credentials and try again."
                )),
            this
        );
        retryMessage->setObjectName(QStringLiteral("errorText"));
        layout->addWidget(retryMessage);
    }

    auto *message = plainTextLabel(content.message, this);
    message->setObjectName(QStringLiteral("authenticationMessage"));
    layout->addWidget(message);

    for (const QString &detail : content.details) {
        if (detail.isEmpty())
            continue;
        auto *label = plainTextLabel(detail, this);
        label->setObjectName(QStringLiteral("fieldHint"));
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        layout->addWidget(label);
    }

    if (content.insecureTransport) {
        auto *warning = plainTextLabel(
            uiText(QT_TRANSLATE_NOOP(
                "CredentialPromptDialog",
                "This connection is not encrypted. The username and password may be intercepted."
            )),
            this
        );
        warning->setObjectName(QStringLiteral("insecureTransportWarning"));
        layout->addWidget(warning);
    }

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_username = new QLineEdit(content.suggestedUsername, this);
    m_username->setObjectName(QStringLiteral("credentialUsername"));
    form->addRow(
        uiText(QT_TRANSLATE_NOOP("CredentialPromptDialog", "Username")),
        m_username
    );
    m_password = new QLineEdit(this);
    m_password->setObjectName(QStringLiteral("credentialPassword"));
    m_password->setEchoMode(QLineEdit::Password);
    form->addRow(
        uiText(QT_TRANSLATE_NOOP("CredentialPromptDialog", "Password")),
        m_password
    );
    layout->addLayout(form);

    if (content.rememberAvailable) {
        m_rememberCredential = new QCheckBox(
            uiText(QT_TRANSLATE_NOOP(
                "CredentialPromptDialog",
                "Remember in the system password manager"
            )),
            this
        );
        m_rememberCredential->setObjectName(QStringLiteral("rememberCredential"));
        m_rememberCredential->setChecked(content.rememberInitiallyChecked);
        layout->addWidget(m_rememberCredential);
    }

    if (!content.privacyHint.isEmpty()) {
        auto *privacyHint = plainTextLabel(content.privacyHint, this);
        privacyHint->setObjectName(QStringLiteral("fieldHint"));
        layout->addWidget(privacyHint);
    }

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        this
    );
    buttons->button(QDialogButtonBox::Ok)->setText(
        uiText(QT_TRANSLATE_NOOP("CredentialPromptDialog", "Sign in"))
    );
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (content.suggestedUsername.isEmpty())
        m_username->setFocus();
    else
        m_password->setFocus();
    resize(520, sizeHint().height());
}

QString CredentialPromptDialog::username() const
{
    return m_username->text();
}

QString CredentialPromptDialog::password() const
{
    return m_password->text();
}

bool CredentialPromptDialog::rememberCredential() const
{
    return m_rememberCredential && m_rememberCredential->isChecked();
}
