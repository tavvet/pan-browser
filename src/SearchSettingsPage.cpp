#include "SearchSettingsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QUuid>
#include <QVBoxLayout>

namespace {

class SearchEngineEditor final : public QDialog {
public:
    SearchEngineEditor(const SearchEngineSettings &engine, bool adding, QWidget *parent)
        : QDialog(parent)
    {
        setObjectName(QStringLiteral("searchEngineDialog"));
        setWindowTitle(adding ? QStringLiteral("Add search engine")
                              : QStringLiteral("Edit search engine"));
        setModal(true);
        resize(620, 300);

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
        m_name = new QLineEdit(engine.name, this);
        m_name->setPlaceholderText(QStringLiteral("Example Search"));
        form->addRow(QStringLiteral("Name"), m_name);
        m_keyword = new QLineEdit(engine.keyword, this);
        m_keyword->setPlaceholderText(QStringLiteral("ex"));
        form->addRow(QStringLiteral("Keyword"), m_keyword);
        m_urlTemplate = new QLineEdit(engine.urlTemplate, this);
        m_urlTemplate->setPlaceholderText(
            QStringLiteral("https://search.example/?q={searchTerms}")
        );
        form->addRow(QStringLiteral("URL template"), m_urlTemplate);
        m_enabled = new QCheckBox(QStringLiteral("Search engine is enabled"), this);
        m_enabled->setChecked(engine.enabled);
        form->addRow(QStringLiteral("Status"), m_enabled);
        layout->addLayout(form);

        auto *hint = new QLabel(
            QStringLiteral("Use {searchTerms} exactly once. The optional keyword is entered as @keyword in the address bar."),
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
            adding ? QStringLiteral("Add engine") : QStringLiteral("Save engine")
        );
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    }

    void applyTo(SearchEngineSettings *engine) const
    {
        engine->name = m_name->text().trimmed();
        engine->keyword = m_keyword->text().trimmed().toLower();
        if (engine->keyword.startsWith(QLatin1Char('@')))
            engine->keyword.removeFirst();
        engine->urlTemplate = m_urlTemplate->text().trimmed();
        engine->enabled = m_enabled->isChecked();
    }

private:
    QLineEdit *m_name = nullptr;
    QLineEdit *m_keyword = nullptr;
    QLineEdit *m_urlTemplate = nullptr;
    QCheckBox *m_enabled = nullptr;
};

} // namespace

SearchSettingsPage::SearchSettingsPage(const SearchSettings &settings, QWidget *parent)
    : QWidget(parent)
    , m_settings(settings)
{
    setObjectName(QStringLiteral("searchSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("Search"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        QStringLiteral("Search from the address bar without sending queries until you press Enter."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    auto *defaultLabel = new QLabel(QStringLiteral("DEFAULT SEARCH ENGINE"), this);
    defaultLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(defaultLabel);
    auto *defaultCard = new QFrame(this);
    defaultCard->setObjectName(QStringLiteral("settingsCard"));
    auto *defaultLayout = new QFormLayout(defaultCard);
    defaultLayout->setContentsMargins(18, 16, 18, 16);
    defaultLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_defaultEngine = new QComboBox(defaultCard);
    defaultLayout->addRow(QStringLiteral("Search engine"), m_defaultEngine);
    layout->addWidget(defaultCard);

    auto *enginesLabel = new QLabel(QStringLiteral("SEARCH ENGINES"), this);
    enginesLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(enginesLabel);
    m_engineList = new QListWidget(this);
    m_engineList->setObjectName(QStringLiteral("searchEnginesList"));
    m_engineList->setSpacing(2);
    layout->addWidget(m_engineList, 1);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("fieldHint"));
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setWordWrap(true);
    layout->addWidget(m_details);

    auto *buttons = new QHBoxLayout();
    auto *addButton = new QPushButton(QStringLiteral("Add engine"), this);
    m_editButton = new QPushButton(QStringLiteral("Edit"), this);
    m_removeButton = new QPushButton(QStringLiteral("Remove"), this);
    m_removeButton->setObjectName(QStringLiteral("dangerButton"));
    auto *restoreButton = new QPushButton(QStringLiteral("Restore built-ins"), this);
    buttons->addWidget(addButton);
    buttons->addWidget(m_editButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();
    buttons->addWidget(restoreButton);
    layout->addLayout(buttons);

    connect(m_defaultEngine, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!m_rebuilding && m_defaultEngine->currentIndex() >= 0)
            m_settings.setDefaultEngineId(m_defaultEngine->currentData().toString());
    });
    connect(m_engineList, &QListWidget::currentRowChanged, this, [this](int) {
        updateActions();
        updateDetails();
    });
    connect(m_engineList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (!m_rebuilding)
            setEngineEnabled(item->data(Qt::UserRole).toString(), item->checkState() == Qt::Checked);
    });
    connect(m_engineList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        editSelectedEngine();
    });
    connect(addButton, &QPushButton::clicked, this, &SearchSettingsPage::addEngine);
    connect(m_editButton, &QPushButton::clicked, this, &SearchSettingsPage::editSelectedEngine);
    connect(m_removeButton, &QPushButton::clicked, this, &SearchSettingsPage::removeSelectedEngine);
    connect(restoreButton, &QPushButton::clicked, this, &SearchSettingsPage::restoreBuiltInEngines);

    rebuildList();
}

