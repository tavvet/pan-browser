#include "WebAppInstallController.h"

#include "BrowserPage.h"
#include "WebAppShortcutManager.h"
#include "WebAppStore.h"

#include <QAction>
#include <QBuffer>
#include <QCoreApplication>
#include <QHash>
#include <QIODevice>
#include <QMessageBox>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QUuid>
#include <QVariantMap>
#include <QWebEngineScript>
#include <QWebEngineView>
#include <QWidget>

#include <optional>
#include <utility>

namespace {

int effectivePort(const QUrl &url)
{
    if (url.port() >= 0)
        return url.port();
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
        return 443;
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0)
        return 80;
    return -1;
}

bool isSameWebOrigin(const QUrl &left, const QUrl &right)
{
    return left.isValid()
        && right.isValid()
        && left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0
        && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
        && effectivePort(left) == effectivePort(right);
}

QString windowText(const char *source)
{
    return QCoreApplication::translate("MainWindow", source);
}

} // namespace

class WebAppInstallController::Impl {
public:
    struct ManifestState {
        QUrl documentUrl;
        QUrl manifestUrl;
        QString title;
    };

    struct PendingRequest {
        QPointer<QWebEngineView> webView;
        QPointer<BrowserPage> page;
        QUrl manifestUrl;
        QUrl documentUrl;
        QString fallbackTitle;
    };

    WebAppStore *store = nullptr;
    QPointer<QAction> installAction;
    QPointer<QWidget> dialogParent;
    QPointer<QWebEngineView> currentView;
    QHash<QWebEngineView *, ManifestState> manifests;
    QHash<QWebEngineView *, quint64> manifestDetectionGenerations;
    QHash<QString, PendingRequest> pendingRequests;
    quint64 nextManifestDetectionGeneration = 0;
};

WebAppInstallController::WebAppInstallController(
    WebAppStore *store,
    QAction *installAction,
    QWidget *dialogParent,
    QObject *parent
)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->store = store;
    m_impl->installAction = installAction;
    m_impl->dialogParent = dialogParent;
}

WebAppInstallController::~WebAppInstallController() = default;

void WebAppInstallController::clearManifest(QWebEngineView *webView)
{
    if (!webView)
        return;
    m_impl->manifestDetectionGenerations.remove(webView);
    m_impl->manifests.remove(webView);
    if (m_impl->currentView == webView)
        currentViewChanged(webView);
}

void WebAppInstallController::forgetView(QWebEngineView *webView)
{
    if (!webView)
        return;

    m_impl->manifestDetectionGenerations.remove(webView);
    m_impl->manifests.remove(webView);
    for (auto iterator = m_impl->pendingRequests.begin();
         iterator != m_impl->pendingRequests.end();) {
        if (iterator->webView != webView) {
            ++iterator;
            continue;
        }
        if (iterator->page)
            iterator->page->cancelWebAppManifestFetch(iterator.key());
        iterator = m_impl->pendingRequests.erase(iterator);
    }
    if (m_impl->currentView == webView)
        m_impl->currentView.clear();
    currentViewChanged(m_impl->currentView);
}

void WebAppInstallController::detectManifest(
    QWebEngineView *webView,
    BrowserPage *page
)
{
    if (!webView || !page)
        return;

    const QUrl documentUrl = page->url();
    if (documentUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
        clearManifest(webView);
        return;
    }

    const QPointer<WebAppInstallController> controller(this);
    const QPointer<QWebEngineView> target(webView);
    const QPointer<BrowserPage> targetPage(page);
    const quint64 generation = ++m_impl->nextManifestDetectionGeneration;
    m_impl->manifestDetectionGenerations.insert(webView, generation);
    page->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const link = document.querySelector('link[rel~="manifest"]');
    if (!link || !link.href)
        return null;
    return { url: link.href, title: document.title || "" };
})()
)JS"),
        QWebEngineScript::ApplicationWorld,
        [controller,
         target,
         targetPage,
         documentUrl,
         generation](const QVariant &result) {
            if (!controller || !target || !targetPage)
                return;
            const auto pending =
                controller->m_impl->manifestDetectionGenerations.constFind(
                    target
                );
            if (pending
                == controller->m_impl->manifestDetectionGenerations.cend()
                || *pending != generation) {
                return;
            }
            controller->m_impl->manifestDetectionGenerations.erase(pending);

            Impl::ManifestState state;
            state.documentUrl = documentUrl;
            if (targetPage->url() == documentUrl) {
                const QVariantMap object = result.toMap();
                const QUrl manifestUrl(object.value(QStringLiteral("url")).toString());
                if (manifestUrl.scheme().compare(
                        QStringLiteral("https"),
                        Qt::CaseInsensitive
                    ) == 0
                    && manifestUrl.userInfo().isEmpty()
                    && isSameWebOrigin(manifestUrl, documentUrl)) {
                    state.manifestUrl = manifestUrl.adjusted(
                        QUrl::NormalizePathSegments
                    );
                    state.title = object.value(
                        QStringLiteral("title")
                    ).toString().simplified();
                }
            }
            controller->m_impl->manifests.insert(target, std::move(state));
            if (controller->m_impl->currentView == target)
                controller->currentViewChanged(target);
        }
    );
}

