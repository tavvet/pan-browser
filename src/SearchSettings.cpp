#include "SearchSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

namespace {

constexpr auto searchTermsPlaceholder = "{searchTerms}";

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString normalizedKeyword(QString keyword)
{
    keyword = keyword.trimmed().toLower();
    if (keyword.startsWith(QLatin1Char('@')))
        keyword.removeFirst();
    return keyword;
}

QList<SearchEngineSettings> builtInEngines()
{
    return {
        {
            QStringLiteral("builtin-duckduckgo"),
            QStringLiteral("DuckDuckGo"),
            QStringLiteral("ddg"),
            QStringLiteral("https://duckduckgo.com/?q={searchTerms}"),
            true,
            true,
        },
        {
            QStringLiteral("builtin-google"),
            QStringLiteral("Google"),
            QStringLiteral("g"),
            QStringLiteral("https://www.google.com/search?q={searchTerms}"),
            true,
            true,
        },
        {
            QStringLiteral("builtin-bing"),
            QStringLiteral("Bing"),
            QStringLiteral("bing"),
            QStringLiteral("https://www.bing.com/search?q={searchTerms}"),
            true,
            true,
        },
        {
            QStringLiteral("builtin-brave"),
            QStringLiteral("Brave Search"),
            QStringLiteral("brave"),
            QStringLiteral("https://search.brave.com/search?q={searchTerms}"),
            true,
            true,
        },
    };
}

bool isWebUrl(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return url.isValid()
        && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"))
        && !url.host().isEmpty();
}

ResolvedAddressInput inputError(const QString &message)
{
    ResolvedAddressInput result;
    result.kind = AddressInputKind::Error;
    result.error = message;
    return result;
}

ResolvedAddressInput searchInput(
    const QString &query,
    const SearchSettings &settings,
    const QString &engineId = QString()
)
{
    QString error;
    const QUrl url = settings.searchUrl(query, engineId, &error);
    if (!url.isValid())
        return inputError(error);

    ResolvedAddressInput result;
    result.kind = AddressInputKind::Search;
    result.url = url;
    result.engineId = engineId.isEmpty() && settings.defaultEngine()
        ? settings.defaultEngine()->id
        : engineId;
    return result;
}

bool looksLikeHost(const QString &input)
{
    if (input.contains(QRegularExpression(QStringLiteral("\\s"))))
        return false;
    if (input.contains(QLatin1Char('@')))
        return false;

    static const QRegularExpression localhostPattern(
        QStringLiteral(R"(^localhost(?=[:/?#]|$))"),
        QRegularExpression::CaseInsensitiveOption
    );
    if (localhostPattern.match(input).hasMatch())
        return true;

    if (input.startsWith(QLatin1Char('[')) && input.contains(QLatin1Char(']')))
        return true;

    QString hostCandidate = input;
    const qsizetype separator = hostCandidate.indexOf(QRegularExpression(QStringLiteral("[/:?#]")));
    if (separator >= 0)
        hostCandidate.truncate(separator);

    QHostAddress address;
    if (address.setAddress(hostCandidate))
        return true;

    return hostCandidate.contains(QLatin1Char('.'))
        && !hostCandidate.startsWith(QLatin1Char('.'))
        && !hostCandidate.endsWith(QLatin1Char('.'));
}

} // namespace

SearchSettings SearchSettings::defaults()
{
    SearchSettings settings;
    settings.m_engines = builtInEngines();
    settings.m_defaultEngineId = QStringLiteral("builtin-duckduckgo");
    return settings;
}

bool SearchSettings::load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fail(error, QCoreApplication::translate("SearchSettings", "Cannot open %1: %2").arg(path, file.errorString()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QCoreApplication::translate("SearchSettings", "Invalid JSON: %1").arg(parseError.errorString()));

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1)
        return fail(error, QCoreApplication::translate("SearchSettings", "Unsupported search settings version"));
    if (!root.value(QStringLiteral("engines")).isArray())
        return fail(error, QCoreApplication::translate("SearchSettings", "Search engines must be an array"));

    SearchSettings loaded;
    loaded.m_defaultEngineId = root.value(QStringLiteral("defaultEngine")).toString();
    for (const QJsonValue &value : root.value(QStringLiteral("engines")).toArray()) {
        if (!value.isObject())
            return fail(error, QCoreApplication::translate("SearchSettings", "Every search engine must be an object"));
        const QJsonObject object = value.toObject();
        SearchEngineSettings engine;
        engine.id = object.value(QStringLiteral("id")).toString().trimmed();
        engine.name = object.value(QStringLiteral("name")).toString().trimmed();
        engine.keyword = normalizedKeyword(object.value(QStringLiteral("keyword")).toString());
        engine.urlTemplate = object.value(QStringLiteral("urlTemplate")).toString().trimmed();
        engine.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        engine.builtIn = object.value(QStringLiteral("builtIn")).toBool(false);
        loaded.m_engines.append(engine);
    }

    QString validationError;
    if (!loaded.validate(&validationError))
        return fail(error, validationError);
    *this = loaded;
    return true;
}

