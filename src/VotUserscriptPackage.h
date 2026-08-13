#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

struct VotUserscript final {
    QString sourceCode;
    QString version;
    QStringList matchPatterns;
    QStringList excludePatterns;
    QStringList connectHosts;
    QByteArray sha256;
};

class VotUserscriptPackage final {
public:
    static QString supportedVersion();
    static QString officialDownloadUrl();
    static QByteArray expectedSha256Hex();

    static bool load(
        const QString &path,
        VotUserscript *userscript,
        QString *error = nullptr
    );

    [[nodiscard]] static bool matchesUrlPattern(
        const QString &pattern,
        const QUrl &url
    );
    [[nodiscard]] static bool isAllowedConnectUrl(
        const QStringList &connectHosts,
        const QUrl &url
    );
};
