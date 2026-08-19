#include "BrowserWindowUi.h"

#include "MainWindow.h"

#include "AddressCompletionPopup.h"
#include "AddressLineEdit.h"
#include "BookmarkStore.h"
#include "BrowserFullScreenController.h"
#include "BrowserTabBar.h"
#include "CrossDomainPrompt.h"
#include "CrossDomainPromptController.h"
#include "DownloadManager.h"
#include "DownloadsPanel.h"
#include "FindBar.h"
#include "PageZoom.h"
#include "PermissionController.h"
#include "PermissionPrompt.h"
#include "TabNavigation.h"
#include "WebAppStore.h"
#include "WindowChrome.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QProgressBar>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QWidget>

#include <algorithm>

namespace {

class BrowserTabsContainer final : public QWidget {
public:
    using QWidget::QWidget;

    void setTabControls(BrowserTabBar *tabBar, QWidget *trailingControl)
    {
        m_tabBar = tabBar;
        m_trailingControl = trailingControl;
        scheduleAvailableWidthUpdate();
    }

protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if (event && (event->type() == QEvent::Resize
                      || event->type() == QEvent::Show
                      || event->type() == QEvent::LayoutRequest)) {
            scheduleAvailableWidthUpdate();
        }
        return handled;
    }

private:
    void scheduleAvailableWidthUpdate()
    {
        if (m_updateScheduled)
            return;
        m_updateScheduled = true;
        QTimer::singleShot(0, this, [this] {
            m_updateScheduled = false;
            updateAvailableWidth();
        });
    }

    void updateAvailableWidth()
    {
        if (!m_tabBar)
            return;

        int availableWidth = width();
        if (QLayout *containerLayout = layout()) {
            const QMargins margins = containerLayout->contentsMargins();
            availableWidth -= margins.left() + margins.right();
            if (m_trailingControl) {
                availableWidth -= m_trailingControl->sizeHint().width();
                availableWidth -= containerLayout->spacing();
            }
        }
        m_tabBar->setAvailableWidth(std::max(0, availableWidth));
    }

    BrowserTabBar *m_tabBar = nullptr;
    QWidget *m_trailingControl = nullptr;
    bool m_updateScheduled = false;
};

} // namespace

