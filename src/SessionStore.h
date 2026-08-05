#pragma once

#include <QList>
#include <QString>
#include <QUrl>

struct SessionTab {
    QUrl url;
    QString title;
};

struct BrowserSession {
    QList<SessionTab> tabs;
    int activeIndex = 0;
};

class SessionStore {
public:
    explicit SessionStore(QString path);

    [[nodiscard]] BrowserSession load(QString *error = nullptr) const;
    bool save(const BrowserSession &session, QString *error = nullptr) const;
    bool clear(QString *error = nullptr) const;

    [[nodiscard]] QString path() const;

private:
    QString m_path;
};
