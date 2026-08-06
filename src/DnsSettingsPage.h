#pragma once

#include "DnsSettings.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

class DnsSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit DnsSettingsPage(const DnsSettings &settings, QWidget *parent = nullptr);

    [[nodiscard]] DnsSettings settings() const;
    bool validate(QString *error = nullptr) const;

private:
    void rebuildProviders(const QString &selectedId = QString());
    void rebuildActiveProviders();
    void updateModeUi();
    void updateSelection();
    void addProvider();
    void editSelectedProvider();
    void removeSelectedProvider();
    [[nodiscard]] DnsProvider *selectedProvider();
    [[nodiscard]] const DnsProvider *selectedProvider() const;
    bool editProvider(DnsProvider *provider, bool adding);

    DnsSettings m_settings;
    bool m_rebuilding = false;
    QComboBox *m_mode = nullptr;
    QComboBox *m_activeProvider = nullptr;
    QLabel *m_modeHint = nullptr;
    QListWidget *m_providerList = nullptr;
    QLabel *m_details = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};