void BrowserWindowUi::build(MainWindow *window)
{
    QFile themeFile(QStringLiteral(":/assets/theme.qss"));
    if (themeFile.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(themeFile.readAll()));

    window->resize(1180, 760);
    window->setWindowTitle(
        window->m_windowRole == MainWindow::WindowRole::WebApp ? window->m_webApp.name : QStringLiteral("PanBrowser")
    );
    QIcon windowIcon(QStringLiteral(":/assets/app-icon.png"));
    if (window->m_windowRole == MainWindow::WindowRole::WebApp && !window->m_webApp.iconPng.isEmpty()) {
        QPixmap pixmap;
        if (pixmap.loadFromData(window->m_webApp.iconPng, "PNG"))
            windowIcon = QIcon(pixmap);
    }
    window->setWindowIcon(windowIcon);

    tabStack = new QStackedWidget(window);
    tabStack->setObjectName(QStringLiteral("browserTabs"));
    window->setCentralWidget(tabStack);

    QToolBar *tabsToolbar = new QToolBar(MainWindow::tr("Tabs"), window);
    tabsToolbar->setObjectName(QStringLiteral("tabsBar"));
    tabsToolbar->setMovable(false);
    tabsToolbar->setFloatable(false);
    tabsToolbar->setIconSize(QSize(17, 17));
    tabsToolbar->setProperty("integratedChrome", window->m_integratedWindowChrome);

    auto *tabsContainer = new BrowserTabsContainer(tabsToolbar);
    tabsContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QHBoxLayout *tabsLayout = new QHBoxLayout(tabsContainer);
    tabsLayout->setContentsMargins(0, 0, 0, 0);
    tabsLayout->setSpacing(4);

    tabBar = new BrowserTabBar(tabsContainer);
    tabBar->setObjectName(QStringLiteral("browserTabBar"));
    tabBar->setDocumentMode(true);
    tabBar->setTabsClosable(true);
    tabBar->setMovable(true);
    tabBar->setExpanding(false);
    tabBar->setElideMode(Qt::ElideRight);
    tabBar->setUsesScrollButtons(true);
    tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    if (window->m_windowRole == MainWindow::WindowRole::Primary)
        tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    tabsLayout->addWidget(tabBar, 0, Qt::AlignBottom);

    QToolButton *newTabButton = new QToolButton(tabsContainer);
    newTabButton->setObjectName(QStringLiteral("newTabButton"));
    newTabButton->setIcon(QIcon(QStringLiteral(":/assets/icons/plus.svg")));
    const QList<QKeySequence> newTabShortcuts = TabNavigation::newTabShortcuts();
    const QString newTabShortcutText = newTabShortcuts.isEmpty()
        ? QString()
        : newTabShortcuts.constFirst().toString(QKeySequence::NativeText);
    newTabButton->setToolTip(
        newTabShortcutText.isEmpty()
            ? MainWindow::tr("New Tab")
            : MainWindow::tr("New Tab (%1)").arg(newTabShortcutText)
    );
    tabsLayout->addWidget(newTabButton, 0, Qt::AlignBottom);
    tabsLayout->addStretch(1);
    tabsContainer->setTabControls(tabBar, newTabButton);
    tabsToolbar->addWidget(tabsContainer);

    if (window->m_integratedWindowChrome) {
        windowChromeController = new WindowChromeController(
            window,
            tabsLayout,
            {tabsToolbar, tabsContainer},
            window
        );
    }

    window->addToolBar(Qt::TopToolBarArea, tabsToolbar);
    window->addToolBarBreak(Qt::TopToolBarArea);

    QToolBar *toolbar = new QToolBar(MainWindow::tr("Navigation"), window);
    toolbar->setObjectName(QStringLiteral("navigationBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(19, 19));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    backAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-left.svg")),
        MainWindow::tr("Back")
    );
    forwardAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        MainWindow::tr("Forward")
    );
    reloadAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        MainWindow::tr("Reload")
    );
    backAction->setEnabled(false);
    forwardAction->setEnabled(false);
    reloadAction->setEnabled(false);

    address = new AddressLineEdit(toolbar);
    address->setObjectName(QStringLiteral("addressBar"));
    window->updateAddressPlaceholder();
    address->setClearButtonEnabled(true);
    securityIndicator = address->addAction(
        QIcon(),
        QLineEdit::LeadingPosition
    );
    bookmarkAction = address->addAction(
        QIcon(QStringLiteral(":/assets/icons/star.svg")),
        QLineEdit::TrailingPosition
    );
    bookmarkAction->setEnabled(false);
    bookmarkAction->setToolTip(MainWindow::tr("Add Bookmark (⌘D)"));
    readerModeAction = new QAction(
        QIcon(QStringLiteral(":/assets/icons/book-open.svg")),
        MainWindow::tr("Reader Mode"),
        window
    );
    readerModeAction->setCheckable(true);
    readerModeAction->setShortcut(QKeySequence(Qt::Key_F9));
    readerModeAction->setShortcutContext(Qt::WindowShortcut);
    readerModeAction->setAutoRepeat(false);
    readerModeAction->setVisible(false);
    address->addAction(readerModeAction, QLineEdit::TrailingPosition);
    QObject::connect(readerModeAction, &QAction::triggered, window, [window] {
        window->toggleReaderMode(window->commandTargetWebView());
    });
    addressCompletionPopup = new AddressCompletionPopup(address, window);
    window->m_addressSuggestionTimer = new QTimer(window);
    window->m_addressSuggestionTimer->setSingleShot(true);
    window->m_addressSuggestionTimer->setInterval(100);
    QAction *addressWidgetAction = toolbar->addWidget(address);
    QAction *go = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        MainWindow::tr("Go")
    );
    downloadButton = new DownloadButton(toolbar);
    downloadButton->setObjectName(QStringLiteral("downloadsButton"));
    downloadButton->setIcon(QIcon(QStringLiteral(":/assets/icons/download.svg")));
    downloadButton->setToolTip(MainWindow::tr("Downloads"));
    toolbar->addWidget(downloadButton);
    window->addToolBar(Qt::TopToolBarArea, toolbar);

    window->addToolBarBreak(Qt::TopToolBarArea);
    findToolbar = new QToolBar(MainWindow::tr("Find in page"), window);
    findToolbar->setObjectName(QStringLiteral("findBar"));
    findToolbar->setMovable(false);
    findToolbar->setFloatable(false);
    findBar = new FindBar(findToolbar);
    findToolbar->addWidget(findBar);
    window->addToolBar(Qt::TopToolBarArea, findToolbar);
    findToolbar->hide();

    window->addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *permissionToolbar = new QToolBar(
        MainWindow::tr("Permission request"),
        window
    );
    permissionToolbar->setObjectName(QStringLiteral("permissionBar"));
    permissionToolbar->setMovable(false);
    permissionToolbar->setFloatable(false);
    permissionPrompt = new PermissionPrompt(permissionToolbar);
    permissionToolbar->addWidget(permissionPrompt);
    window->addToolBar(Qt::TopToolBarArea, permissionToolbar);
    permissionToolbar->hide();

    window->addToolBarBreak(Qt::TopToolBarArea);
    QToolBar *crossDomainToolbar = new QToolBar(
        MainWindow::tr("Site connection request"),
        window
    );
    crossDomainToolbar->setObjectName(QStringLiteral("crossDomainBar"));
    crossDomainToolbar->setMovable(false);
    crossDomainToolbar->setFloatable(false);
    crossDomainPrompt = new CrossDomainPrompt(crossDomainToolbar);
    crossDomainToolbar->addWidget(crossDomainPrompt);
    window->addToolBar(Qt::TopToolBarArea, crossDomainToolbar);
    crossDomainToolbar->hide();

    QObject::connect(findBar, &FindBar::queryChanged, window, [window] {
        window->findInPage(false);
    });
    QObject::connect(findBar, &FindBar::navigationRequested, window, &MainWindow::findInPage);
    QObject::connect(findBar, &FindBar::closeRequested, window, &MainWindow::closeFindBar);
    closeFindAction = new QAction(window);
    closeFindAction->setShortcut(QKeySequence(Qt::Key_Escape));
    closeFindAction->setShortcutContext(Qt::WindowShortcut);
    closeFindAction->setEnabled(false);
    window->addAction(closeFindAction);
    QObject::connect(closeFindAction, &QAction::triggered, window, &MainWindow::closeFindBar);

    downloadsPanel = new DownloadsPanel(window->m_downloadManager, window);
    downloadButton->setActiveCount(window->m_downloadManager->activeCount());
    QObject::connect(downloadButton, &QToolButton::clicked, window, [
        panel = downloadsPanel,
        button = downloadButton,
        window
    ] {
        if (panel->isVisible())
            panel->hide();
        else
            panel->showBelow(button);
    });
    QObject::connect(
        window->m_downloadManager,
        &DownloadManager::activeCountChanged,
        downloadButton,
        &DownloadButton::setActiveCount
    );
    QObject::connect(window->m_downloadManager, &DownloadManager::recordAdded, window, [
        panel = downloadsPanel,
        button = downloadButton,
        window
    ] {
        if (window->isActiveWindow() && window->m_windowRole != MainWindow::WindowRole::WebApp)
            panel->showBelow(button);
    });

    QObject::connect(newTabButton, &QToolButton::clicked, window, &MainWindow::openNewTab);
    QObject::connect(tabBar, &QTabBar::currentChanged, window, [
        stack = tabStack,
        findToolbar = findToolbar,
        window
    ](int index) {
        if (window->m_fullScreenController && window->m_fullScreenController->isActive()) {
            QWebEngineView *requestedView = index >= 0
                ? qobject_cast<QWebEngineView *>(stack->widget(index))
                : nullptr;
            QWebEngineView *fullScreenView = window->m_fullScreenController->webView();
            if (fullScreenView && requestedView != fullScreenView)
                window->requestBrowserFullScreenExit(fullScreenView);
        }
        window->m_zoomAngleRemainder = 0;
        window->m_zoomPixelRemainder = 0;
        if (index >= 0) {
            stack->setCurrentIndex(index);
            if (!window->m_restoringSession)
                window->activatePendingTab(window->currentWebView());
        }
        if (window->m_permissionController)
            window->m_permissionController->currentViewChanged(window->currentWebView());
        if (window->m_crossDomainPromptController)
            window->m_crossDomainPromptController->currentViewChanged(window->currentWebView());
        if (window->isActiveWindow())
            window->m_lastInteractionWebView = window->currentWebView();
        if (window->m_externalUrlSource && window->m_externalUrlSource != window->currentWebView()) {
            const auto sourceState = window->m_tabStates.constFind(window->m_externalUrlSource);
            if (sourceState == window->m_tabStates.cend() || !sourceState->detachedVideoWindow)
                window->cancelExternalUrlPrompt();
        }
        window->updateCurrentTabUi();
        window->updateTabNavigationActions();
        if (findToolbar && findToolbar->isVisible())
            window->findInPage(false);
        window->scheduleSessionSave();
    });
    QObject::connect(tabBar, &QTabBar::tabCloseRequested, window, &MainWindow::closeTab);
    QObject::connect(tabBar, &QTabBar::tabMoved, window, [
        stack = tabStack,
        tabs = tabBar,
        window
    ](int from, int to) {
        QWidget *webView = stack->widget(from);
        if (!webView)
            return;
        stack->removeWidget(webView);
        stack->insertWidget(to, webView);
        stack->setCurrentIndex(tabs->currentIndex());
        window->scheduleSessionSave();
    });
    if (window->m_windowRole == MainWindow::WindowRole::Primary) {
        QObject::connect(
            tabBar,
            &QWidget::customContextMenuRequested,
            window,
            &MainWindow::showTabContextMenu
        );
    }
    QObject::connect(backAction, &QAction::triggered, window, [window] {
        if (QWebEnginePage *page = window->pageForTab(window->currentWebView()))
            page->triggerAction(QWebEnginePage::Back);
    });
    QObject::connect(forwardAction, &QAction::triggered, window, [window] {
        if (QWebEnginePage *page = window->pageForTab(window->currentWebView()))
            page->triggerAction(QWebEnginePage::Forward);
    });
    QObject::connect(reloadAction, &QAction::triggered, window, [window] {
        if (QWebEnginePage *page = window->pageForTab(window->currentWebView()))
            page->triggerAction(QWebEnginePage::Reload);
    });
    QObject::connect(go, &QAction::triggered, window, &MainWindow::navigateFromAddressBar);
    QObject::connect(bookmarkAction, &QAction::triggered, window, &MainWindow::editCurrentBookmark);
    QObject::connect(address, &QLineEdit::returnPressed, window, &MainWindow::navigateFromAddressBar);
    QObject::connect(address, &QLineEdit::textEdited, window, [
        address = address,
        popup = addressCompletionPopup,
        window
    ] {
        address->clearGhostCompletion();
        const bool historyAvailable = window->m_preferences.saveBrowsingHistory()
            && window->m_historyStore
            && window->m_historyStore->isOpen();
        const bool bookmarksAvailable = window->m_bookmarkStore && window->m_bookmarkStore->isOpen();
        if (!historyAvailable && !bookmarksAvailable) {
            popup->hide();
            return;
        }
        window->m_addressSuggestionTimer->start();
    });
    QObject::connect(window->m_addressSuggestionTimer, &QTimer::timeout, window, &MainWindow::showAddressSuggestions);
    QObject::connect(
        addressCompletionPopup,
        &AddressCompletionPopup::urlActivated,
        window,
        [address = address, window](const QUrl &url) {
            address->setText(url.toString());
            window->navigateFromAddressBar();
        }
    );
    QObject::connect(window->m_bookmarkStore, &BookmarkStore::bookmarksChanged, window, &MainWindow::updateBookmarkAction);

