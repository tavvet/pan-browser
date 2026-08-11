#include "TrustRulesSettingsPage.h"

#include "CertificateDetailsDialog.h"
#include "TrustRuleEditor.h"

#include <QCoreApplication>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>

#include <utility>

TrustRulesSettingsPage::TrustRulesSettingsPage(
    const QString &configurationPath,
    QWidget *parent
)
    : QWidget(parent)
    , m_configurationPath(configurationPath)
    , m_certificateRepository(configurationPath)
{
    createInterface();
}

bool TrustRulesSettingsPage::load(QString *error)
{
    if (!m_settings.load(m_configurationPath, error))
        return false;

    rebuildRuleList();
    if (m_ruleList->count() > 0)
        m_ruleList->setCurrentRow(0);
    else
        setEditorEnabled(false);
    return true;
}

bool TrustRulesSettingsPage::validate(QString *error)
{
    storeCurrentRule();
    return m_settings.validate(m_configurationPath, error);
}

bool TrustRulesSettingsPage::save(QString *error)
{
    storeCurrentRule();
    return m_settings.save(m_configurationPath, error);
}

QStringList TrustRulesSettingsPage::finalizeSave()
{
    return m_certificateRepository.finalize(m_settings.rules());
}

QStringList TrustRulesSettingsPage::rollbackPendingCertificates()
{
    return m_certificateRepository.rollback();
}

void TrustRulesSettingsPage::createInterface()
{
    setObjectName(QStringLiteral("trustRulesSettingsPage"));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto *title = new QLabel(QCoreApplication::translate("TrustRulesDialog", "Trust Rules"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    rootLayout->addWidget(title);

    auto *subtitle = new QLabel(
        QCoreApplication::translate(
            "TrustRulesDialog",
            "Control which domains may use certificate authorities added to PanBrowser."
        ),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    rootLayout->addWidget(subtitle);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("rulesSplitter"));
    splitter->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter, 1);

    auto *sidebar = new QWidget(splitter);
    sidebar->setObjectName(QStringLiteral("rulesSidebar"));
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 12, 0);
    sidebarLayout->setSpacing(10);

    auto *rulesLabel = new QLabel(
        QCoreApplication::translate("TrustRulesDialog", "DOMAIN RULES"),
        sidebar
    );
    rulesLabel->setObjectName(QStringLiteral("sectionLabel"));
    sidebarLayout->addWidget(rulesLabel);

    m_ruleList = new QListWidget(sidebar);
    m_ruleList->setObjectName(QStringLiteral("ruleList"));
    m_ruleList->setSpacing(2);
    sidebarLayout->addWidget(m_ruleList, 1);

    auto *ruleButtons = new QHBoxLayout();
    auto *addButton = new QPushButton(
        QCoreApplication::translate("TrustRulesDialog", "Add rule"),
        sidebar
    );
    addButton->setObjectName(QStringLiteral("secondaryButton"));
    m_deleteRule = new QPushButton(
        QCoreApplication::translate("TrustRulesDialog", "Remove rule"),
        sidebar
    );
    m_deleteRule->setObjectName(QStringLiteral("dangerButton"));
    ruleButtons->addWidget(addButton);
    ruleButtons->addWidget(m_deleteRule);
    sidebarLayout->addLayout(ruleButtons);

    auto *editorScroll = new QScrollArea(splitter);
    editorScroll->setObjectName(QStringLiteral("ruleEditorScroll"));
    editorScroll->setWidgetResizable(true);
    editorScroll->setFrameShape(QFrame::NoFrame);
    editorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_editor = new TrustRuleEditor(&m_certificateRepository);

    editorScroll->setWidget(m_editor);

    splitter->addWidget(sidebar);
    splitter->addWidget(editorScroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 660});

    connect(m_ruleList, &QListWidget::currentRowChanged, this, &TrustRulesSettingsPage::selectRule);
    connect(addButton, &QPushButton::clicked, this, &TrustRulesSettingsPage::addRule);
    connect(m_deleteRule, &QPushButton::clicked, this, &TrustRulesSettingsPage::removeRule);
    connect(
        m_editor,
        &TrustRuleEditor::importCertificatesRequested,
        this,
        &TrustRulesSettingsPage::importCertificates
    );
    connect(
        m_editor,
        &TrustRuleEditor::certificateDetailsRequested,
        this,
        &TrustRulesSettingsPage::showCertificateDetails
    );
    connect(
        m_editor,
        &TrustRuleEditor::removeCertificateRequested,
        this,
        &TrustRulesSettingsPage::removeCertificate
    );
    connect(
        m_editor,
        &TrustRuleEditor::domainTestRequested,
        this,
        &TrustRulesSettingsPage::testDomain
    );
    connect(m_editor, &TrustRuleEditor::edited, this, [this] {
        if (!m_rebuildingRuleList)
            storeCurrentRule();
    });
}

