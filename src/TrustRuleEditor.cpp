#include "TrustRuleEditor.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QStringList splitDomains(const QString &text)
{
    QStringList result;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        const QString domain = line.trimmed();
        if (!domain.isEmpty())
            result.append(domain);
    }
    return result;
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

TrustRuleEditor::TrustRuleEditor(
    TrustCertificateRepository *certificateRepository,
    QWidget *parent
)
    : QWidget(parent)
    , m_certificateRepository(certificateRepository)
{
    Q_ASSERT(m_certificateRepository);
    setObjectName(QStringLiteral("ruleEditor"));

    auto *editorLayout = new QVBoxLayout(this);
    editorLayout->setContentsMargins(16, 0, 0, 0);
    editorLayout->setSpacing(12);

    auto *detailsLabel = new QLabel(
        QCoreApplication::translate("TrustRulesDialog", "RULE DETAILS"),
        this
    );
    detailsLabel->setObjectName(QStringLiteral("sectionLabel"));
    editorLayout->addWidget(detailsLabel);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_name = new QLineEdit(this);
    m_name->setPlaceholderText(QCoreApplication::translate("TrustRulesDialog", "Example Bank"));
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Name"), m_name);

    m_enabled = new QCheckBox(
        QCoreApplication::translate("TrustRulesDialog", "Rule is enabled"),
        this
    );
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Status"), m_enabled);

    m_mode = new QComboBox(this);
    m_mode->addItem(
        QCoreApplication::translate("TrustRulesDialog", "System trust only"),
        static_cast<int>(TrustMode::SystemOnly)
    );
    m_mode->addItem(
        QCoreApplication::translate("TrustRulesDialog", "System trust or added certificates"),
        static_cast<int>(TrustMode::SystemPlusCustom)
    );
    m_mode->addItem(
        QCoreApplication::translate("TrustRulesDialog", "Added certificates only"),
        static_cast<int>(TrustMode::CustomOnly)
    );
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Trust mode"), m_mode);
    editorLayout->addLayout(form);

    m_modeDescription = new QLabel(this);
    m_modeDescription->setObjectName(QStringLiteral("fieldHint"));
    m_modeDescription->setWordWrap(true);
    editorLayout->addWidget(m_modeDescription);

    auto *domainsLabel = new QLabel(
        QCoreApplication::translate("TrustRulesDialog", "DOMAINS"),
        this
    );
    domainsLabel->setObjectName(QStringLiteral("sectionLabel"));
    editorLayout->addWidget(domainsLabel);

    m_domains = new QPlainTextEdit(this);
    m_domains->setObjectName(QStringLiteral("domainsEditor"));
    m_domains->setPlaceholderText(QStringLiteral("example.ru\n*.example.ru"));
    m_domains->setMaximumHeight(105);
    editorLayout->addWidget(m_domains);

    auto *domainsHint = new QLabel(
        QCoreApplication::translate(
            "TrustRulesDialog",
            "One domain per line. Wildcards match subdomains only; include the base domain separately."
        ),
        this
    );
    domainsHint->setObjectName(QStringLiteral("fieldHint"));
    domainsHint->setWordWrap(true);
    editorLayout->addWidget(domainsHint);

    auto *certificatesLabel = new QLabel(
        QCoreApplication::translate("TrustRulesDialog", "ADDED CERTIFICATES"),
        this
    );
    certificatesLabel->setObjectName(QStringLiteral("sectionLabel"));
    editorLayout->addWidget(certificatesLabel);

    m_certificates = new QListWidget(this);
    m_certificates->setObjectName(QStringLiteral("certificateList"));
    m_certificates->setMinimumHeight(105);
    editorLayout->addWidget(m_certificates, 1);

    auto *certificateButtons = new QHBoxLayout();
    auto *importButton = new QPushButton(
        QCoreApplication::translate("TrustRulesDialog", "Add certificates…"),
        this
    );
    importButton->setObjectName(QStringLiteral("secondaryButton"));
    m_viewCertificate = new QPushButton(
        QCoreApplication::translate("TrustRulesDialog", "View details…"),
        this
    );
    m_viewCertificate->setObjectName(QStringLiteral("secondaryButton"));
    m_removeCertificate = new QPushButton(
        QCoreApplication::translate("TrustRulesDialog", "Remove from rule"),
        this
    );
    m_removeCertificate->setObjectName(QStringLiteral("secondaryButton"));
    certificateButtons->addWidget(importButton);
    certificateButtons->addWidget(m_viewCertificate);
    certificateButtons->addWidget(m_removeCertificate);
    certificateButtons->addStretch();
    editorLayout->addLayout(certificateButtons);

    auto *testLayout = new QHBoxLayout();
    m_testDomain = new QLineEdit(this);
    m_testDomain->setPlaceholderText(
        QCoreApplication::translate("TrustRulesDialog", "Enter a domain to see which rule applies")
    );
    auto *testButton = new QPushButton(
        QCoreApplication::translate("TrustRulesDialog", "Check domain"),
        this
    );
    testButton->setObjectName(QStringLiteral("secondaryButton"));
    testLayout->addWidget(m_testDomain, 1);
    testLayout->addWidget(testButton);
    editorLayout->addLayout(testLayout);

    m_testResult = new QLabel(this);
    m_testResult->setObjectName(QStringLiteral("testResult"));
    m_testResult->hide();
    editorLayout->addWidget(m_testResult);

    connect(
        importButton,
        &QPushButton::clicked,
        this,
        &TrustRuleEditor::importCertificatesRequested
    );
    connect(
        m_viewCertificate,
        &QPushButton::clicked,
        this,
        &TrustRuleEditor::emitCertificateDetailsRequest
    );
    connect(m_certificates, &QListWidget::itemDoubleClicked, this, [this] {
        emitCertificateDetailsRequest();
    });
    connect(m_removeCertificate, &QPushButton::clicked, this, [this] {
        emit removeCertificateRequested(m_certificates->currentRow());
    });
    connect(testButton, &QPushButton::clicked, this, &TrustRuleEditor::emitDomainTestRequest);
    connect(m_testDomain, &QLineEdit::returnPressed, this, &TrustRuleEditor::emitDomainTestRequest);
    connect(m_certificates, &QListWidget::currentRowChanged, this, [this](int row) {
        m_viewCertificate->setEnabled(row >= 0 && isEnabled());
        m_removeCertificate->setEnabled(row >= 0 && isEnabled());
    });

    const auto emitEdit = [this] {
        if (!m_loading)
            emit edited();
    };
    connect(m_name, &QLineEdit::textChanged, this, emitEdit);
    connect(m_enabled, &QCheckBox::toggled, this, emitEdit);
    connect(m_domains, &QPlainTextEdit::textChanged, this, emitEdit);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this, emitEdit](int) {
        updateModeDescription();
        emitEdit();
    });

    setRule(nullptr);
}

