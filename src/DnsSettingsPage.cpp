#include "DnsSettingsPage.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUuid>
#include <QVBoxLayout>

namespace {

QString uiText(const char *source)
{
    return QCoreApplication::translate("DnsSettingsPage", source);
}

class DnsProviderEditor final : public QDialog {
public:
    DnsProviderEditor(const DnsProvider &provider, bool adding, QWidget *parent)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("dnsProviderDialog"));
        setWindowTitle(adding
            ? uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Add DNS provider"))
            : uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Edit DNS provider")));
        setModal(true);
        resize(660, 430);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(22, 20, 22, 18);
        layout->setSpacing(14);

        auto *title = new QLabel(windowTitle(), this);
        title->setObjectName(QStringLiteral("dialogTitle"));
        layout->addWidget(title);

        auto *form = new QFormLayout();
        form->setHorizontalSpacing(18);
        form->setVerticalSpacing(12);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        m_name = new QLineEdit(provider.name, this);
        m_name->setPlaceholderText(uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "My secure DNS")));
        form->addRow(uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Name")), m_name);
        m_description = new QLineEdit(provider.description, this);
        m_description->setPlaceholderText(uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Optional description")));
        form->addRow(uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Description")), m_description);
        m_templates = new QPlainTextEdit(this);
        m_templates->setPlainText(provider.serverTemplates.join(QLatin1Char('\n')));
        m_templates->setPlaceholderText(
            QStringLiteral("https://dns.example/dns-query{?dns}")
        );
        m_templates->setMinimumHeight(130);
        form->addRow(uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Server templates")), m_templates);
        layout->addLayout(form);

        auto *hint = new QLabel(
            uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Enter one HTTPS DNS-over-HTTPS endpoint per line. The optional {?dns} URI variable makes Chromium use GET; without it Chromium can use POST.")),
            this
        );
        hint->setObjectName(QStringLiteral("fieldHint"));
        hint->setWordWrap(true);
        layout->addWidget(hint);

        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel,
            Qt::Horizontal,
            this
        );
        buttons->button(QDialogButtonBox::Save)->setText(
            adding
                ? uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Add provider"))
                : uiText(QT_TRANSLATE_NOOP("DnsSettingsPage", "Save provider"))
        );
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void applyTo(DnsProvider *provider) const
    {
        provider->name = m_name->text().trimmed();
        provider->description = m_description->text().trimmed();
        provider->serverTemplates.clear();
        for (const QString &line : m_templates->toPlainText().split(QLatin1Char('\n'))) {
            if (!line.trimmed().isEmpty())
                provider->serverTemplates.append(line.trimmed());
        }
    }

private:
    QLineEdit *m_name = nullptr;
    QLineEdit *m_description = nullptr;
    QPlainTextEdit *m_templates = nullptr;
};

} // namespace

DnsSettingsPage::DnsSettingsPage(const DnsSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setObjectName(QStringLiteral("dnsSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("DNS"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Choose how Chromium resolves website names without changing the operating system DNS."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *modeLabel = new QLabel(tr("DNS RESOLUTION"), this);
    modeLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(modeLabel);
    auto *modeCard = new QFrame(this);
    modeCard->setObjectName(QStringLiteral("settingsCard"));
    auto *modeLayout = new QFormLayout(modeCard);
    modeLayout->setContentsMargins(18, 16, 18, 16);
    modeLayout->setHorizontalSpacing(18);
    modeLayout->setVerticalSpacing(10);
    modeLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_mode = new QComboBox(modeCard);
    m_mode->addItem(tr("System DNS"), static_cast<int>(DnsResolutionMode::System));
    m_mode->addItem(
        tr("Secure DNS with system fallback"),
        static_cast<int>(DnsResolutionMode::SecureWithFallback)
    );
    m_mode->addItem(
        tr("Secure DNS only"),
        static_cast<int>(DnsResolutionMode::SecureOnly)
    );
    m_mode->setCurrentIndex(m_mode->findData(static_cast<int>(m_settings.mode())));
    modeLayout->addRow(tr("Mode"), m_mode);
    m_activeProvider = new QComboBox(modeCard);
    modeLayout->addRow(tr("Provider"), m_activeProvider);
    m_modeHint = new QLabel(modeCard);
    m_modeHint->setObjectName(QStringLiteral("fieldHint"));
    m_modeHint->setWordWrap(true);
    modeLayout->addRow(m_modeHint);
    layout->addWidget(modeCard);

    auto *providersLabel = new QLabel(tr("DNS PROVIDERS"), this);
    providersLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(providersLabel);
    m_providerList = new QListWidget(this);
    m_providerList->setObjectName(QStringLiteral("dnsProvidersList"));
    m_providerList->setSpacing(2);
    layout->addWidget(m_providerList, 1);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("fieldHint"));
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setWordWrap(true);
    layout->addWidget(m_details);

    auto *buttons = new QHBoxLayout();
    auto *addButton = new QPushButton(tr("Add provider"), this);
    m_editButton = new QPushButton(tr("Edit"), this);
    m_removeButton = new QPushButton(tr("Remove"), this);
    m_removeButton->setObjectName(QStringLiteral("dangerButton"));
    buttons->addWidget(addButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(m_mode, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_rebuilding)
            m_settings.setMode(static_cast<DnsResolutionMode>(m_mode->currentData().toInt()));
        updateModeUi();
    });
    connect(m_activeProvider, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_rebuilding && m_activeProvider->currentIndex() >= 0)
            m_settings.setSelectedProviderId(m_activeProvider->currentData().toString());
    });
    connect(m_providerList, &QListWidget::currentRowChanged, this, [this](int) {
        updateSelection();
    });
    connect(m_providerList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        editSelectedProvider();
    });
    connect(addButton, &QPushButton::clicked, this, &DnsSettingsPage::addProvider);
    connect(m_editButton, &QPushButton::clicked, this, &DnsSettingsPage::editSelectedProvider);
    connect(m_removeButton, &QPushButton::clicked, this, &DnsSettingsPage::removeSelectedProvider);

    rebuildProviders(m_settings.selectedProviderId());
    updateModeUi();
}