void TrustRulesSettingsPage::rebuildRuleList()
{
    m_rebuildingRuleList = true;
    m_ruleList->clear();
    for (const TrustRuleSettings &rule : std::as_const(m_settings.rules())) {
        auto *item = new QListWidgetItem(m_ruleList);
        item->setSizeHint(QSize(220, 52));
        item->setIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
        const QString summary = rule.enabled
            ? (rule.domains.isEmpty()
                ? QCoreApplication::translate("TrustRulesDialog", "No domains")
                : rule.domains.first())
            : QCoreApplication::translate("TrustRulesDialog", "Disabled");
        item->setText(
            QStringLiteral("%1\n%2").arg(
                rule.name.isEmpty()
                    ? QCoreApplication::translate("TrustRulesDialog", "Untitled rule")
                    : rule.name,
                summary
            )
        );
        item->setToolTip(rule.domains.join(QLatin1Char('\n')));
    }
    m_rebuildingRuleList = false;
    m_currentRule = -1;
    m_deleteRule->setEnabled(!m_settings.rules().isEmpty());
}

void TrustRulesSettingsPage::selectRule(int row)
{
    if (m_rebuildingRuleList)
        return;
    storeCurrentRule();
    m_currentRule = row;
    loadCurrentRule();
}

void TrustRulesSettingsPage::loadCurrentRule()
{
    const bool valid = m_currentRule >= 0 && m_currentRule < m_settings.rules().size();
    setEditorEnabled(valid);
    if (!valid)
        return;

    const TrustRuleSettings &rule = m_settings.rules().at(m_currentRule);
    m_editor->setRule(&rule);
}

void TrustRulesSettingsPage::storeCurrentRule()
{
    if (m_rebuildingRuleList
        || m_currentRule < 0
        || m_currentRule >= m_settings.rules().size()) {
        return;
    }

    TrustRuleSettings &rule = m_settings.rules()[m_currentRule];
    m_editor->applyTo(&rule);
    updateCurrentRuleItem();
}

void TrustRulesSettingsPage::updateCurrentRuleItem()
{
    if (m_currentRule < 0 || m_currentRule >= m_ruleList->count())
        return;

    const TrustRuleSettings &rule = m_settings.rules().at(m_currentRule);
    const QString summary = rule.enabled
        ? (rule.domains.isEmpty()
            ? QCoreApplication::translate("TrustRulesDialog", "No domains")
            : rule.domains.first())
        : QCoreApplication::translate("TrustRulesDialog", "Disabled");
    QListWidgetItem *item = m_ruleList->item(m_currentRule);
    item->setText(
        QStringLiteral("%1\n%2").arg(
            rule.name.isEmpty()
                ? QCoreApplication::translate("TrustRulesDialog", "Untitled rule")
                : rule.name,
            summary
        )
    );
    item->setToolTip(rule.domains.join(QLatin1Char('\n')));
}

void TrustRulesSettingsPage::setEditorEnabled(bool enabled)
{
    m_deleteRule->setEnabled(enabled);
    if (!enabled)
        m_editor->setRule(nullptr);
}

void TrustRulesSettingsPage::refreshCertificates()
{
    if (m_currentRule >= 0 && m_currentRule < m_settings.rules().size())
        m_editor->refreshCertificates(m_settings.rules().at(m_currentRule).anchors);
}

void TrustRulesSettingsPage::addRule()
{
    storeCurrentRule();

    QSet<QString> names;
    for (const TrustRuleSettings &rule : std::as_const(m_settings.rules()))
        names.insert(rule.name.toCaseFolded());

    QString name = QCoreApplication::translate("TrustRulesDialog", "New rule");
    for (int suffix = 2; names.contains(name.toCaseFolded()); ++suffix)
        name = QCoreApplication::translate("TrustRulesDialog", "New rule %1").arg(suffix);

    TrustRuleSettings rule;
    rule.name = name;
    rule.enabled = false;
    rule.mode = TrustMode::SystemPlusCustom;
    m_settings.rules().append(rule);
    rebuildRuleList();
    m_ruleList->setCurrentRow(m_ruleList->count() - 1);
    m_editor->focusName();
}

