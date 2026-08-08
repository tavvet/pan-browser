#include "ApplicationLaunch.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

#include <utility>

namespace {

constexpr qsizetype maximumLaunchPayloadBytes = 4096;
constexpr qsizetype maximumRememberedLaunchRequests = 128;
const QByteArray launchAcknowledgement = QByteArrayLiteral("OK\n");

QString newRequestId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool isValidRequestId(const QString &id)
{
    const QUuid uuid(id);
    return !uuid.isNull()
        && uuid.toString(QUuid::WithoutBraces).compare(id, Qt::CaseInsensitive) == 0;
}

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

QString lockFilePath(const QString &serverName)
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (directory.isEmpty())
        directory = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString suffix = QString::fromLatin1(
        QCryptographicHash::hash(serverName.toUtf8(), QCryptographicHash::Sha256).toHex()
    );
    return QDir(directory).filePath(QStringLiteral("panbrowser-%1.lock").arg(suffix));
}

} // namespace

ApplicationLaunchRequest ApplicationLaunchRequest::activate()
{
    ApplicationLaunchRequest request;
    request.requestId = newRequestId();
    return request;
}

ApplicationLaunchRequest ApplicationLaunchRequest::openWebApp(const QString &id)
{
    ApplicationLaunchRequest request;
    request.command = Command::OpenWebApp;
    request.requestId = newRequestId();
    request.webAppId = id;
    return request;
}

ApplicationLaunchRequest ApplicationLaunchRequest::openUrl(const QUrl &url)
{
    ApplicationLaunchRequest request;
    request.command = Command::OpenUrl;
    request.requestId = newRequestId();
    request.url = url;
    return request;
}

bool ApplicationLaunchRequest::isValid() const
{
    if (!isValidRequestId(requestId))
        return false;
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
    object.insert(QStringLiteral("requestId"), requestId);
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
    request.requestId = object.value(QStringLiteral("requestId")).toString();
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
    , m_instanceLock(lockFilePath(m_serverName))
    , m_server(new QLocalServer(this))
{
    m_instanceLock.setStaleLockTime(0);
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
    if (m_instanceLock.tryLock(0)) {
        QLocalServer::removeServer(m_serverName);
        if (listen())
            return StartResult::Primary;
        m_instanceLock.unlock();
        if (error) {
            *error = tr("Cannot start the local PanBrowser command server: %1")
                .arg(m_server->errorString());
        }
        return StartResult::Error;
    }

    // The primary process can hold the lock just before its local server is ready.
    // Some Windows connection failures return immediately, so use elapsed time
    // instead of a fixed attempt count to preserve the intended retry window.
    QElapsedTimer retryTimer;
    retryTimer.start();
    do {
        if (forwardRequest(initialRequest))
            return StartResult::Forwarded;
        QThread::msleep(50);
    } while (retryTimer.elapsed() < 2000);
    if (error)
        *error = tr("Another PanBrowser instance is running but did not accept the launch request");
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
    socket.connectToServer(m_serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(500))
        return false;
    const QByteArray message = payload + '\n';
    if (socket.write(message) != message.size())
        return false;
    if (socket.bytesToWrite() > 0)
        socket.waitForBytesWritten(1000);

    QElapsedTimer acknowledgementTimer;
    acknowledgementTimer.start();
    while (!socket.canReadLine()) {
        const int remaining = 1000 - static_cast<int>(acknowledgementTimer.elapsed());
        if (remaining <= 0 || !socket.waitForReadyRead(remaining))
            return false;
    }
    if (socket.readLine() != launchAcknowledgement)
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
    if (request) {
        const bool firstDelivery = !m_recentRequestIdSet.contains(request->requestId);
        if (firstDelivery) {
            m_recentRequestIds.enqueue(request->requestId);
            m_recentRequestIdSet.insert(request->requestId);
            if (m_recentRequestIds.size() > maximumRememberedLaunchRequests)
                m_recentRequestIdSet.remove(m_recentRequestIds.dequeue());
        }

        if (socket->write(launchAcknowledgement) == launchAcknowledgement.size()) {
            socket->flush();
            if (socket->bytesToWrite() > 0)
                socket->waitForBytesWritten(1000);
        }
        if (firstDelivery)
            emit launchRequested(*request);
    }
    socket->disconnectFromServer();
}