SearchSettings SearchSettingsPage::settings() const
{
    return m_settings;
}

bool SearchSettingsPage::validate(QString *error) const
{
    return m_settings.validate(error);
}

void SearchSettingsPage::rebuildList(const QString &selectedId)
{
    QString id = selectedId;
    if (id.isEmpty() && m_engineList->currentItem())
        id = m_engineList->currentItem()->data(Qt::UserRole).toString();
    m_rebuilding = true;
    m_engineList->clear();
    int selectedRow = -1;
    for (const SearchEngineSettings &engine : m_settings.engines()) {
        QString text = engine.name;
        if (!engine.keyword.isEmpty())
            text += QStringLiteral("   @%1").arg(engine.keyword);
        if (engine.builtIn)
            text += QStringLiteral("   Built-in");
        auto *item = new QListWidgetItem(text, m_engineList);
        item->setData(Qt::UserRole, engine.id);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(engine.enabled ? Qt::Checked : Qt::Unchecked);
        item->setToolTip(engine.urlTemplate);
        if (engine.id == id)
            selectedRow = m_engineList->count() - 1;
    }
    m_rebuilding = false;
    rebuildDefaultEngines();
    if (m_engineList->count() > 0)
        m_engineList->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    updateActions();
    updateDetails();
}

void SearchSettingsPage::rebuildDefaultEngines()
{
    const QSignalBlocker blocker(m_defaultEngine);
    const QString selected = m_settings.defaultEngineId();
    m_defaultEngine->clear();
    for (const SearchEngineSettings &engine : m_settings.engines()) {
        if (engine.enabled)
            m_defaultEngine->addItem(engine.name, engine.id);
    }
    int index = m_defaultEngine->findData(selected);
    if (index < 0 && m_defaultEngine->count() > 0) {
        index = 0;
        m_settings.setDefaultEngineId(m_defaultEngine->itemData(0).toString());
    }
    m_defaultEngine->setCurrentIndex(index);
}

void SearchSettingsPage::updateActions()
{
    const SearchEngineSettings *engine = selectedEngine();
    m_editButton->setEnabled(engine != nullptr);
    m_removeButton->setEnabled(engine && !engine->builtIn);
}

void SearchSettingsPage::updateDetails()
{
    const SearchEngineSettings *engine = selectedEngine();
    if (!engine) {
        m_details->clear();
        return;
    }
    QString text = engine->urlTemplate;
    if (engine->urlTemplate.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive))
        text += QStringLiteral("\nWarning: searches sent through HTTP are not encrypted.");
    m_details->setText(text);
}

void SearchSettingsPage::addEngine()
{
    SearchEngineSettings engine;
    engine.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!editEngine(&engine, true))
        return;
    m_settings.engines().append(engine);
    rebuildList(engine.id);
}

void SearchSettingsPage::editSelectedEngine()
{
    SearchEngineSettings *engine = selectedEngine();
    if (!engine)
        return;
    const QString id = engine->id;
    if (editEngine(engine, false))
        rebuildList(id);
}

