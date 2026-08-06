#include "WebAppStore.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <utility>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("WebAppStore", source);
}

int effectivePort(const QUrl &url)
{
    if (url.port() >= 0)
        return url.port();
    return url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 ? 443 : -1;
}

bool isSafeHttpsUrl(const QUrl &url)
{
    return url.isValid()
        && url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        && !url.host().isEmpty()
        && url.userInfo().isEmpty();
}

bool sameOrigin(const QUrl &left, const QUrl &right)
{
    return isSafeHttpsUrl(left)
        && isSafeHttpsUrl(right)
        && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
        && effectivePort(left) == effectivePort(right);
}

QUrl normalizedUrl(QUrl url, bool keepQuery)
{
    url.setScheme(url.scheme().toLower());
    url.setHost(url.host().toLower());
    url.setFragment(QString());
    url.setUserInfo(QString());
    if (!keepQuery)
        url.setQuery(QString());
    url = url.adjusted(QUrl::NormalizePathSegments);
    if (url.path().isEmpty())
        url.setPath(QStringLiteral("/"));
    return url;
}

QString sanitizedText(QString value, qsizetype maximumLength)
{
    static const QRegularExpression controls(QStringLiteral("[\\x{0000}-\\x{001F}\\x{007F}]") );
    value.remove(controls);
    value = value.simplified();
    if (value.size() > maximumLength)
        value.truncate(maximumLength);
    return value;
}

QUrl defaultScopeForStartUrl(const QUrl &startUrl)
{
    QUrl scope = normalizedUrl(startUrl, false);
    QString path = scope.path();
    const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
    scope.setPath(slash >= 0 ? path.left(slash + 1) : QStringLiteral("/"));
    return scope;
}

QString stableAppId(QUrl identity)
{
    identity = normalizedUrl(identity, true);
    const QByteArray digest = QCryptographicHash::hash(
        identity.toEncoded(QUrl::FullyEncoded),
        QCryptographicHash::Sha256
    );
    return QString::fromLatin1(digest.toHex());
}

bool isValidAppId(const QString &id)
{
    static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
    return expression.match(id).hasMatch();
}

QJsonObject serialize(const WebApp &app)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), app.id);
    object.insert(QStringLiteral("name"), app.name);
    object.insert(QStringLiteral("shortName"), app.shortName);
    object.insert(QStringLiteral("description"), app.description);
    object.insert(QStringLiteral("displayMode"), app.displayMode);
    object.insert(QStringLiteral("startUrl"), app.startUrl.toString(QUrl::FullyEncoded));
    object.insert(QStringLiteral("scope"), app.scope.toString(QUrl::FullyEncoded));
    object.insert(QStringLiteral("manifestUrl"), app.manifestUrl.toString(QUrl::FullyEncoded));
    object.insert(QStringLiteral("iconPng"), QString::fromLatin1(app.iconPng.toBase64()));
    object.insert(QStringLiteral("installedAt"), app.installedAt.toUTC().toString(Qt::ISODateWithMs));
    return object;
}

std::optional<WebApp> deserialize(const QJsonObject &object)
{
    WebApp app;
    app.id = object.value(QStringLiteral("id")).toString();
    app.name = sanitizedText(object.value(QStringLiteral("name")).toString(), 120);
    app.shortName = sanitizedText(object.value(QStringLiteral("shortName")).toString(), 60);
    app.description = sanitizedText(object.value(QStringLiteral("description")).toString(), 500);
    app.displayMode = object.value(QStringLiteral("displayMode")).toString();
    const QUrl startUrl(object.value(QStringLiteral("startUrl")).toString());
    const QUrl scope(object.value(QStringLiteral("scope")).toString());
    const QUrl manifestUrl(object.value(QStringLiteral("manifestUrl")).toString());
    if (!sameOrigin(startUrl, scope) || !sameOrigin(startUrl, manifestUrl))
        return std::nullopt;
    app.startUrl = normalizedUrl(startUrl, true);
    app.scope = normalizedUrl(scope, false);
    app.manifestUrl = normalizedUrl(manifestUrl, true);
    app.iconPng = QByteArray::fromBase64(
        object.value(QStringLiteral("iconPng")).toString().toLatin1(),
        QByteArray::AbortOnBase64DecodingErrors
    );
    app.installedAt = QDateTime::fromString(
        object.value(QStringLiteral("installedAt")).toString(),
        Qt::ISODateWithMs
    ).toUTC();

    if (!isValidAppId(app.id)
        || app.name.isEmpty()
        || !sameOrigin(app.startUrl, app.scope)
        || !sameOrigin(app.startUrl, app.manifestUrl)
        || !WebAppStore::containsUrl(app, app.startUrl)
        || app.iconPng.size() > WebAppStore::maximumIconBytes
        || !app.installedAt.isValid()) {
        return std::nullopt;
    }
    if (app.shortName.isEmpty())
        app.shortName = app.name;
    return app;
}

} // namespace

