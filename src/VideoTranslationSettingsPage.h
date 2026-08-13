#pragma once

#include "VideoTranslationSettings.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class VotUserscriptManager;

class VideoTranslationSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit VideoTranslationSettingsPage(
        const VideoTranslationSettings &settings,
        VotUserscriptManager *manager,
        QWidget *parent = nullptr
    );

    [[nodiscard]] VideoTranslationSettings settings() const;
    bool validate(QString *error = nullptr) const;

private:
    void chooseUserscript();
    void updateStatus();

    VideoTranslationSettings m_settings;
    VotUserscriptManager *m_manager = nullptr;
    QCheckBox *m_enabled = nullptr;
    QLineEdit *m_sourcePath = nullptr;
    QLabel *m_status = nullptr;
};