#if defined(Q_OS_MACOS)
    QMenu *fileMenu = window->menuBar()->addMenu(MainWindow::tr("Browser"));
#else
    auto *fileMenu = new QMenu(MainWindow::tr("PanBrowser"), window);
#endif
    newTabAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/plus.svg")),
        MainWindow::tr("New Tab")
    );
    newTabAction->setShortcuts(TabNavigation::newTabShortcuts());
    newTabAction->setShortcutContext(Qt::WindowShortcut);
    newTabAction->setAutoRepeat(false);
    QObject::connect(newTabAction, &QAction::triggered, window, &MainWindow::openNewTab);

    closeTabAction = fileMenu->addAction(MainWindow::tr("Close Tab"));
    closeTabAction->setShortcuts(TabNavigation::closeTabShortcuts());
    closeTabAction->setShortcutContext(Qt::WindowShortcut);
    closeTabAction->setAutoRepeat(false);
    QObject::connect(closeTabAction, &QAction::triggered, window, [window] {
        window->closeActiveBrowserSurface(window->commandTargetWebView());
    });

    reopenClosedTabAction = fileMenu->addAction(MainWindow::tr("Reopen Closed Tab"));
    reopenClosedTabAction->setShortcuts(TabNavigation::reopenClosedTabShortcuts());
    reopenClosedTabAction->setShortcutContext(Qt::WindowShortcut);
    reopenClosedTabAction->setAutoRepeat(false);
    QObject::connect(
        reopenClosedTabAction,
        &QAction::triggered,
        window,
        &MainWindow::reopenLastClosedTab
    );

    fileMenu->addSeparator();
    nextTabAction = fileMenu->addAction(MainWindow::tr("Next Tab"));
    nextTabAction->setShortcuts(TabNavigation::nextTabShortcuts());
    nextTabAction->setShortcutContext(Qt::WindowShortcut);
    nextTabAction->setAutoRepeat(false);
    QObject::connect(nextTabAction, &QAction::triggered, window, [window] {
        window->activateAdjacentTab(1);
    });
    previousTabAction = fileMenu->addAction(MainWindow::tr("Previous Tab"));
    previousTabAction->setShortcuts(TabNavigation::previousTabShortcuts());
    previousTabAction->setShortcutContext(Qt::WindowShortcut);
    previousTabAction->setAutoRepeat(false);
    QObject::connect(previousTabAction, &QAction::triggered, window, [window] {
        window->activateAdjacentTab(-1);
    });

    switchToTabMenu = fileMenu->addMenu(MainWindow::tr("Switch to Tab"));
    for (int position = 1; position <= TabNavigation::numberedShortcutCount; ++position) {
        QAction *action = switchToTabMenu->addAction(
            position == TabNavigation::numberedShortcutCount
                ? MainWindow::tr("Last Tab")
                : MainWindow::tr("Tab %1").arg(position)
        );
        action->setShortcut(TabNavigation::numberedTabShortcut(position));
        action->setShortcutContext(Qt::WindowShortcut);
        action->setAutoRepeat(false);
        QObject::connect(action, &QAction::triggered, window, [window, position] {
            window->activateNumberedTab(position);
        });
        numberedTabActions.append(action);
    }

    fileMenu->addSeparator();
    QAction *addBookmarkAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/star.svg")),
        MainWindow::tr("Add Bookmark…")
    );
    addBookmarkAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    QObject::connect(addBookmarkAction, &QAction::triggered, window, &MainWindow::editCurrentBookmark);
    QAction *bookmarksAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/star-filled.svg")),
        MainWindow::tr("Bookmarks…")
    );
    bookmarksAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    QObject::connect(bookmarksAction, &QAction::triggered, window, &MainWindow::openBookmarks);

    installWebAppAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/layout-grid.svg")),
        MainWindow::tr("Install Web App…")
    );
    installWebAppAction->setEnabled(false);
    QObject::connect(
        installWebAppAction,
        &QAction::triggered,
        window,
        &MainWindow::installCurrentWebApp
    );
    webAppsMenu = fileMenu->addMenu(
        QIcon(QStringLiteral(":/assets/icons/layout-grid.svg")),
        MainWindow::tr("Web Apps")
    );
    QObject::connect(webAppsMenu, &QMenu::aboutToShow, window, &MainWindow::rebuildWebAppsMenu);

    fileMenu->addSeparator();
    fileMenu->addAction(readerModeAction);
    QAction *findAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/search.svg")),
        MainWindow::tr("Find in Page…")
    );
    findAction->setShortcut(QKeySequence::Find);
    QObject::connect(findAction, &QAction::triggered, window, &MainWindow::openFindBar);
    QAction *findNextAction = fileMenu->addAction(MainWindow::tr("Find Next"));
    findNextAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    QObject::connect(findNextAction, &QAction::triggered, window, [
        findToolbar = findToolbar,
        window
    ] {
        if (!findToolbar->isVisible())
            window->openFindBar();
        else
            window->findInPage(false);
    });
    QAction *findPreviousAction = fileMenu->addAction(MainWindow::tr("Find Previous"));
    findPreviousAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_G));
    QObject::connect(findPreviousAction, &QAction::triggered, window, [
        findToolbar = findToolbar,
        window
    ] {
        if (!findToolbar->isVisible())
            window->openFindBar();
        else
            window->findInPage(true);
    });

    fileMenu->addSeparator();
    zoomMenu = fileMenu->addMenu(MainWindow::tr("Zoom"));
    zoomLevelAction = zoomMenu->addAction(QStringLiteral("100%"));
    zoomLevelAction->setEnabled(false);
    zoomMenu->addSeparator();
    zoomInAction = zoomMenu->addAction(MainWindow::tr("Zoom In"));
    zoomInAction->setShortcuts(pageZoomInShortcuts());
    zoomInAction->setShortcutContext(Qt::WindowShortcut);
    zoomInAction->setAutoRepeat(false);
    QObject::connect(zoomInAction, &QAction::triggered, window, [window] {
        window->changePageZoomBySteps(window->commandTargetWebView(), 1);
    });
    zoomOutAction = zoomMenu->addAction(MainWindow::tr("Zoom Out"));
    zoomOutAction->setShortcuts(pageZoomOutShortcuts());
    zoomOutAction->setShortcutContext(Qt::WindowShortcut);
    zoomOutAction->setAutoRepeat(false);
    QObject::connect(zoomOutAction, &QAction::triggered, window, [window] {
        window->changePageZoomBySteps(window->commandTargetWebView(), -1);
    });
    resetZoomAction = zoomMenu->addAction(MainWindow::tr("Actual Size"));
    resetZoomAction->setShortcuts(pageZoomResetShortcuts());
    resetZoomAction->setShortcutContext(Qt::WindowShortcut);
    resetZoomAction->setAutoRepeat(false);
    QObject::connect(resetZoomAction, &QAction::triggered, window, [window] {
        window->setPageZoom(window->commandTargetWebView(), defaultPageZoomFactor);
    });
    QObject::connect(zoomMenu, &QMenu::aboutToShow, window, &MainWindow::updateZoomActions);

    fileMenu->addSeparator();
    developerToolsAction = fileMenu->addAction(MainWindow::tr("Developer Tools"));
