#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <optional>

struct WebApp {
    QString id;
    QString name;
    QString shortName;
    QString description;
    QString displayMode;
    QUrl startUrl;
    QUrl scope;
    QUrl manifestUrl;
    QByteArray iconPng;
    QDateTime installedAt;
};

class WebAppStore final : public QObject {
    Q_OBJECT

public:
    static constexpr qsizetype maximumManifestBytes = 256 * 1024;
    static constexpr qsizetype maximumIconBytes = 512 * 1024;
    static constexpr qsizetype maximumApps = 50;

    explicit WebAppStore(QString path, QObject *parent = nullptr);

    bool load(QString *error = nullptr);
    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] QList<WebApp> apps() const;
    [[nodiscard]] std::optional<WebApp> app(const QString &id) const;
    [[nodiscard]] std::optional<WebApp> appForManifest(const QUrl &manifestUrl) const;
    bool install(const WebApp &app, QString *error = nullptr);
    bool remove(const QString &id, QString *error = nullptr);
    [[nodiscard]] QString path() const;

    static std::optional<WebApp> parseManifest(
        const QByteArray &contents,
        const QUrl &manifestUrl,
        const QUrl &documentUrl,
        const QString &fallbackTitle,
        QString *error = nullptr
    );
    [[nodiscard]] static bool containsUrl(const WebApp &app, const QUrl &url);

signals:
    void appsChanged();

private:
    bool save(QString *error = nullptr) const;

    QString m_path;
    QList<WebApp> m_apps;
    bool m_available = false;
};
