#pragma once

#include "TrustCertificateRepository.h"
#include "TrustSettings.h"

#include <QWidget>

class QListWidget;
class QPushButton;
class TrustRuleEditor;

class TrustRulesSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit TrustRulesSettingsPage(
        const QString &configurationPath,
        QWidget *parent = nullptr
    );
    bool load(QString *error = nullptr);
    bool validate(QString *error = nullptr);
    bool save(QString *error = nullptr);
    [[nodiscard]] QStringList finalizeSave();
    [[nodiscard]] QStringList rollbackPendingCertificates();

private:
    void createInterface();
    void rebuildRuleList();
    void selectRule(int row);
    void loadCurrentRule();
    void storeCurrentRule();
    void updateCurrentRuleItem();
    void setEditorEnabled(bool enabled);
    void refreshCertificates();
    void addRule();
    void removeRule();
    void importCertificates();
    void removeCertificate(int row);
    void showCertificateDetails(const QString &configuredPath);
    void testDomain(const QString &input);

    QString m_configurationPath;
    TrustCertificateRepository m_certificateRepository;
    TrustSettings m_settings;
    int m_currentRule = -1;
    bool m_rebuildingRuleList = false;

    QListWidget *m_ruleList = nullptr;
    TrustRuleEditor *m_editor = nullptr;
    QPushButton *m_deleteRule = nullptr;
};