#if defined(Q_OS_MACOS)
    developerToolsAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I),
        QKeySequence(Qt::Key_F12),
    });
#else
    developerToolsAction->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I),
        QKeySequence(Qt::Key_F12),
    });
#endif
    developerToolsAction->setShortcutContext(Qt::WindowShortcut);
    QObject::connect(developerToolsAction, &QAction::triggered, window, [window] {
        window->openDeveloperTools(window->commandTargetWebView());
    });

    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/settings.svg")),
        MainWindow::tr("Settings…")
    );
    settingsAction->setMenuRole(QAction::NoRole);
    settingsAction->setShortcut(QKeySequence::Preferences);
    QObject::connect(settingsAction, &QAction::triggered, window, &MainWindow::openSettings);

    fileMenu->addSeparator();
    QAction *reloadRulesAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        MainWindow::tr("Reload Trust Rules")
    );
    reloadRulesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    QObject::connect(reloadRulesAction, &QAction::triggered, window, &MainWindow::reloadRules);

    QAction *showConfiguration = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/folder-open.svg")),
        MainWindow::tr("Show Configuration Folder")
    );
    QObject::connect(showConfiguration, &QAction::triggered, window, [window] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(window->m_configurationPath).absolutePath()
        ));
    });

#if !defined(Q_OS_MACOS)
    auto *mainMenuButton = new QToolButton(toolbar);
    mainMenuButton->setObjectName(QStringLiteral("mainMenuButton"));
    mainMenuButton->setIcon(
        QIcon(QStringLiteral(":/assets/icons/ellipsis-vertical.svg"))
    );
    mainMenuButton->setToolTip(fileMenu->title());
    mainMenuButton->setAccessibleName(fileMenu->title());
    mainMenuButton->setFocusPolicy(Qt::NoFocus);
    mainMenuButton->setPopupMode(QToolButton::InstantPopup);
    mainMenuButton->setMenu(fileMenu);
    toolbar->addWidget(mainMenuButton);