DnsSettings DnsSettingsPage::settings() const
{
    return m_settings;
}

bool DnsSettingsPage::validate(QString *error) const
{
    return m_settings.validate(error);
}

void DnsSettingsPage::rebuildProviders(const QString &selectedId)
{
    QString id = selectedId;
    if (id.isEmpty() && m_providerList->currentItem())
        id = m_providerList->currentItem()->data(Qt::UserRole).toString();

    m_rebuilding = true;
    m_providerList->clear();
    int selectedRow = -1;
    for (const DnsProvider &provider : m_settings.providers()) {
        QString label = provider.name;
        if (provider.builtIn)
            label += tr("   Built-in");
        auto *item = new QListWidgetItem(label, m_providerList);
        item->setData(Qt::UserRole, provider.id);
        item->setToolTip(provider.description);
        if (provider.id == id)
            selectedRow = m_providerList->count() - 1;
    }
    rebuildActiveProviders();
    m_rebuilding = false;

    if (m_providerList->count() > 0)
        m_providerList->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    updateSelection();
}

void DnsSettingsPage::rebuildActiveProviders()
{
    const QSignalBlocker blocker(m_activeProvider);
    const QString selected = m_settings.selectedProviderId();
    m_activeProvider->clear();
    for (const DnsProvider &provider : m_settings.providers())
        m_activeProvider->addItem(provider.name, provider.id);
    int index = m_activeProvider->findData(selected);
    if (index < 0 && m_activeProvider->count() > 0) {
        index = 0;
        m_settings.setSelectedProviderId(m_activeProvider->itemData(index).toString());
    }
    m_activeProvider->setCurrentIndex(index);
}

void DnsSettingsPage::updateModeUi()
{
    const DnsResolutionMode mode = static_cast<DnsResolutionMode>(
        m_mode->currentData().toInt()
    );
    const bool secure = mode != DnsResolutionMode::System;
    m_activeProvider->setEnabled(secure);
    switch (mode) {
    case DnsResolutionMode::System:
        m_modeHint->setText(tr("Use the DNS configuration provided by the operating system."));
        break;
    case DnsResolutionMode::SecureWithFallback:
        m_modeHint->setText(tr("Use the selected DNS-over-HTTPS provider first, then system DNS if secure resolution fails."));
        break;
    case DnsResolutionMode::SecureOnly:
        m_modeHint->setText(tr("Use only the selected DNS-over-HTTPS provider. Websites will fail to open if it is unavailable."));
        break;
    }
}

