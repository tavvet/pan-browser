#include "TrustRulesDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QSslCertificate>
#include <QSplitter>
#include <QStyle>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

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

QList<QSslCertificate> readCertificates(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray data = file.readAll();
    QList<QSslCertificate> certificates = QSslCertificate::fromData(data, QSsl::Pem);
    if (certificates.isEmpty())
        certificates = QSslCertificate::fromData(data, QSsl::Der);
    return certificates;
}

QString certificateFingerprint(const QSslCertificate &certificate)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(certificate.toDer(), QCryptographicHash::Sha256)
            .toHex(':')
            .toUpper()
    );
}

QString certificateName(const QSslCertificate &certificate)
{
    QStringList names = certificate.subjectInfo(QSslCertificate::CommonName);
    if (names.isEmpty())
        names = certificate.subjectInfo(QSslCertificate::Organization);
    return names.isEmpty()
        ? QCoreApplication::translate("TrustRulesDialog", "Unnamed certificate")
        : names.join(QStringLiteral(", "));
}

QByteArray fileContents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString uniqueDestination(const QString &sourcePath, const QDir &directory)
{
    const QFileInfo source(sourcePath);
    const QString baseName = source.completeBaseName().isEmpty()
        ? QStringLiteral("certificate")
        : source.completeBaseName();
    const QString suffix = source.suffix();

    for (int index = 0; ; ++index) {
        const QString numberedName = index == 0
            ? baseName
            : baseName + QStringLiteral("-%1").arg(index);
        const QString fileName = suffix.isEmpty()
            ? numberedName
            : numberedName + QLatin1Char('.') + suffix;
        const QString candidate = directory.filePath(fileName);
        if (!QFile::exists(candidate) || fileContents(candidate) == fileContents(sourcePath))
            return candidate;
    }
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

TrustRulesDialog::TrustRulesDialog(
    const QString &configurationPath,
    QWidget *parent,
    bool embedded
)
    : QDialog(parent)
    , m_configurationPath(configurationPath)
{
    if (embedded)
        setWindowFlags(Qt::Widget);
    createInterface(embedded);
}

TrustRulesDialog::~TrustRulesDialog()
{
    if (!m_saved)
        cleanupPendingCertificates(false);
}

bool TrustRulesDialog::load(QString *error)
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

bool TrustRulesDialog::validate(QString *error)
{
    storeCurrentRule();
    return m_settings.validate(m_configurationPath, error);
}

bool TrustRulesDialog::save(QString *error)
{
    storeCurrentRule();
    return m_settings.save(m_configurationPath, error);
}

void TrustRulesDialog::finalizeSave()
{
    cleanupPendingCertificates(true);
    m_saved = true;
}

void TrustRulesDialog::createInterface(bool embedded)
{
    setObjectName(QStringLiteral("trustRulesDialog"));
    setWindowTitle(tr("Trust Rules"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
    resize(960, 680);
    setMinimumSize(820, 580);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(22, 20, 22, 18);
    rootLayout->setSpacing(14);

    auto *title = new QLabel(tr("Trust Rules"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    rootLayout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Control which domains may use certificate authorities added to PanBrowser."),
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

    auto *rulesLabel = new QLabel(tr("DOMAIN RULES"), sidebar);
    rulesLabel->setObjectName(QStringLiteral("sectionLabel"));
    sidebarLayout->addWidget(rulesLabel);

    m_ruleList = new QListWidget(sidebar);
    m_ruleList->setObjectName(QStringLiteral("ruleList"));
    m_ruleList->setSpacing(2);
    sidebarLayout->addWidget(m_ruleList, 1);

    auto *ruleButtons = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add rule"), sidebar);
    addButton->setObjectName(QStringLiteral("secondaryButton"));
    m_deleteRule = new QPushButton(tr("Remove rule"), sidebar);
    m_deleteRule->setObjectName(QStringLiteral("dangerButton"));
    ruleButtons->addWidget(addButton);
    ruleButtons->addWidget(m_deleteRule);
    sidebarLayout->addLayout(ruleButtons);

    auto *editorScroll = new QScrollArea(splitter);
    editorScroll->setObjectName(QStringLiteral("ruleEditorScroll"));
    editorScroll->setWidgetResizable(true);
    editorScroll->setFrameShape(QFrame::NoFrame);
    editorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_editor = new QWidget();
    m_editor->setObjectName(QStringLiteral("ruleEditor"));
    auto *editorLayout = new QVBoxLayout(m_editor);
    editorLayout->setContentsMargins(16, 0, 0, 0);
    editorLayout->setSpacing(12);

    auto *detailsLabel = new QLabel(tr("RULE DETAILS"), m_editor);
    detailsLabel->setObjectName(QStringLiteral("sectionLabel"));
    editorLayout->addWidget(detailsLabel);

    auto *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(16);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_name = new QLineEdit(m_editor);
    m_name->setPlaceholderText(tr("Example Bank"));
    form->addRow(tr("Name"), m_name);

    m_enabled = new QCheckBox(tr("Rule is enabled"), m_editor);
    form->addRow(tr("Status"), m_enabled);

    m_mode = new QComboBox(m_editor);
    m_mode->addItem(tr("System trust only"), static_cast<int>(TrustMode::SystemOnly));
    m_mode->addItem(
        tr("System trust or added certificates"),
        static_cast<int>(TrustMode::SystemPlusCustom)
    );
    m_mode->addItem(
        tr("Added certificates only"),
        static_cast<int>(TrustMode::CustomOnly)
    );
    form->addRow(tr("Trust mode"), m_mode);
    editorLayout->addLayout(form);

    m_modeDescription = new QLabel(m_editor);
    m_modeDescription->setObjectName(QStringLiteral("fieldHint"));
    m_modeDescription->setWordWrap(true);
    editorLayout->addWidget(m_modeDescription);

    auto *domainsLabel = new QLabel(tr("DOMAINS"), m_editor);
    domainsLabel->setObjectName(QStringLiteral("sectionLabel"));
    editorLayout->addWidget(domainsLabel);

    m_domains = new QPlainTextEdit(m_editor);
    m_domains->setObjectName(QStringLiteral("domainsEditor"));
    m_domains->setPlaceholderText(QStringLiteral("example.ru\n*.example.ru"));
    m_domains->setMaximumHeight(105);
    editorLayout->addWidget(m_domains);

    auto *domainsHint = new QLabel(
        tr("One domain per line. Wildcards match subdomains only; include the base domain separately."),
        m_editor
    );
    domainsHint->setObjectName(QStringLiteral("fieldHint"));
    domainsHint->setWordWrap(true);
    editorLayout->addWidget(domainsHint);

    auto *certificatesLabel = new QLabel(tr("ADDED CERTIFICATES"), m_editor);
    certificatesLabel->setObjectName(QStringLiteral("sectionLabel"));
    editorLayout->addWidget(certificatesLabel);

    m_certificates = new QListWidget(m_editor);
    m_certificates->setObjectName(QStringLiteral("certificateList"));
    m_certificates->setMinimumHeight(105);
    editorLayout->addWidget(m_certificates, 1);

    auto *certificateButtons = new QHBoxLayout();
    auto *importButton = new QPushButton(tr("Add certificates…"), m_editor);
    importButton->setObjectName(QStringLiteral("secondaryButton"));
    m_viewCertificate = new QPushButton(tr("View details…"), m_editor);
    m_viewCertificate->setObjectName(QStringLiteral("secondaryButton"));
    m_removeCertificate = new QPushButton(tr("Remove from rule"), m_editor);
    m_removeCertificate->setObjectName(QStringLiteral("secondaryButton"));
    certificateButtons->addWidget(importButton);
    certificateButtons->addWidget(m_viewCertificate);
    certificateButtons->addWidget(m_removeCertificate);
    certificateButtons->addStretch();
    editorLayout->addLayout(certificateButtons);

    auto *testLayout = new QHBoxLayout();
    m_testDomain = new QLineEdit(m_editor);
    m_testDomain->setPlaceholderText(tr("Enter a domain to see which rule applies"));
    auto *testButton = new QPushButton(tr("Check domain"), m_editor);
    testButton->setObjectName(QStringLiteral("secondaryButton"));
    testLayout->addWidget(m_testDomain, 1);
    testLayout->addWidget(testButton);
    editorLayout->addLayout(testLayout);

    m_testResult = new QLabel(m_editor);
    m_testResult->setObjectName(QStringLiteral("testResult"));
    m_testResult->hide();
    editorLayout->addWidget(m_testResult);

    editorScroll->setWidget(m_editor);

    splitter->addWidget(sidebar);
    splitter->addWidget(editorScroll);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({260, 660});

    QDialogButtonBox *buttons = nullptr;
    if (!embedded) {
        buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this
        );
        buttons->button(QDialogButtonBox::Save)->setText(tr("Save rules"));
        rootLayout->addWidget(buttons);
    }

    connect(m_ruleList, &QListWidget::currentRowChanged, this, &TrustRulesDialog::selectRule);
    connect(addButton, &QPushButton::clicked, this, &TrustRulesDialog::addRule);
    connect(m_deleteRule, &QPushButton::clicked, this, &TrustRulesDialog::removeRule);
    connect(importButton, &QPushButton::clicked, this, &TrustRulesDialog::importCertificates);
    connect(m_viewCertificate, &QPushButton::clicked, this, &TrustRulesDialog::showCertificateDetails);
    connect(m_removeCertificate, &QPushButton::clicked, this, &TrustRulesDialog::removeCertificate);
    connect(m_certificates, &QListWidget::itemDoubleClicked, this, [this] {
        showCertificateDetails();
    });
    connect(testButton, &QPushButton::clicked, this, &TrustRulesDialog::testDomain);
    connect(m_testDomain, &QLineEdit::returnPressed, this, &TrustRulesDialog::testDomain);
    if (buttons) {
        connect(buttons, &QDialogButtonBox::accepted, this, &TrustRulesDialog::saveAndClose);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }
    connect(m_certificates, &QListWidget::currentRowChanged, this, [this](int row) {
        m_viewCertificate->setEnabled(row >= 0 && m_currentRule >= 0);
        m_removeCertificate->setEnabled(row >= 0 && m_currentRule >= 0);
    });

    const auto storeChange = [this] {
        if (!m_loadingEditor)
            storeCurrentRule();
    };
    connect(m_name, &QLineEdit::textChanged, this, storeChange);
    connect(m_enabled, &QCheckBox::toggled, this, storeChange);
    connect(m_domains, &QPlainTextEdit::textChanged, this, storeChange);
    connect(m_mode, &QComboBox::currentIndexChanged, this, [this](int) {
        updateModeDescription();
        if (!m_loadingEditor)
            storeCurrentRule();
    });
}

void TrustRulesDialog::rebuildRuleList()
{
    m_loadingEditor = true;
    m_ruleList->clear();
    for (const TrustRuleSettings &rule : std::as_const(m_settings.rules())) {
        auto *item = new QListWidgetItem(m_ruleList);
        item->setSizeHint(QSize(220, 52));
        item->setIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
        const QString summary = rule.enabled
            ? (rule.domains.isEmpty() ? tr("No domains") : rule.domains.first())
            : tr("Disabled");
        item->setText(
            QStringLiteral("%1\n%2").arg(
                rule.name.isEmpty() ? tr("Untitled rule") : rule.name,
                summary
            )
        );
        item->setToolTip(rule.domains.join(QLatin1Char('\n')));
    }
    m_loadingEditor = false;
    m_currentRule = -1;
    m_deleteRule->setEnabled(!m_settings.rules().isEmpty());
}

void TrustRulesDialog::selectRule(int row)
{
    if (m_loadingEditor)
        return;
    storeCurrentRule();
    m_currentRule = row;
    loadCurrentRule();
}

void TrustRulesDialog::loadCurrentRule()
{
    const bool valid = m_currentRule >= 0 && m_currentRule < m_settings.rules().size();
    setEditorEnabled(valid);
    if (!valid)
        return;

    const TrustRuleSettings &rule = m_settings.rules().at(m_currentRule);
    m_loadingEditor = true;
    m_name->setText(rule.name);
    m_enabled->setChecked(rule.enabled);
    m_mode->setCurrentIndex(m_mode->findData(static_cast<int>(rule.mode)));
    m_domains->setPlainText(rule.domains.join(QLatin1Char('\n')));
    m_testDomain->clear();
    m_testResult->hide();
    m_loadingEditor = false;
    updateModeDescription();
    refreshCertificates();
}

void TrustRulesDialog::storeCurrentRule()
{
    if (m_loadingEditor
        || m_currentRule < 0
        || m_currentRule >= m_settings.rules().size()) {
        return;
    }

    TrustRuleSettings &rule = m_settings.rules()[m_currentRule];
    rule.name = m_name->text().trimmed();
    rule.enabled = m_enabled->isChecked();
    rule.mode = static_cast<TrustMode>(m_mode->currentData().toInt());
    rule.domains = splitDomains(m_domains->toPlainText());
    updateCurrentRuleItem();
}

void TrustRulesDialog::updateCurrentRuleItem()
{
    if (m_currentRule < 0 || m_currentRule >= m_ruleList->count())
        return;

    const TrustRuleSettings &rule = m_settings.rules().at(m_currentRule);
    const QString summary = rule.enabled
        ? (rule.domains.isEmpty() ? tr("No domains") : rule.domains.first())
        : tr("Disabled");
    QListWidgetItem *item = m_ruleList->item(m_currentRule);
    item->setText(
        QStringLiteral("%1\n%2").arg(
            rule.name.isEmpty() ? tr("Untitled rule") : rule.name,
            summary
        )
    );
    item->setToolTip(rule.domains.join(QLatin1Char('\n')));
}

void TrustRulesDialog::setEditorEnabled(bool enabled)
{
    m_editor->setEnabled(enabled);
    m_deleteRule->setEnabled(enabled);
    if (!enabled) {
        m_loadingEditor = true;
        m_name->clear();
        m_enabled->setChecked(false);
        m_domains->clear();
        m_certificates->clear();
        m_testResult->hide();
        m_loadingEditor = false;
    }
}

void TrustRulesDialog::updateModeDescription()
{
    const TrustMode mode = static_cast<TrustMode>(m_mode->currentData().toInt());
    switch (mode) {
    case TrustMode::SystemOnly:
        m_modeDescription->setText(
            tr("Use Chromium’s normal certificate validation. Certificates added below are ignored.")
        );
        break;
    case TrustMode::SystemPlusCustom:
        m_modeDescription->setText(
            tr("Use normal system trust. If the certificate authority is unknown, also try the certificates added below.")
        );
        break;
    case TrustMode::CustomOnly:
        m_modeDescription->setText(
            tr("For an unknown certificate authority, accept only chains ending at a certificate added below. Sites Chromium already trusts are unaffected.")
        );
        break;
    }
}

void TrustRulesDialog::refreshCertificates()
{
    m_certificates->clear();
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;

    const QDir baseDirectory = QFileInfo(m_configurationPath).absoluteDir();
    const TrustRuleSettings &rule = m_settings.rules().at(m_currentRule);
    for (const QString &configuredPath : rule.anchors) {
        const QString absolutePath = QFileInfo(configuredPath).isAbsolute()
            ? configuredPath
            : baseDirectory.filePath(configuredPath);
        const QList<QSslCertificate> certificates = readCertificates(absolutePath);

        auto *item = new QListWidgetItem(m_certificates);
        item->setData(Qt::UserRole, configuredPath);
        item->setSizeHint(QSize(0, 48));
        if (certificates.isEmpty()) {
            item->setText(
                tr("%1\nMissing or invalid certificate").arg(
                    QFileInfo(configuredPath).fileName()
                )
            );
            item->setIcon(QIcon(QStringLiteral(":/assets/icons/triangle-alert.svg")));
            item->setToolTip(absolutePath);
            continue;
        }

        const QSslCertificate &certificate = certificates.first();
        const QString fingerprint = certificateFingerprint(certificate);
        const QString shortFingerprint = fingerprint.size() > 23
            ? fingerprint.left(23) + QChar(0x2026)
            : fingerprint;
        const QString extra = certificates.size() > 1
            ? tr(" · %n certificates", nullptr, certificates.size())
            : QString();
        item->setText(
            QStringLiteral("%1\n%2 · SHA-256 %3%4").arg(
                QFileInfo(configuredPath).fileName(),
                certificateName(certificate),
                shortFingerprint,
                extra
            )
        );
        item->setIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
        item->setToolTip(
            tr(
                "Path: %1\nSubject: %2\nIssuer: %3\nValid until: %4\nSHA-256: %5"
            ).arg(
                absolutePath,
                certificateName(certificate),
                certificate.issuerInfo(QSslCertificate::CommonName).join(QStringLiteral(", ")),
                certificate.expiryDate().toLocalTime().toString(Qt::ISODate),
                fingerprint
            )
        );
    }
    m_removeCertificate->setEnabled(m_certificates->currentRow() >= 0);
    m_viewCertificate->setEnabled(m_certificates->currentRow() >= 0);
}

void TrustRulesDialog::addRule()
{
    storeCurrentRule();

    QSet<QString> names;
    for (const TrustRuleSettings &rule : std::as_const(m_settings.rules()))
        names.insert(rule.name.toCaseFolded());

    QString name = tr("New rule");
    for (int suffix = 2; names.contains(name.toCaseFolded()); ++suffix)
        name = tr("New rule %1").arg(suffix);

    TrustRuleSettings rule;
    rule.name = name;
    rule.enabled = false;
    rule.mode = TrustMode::SystemPlusCustom;
    m_settings.rules().append(rule);
    rebuildRuleList();
    m_ruleList->setCurrentRow(m_ruleList->count() - 1);
    m_name->selectAll();
    m_name->setFocus();
}

void TrustRulesDialog::removeRule()
{
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;

    const QString name = m_settings.rules().at(m_currentRule).name;
    if (QMessageBox::question(
            this,
            tr("Remove trust rule"),
            tr("Remove “%1”? The certificate files will be kept.").arg(name),
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

void TrustRulesDialog::importCertificates()
{
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;

    const QStringList sourcePaths = QFileDialog::getOpenFileNames(
        this,
        tr("Import CA certificates"),
        QString(),
        tr("Certificates (*.cer *.crt *.der *.pem);;All files (*)")
    );
    if (sourcePaths.isEmpty())
        return;

    const QDir configurationDirectory = QFileInfo(m_configurationPath).absoluteDir();
    const QString certificateDirectoryPath = configurationDirectory.filePath(
        QStringLiteral("Certificates")
    );
    if (!QDir().mkpath(certificateDirectoryPath)) {
        QMessageBox::critical(
            this,
            tr("Import failed"),
            tr("Cannot create %1").arg(certificateDirectoryPath)
        );
        return;
    }
    const QDir certificateDirectory(certificateDirectoryPath);

    QStringList failures;
    TrustRuleSettings &rule = m_settings.rules()[m_currentRule];
    for (const QString &sourcePath : sourcePaths) {
        if (readCertificates(sourcePath).isEmpty()) {
            failures.append(tr("%1 is not a readable certificate").arg(sourcePath));
            continue;
        }

        const QString destination = uniqueDestination(sourcePath, certificateDirectory);
        const bool alreadyThere = QFileInfo(sourcePath).canonicalFilePath()
            == QFileInfo(destination).canonicalFilePath();
        const bool sameExistingFile = QFile::exists(destination)
            && fileContents(destination) == fileContents(sourcePath);

        if (!alreadyThere && !sameExistingFile) {
            if (!QFile::copy(sourcePath, destination)) {
                failures.append(tr("Cannot copy %1").arg(sourcePath));
                continue;
            }
            m_pendingCertificateFiles.append(destination);
        }

        const QString relativePath = QDir::fromNativeSeparators(
            configurationDirectory.relativeFilePath(destination)
        );
        if (!rule.anchors.contains(relativePath))
            rule.anchors.append(relativePath);
    }

    refreshCertificates();
    if (!failures.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Some certificates were not imported"),
            failures.join(QLatin1Char('\n'))
        );
    }
}

void TrustRulesDialog::removeCertificate()
{
    if (m_currentRule < 0 || m_currentRule >= m_settings.rules().size())
        return;
    const int row = m_certificates->currentRow();
    if (row < 0)
        return;

    m_settings.rules()[m_currentRule].anchors.removeAt(row);
    refreshCertificates();
}

void TrustRulesDialog::showCertificateDetails()
{
    const QListWidgetItem *item = m_certificates->currentItem();
    if (!item)
        return;

    const QString configuredPath = item->data(Qt::UserRole).toString();
    const QDir baseDirectory = QFileInfo(m_configurationPath).absoluteDir();
    const QString absolutePath = QFileInfo(configuredPath).isAbsolute()
        ? configuredPath
        : baseDirectory.filePath(configuredPath);
    const QList<QSslCertificate> certificates = readCertificates(absolutePath);
    if (certificates.isEmpty()) {
        QMessageBox::warning(
            this,
            tr("Certificate unavailable"),
            tr("Cannot read certificate file:\n%1").arg(absolutePath)
        );
        return;
    }

    QDialog details(this);
    details.setObjectName(QStringLiteral("certificateDetailsDialog"));
    details.setWindowTitle(tr("Certificate Details"));
    details.setWindowIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
    details.resize(640, 510);

    auto *layout = new QVBoxLayout(&details);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Certificate Details"), &details);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);

    auto *fileName = new QLabel(QFileInfo(absolutePath).fileName(), &details);
    fileName->setObjectName(QStringLiteral("dialogSubtitle"));
    fileName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileName->setToolTip(absolutePath);
    layout->addWidget(fileName);

    QComboBox *certificateSelector = nullptr;
    if (certificates.size() > 1) {
        certificateSelector = new QComboBox(&details);
        for (qsizetype index = 0; index < certificates.size(); ++index) {
            certificateSelector->addItem(
                tr("Certificate %1 — %2")
                    .arg(index + 1)
                    .arg(certificateName(certificates.at(index)))
            );
        }
        layout->addWidget(certificateSelector);
    }

    auto *status = new QLabel(&details);
    status->setObjectName(QStringLiteral("certificateStatus"));
    layout->addWidget(status);

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const auto selectableValue = [&details] {
        auto *label = new QLabel(&details);
        label->setObjectName(QStringLiteral("certificateValue"));
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };

    auto *subject = selectableValue();
    auto *issuer = selectableValue();
    auto *serial = selectableValue();
    auto *validFrom = selectableValue();
    auto *validUntil = selectableValue();
    auto *fingerprint = selectableValue();
    fingerprint->setObjectName(QStringLiteral("certificateFingerprint"));

    form->addRow(tr("Subject"), subject);
    form->addRow(tr("Issuer"), issuer);
    form->addRow(tr("Serial number"), serial);
    form->addRow(tr("Valid from"), validFrom);
    form->addRow(tr("Valid until"), validUntil);
    form->addRow(QStringLiteral("SHA-256"), fingerprint);
    layout->addLayout(form, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &details);
    auto *copyFingerprint = buttons->addButton(
        tr("Copy fingerprint"),
        QDialogButtonBox::ActionRole
    );
    layout->addWidget(buttons);

    const auto showCertificate = [=](int index) {
        const QSslCertificate &certificate = certificates.at(index);
        subject->setText(certificateName(certificate));

        QStringList issuerNames = certificate.issuerInfo(QSslCertificate::CommonName);
        if (issuerNames.isEmpty())
            issuerNames = certificate.issuerInfo(QSslCertificate::Organization);
        issuer->setText(
            issuerNames.isEmpty() ? tr("Unknown") : issuerNames.join(QStringLiteral(", "))
        );
        serial->setText(QString::fromLatin1(certificate.serialNumber()).toUpper());
        validFrom->setText(certificate.effectiveDate().toLocalTime().toString(Qt::ISODate));
        validUntil->setText(certificate.expiryDate().toLocalTime().toString(Qt::ISODate));
        fingerprint->setText(certificateFingerprint(certificate));

        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (now < certificate.effectiveDate().toUTC()) {
            status->setText(tr("Not valid yet"));
            status->setProperty("state", QStringLiteral("warning"));
        } else if (now > certificate.expiryDate().toUTC()) {
            status->setText(tr("Expired"));
            status->setProperty("state", QStringLiteral("error"));
        } else {
            status->setText(tr("Valid"));
            status->setProperty("state", QStringLiteral("valid"));
        }
        repolish(status);
    };

    if (certificateSelector) {
        connect(
            certificateSelector,
            &QComboBox::currentIndexChanged,
            &details,
            showCertificate
        );
    }
    connect(copyFingerprint, &QPushButton::clicked, &details, [fingerprint] {
        QApplication::clipboard()->setText(fingerprint->text());
    });
    connect(buttons, &QDialogButtonBox::rejected, &details, &QDialog::reject);
    showCertificate(0);
    details.exec();
}

void TrustRulesDialog::testDomain()
{
    storeCurrentRule();
    QString host = m_testDomain->text().trimmed();
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

    m_testResult->setProperty("state", match.isEmpty() ? QStringLiteral("none") : QStringLiteral("match"));
    m_testResult->setText(
        match.isEmpty()
            ? tr("No enabled rule matches %1").arg(host)
            : tr("%1 matches rule “%2”").arg(host, match)
    );
    m_testResult->show();
    repolish(m_testResult);
}

void TrustRulesDialog::saveAndClose()
{
    QString error;
    if (!save(&error)) {
        QMessageBox::warning(this, tr("Cannot save trust rules"), error);
        return;
    }
    finalizeSave();
    accept();
}

void TrustRulesDialog::cleanupPendingCertificates(bool keepReferenced)
{
    QSet<QString> referencedFiles;
    if (keepReferenced) {
        const QDir baseDirectory = QFileInfo(m_configurationPath).absoluteDir();
        for (const TrustRuleSettings &rule : std::as_const(m_settings.rules())) {
            for (const QString &anchor : rule.anchors) {
                referencedFiles.insert(
                    QFileInfo(anchor).isAbsolute()
                        ? QFileInfo(anchor).absoluteFilePath()
                        : QFileInfo(baseDirectory.filePath(anchor)).absoluteFilePath()
                );
            }
        }
    }

    for (const QString &path : std::as_const(m_pendingCertificateFiles)) {
        if (!keepReferenced || !referencedFiles.contains(QFileInfo(path).absoluteFilePath()))
            QFile::remove(path);
    }
    m_pendingCertificateFiles.clear();
}