#endif

    trustStatus = new QLabel(MainWindow::tr("Ready"), window);
    trustStatus->setObjectName(QStringLiteral("trustStatus"));
    ruleCount = new QLabel(MainWindow::tr("No rules loaded"), window);
    ruleCount->setObjectName(QStringLiteral("ruleCount"));
    progress = new QProgressBar(window);
    progress->setObjectName(QStringLiteral("pageProgress"));
    progress->setRange(0, 100);
    progress->setMaximumWidth(120);
    progress->setTextVisible(false);
    progress->hide();

    window->statusBar()->setSizeGripEnabled(false);
    window->statusBar()->addWidget(trustStatus, 1);
    window->statusBar()->addPermanentWidget(progress);
    window->statusBar()->addPermanentWidget(ruleCount);

    window->m_sessionSaveTimer = new QTimer(window);
    window->m_sessionSaveTimer->setSingleShot(true);
    window->m_sessionSaveTimer->setInterval(750);
    QObject::connect(window->m_sessionSaveTimer, &QTimer::timeout, window, &MainWindow::saveSession);

    if (window->m_webAppStore) {
        QObject::connect(window->m_webAppStore, &WebAppStore::appsChanged, window, [window] {
            window->updateInstallWebAppAction();
            if (window->m_windowRole == MainWindow::WindowRole::WebApp && !window->m_webAppStore->app(window->m_webApp.id))
                window->close();
        });
    }

    if (window->m_windowRole == MainWindow::WindowRole::WebApp) {
        tabsToolbar->hide();
        addressWidgetAction->setVisible(false);
        go->setVisible(false);
        newTabAction->setVisible(false);
        closeTabAction->setText(MainWindow::tr("Close Window"));
        reopenClosedTabAction->setVisible(false);
        nextTabAction->setVisible(false);
        previousTabAction->setVisible(false);
        switchToTabMenu->menuAction()->setVisible(false);
        addBookmarkAction->setVisible(false);
        bookmarksAction->setVisible(false);
        installWebAppAction->setVisible(false);
        webAppsMenu->menuAction()->setVisible(false);
        readerModeAction->setVisible(false);
        settingsAction->setVisible(false);
        reloadRulesAction->setVisible(false);
        showConfiguration->setVisible(false);
        ruleCount->hide();
    }

    window->updateZoomActions();
    window->updateTabNavigationActions();
    window->applyDeveloperToolsPreference();
    qApp->installEventFilter(window);
}
