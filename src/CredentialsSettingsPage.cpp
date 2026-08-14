#include "CredentialsSettingsPage.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QFrame>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <utility>

namespace {

constexpr qsizetype maximumDisplayTextLength = 160;

int numerusCount(qsizetype count)
{
    return static_cast<int>(qMin(
        count,
        static_cast<qsizetype>(std::numeric_limits<int>::max())
    ));
}

QString boundedDisplayText(const QString &text)
{
    QString displayed;
    displayed.reserve(qMin(text.size(), maximumDisplayTextLength));
    for (const QChar character : text.trimmed()) {
        switch (character.category()) {
        case QChar::Other_Control:
        case QChar::Other_Format:
        case QChar::Separator_Line:
        case QChar::Separator_Paragraph:
            displayed.append(QLatin1Char(' '));
            break;
        default:
            displayed.append(character);
            break;
        }
    }
    displayed = displayed.simplified();
    if (displayed.size() <= maximumDisplayTextLength)
        return displayed;
    return displayed.left(maximumDisplayTextLength - 1) + QChar(0x2026);
}

QString hostAndPort(const CredentialTarget &target, bool omitDefaultHttpsPort)
{
    const QString displayedHost = boundedDisplayText(target.host);
    const QString host = displayedHost.contains(QLatin1Char(':'))
        ? QStringLiteral("[%1]").arg(displayedHost)
        : displayedHost;
    if (omitDefaultHttpsPort && target.port == 443)
        return host;
    return QStringLiteral("%1:%2").arg(host).arg(target.port);
}

QString targetTitle(const CredentialTarget &target)
{
    if (target.kind == CredentialKind::HttpServer) {
        return QStringLiteral("https://%1").arg(
            hostAndPort(target, true)
        );
    }
    return hostAndPort(target, false);
}

QString targetKindText(const CredentialTarget &target)
{
    return target.kind == CredentialKind::HttpServer
        ? QCoreApplication::translate("CredentialsSettingsPage", "Website")
        : QCoreApplication::translate("CredentialsSettingsPage", "Proxy");
}

} // namespace

CredentialsSettingsPage::CredentialsSettingsPage(
    CredentialStore *credentialStore,
    QWidget *parent
)
    : QWidget(parent)
{
    if (credentialStore) {
        m_credentialStore = std::shared_ptr<CredentialStore>(
            credentialStore,
            [](CredentialStore *) {}
        );
    } else {
        m_credentialStore = createSystemCredentialStore();
    }

    setObjectName(QStringLiteral("credentialsSettingsPage"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);

    auto *title = new QLabel(tr("Saved Credentials"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        tr(
            "Review usernames saved by PanBrowser and remove credentials you "
            "no longer want to keep."
        ),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *privacyCard = new QFrame(this);
    privacyCard->setObjectName(QStringLiteral("settingsCard"));
    auto *privacyLayout = new QVBoxLayout(privacyCard);
    privacyLayout->setContentsMargins(18, 14, 18, 14);
    privacyLayout->setSpacing(7);
    auto *privacyTitle = new QLabel(
        tr("Passwords stay hidden"),
        privacyCard
    );
    privacyTitle->setObjectName(QStringLiteral("settingsCardTitle"));
    privacyLayout->addWidget(privacyTitle);
    auto *privacyHint = new QLabel(
        tr(
            "Password values are stored by the operating system and are never "
            "displayed here. Deletions take effect immediately and are not "
            "reverted by Cancel."
        ),
        privacyCard
    );
    privacyHint->setObjectName(QStringLiteral("fieldHint"));
    privacyHint->setWordWrap(true);
    privacyLayout->addWidget(privacyHint);
    layout->addWidget(privacyCard);

    auto *section = new QLabel(tr("SAVED LOGINS"), this);
    section->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(section);

    m_credentials = new QListWidget(this);
    m_credentials->setObjectName(QStringLiteral("credentialsList"));
    m_credentials->setIconSize(QSize(24, 24));
    m_credentials->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_credentials->setSpacing(3);
    layout->addWidget(m_credentials, 1);

    auto *footer = new QHBoxLayout();
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("fieldHint"));
    m_status->setWordWrap(true);
    footer->addWidget(m_status, 1);

    m_refresh = new QPushButton(tr("Refresh"), this);
    m_refresh->setObjectName(QStringLiteral("refreshCredentials"));
    m_removeSelected = new QPushButton(tr("Remove selected…"), this);
    m_removeSelected->setObjectName(QStringLiteral("removeSelectedCredentials"));
    m_removeAll = new QPushButton(tr("Remove all…"), this);
    m_removeAll->setObjectName(QStringLiteral("removeAllCredentials"));
    m_removeSelected->setProperty("dangerAction", true);
    m_removeAll->setProperty("dangerAction", true);
    footer->addWidget(m_refresh);
    footer->addWidget(m_removeSelected);
    footer->addWidget(m_removeAll);
    layout->addLayout(footer);

    connect(
        m_credentials,
        &QListWidget::itemSelectionChanged,
        this,
        &CredentialsSettingsPage::updateActions
    );
    connect(
        m_refresh,
        &QPushButton::clicked,
        this,
        &CredentialsSettingsPage::reload
    );
    connect(
        m_removeSelected,
        &QPushButton::clicked,
        this,
        &CredentialsSettingsPage::removeSelected
    );
    connect(
        m_removeAll,
        &QPushButton::clicked,
        this,
        &CredentialsSettingsPage::removeAll
    );
    updateActions();
}

void CredentialsSettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_loaded)
        reload();
}