void TrustRuleEditor::setRule(const TrustRuleSettings *rule)
{
    m_loading = true;
    setEnabled(rule != nullptr);
    m_name->setText(rule ? rule->name : QString());
    m_enabled->setChecked(rule && rule->enabled);
    m_mode->setCurrentIndex(
        rule ? m_mode->findData(static_cast<int>(rule->mode)) : 0
    );
    m_domains->setPlainText(rule ? rule->domains.join(QLatin1Char('\n')) : QString());
    m_testDomain->clear();
    m_testResult->hide();
    refreshCertificates(rule ? rule->anchors : QStringList());
    m_loading = false;
    updateModeDescription();
}

void TrustRuleEditor::applyTo(TrustRuleSettings *rule) const
{
    Q_ASSERT(rule);
    rule->name = m_name->text().trimmed();
    rule->enabled = m_enabled->isChecked();
    rule->mode = static_cast<TrustMode>(m_mode->currentData().toInt());
    rule->domains = splitDomains(m_domains->toPlainText());
}

void TrustRuleEditor::refreshCertificates(const QStringList &anchors)
{
    m_certificates->clear();
    for (const QString &configuredPath : anchors) {
        const TrustCertificateInfo info = m_certificateRepository->inspect(configuredPath);
        auto *item = new QListWidgetItem(m_certificates);
        item->setData(Qt::UserRole, configuredPath);
        item->setSizeHint(QSize(0, 48));
        if (!info.isReadable()) {
            item->setText(
                QCoreApplication::translate(
                    "TrustRulesDialog",
                    "%1\nMissing or invalid certificate"
                ).arg(
                    QFileInfo(configuredPath).fileName()
                )
            );
            item->setIcon(QIcon(QStringLiteral(":/assets/icons/triangle-alert.svg")));
            item->setToolTip(info.absolutePath);
            continue;
        }

        const QSslCertificate &certificate = info.certificates.first();
        const QString fingerprint = TrustCertificateRepository::fingerprint(certificate);
        const QString shortFingerprint = fingerprint.size() > 23
            ? fingerprint.left(23) + QChar(0x2026)
            : fingerprint;
        const QString extra = info.certificates.size() > 1
            ? QCoreApplication::translate(
                "TrustRulesDialog",
                " · %n certificates",
                nullptr,
                static_cast<int>(info.certificates.size())
            )
            : QString();
        item->setText(
            QStringLiteral("%1\n%2 · SHA-256 %3%4").arg(
                QFileInfo(configuredPath).fileName(),
                TrustCertificateRepository::displayName(certificate),
                shortFingerprint,
                extra
            )
        );
        item->setIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
        item->setToolTip(
            QCoreApplication::translate(
                "TrustRulesDialog",
                "Path: %1\nSubject: %2\nIssuer: %3\nValid until: %4\nSHA-256: %5"
            ).arg(
                info.absolutePath,
                TrustCertificateRepository::displayName(certificate),
                certificate.issuerInfo(QSslCertificate::CommonName).join(QStringLiteral(", ")),
                certificate.expiryDate().toLocalTime().toString(Qt::ISODate),
                fingerprint
            )
        );
    }
    m_removeCertificate->setEnabled(false);
    m_viewCertificate->setEnabled(false);
}

