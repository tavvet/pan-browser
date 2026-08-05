#pragma once

#include <QWidget>

class HistoryStore;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;

class HistorySettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit HistorySettingsPage(
        HistoryStore *store,
        bool saveHistoryEnabled,
        QWidget *parent = nullptr
    );

    [[nodiscard]] bool saveHistoryEnabled() const;

private:
    void reload();
    void updateActions();
    void removeSelected();
    void clearAll();

    HistoryStore *m_store = nullptr;
    QCheckBox *m_saveHistory = nullptr;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_visits = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_remove = nullptr;
    QTimer *m_filterTimer = nullptr;
};