void DnsSettingsPage::updateSelection()
{
    const DnsProvider *provider = selectedProvider();
    m_editButton->setEnabled(provider && !provider->builtIn);
    m_removeButton->setEnabled(provider && !provider->builtIn);
    if (!provider) {
        m_details->clear();
        return;
    }
    QString details = provider->description;
    if (!details.isEmpty())
        details += QLatin1Char('\n');
    details += provider->serverTemplates.join(QLatin1Char('\n'));
    m_details->setText(details);
}

void DnsSettingsPage::addProvider()
{
    DnsProvider provider;
    provider.id = QStringLiteral("custom-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces)
    );
    if (!editProvider(&provider, true))
        return;
    m_settings.providers().append(provider);
    m_settings.setSelectedProviderId(provider.id);
    rebuildProviders(provider.id);
}

void DnsSettingsPage::editSelectedProvider()
{
    DnsProvider *provider = selectedProvider();
    if (!provider || provider->builtIn)
        return;
    const QString id = provider->id;
    if (editProvider(provider, false))
        rebuildProviders(id);
}

void DnsSettingsPage::removeSelectedProvider()
{
    const DnsProvider *provider = selectedProvider();
    if (!provider || provider->builtIn)
        return;
    const QString id = provider->id;
    const QString name = provider->name;
    const bool active = m_settings.selectedProviderId() == id;
    const QString question = active
        ? tr("Remove “%1”? DNS mode will return to System DNS.").arg(name)
        : tr("Remove “%1”?").arg(name);
    if (QMessageBox::question(
            this,
            tr("Remove DNS provider"),
            question,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }

    for (qsizetype index = 0; index < m_settings.providers().size(); ++index) {
        if (m_settings.providers().at(index).id == id) {
            m_settings.providers().removeAt(index);
            break;
        }
    }
    if (active) {
        m_settings.setMode(DnsResolutionMode::System);
        m_settings.setSelectedProviderId(QStringLiteral("builtin-adguard"));
        m_rebuilding = true;
        m_mode->setCurrentIndex(m_mode->findData(
            static_cast<int>(DnsResolutionMode::System)
        ));
        m_rebuilding = false;
    }
    rebuildProviders(m_settings.selectedProviderId());
    updateModeUi();
}

DnsProvider *DnsSettingsPage::selectedProvider()
{
    const QListWidgetItem *item = m_providerList->currentItem();
    if (!item)
        return nullptr;
    const QString id = item->data(Qt::UserRole).toString();
    for (DnsProvider &provider : m_settings.providers()) {
        if (provider.id == id)
            return &provider;
    }
    return nullptr;
}

const DnsProvider *DnsSettingsPage::selectedProvider() const
{
    const QListWidgetItem *item = m_providerList->currentItem();
    return item ? m_settings.providerById(item->data(Qt::UserRole).toString()) : nullptr;
}

bool DnsSettingsPage::editProvider(DnsProvider *provider, bool adding)
{
    DnsProviderEditor editor(*provider, adding, this);
    while (editor.exec() == QDialog::Accepted) {
        DnsProvider candidate = *provider;
        editor.applyTo(&candidate);
        DnsSettings candidateSettings = m_settings;
        if (adding) {
            candidateSettings.providers().append(candidate);
            candidateSettings.setSelectedProviderId(candidate.id);
        } else {
            for (DnsProvider &existing : candidateSettings.providers()) {
                if (existing.id == candidate.id) {
                    existing = candidate;
                    break;
                }
            }
        }
        QString error;
        if (!candidateSettings.validate(&error)) {
            QMessageBox::warning(&editor, tr("Invalid DNS provider"), error);
            continue;
        }
        *provider = candidate;
        return true;
    }
    return false;
}