bool SearchSettings::save(const QString &path, QString *error) const
{
    if (!validate(error))
        return false;

    QJsonArray engines;
    for (const SearchEngineSettings &engine : m_engines) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), engine.id.trimmed());
        object.insert(QStringLiteral("name"), engine.name.trimmed());
        object.insert(QStringLiteral("keyword"), normalizedKeyword(engine.keyword));
        object.insert(QStringLiteral("urlTemplate"), engine.urlTemplate.trimmed());
        object.insert(QStringLiteral("enabled"), engine.enabled);
        object.insert(QStringLiteral("builtIn"), engine.builtIn);
        engines.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("defaultEngine"), m_defaultEngineId);
    root.insert(QStringLiteral("engines"), engines);

    if (QFile::exists(path)) {
        const QString backupPath = path + QStringLiteral(".backup");
        if (QFile::exists(backupPath) && !QFile::remove(backupPath))
            return fail(error, QCoreApplication::translate("SearchSettings", "Cannot replace backup %1").arg(backupPath));
        if (!QFile::copy(path, backupPath))
            return fail(error, QCoreApplication::translate("SearchSettings", "Cannot create backup %1").arg(backupPath));
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, QCoreApplication::translate("SearchSettings", "Cannot write %1: %2").arg(path, file.errorString()));
    const QByteArray contents = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(contents) != contents.size())
        return fail(error, QCoreApplication::translate("SearchSettings", "Cannot write %1: %2").arg(path, file.errorString()));
    if (!file.commit())
        return fail(error, QCoreApplication::translate("SearchSettings", "Cannot commit %1: %2").arg(path, file.errorString()));
    return true;
}

bool SearchSettings::validate(QString *error) const
{
    if (m_engines.isEmpty())
        return fail(error, QCoreApplication::translate("SearchSettings", "Add at least one search engine"));

    static const QRegularExpression keywordPattern(QStringLiteral("^[a-z0-9][a-z0-9_-]{0,15}$"));
    QSet<QString> ids;
    QSet<QString> names;
    QSet<QString> keywords;
    int enabledCount = 0;

    for (const SearchEngineSettings &engine : m_engines) {
        const QString id = engine.id.trimmed();
        const QString name = engine.name.trimmed();
        const QString keyword = normalizedKeyword(engine.keyword);
        const QString urlTemplate = engine.urlTemplate.trimmed();
        if (id.isEmpty())
            return fail(error, QCoreApplication::translate("SearchSettings", "Search engine id cannot be empty"));
        if (ids.contains(id))
            return fail(error, QCoreApplication::translate("SearchSettings", "Duplicate search engine id: %1").arg(id));
        ids.insert(id);
        if (name.isEmpty())
            return fail(error, QCoreApplication::translate("SearchSettings", "Search engine name cannot be empty"));
        const QString foldedName = name.toCaseFolded();
        if (names.contains(foldedName))
            return fail(error, QCoreApplication::translate("SearchSettings", "Duplicate search engine name: %1").arg(name));
        names.insert(foldedName);
        if (!keyword.isEmpty() && !keywordPattern.match(keyword).hasMatch()) {
            return fail(
                error,
                QCoreApplication::translate("SearchSettings", "Keyword for %1 must contain 1–16 lowercase letters, numbers, _ or -")
                    .arg(name)
            );
        }
        if (!keyword.isEmpty() && keywords.contains(keyword))
            return fail(error, QCoreApplication::translate("SearchSettings", "Duplicate search keyword: @%1").arg(keyword));
        if (!keyword.isEmpty())
            keywords.insert(keyword);
        if (urlTemplate.count(QString::fromLatin1(searchTermsPlaceholder)) != 1) {
            return fail(
                error,
                QCoreApplication::translate("SearchSettings", "URL template for %1 must contain {searchTerms} exactly once")
                    .arg(name)
            );
        }
        QString testTemplate = urlTemplate;
        testTemplate.replace(QString::fromLatin1(searchTermsPlaceholder), QStringLiteral("test"));
        const QUrl testUrl(testTemplate, QUrl::StrictMode);
        if (!isWebUrl(testUrl))
            return fail(error, QCoreApplication::translate("SearchSettings", "%1 has an invalid HTTP or HTTPS URL template").arg(name));
        if (!testUrl.userName().isEmpty() || !testUrl.password().isEmpty())
            return fail(error, QCoreApplication::translate("SearchSettings", "URL template for %1 must not contain credentials").arg(name));
        if (engine.enabled)
            ++enabledCount;
    }

    if (enabledCount == 0)
        return fail(error, QCoreApplication::translate("SearchSettings", "Enable at least one search engine"));
    const SearchEngineSettings *selected = engineById(m_defaultEngineId);
    if (!selected || !selected->enabled)
        return fail(error, QCoreApplication::translate("SearchSettings", "Choose an enabled default search engine"));
    return true;
}

