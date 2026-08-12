#include "CrossDomainSettingsPage.h"

#include "CrossDomainPresetCatalog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include <utility>

namespace {

QString presetName(const QString &id)
{
    if (id == QStringLiteral("common-trackers"))
        return QCoreApplication::translate("CrossDomainSettingsPage", "Common trackers");
    if (id == QStringLiteral("public-cdns"))
        return QCoreApplication::translate("CrossDomainSettingsPage", "Public CDNs");
    return id;
}

QString presetDescription(const QString &id)
{
    if (id == QStringLiteral("common-trackers")) {
        return QCoreApplication::translate(
            "CrossDomainSettingsPage",
            "Blocks a small PanBrowser-curated set of common analytics, advertising, and behavioral tracking hosts."
        );
    }
    if (id == QStringLiteral("public-cdns")) {
        return QCoreApplication::translate(
            "CrossDomainSettingsPage",
            "Allows selected public library and font CDNs. This improves compatibility but weakens third-party isolation."
        );
    }
    return {};
}

QString presetDecisionName(CrossDomainPresetDecision decision)
{
    return decision == CrossDomainPresetDecision::Allow
        ? QCoreApplication::translate("CrossDomainSettingsPage", "Allow")
        : QCoreApplication::translate("CrossDomainSettingsPage", "Block");
}

} // namespace

