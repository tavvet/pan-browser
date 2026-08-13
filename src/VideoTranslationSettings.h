#pragma once

#include <QString>

class VideoTranslationSettings {
public:
    static VideoTranslationSettings defaults();

    bool load(const QString &path, QString *error = nullptr);
    bool save(const QString &path, QString *error = nullptr) const;
    bool validate(QString *error = nullptr) const;

    [[nodiscard]] bool enabled() const;
    void setEnabled(bool enabled);

    [[nodiscard]] QString sourcePath() const;
    void setSourcePath(const QString &path);

private:
    bool m_enabled = false;
    QString m_sourcePath;
};
