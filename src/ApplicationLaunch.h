#pragma once

#include <QByteArray>
#include <QLockFile>
#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

class QLocalServer;
class QLocalSocket;

struct ApplicationLaunchRequest {
    enum class Command {
        Activate,
        OpenWebApp,
        OpenUrl,
    };

    Command command = Command::Activate;
    QString webAppId;
    QUrl url;

    [[nodiscard]] static ApplicationLaunchRequest activate();
    [[nodiscard]] static ApplicationLaunchRequest openWebApp(const QString &id);
    [[nodiscard]] static ApplicationLaunchRequest openUrl(const QUrl &url);
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QByteArray toPayload() const;
    [[nodiscard]] static std::optional<ApplicationLaunchRequest> fromPayload(
        const QByteArray &payload
    );
};

Q_DECLARE_METATYPE(ApplicationLaunchRequest)

class SingleInstanceCoordinator final : public QObject {
    Q_OBJECT

public:
    enum class StartResult {
        Primary,
        Forwarded,
        Error,
    };

    explicit SingleInstanceCoordinator(QObject *parent = nullptr);
    explicit SingleInstanceCoordinator(QString serverName, QObject *parent = nullptr);
    [[nodiscard]] StartResult start(
        const ApplicationLaunchRequest &initialRequest,
        QString *error = nullptr
    );

signals:
    void launchRequested(const ApplicationLaunchRequest &request);

private:
    [[nodiscard]] bool forwardRequest(const ApplicationLaunchRequest &request) const;
    [[nodiscard]] bool listen();
    void acceptPendingConnections();
    void consumeSocket(QLocalSocket *socket);

    QString m_serverName;
    QLockFile m_instanceLock;
    QLocalServer *m_server = nullptr;
};
