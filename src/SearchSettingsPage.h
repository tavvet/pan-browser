#pragma once

#include "SearchSettings.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;

class SearchSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit SearchSettingsPage(const SearchSettings &settings, QWidget *parent = nullptr);

    [[nodiscard]] SearchSettings settings() const;
    bool validate(QString *error = nullptr) const;

private:
    void rebuildList(const QString &selectedId = QString());
    void rebuildDefaultEngines();
    void updateActions();
    void updateDetails();
    void addEngine();
    void editSelectedEngine();
    void removeSelectedEngine();
    void restoreBuiltInEngines();
    SearchEngineSettings *selectedEngine();
    const SearchEngineSettings *selectedEngine() const;
    bool editEngine(SearchEngineSettings *engine, bool adding);
    void setEngineEnabled(const QString &id, bool enabled);

    SearchSettings m_settings;
    bool m_rebuilding = false;
    QComboBox *m_defaultEngine = nullptr;
    QListWidget *m_engineList = nullptr;
    QLabel *m_details = nullptr;
    QPushButton *m_editButton = nullptr;
    QPushButton *m_removeButton = nullptr;
};