void SearchSettingsPage::removeSelectedEngine()
{
    const SearchEngineSettings *engine = selectedEngine();
    if (!engine || engine->builtIn)
        return;
    const QString id = engine->id;
    const QString name = engine->name;
    if (QMessageBox::question(
            this,
            QStringLiteral("Remove search engine"),
            QStringLiteral("Remove “%1”?").arg(name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    for (qsizetype index = 0; index < m_settings.engines().size(); ++index) {
        if (m_settings.engines().at(index).id == id) {
            m_settings.engines().removeAt(index);
            break;
        }
    }
    rebuildList();
}

void SearchSettingsPage::restoreBuiltInEngines()
{
    if (QMessageBox::question(
            this,
            QStringLiteral("Restore built-in search engines"),
            QStringLiteral("Restore the built-in engines and their original settings? Custom engines will be kept."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    SearchSettings restored = m_settings;
    restored.restoreBuiltIns();
    QString error;
    if (!restored.validate(&error)) {
        QMessageBox::warning(this, QStringLiteral("Cannot restore built-ins"), error);
        return;
    }
    m_settings = restored;
    rebuildList();
}

SearchEngineSettings *SearchSettingsPage::selectedEngine()
{
    const QListWidgetItem *item = m_engineList->currentItem();
    if (!item)
        return nullptr;
    const QString id = item->data(Qt::UserRole).toString();
    for (SearchEngineSettings &engine : m_settings.engines()) {
        if (engine.id == id)
            return &engine;
    }
    return nullptr;
}

const SearchEngineSettings *SearchSettingsPage::selectedEngine() const
{
    const QListWidgetItem *item = m_engineList->currentItem();
    return item ? m_settings.engineById(item->data(Qt::UserRole).toString()) : nullptr;
}

bool SearchSettingsPage::editEngine(SearchEngineSettings *engine, bool adding)
{
    SearchEngineEditor editor(*engine, adding, this);
    while (editor.exec() == QDialog::Accepted) {
        SearchEngineSettings candidate = *engine;
        editor.applyTo(&candidate);
        SearchSettings candidateSettings = m_settings;
        if (adding) {
            candidateSettings.engines().append(candidate);
        } else {
            for (SearchEngineSettings &existing : candidateSettings.engines()) {
                if (existing.id == candidate.id) {
                    existing = candidate;
                    break;
                }
            }
        }
        if (!candidate.enabled && candidate.id == candidateSettings.defaultEngineId()) {
            for (const SearchEngineSettings &existing : candidateSettings.engines()) {
                if (existing.enabled) {
                    candidateSettings.setDefaultEngineId(existing.id);
                    break;
                }
            }
        }
        QString error;
        if (!candidateSettings.validate(&error)) {
            QMessageBox::warning(&editor, QStringLiteral("Invalid search engine"), error);
            continue;
        }
        *engine = candidate;
        if (candidateSettings.defaultEngineId() != m_settings.defaultEngineId())
            m_settings.setDefaultEngineId(candidateSettings.defaultEngineId());
        return true;
    }
    return false;
}

void SearchSettingsPage::setEngineEnabled(const QString &id, bool enabled)
{
    SearchSettings candidate = m_settings;
    for (SearchEngineSettings &engine : candidate.engines()) {
        if (engine.id == id) {
            engine.enabled = enabled;
            break;
        }
    }
    if (!enabled && candidate.defaultEngineId() == id) {
        for (const SearchEngineSettings &engine : candidate.engines()) {
            if (engine.enabled) {
                candidate.setDefaultEngineId(engine.id);
                break;
            }
        }
    }
    QString error;
    if (!candidate.validate(&error)) {
        QMessageBox::warning(this, QStringLiteral("Cannot change search engine"), error);
        if (const SearchEngineSettings *current = m_settings.engineById(id)) {
            for (int row = 0; row < m_engineList->count(); ++row) {
                QListWidgetItem *item = m_engineList->item(row);
                if (item->data(Qt::UserRole).toString() != id)
                    continue;
                m_rebuilding = true;
                item->setCheckState(current->enabled ? Qt::Checked : Qt::Unchecked);
                m_rebuilding = false;
                break;
            }
        }
        return;
    }
    m_settings = candidate;
    rebuildDefaultEngines();
    updateDetails();
}