WebAppStore::WebAppStore(QString path, QObject *parent)
    : QObject(parent)
    , m_path(std::move(path))
{
}

bool WebAppStore::load(QString *error)
{
    if (error)
        error->clear();
    m_apps.clear();
    m_available = false;

    QFile file(m_path);
    if (!file.exists()) {
        m_available = true;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Cannot open installed web apps: %1")).arg(file.errorString()));
    constexpr qint64 maximumStoreBytes = 40LL * 1024 * 1024;
    if (file.size() > maximumStoreBytes)
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file is too large")));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file is invalid: %1")).arg(parseError.errorString()));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1)
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file uses an unsupported version")));

    const QJsonValue appsValue = root.value(QStringLiteral("apps"));
    if (!appsValue.isArray())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file does not contain an apps array")));

    QList<WebApp> loadedApps;
    const QJsonArray items = appsValue.toArray();
    if (items.size() > maximumApps)
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file contains too many entries")));
    for (const QJsonValue &value : items) {
        const std::optional<WebApp> parsed = deserialize(value.toObject());
        if (!parsed)
            return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file contains an invalid entry")));
        const auto duplicate = std::find_if(loadedApps.cbegin(), loadedApps.cend(), [&](const WebApp &app) {
            return app.id == parsed->id || app.manifestUrl == parsed->manifestUrl;
        });
        if (duplicate != loadedApps.cend())
            return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps file contains duplicate entries")));
        loadedApps.append(*parsed);
    }
    m_apps = loadedApps;
    m_available = true;
    return true;
}

bool WebAppStore::isAvailable() const
{
    return m_available;
}

QList<WebApp> WebAppStore::apps() const
{
    return m_apps;
}

std::optional<WebApp> WebAppStore::app(const QString &id) const
{
    const auto found = std::find_if(m_apps.cbegin(), m_apps.cend(), [&](const WebApp &item) {
        return item.id == id;
    });
    return found == m_apps.cend() ? std::nullopt : std::optional<WebApp>(*found);
}

std::optional<WebApp> WebAppStore::appForManifest(const QUrl &manifestUrl) const
{
    const QUrl normalized = normalizedUrl(manifestUrl, true);
    const auto found = std::find_if(m_apps.cbegin(), m_apps.cend(), [&](const WebApp &item) {
        return item.manifestUrl == normalized;
    });
    return found == m_apps.cend() ? std::nullopt : std::optional<WebApp>(*found);
}

bool WebAppStore::install(const WebApp &source, QString *error)
{
    if (error)
        error->clear();
    if (!m_available)
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps are unavailable")));
    WebApp app = source;
    app.name = sanitizedText(app.name, 120);
    app.shortName = sanitizedText(app.shortName, 60);
    app.description = sanitizedText(app.description, 500);
    if (!sameOrigin(app.startUrl, app.scope) || !sameOrigin(app.startUrl, app.manifestUrl))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app is not valid")));
    app.startUrl = normalizedUrl(app.startUrl, true);
    app.scope = normalizedUrl(app.scope, false);
    app.manifestUrl = normalizedUrl(app.manifestUrl, true);
    if (app.iconPng.size() > maximumIconBytes)
        app.iconPng.clear();
    if (app.installedAt.isValid() == false)
        app.installedAt = QDateTime::currentDateTimeUtc();

    if (!isValidAppId(app.id)
        || app.name.isEmpty()
        || !sameOrigin(app.startUrl, app.scope)
        || !sameOrigin(app.startUrl, app.manifestUrl)
        || !containsUrl(app, app.startUrl)) {
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app is not valid")));
    }

    QList<WebApp> previous = m_apps;
    const auto found = std::find_if(m_apps.begin(), m_apps.end(), [&](const WebApp &item) {
        return item.id == app.id || item.manifestUrl == app.manifestUrl;
    });
    if (found == m_apps.end() && m_apps.size() >= maximumApps)
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The installed web app limit has been reached")));
    if (found == m_apps.end())
        m_apps.append(app);
    else
        *found = app;

    if (!save(error)) {
        m_apps = previous;
        return false;
    }
    emit appsChanged();
    return true;
}

bool WebAppStore::remove(const QString &id, QString *error)
{
    if (error)
        error->clear();
    if (!m_available)
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Installed web apps are unavailable")));
    const auto found = std::find_if(m_apps.begin(), m_apps.end(), [&](const WebApp &item) {
        return item.id == id;
    });
    if (found == m_apps.end())
        return true;

    const QList<WebApp> previous = m_apps;
    m_apps.erase(found);
    if (!save(error)) {
        m_apps = previous;
        return false;
    }
    emit appsChanged();
    return true;
}

