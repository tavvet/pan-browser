#pragma once

#include <QList>
#include <QString>
#include <QUrl>

struct SearchEngineSettings {
    QString id;
    QString name;
    QString keyword;
    QString urlTemplate;
    bool enabled = true;
    bool builtIn = false;
};

class SearchSettings {
public:
    static SearchSettings defaults();

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] const QList<SearchEngineSettings> &engines() const;
    [[nodiscard]] QList<SearchEngineSettings> &engines();
    [[nodiscard]] QString defaultEngineId() const;
    void setDefaultEngineId(const QString &id);

    [[nodiscard]] const SearchEngineSettings *defaultEngine() const;
    [[nodiscard]] const SearchEngineSettings *engineById(const QString &id) const;
    [[nodiscard]] const SearchEngineSettings *engineForKeyword(const QString &keyword) const;
    [[nodiscard]] QUrl searchUrl(
        const QString &query,
        const QString &engineId = QString(),
        QString *error = nullptr
    ) const;

    void restoreBuiltIns();

private:
    QList<SearchEngineSettings> m_engines;
    QString m_defaultEngineId;
};

enum class AddressInputKind {
    Navigate,
    Search,
    Error,
};

struct ResolvedAddressInput {
    AddressInputKind kind = AddressInputKind::Error;
    QUrl url;
    QString error;
    QString engineId;
};

[[nodiscard]] ResolvedAddressInput resolveAddressInput(
    const QString &input,
    const SearchSettings &settings
);
