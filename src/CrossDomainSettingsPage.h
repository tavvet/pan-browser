#pragma once

#include "CrossDomainSettings.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QFrame;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class CrossDomainSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit CrossDomainSettingsPage(
        const CrossDomainSettings &settings,
        QWidget *parent = nullptr
    );

    [[nodiscard]] CrossDomainSettings settings() const;
    bool validate(QString *error = nullptr) const;

private:
    void rebuildRules();
    void updateUi();
    void addRule();
    void removeSelectedRule();
    void clearRules();
    void useRecommendedPresets();
    void showPresetLists();

    CrossDomainSettings m_initialSettings;
    QList<CrossDomainRule> m_rules;
    QCheckBox *m_enabled = nullptr;
    QFrame *m_exceptionsCard = nullptr;
    QFrame *m_blockedCard = nullptr;
    QFrame *m_rulesCard = nullptr;
    QPlainTextEdit *m_globalExceptions = nullptr;
    QPlainTextEdit *m_globalBlockedHosts = nullptr;
    QListWidget *m_rulesList = nullptr;
    QPushButton *m_addRule = nullptr;
    QPushButton *m_removeRule = nullptr;
    QPushButton *m_clearRules = nullptr;
    QPushButton *m_useRecommendedPresets = nullptr;
    QPushButton *m_viewPresets = nullptr;
    QHash<QString, QCheckBox *> m_presetChecks;
};
