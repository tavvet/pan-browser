#pragma once

#include "ProxySettings.h"

#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QSpinBox;

class ProxySettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit ProxySettingsPage(const ProxySettings &settings, QWidget *parent = nullptr);

    [[nodiscard]] ProxySettings settings() const;
    bool validate(QString *error = nullptr) const;

private:
    void updateModeUi();

    ProxySettings m_initialSettings;
    QComboBox *m_mode = nullptr;
    QLabel *m_modeHint = nullptr;
    QLabel *m_credentialsHint = nullptr;
    QFrame *m_manualCard = nullptr;
    QComboBox *m_type = nullptr;
    QLineEdit *m_host = nullptr;
    QSpinBox *m_port = nullptr;
    QLineEdit *m_username = nullptr;
};