QString WebAppStore::path() const
{
    return m_path;
}

std::optional<WebApp> WebAppStore::parseManifest(
    const QByteArray &contents,
    const QUrl &manifestUrl,
    const QUrl &documentUrl,
    const QString &fallbackTitle,
    QString *error
)
{
    if (error)
        error->clear();
    if (contents.isEmpty() || contents.size() > maximumManifestBytes) {
        fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app manifest is empty or too large")));
        return std::nullopt;
    }
    if (!sameOrigin(manifestUrl, documentUrl)) {
        fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app manifest must use HTTPS and the same origin as the page")));
        return std::nullopt;
    }
    const QUrl safeManifestUrl = normalizedUrl(manifestUrl, true);
    const QUrl safeDocumentUrl = normalizedUrl(documentUrl, true);

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(contents, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app manifest is invalid: %1")).arg(parseError.errorString()));
        return std::nullopt;
    }
    const QJsonObject object = document.object();

    QUrl startUrl = safeDocumentUrl;
    const QString startValue = object.value(QStringLiteral("start_url")).toString().trimmed();
    if (!startValue.isEmpty())
        startUrl = safeManifestUrl.resolved(QUrl(startValue));
    if (!sameOrigin(startUrl, safeDocumentUrl)) {
        fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app start URL must use the same HTTPS origin as the page")));
        return std::nullopt;
    }
    startUrl = normalizedUrl(startUrl, true);

    QUrl scope = defaultScopeForStartUrl(startUrl);
    const QString scopeValue = object.value(QStringLiteral("scope")).toString().trimmed();
    if (!scopeValue.isEmpty())
        scope = safeManifestUrl.resolved(QUrl(scopeValue));
    if (!sameOrigin(scope, startUrl)) {
        fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app scope must use the same origin as its start URL")));
        return std::nullopt;
    }
    scope = normalizedUrl(scope, false);

    WebApp app;
    app.name = sanitizedText(object.value(QStringLiteral("name")).toString(), 120);
    app.shortName = sanitizedText(object.value(QStringLiteral("short_name")).toString(), 60);
    if (app.name.isEmpty())
        app.name = app.shortName;
    if (app.name.isEmpty())
        app.name = sanitizedText(fallbackTitle, 120);
    if (app.name.isEmpty())
        app.name = safeDocumentUrl.host();
    if (app.shortName.isEmpty())
        app.shortName = app.name.left(60);
    app.description = sanitizedText(object.value(QStringLiteral("description")).toString(), 500);
    app.startUrl = startUrl;
    app.scope = scope;
    app.manifestUrl = safeManifestUrl;

    static const QStringList supportedDisplayModes = {
        QStringLiteral("standalone"),
        QStringLiteral("minimal-ui"),
        QStringLiteral("fullscreen"),
        QStringLiteral("browser"),
    };
    app.displayMode = object.value(QStringLiteral("display")).toString().trimmed().toLower();
    if (!supportedDisplayModes.contains(app.displayMode))
        app.displayMode = QStringLiteral("standalone");

    QUrl identity = startUrl;
    const QString identityValue = object.value(QStringLiteral("id")).toString().trimmed();
    if (!identityValue.isEmpty()) {
        QUrl identityBase;
        identityBase.setScheme(startUrl.scheme());
        identityBase.setHost(startUrl.host());
        identityBase.setPort(startUrl.port());
        identityBase.setPath(QStringLiteral("/"));
        const QUrl candidate = identityBase.resolved(QUrl(identityValue));
        if (sameOrigin(candidate, startUrl))
            identity = normalizedUrl(candidate, true);
    }
    app.id = stableAppId(identity);
    app.installedAt = QDateTime::currentDateTimeUtc();
    if (!containsUrl(app, app.startUrl)) {
        fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "The web app start URL is outside its declared scope")));
        return std::nullopt;
    }
    return app;
}

bool WebAppStore::containsUrl(const WebApp &app, const QUrl &url)
{
    if (!isSafeHttpsUrl(url))
        return false;
    const QUrl candidate = normalizedUrl(url, true);
    const QUrl scope = normalizedUrl(app.scope, false);
    if (!sameOrigin(candidate, scope))
        return false;
    return candidate.path(QUrl::FullyEncoded).startsWith(scope.path(QUrl::FullyEncoded));
}

bool WebAppStore::save(QString *error) const
{
    QJsonArray items;
    for (const WebApp &app : m_apps)
        items.append(serialize(app));
    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("apps"), items);
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);

    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Cannot create the installed web apps directory")));
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Cannot write installed web apps: %1")).arg(file.errorString()));
    if (file.write(contents) != contents.size())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Cannot write installed web apps: %1")).arg(file.errorString()));
    if (!file.commit())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppStore", "Cannot save installed web apps: %1")).arg(file.errorString()));
    return true;
}