const QList<SearchEngineSettings> &SearchSettings::engines() const
{
    return m_engines;
}

QList<SearchEngineSettings> &SearchSettings::engines()
{
    return m_engines;
}

QString SearchSettings::defaultEngineId() const
{
    return m_defaultEngineId;
}

void SearchSettings::setDefaultEngineId(const QString &id)
{
    m_defaultEngineId = id;
}

const SearchEngineSettings *SearchSettings::defaultEngine() const
{
    return engineById(m_defaultEngineId);
}

const SearchEngineSettings *SearchSettings::engineById(const QString &id) const
{
    for (const SearchEngineSettings &engine : m_engines) {
        if (engine.id == id)
            return &engine;
    }
    return nullptr;
}

const SearchEngineSettings *SearchSettings::engineForKeyword(const QString &keyword) const
{
    const QString wanted = normalizedKeyword(keyword);
    for (const SearchEngineSettings &engine : m_engines) {
        if (engine.enabled && normalizedKeyword(engine.keyword) == wanted)
            return &engine;
    }
    return nullptr;
}

QUrl SearchSettings::searchUrl(
    const QString &query,
    const QString &engineId,
    QString *error
) const
{
    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty()) {
        fail(error, QCoreApplication::translate("SearchSettings", "Enter a search query"));
        return {};
    }
    const SearchEngineSettings *engine = engineId.isEmpty()
        ? defaultEngine()
        : engineById(engineId);
    if (!engine || !engine->enabled) {
        fail(error, QCoreApplication::translate("SearchSettings", "The selected search engine is unavailable"));
        return {};
    }

    QString target = engine->urlTemplate.trimmed();
    target.replace(
        QString::fromLatin1(searchTermsPlaceholder),
        QString::fromLatin1(QUrl::toPercentEncoding(trimmedQuery))
    );
    const QUrl url(target, QUrl::StrictMode);
    if (!isWebUrl(url)) {
        fail(error, QCoreApplication::translate("SearchSettings", "The search engine produced an invalid URL"));
        return {};
    }
    return url;
}

void SearchSettings::restoreBuiltIns()
{
    QList<SearchEngineSettings> customEngines;
    for (const SearchEngineSettings &engine : std::as_const(m_engines)) {
        if (!engine.builtIn)
            customEngines.append(engine);
    }
    m_engines = builtInEngines();
    m_engines.append(customEngines);
    if (!defaultEngine() || !defaultEngine()->enabled)
        m_defaultEngineId = QStringLiteral("builtin-duckduckgo");
}

ResolvedAddressInput resolveAddressInput(
    const QString &source,
    const SearchSettings &settings
)
{
    const QString input = source.trimmed();
    if (input.isEmpty())
        return inputError(QCoreApplication::translate("SearchSettings", "Enter an address or search query"));

    if (input.startsWith(QLatin1Char('?')))
        return searchInput(input.sliced(1).trimmed(), settings);

    if (input.startsWith(QLatin1Char('@'))) {
        static const QRegularExpression keywordInput(
            QStringLiteral(R"(^@([A-Za-z0-9_-]+)(?:\s+(.*))?$)")
        );
        const QRegularExpressionMatch match = keywordInput.match(input);
        if (!match.hasMatch())
            return inputError(QCoreApplication::translate("SearchSettings", "Use @keyword followed by a search query"));
        const QString keyword = match.captured(1).toLower();
        const SearchEngineSettings *engine = settings.engineForKeyword(keyword);
        if (!engine)
            return inputError(QCoreApplication::translate("SearchSettings", "Unknown or disabled search keyword: @%1").arg(keyword));
        return searchInput(match.captured(2), settings, engine->id);
    }

    if (looksLikeHost(input)) {
        const QUrl url = QUrl::fromUserInput(input);
        if (isWebUrl(url)) {
            ResolvedAddressInput result;
            result.kind = AddressInputKind::Navigate;
            result.url = url;
            return result;
        }
        return inputError(QCoreApplication::translate("SearchSettings", "Invalid web address"));
    }

    static const QRegularExpression explicitScheme(
        QStringLiteral(R"(^[A-Za-z][A-Za-z0-9+.-]*:)")
    );
    if (explicitScheme.match(input).hasMatch()) {
        const QUrl url(input, QUrl::StrictMode);
        if (isWebUrl(url)) {
            ResolvedAddressInput result;
            result.kind = AddressInputKind::Navigate;
            result.url = url;
            return result;
        }
        return inputError(QCoreApplication::translate("SearchSettings", "Only HTTP and HTTPS URLs are supported"));
    }

    return searchInput(input, settings);
}
