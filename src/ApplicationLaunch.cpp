#include "ApplicationLaunch.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QStandardPaths>

#include <utility>

namespace {

constexpr qsizetype maximumLaunchPayloadBytes = 4096;

bool isValidWebAppId(const QString &id)
{
    static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
    return expression.match(id).hasMatch();
}

bool isValidBrowserUrl(const QUrl &url)
{
    if (!url.isValid() || url.isEmpty() || url.toString(QUrl::FullyEncoded).size() > 2048)
        return false;
    return url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
}

QString defaultServerName()
{
    const QByteArray dataRoot = QDir::cleanPath(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
    ).toUtf8();
    const QByteArray suffix = QCryptographicHash::hash(
        dataRoot,
        QCryptographicHash::Sha256
    ).toHex().left(16);
    return QStringLiteral("dev.panbrowser.app.%1").arg(QString::fromLatin1(suffix));
}

} // namespace

ApplicationLaunchRequest ApplicationLaunchRequest::activate()
{
    return ApplicationLaunchRequest();
}

ApplicationLaunchRequest ApplicationLaunchRequest::openWebApp(const QString &id)
{
    ApplicationLaunchRequest request;
    request.command = Command::OpenWebApp;
    request.webAppId = id;
    return request;
}

ApplicationLaunchRequest ApplicationLaunchRequest::openUrl(const QUrl &url)
{
    ApplicationLaunchRequest request;
    request.command = Command::OpenUrl;
    request.url = url;
    return request;
}

bool ApplicationLaunchRequest::isValid() const
{
    switch (command) {
    case Command::Activate:
        return webAppId.isEmpty() && url.isEmpty();
    case Command::OpenWebApp:
        return isValidWebAppId(webAppId) && url.isEmpty();
    case Command::OpenUrl:
        return webAppId.isEmpty() && isValidBrowserUrl(url);
    }
    return false;
}

QByteArray ApplicationLaunchRequest::toPayload() const
{
    if (!isValid())
        return QByteArray();
    QJsonObject object;
    object.insert(QStringLiteral("version"), 1);
    QString commandName;
    switch (command) {
    case Command::Activate:
        commandName = QStringLiteral("activate");
        break;
    case Command::OpenWebApp:
        commandName = QStringLiteral("open-web-app");
        break;
    case Command::OpenUrl:
        commandName = QStringLiteral("open-url");
        break;
    }
    object.insert(QStringLiteral("command"), commandName);
    if (command == Command::OpenWebApp) {
        object.insert(QStringLiteral("appId"), webAppId);
    } else if (command == Command::OpenUrl) {
        object.insert(QStringLiteral("url"), url.toString(QUrl::FullyEncoded));
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<ApplicationLaunchRequest> ApplicationLaunchRequest::fromPayload(
    const QByteArray &payload
)
{
    if (payload.isEmpty() || payload.size() > maximumLaunchPayloadBytes)
        return std::nullopt;
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("version")).toInt(-1) != 1)
        return std::nullopt;

    ApplicationLaunchRequest request;
    const QString command = object.value(QStringLiteral("command")).toString();
    if (command == QStringLiteral("activate")) {
        request.command = Command::Activate;
    } else if (command == QStringLiteral("open-web-app")) {
        request.command = Command::OpenWebApp;
        request.webAppId = object.value(QStringLiteral("appId")).toString();
    } else if (command == QStringLiteral("open-url")) {
        request.command = Command::OpenUrl;
        request.url = QUrl(object.value(QStringLiteral("url")).toString());
    } else {
        return std::nullopt;
    }
    return request.isValid() ? std::optional<ApplicationLaunchRequest>(request) : std::nullopt;
}

SingleInstanceCoordinator::SingleInstanceCoordinator(QObject *parent)
    : SingleInstanceCoordinator(defaultServerName(), parent)
{
}

SingleInstanceCoordinator::SingleInstanceCoordinator(
    QString serverName,
    QObject *parent
)
    : QObject(parent)
    , m_serverName(std::move(serverName))
    , m_server(new QLocalServer(this))
{
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstanceCoordinator::acceptPendingConnections);
}

SingleInstanceCoordinator::StartResult SingleInstanceCoordinator::start(
    const ApplicationLaunchRequest &initialRequest,
    QString *error
)
{
    if (error)
        error->clear();
    if (!initialRequest.isValid()) {
        if (error)
            *error = tr("The application launch request is invalid");
        return StartResult::Error;
    }
    if (forwardRequest(initialRequest))
        return StartResult::Forwarded;
    if (listen())
        return StartResult::Primary;

    // Another process may have won the listen race after our first connection attempt.
    if (forwardRequest(initialRequest))
        return StartResult::Forwarded;

    QLocalServer::removeServer(m_serverName);
    if (listen())
        return StartResult::Primary;
    if (error)
        *error = tr("Cannot start the local PanBrowser command server: %1").arg(m_server->errorString());
    return StartResult::Error;
}

bool SingleInstanceCoordinator::forwardRequest(
    const ApplicationLaunchRequest &request
) const
{
    const QByteArray payload = request.toPayload();
    if (payload.isEmpty())
        return false;

    QLocalSocket socket;
    socket.connectToServer(m_serverName, QIODevice::WriteOnly);
    if (!socket.waitForConnected(500))
        return false;
    const QByteArray message = payload + '\n';
    if (socket.write(message) != message.size() || !socket.waitForBytesWritten(1000))
        return false;
    socket.disconnectFromServer();
    return true;
}

bool SingleInstanceCoordinator::listen()
{
    return m_server->listen(m_serverName);
}

void SingleInstanceCoordinator::acceptPendingConnections()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            consumeSocket(socket);
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        consumeSocket(socket);
    }
}

void SingleInstanceCoordinator::consumeSocket(QLocalSocket *socket)
{
    QByteArray buffer = socket->property("panbrowserLaunchBuffer").toByteArray();
    buffer += socket->readAll();
    if (buffer.size() > maximumLaunchPayloadBytes + 1) {
        socket->disconnectFromServer();
        return;
    }
    const qsizetype newline = buffer.indexOf('\n');
    if (newline < 0) {
        socket->setProperty("panbrowserLaunchBuffer", buffer);
        return;
    }
    const std::optional<ApplicationLaunchRequest> request =
        ApplicationLaunchRequest::fromPayload(buffer.left(newline));
    if (request)
        emit launchRequested(*request);
    socket->disconnectFromServer();
}
