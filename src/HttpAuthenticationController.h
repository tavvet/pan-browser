#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class QAuthenticator;
class QUrl;
class QWidget;

namespace HttpAuthenticationPolicy {

[[nodiscard]] bool promptAllowed(
    const QUrl &requestUrl,
    const QUrl &topLevelUrl,
    bool currentTab,
    bool activeWindow
);
[[nodiscard]] QString realmForDisplay(const QString &realm);

} // namespace HttpAuthenticationPolicy

class HttpAuthenticationController final : public QObject {
public:
    explicit HttpAuthenticationController(QObject *parent = nullptr);

    void requestAuthentication(
        QWidget *parent,
        const QUrl &requestUrl,
        QAuthenticator *authenticator
    );

private:
    QSet<QString> m_submittedChallenges;
    bool m_promptActive = false;
};