void CredentialsSettingsPage::reload()
{
    if (m_busy)
        return;
    m_loaded = true;
    if (!m_credentialStore) {
        CredentialStoreListResult result;
        result.error.code = CredentialStoreErrorCode::Unavailable;
        result.error.message = tr("The system password manager is unavailable");
        applyListResult(std::move(result));
        return;
    }

    setBusy(true, tr("Loading saved credentials…"));
    auto *watcher = new QFutureWatcher<CredentialStoreListResult>(this);
    connect(
        watcher,
        &QFutureWatcher<CredentialStoreListResult>::finished,
        this,
        [this, watcher] {
            CredentialStoreListResult result = watcher->result();
            watcher->deleteLater();
            applyListResult(std::move(result));
        }
    );
    watcher->setFuture(m_credentialStore->listAsync());
}

void CredentialsSettingsPage::applyListResult(CredentialStoreListResult result)
{
    QStringList selectedIdentifiers;
    for (const QListWidgetItem *item : m_credentials->selectedItems())
        selectedIdentifiers.append(item->data(Qt::UserRole).toString());

    m_credentials->clear();
    m_summaries = std::move(result.summaries);
    m_listError = result.error.code;
    std::sort(
        m_summaries.begin(),
        m_summaries.end(),
        [](const StoredCredentialSummary &left, const StoredCredentialSummary &right) {
            if (left.lastModified.isValid() != right.lastModified.isValid())
                return left.lastModified.isValid();
            if (left.lastModified != right.lastModified)
                return left.lastModified > right.lastModified;
            return targetTitle(left.target).compare(
                targetTitle(right.target),
                Qt::CaseInsensitive
            ) < 0;
        }
    );

    for (const StoredCredentialSummary &summary : std::as_const(m_summaries)) {
        const QString identifier = summary.target.identifier();
        const QString username = boundedDisplayText(summary.username);
        const QString realm = boundedDisplayText(summary.target.realm);
        const QString modified = summary.lastModified.isValid()
            ? QLocale().toString(
                summary.lastModified.toLocalTime(),
                QLocale::ShortFormat
            )
            : tr("Unknown date");
        const QString visibleUsername = username.isEmpty()
            ? tr("No username")
            : username;
        const QString visibleRealm = realm.isEmpty() ? tr("No realm") : realm;
        const QString details = tr("%1 · Username: %2 · Realm: %3 · Updated: %4")
            .arg(
                targetKindText(summary.target),
                visibleUsername,
                visibleRealm,
                modified
            );
        auto *item = new QListWidgetItem(
            QIcon(QStringLiteral(":/assets/icons/key-round.svg")),
            QStringLiteral("%1\n%2").arg(targetTitle(summary.target), details),
            m_credentials
        );
        item->setData(Qt::UserRole, identifier);
        item->setToolTip(
            QStringLiteral("%1\n%2").arg(targetTitle(summary.target), details)
        );
        item->setSizeHint(QSize(0, 54));
        if (selectedIdentifiers.contains(identifier))
            item->setSelected(true);
    }

    if (m_summaries.isEmpty()) {
        auto *empty = new QListWidgetItem(
            result.error.shouldReport()
                ? tr("No readable credentials")
                : tr("No saved credentials"),
            m_credentials
        );
        empty->setFlags(Qt::NoItemFlags);
    }

    if (result.error.shouldReport()) {
        if (m_summaries.isEmpty()) {
            const QString errorText = boundedDisplayText(result.error.message);
            m_status->setText(
                tr("Saved credentials could not be loaded: %1").arg(
                    errorText.isEmpty() ? tr("Unknown error") : errorText
                )
            );
        } else {
            m_status->setText(
                tr(
                    "%n saved credential(s). Some entries could not be read.",
                    nullptr,
                    numerusCount(m_summaries.size())
                )
            );
        }
    } else {
        m_status->setText(
            tr(
                "%n saved credential(s)",
                nullptr,
                numerusCount(m_summaries.size())
            )
        );
    }
    setBusy(false);
}

void CredentialsSettingsPage::setBusy(bool busy, const QString &status)
{
    m_busy = busy;
    if (!status.isNull())
        m_status->setText(status);
    m_credentials->setEnabled(!busy);
    updateActions();
}

void CredentialsSettingsPage::setDestructiveOperationActive(bool active)
{
    if (m_destructiveOperationActive == active)
        return;
    m_destructiveOperationActive = active;
    emit destructiveOperationActiveChanged(active);
}

