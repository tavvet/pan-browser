#pragma once

#include "WebAppShortcutManager.h"

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class WebAppStore;

class WebAppsSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit WebAppsSettingsPage(WebAppStore *store, QWidget *parent = nullptr);

signals:
    void openRequested(const QString &id);

private:
    void rebuildList(const QString &selectedId = QString());
    void updateSelection();
    void removeSelected();
    void createOrRepairShortcut();
    void removeShortcut();

    WebAppStore *m_store = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_details = nullptr;
    QPushButton *m_openButton = nullptr;
    QPushButton *m_removeButton = nullptr;
    QPushButton *m_createShortcutButton = nullptr;
    QPushButton *m_removeShortcutButton = nullptr;
    WebAppShortcutManager m_shortcutManager;
};