CrossDomainSettingsPage::CrossDomainSettingsPage(
    const CrossDomainSettings &settings,
    QWidget *parent
)
    : QWidget(parent)
    , m_initialSettings(settings)
    , m_rules(settings.rules())
{
    setObjectName(QStringLiteral("crossDomainSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Site Connections"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Control connections from the current site to third-party hosts."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *modeLabel = new QLabel(tr("EXPERIMENTAL PROTECTION"), this);
    modeLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(modeLabel);
    auto *modeCard = new QFrame(this);
    modeCard->setObjectName(QStringLiteral("settingsCard"));
    auto *modeLayout = new QVBoxLayout(modeCard);
    modeLayout->setContentsMargins(18, 16, 18, 16);
    modeLayout->setSpacing(8);
    m_enabled = new QCheckBox(tr("Block unknown third-party connections"), modeCard);
    m_enabled->setChecked(settings.enabled());
    modeLayout->addWidget(m_enabled);
    auto *modeHint = new QLabel(
        tr("Top-level navigation remains allowed. Unknown subresources and data requests are blocked until you decide. Some sites may not work correctly."),
        modeCard
    );
    modeHint->setObjectName(QStringLiteral("fieldHint"));
    modeHint->setWordWrap(true);
    modeLayout->addWidget(modeHint);
    layout->addWidget(modeCard);

    auto *presetsLabel = new QLabel(tr("PANBROWSER RECOMMENDATIONS"), this);
    presetsLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(presetsLabel);
    auto *presetsCard = new QFrame(this);
    presetsCard->setObjectName(QStringLiteral("settingsCard"));
    auto *presetsLayout = new QVBoxLayout(presetsCard);
    presetsLayout->setContentsMargins(18, 14, 18, 14);
    presetsLayout->setSpacing(6);
    const CrossDomainPresetCatalog &catalog = CrossDomainPresetCatalog::bundled();
    const QStringList enabledPresetIds = settings.enabledPresetIds();
    for (const CrossDomainPreset &preset : catalog.presets()) {
        QString label = tr("%1 — %n host(s)", nullptr, int(preset.patterns.size()))
                            .arg(presetName(preset.id));
        if (preset.recommended)
            label += tr(" (Recommended)");
        auto *checkBox = new QCheckBox(label, presetsCard);
        checkBox->setObjectName(QStringLiteral("connectionPreset_%1").arg(preset.id));
        checkBox->setChecked(enabledPresetIds.contains(preset.id));
        checkBox->setToolTip(presetDescription(preset.id));
        presetsLayout->addWidget(checkBox);
        auto *description = new QLabel(presetDescription(preset.id), presetsCard);
        description->setObjectName(QStringLiteral("fieldHint"));
        description->setWordWrap(true);
        description->setContentsMargins(22, 0, 0, 2);
        presetsLayout->addWidget(description);
        m_presetChecks.insert(preset.id, checkBox);
    }
    auto *presetActions = new QHBoxLayout();
    presetActions->setContentsMargins(0, 2, 0, 0);
    m_useRecommendedPresets = new QPushButton(tr("Use recommended"), presetsCard);
    m_useRecommendedPresets->setObjectName(QStringLiteral("useRecommendedPresets"));
    m_viewPresets = new QPushButton(tr("View lists…"), presetsCard);
    m_viewPresets->setObjectName(QStringLiteral("viewConnectionPresets"));
    auto *revision = new QLabel(
        tr("Bundled revision: %1").arg(catalog.revision()),
        presetsCard
    );
    revision->setObjectName(QStringLiteral("fieldHint"));
    presetActions->addWidget(m_useRecommendedPresets);
    presetActions->addWidget(m_viewPresets);
    presetActions->addStretch();
    presetActions->addWidget(revision);
    presetsLayout->addLayout(presetActions);
    layout->addWidget(presetsCard);

    auto *listsRow = new QWidget(this);
    auto *listsLayout = new QHBoxLayout(listsRow);
    listsLayout->setContentsMargins(0, 0, 0, 0);
    listsLayout->setSpacing(14);

    auto *exceptionsColumn = new QVBoxLayout();
    exceptionsColumn->setContentsMargins(0, 0, 0, 0);
    exceptionsColumn->setSpacing(8);
    auto *exceptionsLabel = new QLabel(tr("ALWAYS ALLOWED"), listsRow);
    exceptionsLabel->setObjectName(QStringLiteral("sectionLabel"));
    exceptionsColumn->addWidget(exceptionsLabel);
    m_exceptionsCard = new QFrame(listsRow);
    m_exceptionsCard->setObjectName(QStringLiteral("settingsCard"));
    auto *exceptionsLayout = new QVBoxLayout(m_exceptionsCard);
    exceptionsLayout->setContentsMargins(18, 16, 18, 16);
    exceptionsLayout->setSpacing(8);
    auto *exceptionsHint = new QLabel(
        tr("Every site may connect to these hosts and their subdomains. Enter one hostname per line."),
        m_exceptionsCard
    );
    exceptionsHint->setObjectName(QStringLiteral("fieldHint"));
    exceptionsHint->setWordWrap(true);
    exceptionsLayout->addWidget(exceptionsHint);
    m_globalExceptions = new QPlainTextEdit(m_exceptionsCard);
    m_globalExceptions->setObjectName(QStringLiteral("crossDomainExceptions"));
    m_globalExceptions->setPlaceholderText(
        QStringLiteral("cdn.example.com\n*.static.example.net")
    );
    m_globalExceptions->setPlainText(settings.globalAllowPatterns().join(QLatin1Char('\n')));
    m_globalExceptions->setMaximumHeight(94);
    exceptionsLayout->addWidget(m_globalExceptions);
    exceptionsColumn->addWidget(m_exceptionsCard, 1);
    listsLayout->addLayout(exceptionsColumn, 1);

    auto *blockedColumn = new QVBoxLayout();
    blockedColumn->setContentsMargins(0, 0, 0, 0);
    blockedColumn->setSpacing(8);
    auto *blockedLabel = new QLabel(tr("ALWAYS BLOCKED"), listsRow);
    blockedLabel->setObjectName(QStringLiteral("sectionLabel"));
    blockedColumn->addWidget(blockedLabel);
    m_blockedCard = new QFrame(listsRow);
    m_blockedCard->setObjectName(QStringLiteral("settingsCard"));
    auto *blockedLayout = new QVBoxLayout(m_blockedCard);
    blockedLayout->setContentsMargins(18, 16, 18, 16);
    blockedLayout->setSpacing(8);
    auto *blockedHint = new QLabel(
        tr("Connections to these hosts and their subdomains are blocked for every site without prompting. Enter one hostname per line."),
        m_blockedCard
    );
    blockedHint->setObjectName(QStringLiteral("fieldHint"));
    blockedHint->setWordWrap(true);
    blockedLayout->addWidget(blockedHint);
    m_globalBlockedHosts = new QPlainTextEdit(m_blockedCard);
    m_globalBlockedHosts->setObjectName(QStringLiteral("crossDomainBlockedHosts"));
    m_globalBlockedHosts->setPlaceholderText(
        QStringLiteral("tracker.example.com\n*.metrics.example.net")
    );
    m_globalBlockedHosts->setPlainText(
        settings.globalBlockPatterns().join(QLatin1Char('\n'))
    );
    m_globalBlockedHosts->setMaximumHeight(94);
    blockedLayout->addWidget(m_globalBlockedHosts);
    blockedColumn->addWidget(m_blockedCard, 1);
    listsLayout->addLayout(blockedColumn, 1);
    layout->addWidget(listsRow);

    auto *rulesLabel = new QLabel(tr("SAVED SITE RULES"), this);
    rulesLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(rulesLabel);
    m_rulesCard = new QFrame(this);
    m_rulesCard->setObjectName(QStringLiteral("settingsCard"));
    auto *rulesLayout = new QVBoxLayout(m_rulesCard);
    rulesLayout->setContentsMargins(18, 16, 18, 16);
    rulesLayout->setSpacing(8);
    auto *rulesHint = new QLabel(
        tr("Rules created from connection prompts or added manually appear here. Saved rules apply to one exact source site and target host."),
        m_rulesCard
    );
    rulesHint->setObjectName(QStringLiteral("fieldHint"));
    rulesHint->setWordWrap(true);
    rulesLayout->addWidget(rulesHint);
    m_rulesList = new QListWidget(m_rulesCard);
    m_rulesList->setObjectName(QStringLiteral("crossDomainRulesList"));
    m_rulesList->setMinimumHeight(105);
    rulesLayout->addWidget(m_rulesList, 1);
    auto *ruleActions = new QHBoxLayout();
    ruleActions->setContentsMargins(0, 0, 0, 0);
    m_addRule = new QPushButton(tr("Add rule…"), m_rulesCard);
    m_addRule->setObjectName(QStringLiteral("addCrossDomainRule"));
    m_removeRule = new QPushButton(tr("Remove selected"), m_rulesCard);
    m_clearRules = new QPushButton(tr("Clear all rules"), m_rulesCard);
    m_clearRules->setObjectName(QStringLiteral("dangerButton"));
    ruleActions->addWidget(m_addRule);
    ruleActions->addWidget(m_removeRule);
    ruleActions->addStretch();
    ruleActions->addWidget(m_clearRules);
    rulesLayout->addLayout(ruleActions);
    layout->addWidget(m_rulesCard, 1);

    connect(m_enabled, &QCheckBox::toggled, this, &CrossDomainSettingsPage::updateUi);
    connect(
        m_rulesList,
        &QListWidget::itemSelectionChanged,
        this,
        &CrossDomainSettingsPage::updateUi
    );
    connect(
        m_addRule,
        &QPushButton::clicked,
        this,
        &CrossDomainSettingsPage::addRule
    );
    connect(
        m_removeRule,
        &QPushButton::clicked,
        this,
        &CrossDomainSettingsPage::removeSelectedRule
    );
    connect(
        m_clearRules,
        &QPushButton::clicked,
        this,
        &CrossDomainSettingsPage::clearRules
    );
    connect(
        m_useRecommendedPresets,
        &QPushButton::clicked,
        this,
        &CrossDomainSettingsPage::useRecommendedPresets
    );
    connect(
        m_viewPresets,
        &QPushButton::clicked,
        this,
        &CrossDomainSettingsPage::showPresetLists
    );
    m_useRecommendedPresets->setEnabled(!catalog.presets().isEmpty());
    m_viewPresets->setEnabled(!catalog.presets().isEmpty());
    rebuildRules();
    updateUi();
}

CrossDomainSettings CrossDomainSettingsPage::settings() const
{
    CrossDomainSettings result = m_initialSettings;
    result.setEnabled(m_enabled->isChecked());
    result.setGlobalAllowPatterns(
        m_globalExceptions->toPlainText().split(
            QLatin1Char('\n'),
            Qt::SkipEmptyParts
        )
    );
    result.setGlobalBlockPatterns(
        m_globalBlockedHosts->toPlainText().split(
            QLatin1Char('\n'),
            Qt::SkipEmptyParts
        )
    );
    QStringList enabledPresetIds = m_initialSettings.enabledPresetIds();
    const QStringList knownPresetIds = CrossDomainPresetCatalog::bundled().knownPresetIds();
    for (const QString &id : knownPresetIds)
        enabledPresetIds.removeAll(id);
    for (auto iterator = m_presetChecks.cbegin(); iterator != m_presetChecks.cend(); ++iterator) {
        if (iterator.value()->isChecked())
            enabledPresetIds.append(iterator.key());
    }
    result.setEnabledPresetIds(enabledPresetIds);
    result.setRules(m_rules);
    return result;
}

bool CrossDomainSettingsPage::validate(QString *error) const
{
    return settings().validate(error);
}

void CrossDomainSettingsPage::rebuildRules()
{
    m_rulesList->clear();
    for (const CrossDomainRule &rule : std::as_const(m_rules)) {
        auto *item = new QListWidgetItem(
            tr("%1 → %2 — %3")
                .arg(
                    rule.sourceSite,
                    rule.targetHost,
                    crossDomainDecisionDisplayName(rule.decision)
                ),
            m_rulesList
        );
        item->setToolTip(
            tr("Source site: %1\nTarget host: %2\nDecision: %3")
                .arg(
                    rule.sourceSite,
                    rule.targetHost,
                    crossDomainDecisionDisplayName(rule.decision)
                )
        );
    }
}

void CrossDomainSettingsPage::updateUi()
{
    m_removeRule->setEnabled(m_rulesList->currentRow() >= 0);
    m_clearRules->setEnabled(!m_rules.isEmpty());
}

void CrossDomainSettingsPage::addRule()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Add site connection rule"));
    dialog.setObjectName(QStringLiteral("crossDomainRuleDialog"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(
        tr("Create an exact-host exception for connections made by one site."),
        &dialog
    );
    intro->setWordWrap(true);
    intro->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(intro);

    auto *form = new QFormLayout();
    auto *sourceSite = new QLineEdit(&dialog);
    sourceSite->setPlaceholderText(QStringLiteral("example.com"));
    auto *targetHost = new QLineEdit(&dialog);
    targetHost->setPlaceholderText(QStringLiteral("cdn.example.net"));
    auto *decision = new QComboBox(&dialog);
    decision->addItem(tr("Allow"), int(CrossDomainRuleDecision::Allow));
    decision->addItem(tr("Block"), int(CrossDomainRuleDecision::Block));
    form->addRow(tr("Source site"), sourceSite);
    form->addRow(tr("Target host"), targetHost);
    form->addRow(tr("Action"), decision);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
    );
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        CrossDomainSettings candidate;
        candidate.setEnabled(true);
        candidate.setRules(m_rules);
        candidate.setRule(
            sourceSite->text(),
            targetHost->text(),
            static_cast<CrossDomainRuleDecision>(decision->currentData().toInt())
        );
        QString error;
        if (!candidate.validate(&error)) {
            QMessageBox::warning(&dialog, tr("Cannot add rule"), error);
            return;
        }
        m_rules = candidate.rules();
        dialog.accept();
    });
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;
    rebuildRules();
    updateUi();
}

