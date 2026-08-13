#include "VotUserscriptPackage.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringConverter>

#include <utility>

namespace {

constexpr qsizetype maximumUserscriptSize = 4 * 1024 * 1024;

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("VotUserscriptPackage", source);
}

bool hostMatches(const QString &pattern, const QString &host)
{
    if (pattern == QStringLiteral("*"))
        return true;
    if (pattern.startsWith(QStringLiteral("*."))) {
        const QString suffix = pattern.mid(2).toLower();
        return host == suffix || host.endsWith(QStringLiteral(".") + suffix);
    }
    return host == pattern.toLower();
}

QRegularExpression wildcardExpression(const QString &pattern)
{
    QString expression = QRegularExpression::escape(pattern);
    expression.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
    return QRegularExpression(QStringLiteral("^%1$").arg(expression));
}

} // namespace

QString VotUserscriptPackage::supportedVersion()
{
    return QStringLiteral("1.11.8");
}

QString VotUserscriptPackage::officialDownloadUrl()
{
    return QStringLiteral(
        "https://github.com/ilyhalight/voice-over-translation/releases/download/1.11.8/vot.user.js"
    );
}

QByteArray VotUserscriptPackage::expectedSha256Hex()
{
    return QByteArrayLiteral(
        "2f98eae1d35f376dad6777aeccee1d1821f4f300e6a156222b587ab05c3a1efb"
    );
}

bool VotUserscriptPackage::load(
    const QString &path,
    VotUserscript *userscript,
    QString *error
)
{
    if (error)
        error->clear();
    if (!userscript)
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptPackage",
            "The VOT userscript destination is unavailable"
        )));

    const QFileInfo info(path);
    if (!info.isFile())
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptPackage",
            "The selected VOT userscript does not exist"
        )));
    if (info.size() <= 0 || info.size() > maximumUserscriptSize)
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptPackage",
            "The selected VOT userscript has an invalid size"
        )));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VotUserscriptPackage",
                "Cannot read the selected VOT userscript: %1"
            )).arg(file.errorString())
        );
    }
    const QByteArray contents = file.readAll();
    const QByteArray digest = QCryptographicHash::hash(
        contents,
        QCryptographicHash::Sha256
    ).toHex();
    if (digest != expectedSha256Hex()) {
        return fail(
            error,
            text(QT_TRANSLATE_NOOP(
                "VotUserscriptPackage",
                "The selected file is not the verified official VOT %1 userscript"
            ))
                .arg(supportedVersion())
        );
    }

    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString source = decoder.decode(contents);
    if (decoder.hasError())
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptPackage",
            "The selected VOT userscript is not valid UTF-8"
        )));

    const qsizetype metadataStart = source.indexOf(QStringLiteral("// ==UserScript=="));
    const qsizetype metadataEnd = source.indexOf(QStringLiteral("// ==/UserScript=="));
    if (metadataStart != 0 || metadataEnd <= metadataStart)
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptPackage",
            "The selected VOT userscript has invalid metadata"
        )));

    VotUserscript loaded;
    loaded.sourceCode = source;
    loaded.sha256 = digest;
    const QString metadata = source.mid(
        metadataStart,
        metadataEnd - metadataStart
    );
    static const QRegularExpression fieldPattern(
        QStringLiteral("^//\\s+@(version|match|exclude|connect)\\s+(.+?)\\s*$"),
        QRegularExpression::MultilineOption
    );
    QRegularExpressionMatchIterator iterator = fieldPattern.globalMatch(metadata);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QString key = match.captured(1);
        const QString value = match.captured(2).trimmed();
        if (key == QStringLiteral("version"))
            loaded.version = value;
        else if (key == QStringLiteral("match"))
            loaded.matchPatterns.append(value);
        else if (key == QStringLiteral("exclude"))
            loaded.excludePatterns.append(value);
        else if (key == QStringLiteral("connect"))
            loaded.connectHosts.append(value.toLower());
    }

    if (loaded.version != supportedVersion()
        || loaded.matchPatterns.isEmpty()
        || loaded.connectHosts.isEmpty()
        || loaded.connectHosts.contains(QStringLiteral("*"))) {
        return fail(error, text(QT_TRANSLATE_NOOP(
            "VotUserscriptPackage",
            "The selected VOT userscript metadata is not supported"
        )));
    }
    for (const QString &host : std::as_const(loaded.connectHosts)) {
        const QUrl probe(QStringLiteral("https://") + host);
        if (!probe.isValid() || probe.host().isEmpty() || probe.host() != host) {
            return fail(error, text(QT_TRANSLATE_NOOP(
                "VotUserscriptPackage",
                "The selected VOT userscript contains an invalid network host"
            )));
        }
    }

    *userscript = std::move(loaded);
    return true;
}

bool VotUserscriptPackage::matchesUrlPattern(
    const QString &pattern,
    const QUrl &url
)
{
    const qsizetype schemeSeparator = pattern.indexOf(QStringLiteral("://"));
    if (schemeSeparator <= 0 || !url.isValid())
        return false;
    const QString schemePattern = pattern.left(schemeSeparator).toLower();
    const QString scheme = url.scheme().toLower();
    if (schemePattern == QStringLiteral("*")) {
        if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
            return false;
    } else if (schemePattern != scheme) {
        return false;
    }

    const qsizetype hostStart = schemeSeparator + 3;
    const qsizetype pathStart = pattern.indexOf(QLatin1Char('/'), hostStart);
    if (pathStart < 0)
        return false;
    const QString hostPattern = pattern.mid(hostStart, pathStart - hostStart);
    if (!hostMatches(hostPattern, url.host().toLower()))
        return false;

    QString path = url.path(QUrl::FullyEncoded);
    if (path.isEmpty())
        path = QStringLiteral("/");
    if (url.hasQuery())
        path += QLatin1Char('?') + url.query(QUrl::FullyEncoded);
    return wildcardExpression(pattern.mid(pathStart)).match(path).hasMatch();
}

bool VotUserscriptPackage::isAllowedConnectUrl(
    const QStringList &connectHosts,
    const QUrl &url
)
{
    if (!url.isValid()
        || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
        || url.host().isEmpty()
        || !url.userInfo().isEmpty()) {
        return false;
    }
    const QString host = url.host().toLower();
    for (const QString &allowed : connectHosts) {
        const QString normalized = allowed.trimmed().toLower();
        if (!normalized.isEmpty()
            && normalized != QStringLiteral("*")
            && (host == normalized || host.endsWith(QStringLiteral(".") + normalized))) {
            return true;
        }
    }
    return false;
}