void WebAppInstallController::currentViewChanged(QWebEngineView *webView)
{
    m_impl->currentView = webView;
    if (!m_impl->installAction)
        return;

    const auto state = webView ? m_impl->manifests.constFind(webView)
                               : m_impl->manifests.cend();
    if (!webView
        || state == m_impl->manifests.cend()
        || state->manifestUrl.isEmpty()
        || !m_impl->store
        || !m_impl->store->isAvailable()) {
        m_impl->installAction->setText(windowText(QT_TRANSLATE_NOOP(
            "MainWindow",
            "Install Web App…"
        )));
        m_impl->installAction->setEnabled(false);
        return;
    }

    const std::optional<WebApp> installed = m_impl->store->appForManifest(
        state->manifestUrl
    );
    if (installed) {
        m_impl->installAction->setText(
            windowText(QT_TRANSLATE_NOOP("MainWindow", "Open “%1”"))
                .arg(installed->name)
        );
        m_impl->installAction->setEnabled(true);
        return;
    }

    const QString name = state->title.isEmpty()
        ? state->documentUrl.host()
        : state->title.left(80);
    m_impl->installAction->setText(
        windowText(QT_TRANSLATE_NOOP("MainWindow", "Install “%1”…"))
            .arg(name)
    );
    m_impl->installAction->setEnabled(true);
}

void WebAppInstallController::installCurrent(
    QWebEngineView *webView,
    BrowserPage *page
)
{
    if (!webView || !page || !m_impl->store)
        return;

    const auto state = m_impl->manifests.constFind(webView);
    if (state == m_impl->manifests.cend() || state->manifestUrl.isEmpty())
        return;

    if (const std::optional<WebApp> installed = m_impl->store->appForManifest(
            state->manifestUrl
        )) {
        emit openInstalledAppRequested(installed->id);
        return;
    }

    const QString requestId = QUuid::createUuid().toString(
        QUuid::WithoutBraces
    );
    m_impl->pendingRequests.insert(requestId, {
        webView,
        page,
        state->manifestUrl,
        state->documentUrl,
        state->title,
    });
    if (m_impl->installAction) {
        m_impl->installAction->setEnabled(false);
        m_impl->installAction->setText(
            windowText(QT_TRANSLATE_NOOP(
                "MainWindow",
                "Reading web app manifest…"
            ))
        );
    }
    page->fetchWebAppManifest(
        requestId,
        state->manifestUrl,
        WebAppStore::maximumManifestBytes
    );

    QTimer::singleShot(15000, this, [this, requestId] {
        const auto found = m_impl->pendingRequests.find(requestId);
        if (found == m_impl->pendingRequests.end())
            return;
        const QPointer<BrowserPage> page = found->page;
        m_impl->pendingRequests.erase(found);
        if (page)
            page->cancelWebAppManifestFetch(requestId);
        currentViewChanged(m_impl->currentView);
        emit statusMessageRequested(
            windowText(QT_TRANSLATE_NOOP(
                "MainWindow",
                "Timed out while reading the web app manifest"
            )),
            5000
        );
    });
}

