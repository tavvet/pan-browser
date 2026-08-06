#include "WebAppsSettingsPage.h"

#include "WebAppStore.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

WebAppsSettingsPage::WebAppsSettingsPage(WebAppStore *store, QWidget *parent)
    : QWidget(parent)
    , m_store(store)
{
    setObjectName(QStringLiteral("webAppsSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Web Apps"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr("Open websites in focused app windows while sharing PanBrowser sign-ins and trust rules."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *appsLabel = new QLabel(tr("INSTALLED WEB APPS"), this);
    appsLabel->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(appsLabel);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("webAppsList"));
    m_list->setIconSize(QSize(36, 36));
    m_list->setSpacing(3);
    layout->addWidget(m_list, 1);

    m_details = new QLabel(this);
    m_details->setObjectName(QStringLiteral("fieldHint"));
    m_details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_details->setWordWrap(true);
    layout->addWidget(m_details);

    auto *buttons = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open"), this);
    m_removeButton = new QPushButton(tr("Remove…"), this);
    m_removeButton->setObjectName(QStringLiteral("dangerButton"));
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int) {
        updateSelection();
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item)
            emit openRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_openButton, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem *item = m_list->currentItem())
            emit openRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_removeButton, &QPushButton::clicked, this, &WebAppsSettingsPage::removeSelected);
    if (m_store)
        connect(m_store, &WebAppStore::appsChanged, this, [this] { rebuildList(); });
    rebuildList();
}

void WebAppsSettingsPage::rebuildList(const QString &selectedId)
{
    QString id = selectedId;
    if (id.isEmpty() && m_list->currentItem())
        id = m_list->currentItem()->data(Qt::UserRole).toString();
    m_list->clear();

    int selectedRow = -1;
    if (m_store && m_store->isAvailable()) {
        const QList<WebApp> apps = m_store->apps();
        for (const WebApp &app : apps) {
            QIcon icon(QStringLiteral(":/assets/app-icon.svg"));
            QPixmap pixmap;
            if (!app.iconPng.isEmpty() && pixmap.loadFromData(app.iconPng, "PNG"))
                icon = QIcon(pixmap);
            auto *item = new QListWidgetItem(icon, app.name, m_list);
            item->setData(Qt::UserRole, app.id);
            item->setToolTip(app.startUrl.toDisplayString(QUrl::RemovePassword));
            if (app.id == id)
                selectedRow = m_list->count() - 1;
        }
    }
    if (m_list->count() > 0)
        m_list->setCurrentRow(selectedRow >= 0 ? selectedRow : 0);
    updateSelection();
}

void WebAppsSettingsPage::updateSelection()
{
    const QListWidgetItem *item = m_list->currentItem();
    const std::optional<WebApp> app = item && m_store
        ? m_store->app(item->data(Qt::UserRole).toString())
        : std::nullopt;
    m_openButton->setEnabled(app.has_value());
    m_removeButton->setEnabled(app.has_value());
    if (!app) {
        m_details->setText(
            m_store && !m_store->isAvailable()
                ? tr("Installed web apps are unavailable because their data file could not be read.")
                : tr("No web apps installed. Open a PWA website and choose Install Web App from the PanBrowser menu.")
        );
        return;
    }
    m_details->setText(
        tr("Start page: %1\nAllowed scope: %2")
            .arg(
                app->startUrl.toDisplayString(QUrl::RemovePassword),
                app->scope.toDisplayString(QUrl::RemovePassword)
            )
    );
}

void WebAppsSettingsPage::removeSelected()
{
    QListWidgetItem *item = m_list->currentItem();
    const std::optional<WebApp> app = item && m_store
        ? m_store->app(item->data(Qt::UserRole).toString())
        : std::nullopt;
    if (!app)
        return;
    if (QMessageBox::question(
            this,
            tr("Remove web app"),
            tr("Remove “%1”? Cookies and other site data will be kept.").arg(app->name),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_store->remove(app->id, &error))
        QMessageBox::warning(this, tr("Cannot remove web app"), error);
}