void TrustRulesSettingsPage::removeRule()
{
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;

    const QString name = m_settings.rules().at(m_currentRule).name;
    if (QMessageBox::question(
            this,
            QCoreApplication::translate("TrustRulesDialog", "Remove trust rule"),
            QCoreApplication::translate(
                "TrustRulesDialog",
                "Remove “%1”? The certificate files will be kept."
            ).arg(name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }

    m_settings.rules().removeAt(m_currentRule);
    const int nextRow = qMin(
        m_currentRule,
        static_cast<int>(m_settings.rules().size()) - 1
    );
    rebuildRuleList();
    if (!m_settings.rules().isEmpty())
        m_ruleList->setCurrentRow(qMax(0, nextRow));
    else
        setEditorEnabled(false);
}

void TrustRulesSettingsPage::importCertificates()
{
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;

    const QStringList sourcePaths = QFileDialog::getOpenFileNames(
        this,
        QCoreApplication::translate("TrustRulesDialog", "Import CA certificates"),
        QString(),
        QCoreApplication::translate(
            "TrustRulesDialog",
            "Certificates (*.cer *.crt *.der *.pem);;All files (*)"
        )
    );
    if (sourcePaths.isEmpty())
        return;

    const TrustCertificateImportResult result = m_certificateRepository.importFiles(
        sourcePaths
    );
    if (!result.certificateDirectoryError.isEmpty()) {
        QMessageBox::critical(
            this,
            QCoreApplication::translate("TrustRulesDialog", "Import failed"),
            QCoreApplication::translate("TrustRulesDialog", "Cannot create %1")
                .arg(result.certificateDirectoryError)
        );
        return;
    }

    QStringList failures;
    TrustRuleSettings &rule = m_settings.rules()[m_currentRule];
    for (const QString &anchor : result.anchors) {
        if (!rule.anchors.contains(anchor))
            rule.anchors.append(anchor);
    }
    for (const TrustCertificateImportFailure &failure : result.failures) {
        failures.append(
            failure.reason == TrustCertificateImportFailureReason::InvalidCertificate
                ? QCoreApplication::translate(
                    "TrustRulesDialog",
                    "%1 is not a readable certificate"
                ).arg(failure.sourcePath)
                : QCoreApplication::translate("TrustRulesDialog", "Cannot copy %1")
                    .arg(failure.sourcePath)
        );
    }

    refreshCertificates();
    if (!failures.isEmpty()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("TrustRulesDialog", "Some certificates were not imported"),
            failures.join(QLatin1Char('\n'))
        );
    }
}

void TrustRulesSettingsPage::removeCertificate(int row)
{
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;
    if (row < 0 || row >= m_settings.rules().at(m_currentRule).anchors.size())
        return;

    m_settings.rules()[m_currentRule].anchors.removeAt(row);
    refreshCertificates();
}

void TrustRulesSettingsPage::showCertificateDetails(const QString &configuredPath)
{
    TrustCertificateInfo info = m_certificateRepository.inspect(configuredPath);
    if (!info.isReadable()) {
        QMessageBox::warning(
            this,
            QCoreApplication::translate("TrustRulesDialog", "Certificate unavailable"),
            QCoreApplication::translate(
                "TrustRulesDialog",
                "Cannot read certificate file:\n%1"
            ).arg(info.absolutePath)
        );
        return;
    }

    CertificateDetailsDialog details(std::move(info), this);
    details.exec();
}

void TrustRulesSettingsPage::testDomain(const QString &input)
{
    storeCurrentRule();
    QString host = input.trimmed();
    const QUrl asUrl = QUrl::fromUserInput(host);
    if (!asUrl.host().isEmpty())
        host = asUrl.host();

    QString match;
    for (const TrustRuleSettings &rule : std::as_const(m_settings.rules())) {
        if (!rule.enabled)
            continue;
        for (const QString &domain : rule.domains) {
            if (DomainPattern::parse(domain).matches(host)) {
                match = rule.name;
                break;
            }
        }
        if (!match.isEmpty())
            break;
    }

    m_editor->showDomainTestResult(host, match);
}
