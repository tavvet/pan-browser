#include "ProxyAuthenticationController.h"

#include <QAuthenticator>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString uiText(const char *source)
{
    return QCoreApplication::translate("ProxyAuthenticationController", source);
}

QString displayOrigin(const QUrl &url)
{
    if (!url.isValid() || url.host().isEmpty())
        return uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Unknown site"));
    QUrl origin;
    origin.setScheme(url.scheme());
    origin.setHost(url.host());
    origin.setPort(url.port());
    return origin.toDisplayString(QUrl::RemovePassword | QUrl::RemoveUserInfo);
}

class ProxyCredentialsDialog final : public QDialog {
public:
    ProxyCredentialsDialog(
        const QString &proxyHost,
        const QUrl &requestUrl,
        const QString &suggestedUsername,
        bool retry,
        QWidget *parent
    )
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("proxyAuthenticationDialog"));
        setWindowTitle(uiText(QT_TRANSLATE_NOOP(
            "ProxyAuthenticationController",
            "Proxy authentication"
        )));
        setModal(true);
        resize(520, 320);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(22, 20, 22, 18);
        layout->setSpacing(14);

        auto *title = new QLabel(windowTitle(), this);
        title->setObjectName(QStringLiteral("dialogTitle"));
        layout->addWidget(title);

        if (retry) {
            auto *retryMessage = new QLabel(
                uiText(QT_TRANSLATE_NOOP(
                    "ProxyAuthenticationController",
                    "Authentication failed. Check the credentials and try again."
                )),
                this
            );
            retryMessage->setObjectName(QStringLiteral("errorText"));
            retryMessage->setWordWrap(true);
            layout->addWidget(retryMessage);
        }

        auto *message = new QLabel(
            uiText(QT_TRANSLATE_NOOP(
                "ProxyAuthenticationController",
                "The proxy “%1” requires a username and password."
            )).arg(proxyHost),
            this
        );
        message->setTextFormat(Qt::PlainText);
        message->setWordWrap(true);
        layout->addWidget(message);

        auto *origin = new QLabel(
            uiText(QT_TRANSLATE_NOOP(
                "ProxyAuthenticationController",
                "Requesting site: %1"
            )).arg(displayOrigin(requestUrl)),
            this
        );
        origin->setTextFormat(Qt::PlainText);
        origin->setObjectName(QStringLiteral("fieldHint"));
        origin->setTextInteractionFlags(Qt::TextSelectableByMouse);
        origin->setWordWrap(true);
        layout->addWidget(origin);

        auto *form = new QFormLayout();
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(12);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        m_username = new QLineEdit(suggestedUsername, this);
        form->addRow(
            uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Username")),
            m_username
        );
        m_password = new QLineEdit(this);
        m_password->setEchoMode(QLineEdit::Password);
        form->addRow(
            uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Password")),
            m_password
        );
        layout->addLayout(form);

        auto *privacyHint = new QLabel(
            uiText(QT_TRANSLATE_NOOP(
                "ProxyAuthenticationController",
                "The password is used for this browser session and is never written to PanBrowser settings."
            )),
            this
        );
        privacyHint->setObjectName(QStringLiteral("fieldHint"));
        privacyHint->setWordWrap(true);
        layout->addWidget(privacyHint);

        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this
        );
        buttons->button(QDialogButtonBox::Ok)->setText(
            uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Sign in"))
        );
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        m_password->setFocus();
    }

    [[nodiscard]] QString username() const
    {
        return m_username->text();
    }

    [[nodiscard]] QString password() const
    {
        return m_password->text();
    }

private:
    QLineEdit *m_username = nullptr;
    QLineEdit *m_password = nullptr;
};

} // namespace

ProxyAuthenticationController::ProxyAuthenticationController(
    const ProxySettings &activeSettings,
    QObject *parent
)
    : QObject(parent)
    , m_activeSettings(activeSettings)
{
}

void ProxyAuthenticationController::requestAuthentication(
    QWidget *parent,
    const QUrl &requestUrl,
    QAuthenticator *authenticator,
    const QString &proxyHost
)
{
    if (!authenticator)
        return;
    if (m_activeSettings.mode() == ProxyMode::NoProxy
        || (m_activeSettings.mode() == ProxyMode::Manual
            && !manualProxyAuthenticationSupported(m_activeSettings.manualType()))
        || m_promptActive) {
        *authenticator = QAuthenticator();
        return;
    }

    const QString displayedHost = proxyHost.trimmed().isEmpty()
        ? (m_activeSettings.host().isEmpty()
            ? uiText(QT_TRANSLATE_NOOP("ProxyAuthenticationController", "Unknown proxy"))
            : m_activeSettings.host())
        : proxyHost.trimmed();
    const QString key = displayedHost.toCaseFolded();
    QString suggestedUsername = authenticator->user();
    if (suggestedUsername.isEmpty()
        && m_activeSettings.mode() == ProxyMode::Manual
        && manualProxyAuthenticationSupported(m_activeSettings.manualType())) {
        suggestedUsername = m_activeSettings.username();
    }

    m_promptActive = true;
    ProxyCredentialsDialog dialog(
        displayedHost,
        requestUrl,
        suggestedUsername,
        m_promptedHosts.contains(key),
        parent
    );
    const int result = dialog.exec();
    m_promptActive = false;
    if (result != QDialog::Accepted) {
        *authenticator = QAuthenticator();
        return;
    }

    authenticator->setUser(dialog.username());
    authenticator->setPassword(dialog.password());
    m_promptedHosts.insert(key);
}