void WebAppInstallController::handleManifestFetched(
    const QString &requestId,
    const QByteArray &contents,
    const QString &fetchError
)
{
    const auto found = m_impl->pendingRequests.find(requestId);
    if (found == m_impl->pendingRequests.end())
        return;

    const Impl::PendingRequest request = found.value();
    m_impl->pendingRequests.erase(found);
    currentViewChanged(m_impl->currentView);

    const auto state = request.webView
        ? m_impl->manifests.constFind(request.webView)
        : m_impl->manifests.cend();
    if (!request.webView
        || !request.page
        || state == m_impl->manifests.cend()
        || state->manifestUrl != request.manifestUrl) {
        return;
    }

    if (!fetchError.isEmpty()) {
        QMessageBox::warning(
            m_impl->dialogParent,
            windowText(QT_TRANSLATE_NOOP(
                "MainWindow",
                "Cannot install web app"
            )),
            windowText(QT_TRANSLATE_NOOP(
                "MainWindow",
                "PanBrowser could not read the web app manifest: %1"
            ))
                .arg(fetchError)
        );
        return;
    }

    QString error;
    std::optional<WebApp> app = WebAppStore::parseManifest(
        contents,
        request.manifestUrl,
        request.documentUrl,
        request.fallbackTitle,
        &error
    );
    if (!app) {
        QMessageBox::warning(
            m_impl->dialogParent,
            windowText(QT_TRANSLATE_NOOP(
                "MainWindow",
                "Cannot install web app"
            )),
            error
        );
        return;
    }

    const QPixmap pixmap = request.page->icon().pixmap(256, 256);
    if (!pixmap.isNull()) {
        QByteArray iconPng;
        QBuffer buffer(&iconPng);
        if (buffer.open(QIODevice::WriteOnly)
            && pixmap.save(&buffer, "PNG")
            && iconPng.size() <= WebAppStore::maximumIconBytes) {
            app->iconPng = iconPng;
        }
    }

    QMessageBox dialog(m_impl->dialogParent);
    dialog.setWindowTitle(windowText(QT_TRANSLATE_NOOP(
        "MainWindow",
        "Install web app"
    )));
    dialog.setIcon(QMessageBox::Question);
    if (!pixmap.isNull()) {
        dialog.setIconPixmap(
            pixmap.scaled(
                72,
                72,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            )
        );
    }
    dialog.setText(
        windowText(QT_TRANSLATE_NOOP("MainWindow", "Install “%1”?"))
            .arg(app->name)
    );
    dialog.setInformativeText(
        windowText(QT_TRANSLATE_NOOP(
            "MainWindow",
            "The app will open in its own window and share PanBrowser cookies, "
            "site data, permissions, and trust rules.\n\n"
            "Start page: %1\nAllowed scope: %2"
        )).arg(
            app->startUrl.toDisplayString(QUrl::RemovePassword),
            app->scope.toDisplayString(QUrl::RemovePassword)
        )
    );
    QPushButton *installButton = dialog.addButton(
        windowText(QT_TRANSLATE_NOOP("MainWindow", "Install")),
        QMessageBox::AcceptRole
    );
    dialog.addButton(
        windowText(QT_TRANSLATE_NOOP("MainWindow", "Cancel")),
        QMessageBox::RejectRole
    );
    dialog.setDefaultButton(installButton);
    dialog.exec();
    if (dialog.clickedButton() != installButton)
        return;

    if (!m_impl->store->install(*app, &error)) {
        QMessageBox::warning(
            m_impl->dialogParent,
            windowText(QT_TRANSLATE_NOOP(
                "MainWindow",
                "Cannot install web app"
            )),
            error
        );
        return;
    }

    WebAppShortcutManager shortcutManager;
    if (shortcutManager.isSupported()) {
        QString shortcutError;
        if (!shortcutManager.createOrUpdate(*app, &shortcutError)) {
            QMessageBox::warning(
                m_impl->dialogParent,
                windowText(QT_TRANSLATE_NOOP(
                    "MainWindow",
                    "Web app installed without a system shortcut"
                )),
                windowText(QT_TRANSLATE_NOOP(
                    "MainWindow",
                    "PanBrowser installed the web app, but could not create "
                    "its system shortcut: %1"
                )).arg(shortcutError)
            );
        }
    }

    emit statusMessageRequested(
        windowText(QT_TRANSLATE_NOOP("MainWindow", "“%1” installed"))
            .arg(app->name),
        4000
    );
    emit openInstalledAppRequested(app->id);
}
