#pragma once

#include "TrustCertificateRepository.h"
#include "TrustSettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

class TrustRuleEditor final : public QWidget {
    Q_OBJECT

public:
    explicit TrustRuleEditor(
        TrustCertificateRepository *certificateRepository,
        QWidget *parent = nullptr
    );

    void setRule(const TrustRuleSettings *rule);
    void applyTo(TrustRuleSettings *rule) const;
    void refreshCertificates(const QStringList &anchors);
    void focusName();
    void showDomainTestResult(const QString &host, const QString &matchingRule);

signals:
    void edited();
    void importCertificatesRequested();
    void removeCertificateRequested(int row);
    void certificateDetailsRequested(const QString &configuredPath);
    void domainTestRequested(const QString &input);

private:
    void updateModeDescription();
    void emitCertificateDetailsRequest();
    void emitDomainTestRequest();

    TrustCertificateRepository *m_certificateRepository = nullptr;
    bool m_loading = false;

    QLineEdit *m_name = nullptr;
    QCheckBox *m_enabled = nullptr;
    QComboBox *m_mode = nullptr;
    QLabel *m_modeDescription = nullptr;
    QPlainTextEdit *m_domains = nullptr;
    QListWidget *m_certificates = nullptr;
    QPushButton *m_removeCertificate = nullptr;
    QPushButton *m_viewCertificate = nullptr;
    QLineEdit *m_testDomain = nullptr;
    QLabel *m_testResult = nullptr;
};
