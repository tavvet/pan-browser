#pragma once

#include "TrustSettings.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QWidget;

class TrustRulesDialog final : public QDialog {
    Q_OBJECT

public:
    explicit TrustRulesDialog(const QString &configurationPath, QWidget *parent = nullptr);
    ~TrustRulesDialog() override;

    bool load(QString *error = nullptr);

private:
    void createInterface();
    void rebuildRuleList();
    void selectRule(int row);
    void loadCurrentRule();
    void storeCurrentRule();
    void updateCurrentRuleItem();
    void setEditorEnabled(bool enabled);
    void updateModeDescription();
    void refreshCertificates();
    void addRule();
    void removeRule();
    void importCertificates();
    void removeCertificate();
    void showCertificateDetails();
    void testDomain();
    void saveAndClose();
    void cleanupPendingCertificates(bool keepReferenced);

    QString m_configurationPath;
    TrustSettings m_settings;
    int m_currentRule = -1;
    bool m_loadingEditor = false;
    bool m_saved = false;
    QStringList m_pendingCertificateFiles;

    QListWidget *m_ruleList = nullptr;
    QWidget *m_editor = nullptr;
    QLineEdit *m_name = nullptr;
    QCheckBox *m_enabled = nullptr;
    QComboBox *m_mode = nullptr;
    QLabel *m_modeDescription = nullptr;
    QPlainTextEdit *m_domains = nullptr;
    QListWidget *m_certificates = nullptr;
    QPushButton *m_deleteRule = nullptr;
    QPushButton *m_removeCertificate = nullptr;
    QPushButton *m_viewCertificate = nullptr;
    QLineEdit *m_testDomain = nullptr;
    QLabel *m_testResult = nullptr;
};