void CrossDomainSettingsPage::removeSelectedRule()
{
    const int row = m_rulesList->currentRow();
    if (row < 0 || row >= m_rules.size())
        return;
    m_rules.removeAt(row);
    rebuildRules();
    updateUi();
}

void CrossDomainSettingsPage::clearRules()
{
    if (m_rules.isEmpty())
        return;
    if (QMessageBox::question(
            this,
            tr("Clear saved site rules?"),
            tr("All persistent allow and block decisions will be removed."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    m_rules.clear();
    rebuildRules();
    updateUi();
}

void CrossDomainSettingsPage::useRecommendedPresets()
{
    for (const CrossDomainPreset &preset : CrossDomainPresetCatalog::bundled().presets()) {
        if (preset.recommended) {
            if (QCheckBox *checkBox = m_presetChecks.value(preset.id))
                checkBox->setChecked(true);
        }
    }
}

void CrossDomainSettingsPage::showPresetLists()
{
    const CrossDomainPresetCatalog &catalog = CrossDomainPresetCatalog::bundled();
    if (catalog.presets().isEmpty())
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(tr("PanBrowser recommendation lists"));
    dialog.resize(660, 520);
    auto *dialogLayout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(
        tr("These bundled lists are managed by PanBrowser and updated only with the application. Your personal allow and block lists remain separate."),
        &dialog
    );
    intro->setWordWrap(true);
    intro->setObjectName(QStringLiteral("dialogSubtitle"));
    dialogLayout->addWidget(intro);

    auto *tabs = new QTabWidget(&dialog);
    for (const CrossDomainPreset &preset : catalog.presets()) {
        auto *page = new QWidget(tabs);
        auto *pageLayout = new QVBoxLayout(page);
        auto *description = new QLabel(presetDescription(preset.id), page);
        description->setWordWrap(true);
        pageLayout->addWidget(description);
        auto *summary = new QLabel(
            tr("Action: %1 · %n host(s)", nullptr, int(preset.patterns.size()))
                .arg(presetDecisionName(preset.decision)),
            page
        );
        summary->setObjectName(QStringLiteral("fieldHint"));
        pageLayout->addWidget(summary);
        auto *hosts = new QListWidget(page);
        hosts->addItems(preset.patterns);
        hosts->setSelectionMode(QAbstractItemView::NoSelection);
        pageLayout->addWidget(hosts, 1);
        tabs->addTab(page, presetName(preset.id));
    }
    dialogLayout->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialogLayout->addWidget(buttons);
    dialog.exec();
}