void TrustRuleEditor::focusName()
{
    m_name->selectAll();
    m_name->setFocus();
}

void TrustRuleEditor::showDomainTestResult(
    const QString &host,
    const QString &matchingRule
)
{
    m_testResult->setProperty(
        "state",
        matchingRule.isEmpty() ? QStringLiteral("none") : QStringLiteral("match")
    );
    m_testResult->setText(
        matchingRule.isEmpty()
            ? QCoreApplication::translate(
                "TrustRulesDialog",
                "No enabled rule matches %1"
            ).arg(host)
            : QCoreApplication::translate(
                "TrustRulesDialog",
                "%1 matches rule “%2”"
            ).arg(host, matchingRule)
    );
    m_testResult->show();
    repolish(m_testResult);
}

void TrustRuleEditor::updateModeDescription()
{
    const TrustMode mode = static_cast<TrustMode>(m_mode->currentData().toInt());
    switch (mode) {
    case TrustMode::SystemOnly:
        m_modeDescription->setText(QCoreApplication::translate(
            "TrustRulesDialog",
            "Use Chromium’s normal certificate validation. Certificates added below are ignored."
        ));
        break;
    case TrustMode::SystemPlusCustom:
        m_modeDescription->setText(QCoreApplication::translate(
            "TrustRulesDialog",
            "Use normal system trust. If the certificate authority is unknown, also try the certificates added below."
        ));
        break;
    case TrustMode::CustomOnly:
        m_modeDescription->setText(QCoreApplication::translate(
            "TrustRulesDialog",
            "For an unknown certificate authority, accept only chains ending at a certificate added below. Sites Chromium already trusts are unaffected."
        ));
        break;
    }
}

void TrustRuleEditor::emitCertificateDetailsRequest()
{
    const QListWidgetItem *item = m_certificates->currentItem();
    if (item)
        emit certificateDetailsRequested(item->data(Qt::UserRole).toString());
}

void TrustRuleEditor::emitDomainTestRequest()
{
    emit domainTestRequested(m_testDomain->text().trimmed());
}