void CredentialsSettingsPage::updateActions()
{
    m_refresh->setEnabled(!m_busy);
    m_removeSelected->setEnabled(
        !m_busy && !selectedTargets().isEmpty()
    );
    m_removeAll->setEnabled(!m_busy && canRemoveAll());
}

bool CredentialsSettingsPage::canRemoveAll() const
{
    if (!m_summaries.isEmpty())
        return true;

    switch (m_listError) {
    case CredentialStoreErrorCode::AccessDenied:
    case CredentialStoreErrorCode::InvalidTarget:
    case CredentialStoreErrorCode::TooLarge:
    case CredentialStoreErrorCode::CorruptData:
    case CredentialStoreErrorCode::PlatformError:
        return true;
    case CredentialStoreErrorCode::None:
    case CredentialStoreErrorCode::NotFound:
    case CredentialStoreErrorCode::Unavailable:
        return false;
    }
    return false;
}

QList<CredentialTarget> CredentialsSettingsPage::selectedTargets() const
{
    QStringList identifiers;
    for (const QListWidgetItem *item : m_credentials->selectedItems()) {
        const QString identifier = item->data(Qt::UserRole).toString();
        if (!identifier.isEmpty())
            identifiers.append(identifier);
    }

    QList<CredentialTarget> targets;
    for (const StoredCredentialSummary &summary : m_summaries) {
        if (identifiers.contains(summary.target.identifier()))
            targets.append(summary.target);
    }
    return targets;
}

void CredentialsSettingsPage::removeSelected()
{
    const QList<CredentialTarget> targets = selectedTargets();
    if (targets.isEmpty())
        return;

    const QString question = targets.size() == 1
        ? tr(
            "Remove the saved credentials for “%1”? The website or proxy may "
            "ask you to sign in again."
        ).arg(
            targetTitle(targets.constFirst())
        )
        : tr(
            "Remove %n selected saved credential(s)? The affected websites and "
            "proxies may ask you to sign in again.",
            nullptr,
            numerusCount(targets.size())
        );
    if (QMessageBox::question(
            this,
            tr("Remove saved credentials"),
            question,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }
    removeTargets(targets);
}

void CredentialsSettingsPage::removeAll()
{
    if (!canRemoveAll()) {
        return;
    }

    const QString question = m_listError != CredentialStoreErrorCode::None
        ? tr(
            "Remove every saved credential managed by PanBrowser, including "
            "entries that could not be displayed? Websites and proxies may ask "
            "you to sign in again."
        )
        : tr(
            "Remove all %n saved credential(s) managed by PanBrowser? Websites "
            "and proxies may ask you to sign in again.",
            nullptr,
            numerusCount(m_summaries.size())
        );

    if (QMessageBox::question(
            this,
            tr("Remove all saved credentials"),
            question,
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel
        ) != QMessageBox::Yes) {
        return;
    }

    setDestructiveOperationActive(true);
    setBusy(true, tr("Removing saved credentials…"));
    auto *watcher = new QFutureWatcher<CredentialStoreOperationResult>(this);
    connect(
        watcher,
        &QFutureWatcher<CredentialStoreOperationResult>::finished,
        this,
        [this, watcher] {
            const CredentialStoreOperationResult result = watcher->result();
            watcher->deleteLater();
            setBusy(false);
            setDestructiveOperationActive(false);
            if (!result.succeeded) {
                const QString errorText = boundedDisplayText(result.error.message);
                QMessageBox::warning(
                    this,
                    tr("Credentials were not removed"),
                    errorText.isEmpty() ? tr("Unknown error") : errorText
                );
            }
            reload();
        }
    );
    watcher->setFuture(m_credentialStore->removeAllAsync());
}

void CredentialsSettingsPage::removeTargets(
    const QList<CredentialTarget> &targets
)
{
    setDestructiveOperationActive(true);
    setBusy(true, tr("Removing saved credentials…"));
    auto *watcher = new QFutureWatcher<CredentialStoreRemovalResult>(this);
    connect(
        watcher,
        &QFutureWatcher<CredentialStoreRemovalResult>::finished,
        this,
        [this, watcher] {
            const CredentialStoreRemovalResult result = watcher->result();
            watcher->deleteLater();
            setBusy(false);
            setDestructiveOperationActive(false);

            QStringList failures;
            for (const CredentialRemovalFailure &failure : result.failures) {
                const QString errorText = boundedDisplayText(failure.error.message);
                failures.append(
                    tr("%1: %2").arg(
                        targetTitle(failure.target),
                        errorText.isEmpty() ? tr("Unknown error") : errorText
                    )
                );
            }
            if (!failures.isEmpty()) {
                QMessageBox::warning(
                    this,
                    tr("Some credentials were not removed"),
                    failures.join(QLatin1Char('\n'))
                );
            }
            reload();
        }
    );
    watcher->setFuture(m_credentialStore->removeAsync(targets));
}
