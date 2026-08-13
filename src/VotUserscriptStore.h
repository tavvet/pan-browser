#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class QJsonValue;

class VotUserscriptStore final : public QObject {
    Q_OBJECT

public:
    explicit VotUserscriptStore(
        QString path = {},
        QObject *parent = nullptr
    );

    bool load(QString *error = nullptr);
    bool setValue(
        const QString &name,
        const QJsonValue &value,
        QString *error = nullptr
    );
    bool removeValue(const QString &name, QString *error = nullptr);

    [[nodiscard]] QJsonObject values() const;
    [[nodiscard]] QString path() const;

signals:
    void valueChanged(
        const QString &name,
        const QJsonValue &value,
        bool removed
    );

private:
    bool save(const QJsonObject &values, QString *error);

    QString m_path;
    QJsonObject m_values;
};
