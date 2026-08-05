#include "MainWindow.h"

#include "BrowserProfile.h"
#include "CertificateTrustValidator.h"
#include "TrustRulesDialog.h"
#include "WindowPlacement.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QIcon>
#include <QProgressBar>
#include <QSaveFile>
#include <QScreen>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QToolBar>
#include <QToolButton>
#include <QWebEngineCertificateError>
#include <QWebEngineHistory>
#include <QWebEngineNewWindowRequest>
#include <QWebEnginePage>
#include <QWebEngineView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_profile = new BrowserProfile();
    createInterface();
    restoreWindowPlacement();
    reloadRules();

    const QStringList arguments = QApplication::arguments();
    const QUrl initialUrl = arguments.size() > 1
        ? QUrl::fromUserInput(arguments.at(1))
        : m_trustPolicy.startPage();
    createTab(initialUrl);
}

MainWindow::~MainWindow()
{
    delete takeCentralWidget();
    m_tabStack = nullptr;
    m_tabStates.clear();
    delete m_profile;
    m_profile = nullptr;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("state"), saveState(1));
    settings.endGroup();
    settings.sync();
    QMainWindow::closeEvent(event);
}

void MainWindow::createInterface()
{
    QFile themeFile(QStringLiteral(":/assets/theme.qss"));
    if (themeFile.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(themeFile.readAll()));

    resize(1180, 760);
    setWindowTitle(QStringLiteral("PanBrowser"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.svg")));

    m_tabStack = new QStackedWidget(this);
    m_tabStack->setObjectName(QStringLiteral("browserTabs"));
    setCentralWidget(m_tabStack);

    QToolBar *tabsToolbar = new QToolBar(QStringLiteral("Tabs"), this);
    tabsToolbar->setObjectName(QStringLiteral("tabsBar"));
    tabsToolbar->setMovable(false);
    tabsToolbar->setFloatable(false);
    tabsToolbar->setIconSize(QSize(17, 17));

    m_tabBar = new QTabBar(tabsToolbar);
    m_tabBar->setObjectName(QStringLiteral("browserTabBar"));
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setElideMode(Qt::ElideRight);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    m_tabBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tabsToolbar->addWidget(m_tabBar);

    QToolButton *newTabButton = new QToolButton(tabsToolbar);
    newTabButton->setObjectName(QStringLiteral("newTabButton"));
    newTabButton->setIcon(QIcon(QStringLiteral(":/assets/icons/plus.svg")));
    newTabButton->setToolTip(QStringLiteral("New Tab (⌘T)"));
    tabsToolbar->addWidget(newTabButton);

    addToolBar(Qt::TopToolBarArea, tabsToolbar);
    addToolBarBreak(Qt::TopToolBarArea);

    QToolBar *toolbar = new QToolBar(QStringLiteral("Navigation"), this);
    toolbar->setObjectName(QStringLiteral("navigationBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(19, 19));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    m_backAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-left.svg")),
        QStringLiteral("Back")
    );
    m_forwardAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        QStringLiteral("Forward")
    );
    m_reloadAction = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        QStringLiteral("Reload")
    );
    m_backAction->setEnabled(false);
    m_forwardAction->setEnabled(false);
    m_reloadAction->setEnabled(false);

    m_address = new QLineEdit(toolbar);
    m_address->setObjectName(QStringLiteral("addressBar"));
    m_address->setPlaceholderText(QStringLiteral("https://example.com"));
    m_address->setClearButtonEnabled(true);
    m_securityIndicator = m_address->addAction(
        QIcon(),
        QLineEdit::LeadingPosition
    );
    toolbar->addWidget(m_address);
    QAction *go = toolbar->addAction(
        QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")),
        QStringLiteral("Go")
    );
    addToolBar(Qt::TopToolBarArea, toolbar);

    connect(newTabButton, &QToolButton::clicked, this, [this] {
        createTab(m_trustPolicy.startPage());
    });
    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) {
        if (index >= 0)
            m_tabStack->setCurrentIndex(index);
        updateCurrentTabUi();
    });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        QWidget *webView = m_tabStack->widget(from);
        if (!webView)
            return;
        m_tabStack->removeWidget(webView);
        m_tabStack->insertWidget(to, webView);
        m_tabStack->setCurrentIndex(m_tabBar->currentIndex());
    });
    connect(m_backAction, &QAction::triggered, this, [this] {
        if (QWebEngineView *webView = currentWebView())
            webView->back();
    });
    connect(m_forwardAction, &QAction::triggered, this, [this] {
        if (QWebEngineView *webView = currentWebView())
            webView->forward();
    });
    connect(m_reloadAction, &QAction::triggered, this, [this] {
        if (QWebEngineView *webView = currentWebView())
            webView->reload();
    });
    connect(go, &QAction::triggered, this, &MainWindow::navigateFromAddressBar);
    connect(m_address, &QLineEdit::returnPressed, this, &MainWindow::navigateFromAddressBar);

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("PanBrowser"));
    QAction *newTabAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/plus.svg")),
        QStringLiteral("New Tab")
    );
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(newTabAction, &QAction::triggered, this, [this] {
        createTab(m_trustPolicy.startPage());
    });

    QAction *closeTabAction = fileMenu->addAction(QStringLiteral("Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, [this] {
        closeTab(m_tabBar->currentIndex());
    });

    fileMenu->addSeparator();
    QAction *editRulesAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/shield-check.svg")),
        QStringLiteral("Trust Rules…")
    );
    editRulesAction->setMenuRole(QAction::PreferencesRole);
    editRulesAction->setShortcut(QKeySequence::Preferences);
    connect(editRulesAction, &QAction::triggered, this, &MainWindow::openTrustRules);

    fileMenu->addSeparator();
    QAction *reloadRulesAction = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")),
        QStringLiteral("Reload Trust Rules")
    );
    reloadRulesAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
    connect(reloadRulesAction, &QAction::triggered, this, &MainWindow::reloadRules);

    QAction *showConfiguration = fileMenu->addAction(
        QIcon(QStringLiteral(":/assets/icons/folder-open.svg")),
        QStringLiteral("Show Configuration Folder")
    );
    connect(showConfiguration, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
            QFileInfo(m_configurationPath).absolutePath()
        ));
    });

    m_trustStatus = new QLabel(QStringLiteral("Ready"), this);
    m_trustStatus->setObjectName(QStringLiteral("trustStatus"));
    m_ruleCount = new QLabel(QStringLiteral("No rules loaded"), this);
    m_ruleCount->setObjectName(QStringLiteral("ruleCount"));
    m_progress = new QProgressBar(this);
    m_progress->setObjectName(QStringLiteral("pageProgress"));
    m_progress->setRange(0, 100);
    m_progress->setMaximumWidth(120);
    m_progress->setTextVisible(false);
    m_progress->hide();

    statusBar()->setSizeGripEnabled(false);
    statusBar()->addWidget(m_trustStatus, 1);
    statusBar()->addPermanentWidget(m_progress);
    statusBar()->addPermanentWidget(m_ruleCount);
}

QWebEngineView *MainWindow::createTab(const QUrl &url, bool activate)
{
    QWebEngineView *webView = new QWebEngineView(m_tabStack);
    webView->setPage(new QWebEnginePage(m_profile, webView));
    m_tabStates.insert(webView, BrowserTabState());
    connectBrowserSignals(webView);

    const int stackIndex = m_tabStack->addWidget(webView);
    const int tabIndex = m_tabBar->addTab(QStringLiteral("New Tab"));
    Q_ASSERT(stackIndex == tabIndex);
    m_tabBar->setTabToolTip(tabIndex, QStringLiteral("New Tab"));

    if (activate)
        m_tabBar->setCurrentIndex(tabIndex);
    if (url.isValid() && !url.isEmpty())
        webView->setUrl(url);

    return webView;
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabBar->count())
        return;

    QWebEngineView *webView = qobject_cast<QWebEngineView *>(m_tabStack->widget(index));
    if (!webView)
        return;

    disconnect(webView, nullptr, this, nullptr);
    disconnect(webView->page(), nullptr, this, nullptr);
    webView->stop();
    m_tabStates.remove(webView);
    m_tabStack->removeWidget(webView);
    m_tabBar->removeTab(index);
    webView->deleteLater();

    if (m_tabBar->count() == 0) {
        close();
        return;
    }

    m_tabStack->setCurrentIndex(m_tabBar->currentIndex());
    updateCurrentTabUi();
}

QWebEngineView *MainWindow::currentWebView() const
{
    return qobject_cast<QWebEngineView *>(m_tabStack->currentWidget());
}

void MainWindow::connectBrowserSignals(QWebEngineView *webView)
{
    connect(webView, &QWebEngineView::urlChanged, this, [this, webView](const QUrl &url) {
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0 && webView->title().isEmpty()) {
            const QString label = url.host().isEmpty() ? QStringLiteral("New Tab") : url.host();
            m_tabBar->setTabText(index, label);
            m_tabBar->setTabToolTip(index, url.toString());
        }
        if (webView == currentWebView())
            m_address->setText(url.toString());
        updateNavigationActions();
    });
    connect(webView, &QWebEngineView::titleChanged, this, [this, webView](const QString &title) {
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0) {
            const QString label = title.isEmpty()
                ? (webView->url().host().isEmpty() ? QStringLiteral("New Tab") : webView->url().host())
                : title;
            m_tabBar->setTabText(index, label);
            m_tabBar->setTabToolTip(index, title.isEmpty() ? webView->url().toString() : title);
        }
        if (webView == currentWebView())
            setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));
    });
    connect(webView, &QWebEngineView::iconChanged, this, [this, webView](const QIcon &icon) {
        const int index = m_tabStack->indexOf(webView);
        if (index >= 0)
            m_tabBar->setTabIcon(index, icon);
    });
    connect(webView, &QWebEngineView::loadStarted, this, [this, webView] {
        BrowserTabState &state = m_tabStates[webView];
        state.lastAcceptedRule.clear();
        state.loading = true;
        state.progress = 0;
        setTabTrustStatus(webView, QStringLiteral("Loading…"));
        if (webView == currentWebView()) {
            m_progress->setValue(0);
            m_progress->show();
        }
        updateNavigationActions();
    });
    connect(webView, &QWebEngineView::loadProgress, this, [this, webView](int progress) {
        m_tabStates[webView].progress = progress;
        if (webView == currentWebView())
            m_progress->setValue(progress);
    });
    connect(webView, &QWebEngineView::loadFinished, this, [this, webView](bool ok) {
        BrowserTabState &state = m_tabStates[webView];
        state.loading = false;
        state.progress = 100;
        if (webView == currentWebView())
            m_progress->hide();
        if (ok) {
            if (state.lastAcceptedRule.isEmpty())
                setTabTrustStatus(webView, QStringLiteral("Secure · Chromium system trust"));
        } else if (state.lastAcceptedRule.isEmpty()) {
            setTabTrustStatus(webView, QStringLiteral("Page loading failed"), true);
        }
        updateNavigationActions();
    });
    connect(
        webView->page(),
        &QWebEnginePage::certificateError,
        this,
        [this, webView](const QWebEngineCertificateError &error) {
            handleCertificateError(webView, error);
        }
    );
    connect(
        webView->page(),
        &QWebEnginePage::newWindowRequested,
        this,
        [this](QWebEngineNewWindowRequest &request) {
            if (!request.isUserInitiated())
                return;

            const bool activate = request.destination()
                != QWebEngineNewWindowRequest::InNewBackgroundTab;
            QWebEngineView *newView = createTab(QUrl(), activate);
            request.openIn(newView->page());
        }
    );
}

void MainWindow::updateCurrentTabUi()
{
    QWebEngineView *webView = currentWebView();
    if (!webView) {
        m_address->clear();
        setWindowTitle(QStringLiteral("PanBrowser"));
        m_progress->hide();
        updateNavigationActions();
        return;
    }

    m_address->setText(webView->url().toString());
    const QString title = webView->title();
    setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));

    const BrowserTabState state = m_tabStates.value(webView);
    setTrustStatus(state.trustStatus, state.trustError);
    m_progress->setValue(state.progress);
    m_progress->setVisible(state.loading);
    updateNavigationActions();
}

void MainWindow::updateNavigationActions()
{
    QWebEngineView *webView = currentWebView();
    m_backAction->setEnabled(webView && webView->history()->canGoBack());
    m_forwardAction->setEnabled(webView && webView->history()->canGoForward());
    m_reloadAction->setEnabled(webView != nullptr);
}

void MainWindow::navigateFromAddressBar()
{
    const QUrl url = QUrl::fromUserInput(m_address->text().trimmed());
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("http")
            && url.scheme() != QStringLiteral("https"))) {
        setTrustStatus(QStringLiteral("Only HTTP and HTTPS URLs are supported"), true);
        return;
    }
    if (QWebEngineView *webView = currentWebView())
        webView->setUrl(url);
}

void MainWindow::openTrustRules()
{
    TrustRulesDialog dialog(m_configurationPath, this);
    QString error;
    if (!dialog.load(&error)) {
        setTrustStatus(QStringLiteral("Rules error: %1").arg(error), true);
        return;
    }

    if (dialog.exec() == QDialog::Accepted)
        reloadRules();
}

void MainWindow::reloadRules()
{
    m_configurationPath = ensureConfiguration();
    QString error;
    if (!m_trustPolicy.load(m_configurationPath, &error)) {
        m_ruleCount->setText(QStringLiteral("Rules unavailable"));
        setTrustStatus(QStringLiteral("Rules error: %1").arg(error), true);
        return;
    }

    const qsizetype count = m_trustPolicy.ruleCount();
    m_ruleCount->setText(
        QStringLiteral("%1 custom rule%2").arg(count).arg(count == 1 ? QString() : QStringLiteral("s"))
    );
    setTrustStatus(QStringLiteral("Trust rules loaded"));
}

void MainWindow::handleCertificateError(
    QWebEngineView *webView,
    const QWebEngineCertificateError &error
)
{
    QWebEngineCertificateError decision(error);
    const QString host = error.url().host();
    const TrustRule *rule = m_trustPolicy.ruleForHost(host);

    if (!rule) {
        qWarning().noquote() << "[PanBrowser TLS] rejected unconfigured host" << host
                             << error.description();
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: no matching trust rule").arg(host),
            true
        );
        return;
    }

    if (!error.isOverridable()) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: non-overridable certificate error").arg(host),
            true
        );
        return;
    }

    if (error.type() != QWebEngineCertificateError::CertificateAuthorityInvalid) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: only an unknown CA may be overridden").arg(host),
            true
        );
        return;
    }

    if (rule->mode == TrustMode::SystemOnly) {
        decision.rejectCertificate();
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: system validation failed").arg(host),
            true
        );
        return;
    }

    const CertificateValidationResult result = CertificateTrustValidator::evaluate(
        error.certificateChain(),
        rule->anchors,
        host,
        rule->mode == TrustMode::CustomOnly
    );

    if (result.trusted) {
        decision.acceptCertificate();
        m_tabStates[webView].lastAcceptedRule = rule->name;
        qInfo().noquote() << "[PanBrowser TLS] accepted" << host << "using rule" << rule->name;
        setTabTrustStatus(webView,
            QStringLiteral("Secure · %1 · %2").arg(rule->name, result.explanation)
        );
    } else {
        decision.rejectCertificate();
        qWarning().noquote() << "[PanBrowser TLS] rejected" << host << result.explanation;
        setTabTrustStatus(webView,
            QStringLiteral("Blocked %1: %2").arg(host, result.explanation),
            true
        );
    }
}

void MainWindow::setTabTrustStatus(QWebEngineView *webView, const QString &text, bool error)
{
    if (!webView || !m_tabStates.contains(webView))
        return;

    BrowserTabState &state = m_tabStates[webView];
    state.trustStatus = text;
    state.trustError = error;
    if (webView == currentWebView())
        setTrustStatus(text, error);
}

void MainWindow::setTrustStatus(const QString &text, bool error)
{
    m_trustStatus->setText(text);
    const bool secure = !error && text.startsWith(QStringLiteral("Secure"));
    m_trustStatus->setProperty(
        "state",
        error ? QStringLiteral("error")
              : secure ? QStringLiteral("secure") : QStringLiteral("neutral")
    );
    m_securityIndicator->setIcon(QIcon(
        error ? QStringLiteral(":/assets/icons/triangle-alert.svg")
              : secure ? QStringLiteral(":/assets/icons/shield-check.svg") : QString()
    ));
    m_trustStatus->style()->unpolish(m_trustStatus);
    m_trustStatus->style()->polish(m_trustStatus);
    m_trustStatus->update();
}

void MainWindow::restoreWindowPlacement()
{
    QSettings settings(QStringLiteral("PanBrowser"), QStringLiteral("PanBrowser"));
    settings.beginGroup(QStringLiteral("MainWindow"));
    const QByteArray geometryData = settings.value(QStringLiteral("geometry")).toByteArray();
    const QByteArray stateData = settings.value(QStringLiteral("state")).toByteArray();
    settings.endGroup();

    if (geometryData.isEmpty())
        return;

    restoreGeometry(geometryData);
    if (!stateData.isEmpty())
        restoreState(stateData, 1);

    QList<QRect> availableScreens;
    for (const QScreen *screen : QGuiApplication::screens())
        availableScreens.append(screen->availableGeometry());

    const QScreen *primaryScreen = QGuiApplication::primaryScreen();
    const QRect fallback = primaryScreen ? primaryScreen->availableGeometry() : QRect();
    const bool specialState = isMaximized() || isFullScreen();
    const QRect restored = specialState ? normalGeometry() : geometry();
    const QRect adjusted = adjustedWindowGeometry(restored, availableScreens, fallback);
    if (adjusted == restored)
        return;

    const Qt::WindowStates savedState = windowState();
    setWindowState(Qt::WindowNoState);
    setGeometry(adjusted);
    setWindowState(savedState);
}

QString MainWindow::ensureConfiguration()
{
    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(QDir(directory).filePath(QStringLiteral("Certificates")));
    const QString path = QDir(directory).filePath(QStringLiteral("rules.json"));
    if (QFile::exists(path))
        return path;

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("startPage"), QStringLiteral("https://example.com"));
    root.insert(QStringLiteral("rules"), QJsonArray());

    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.commit();
    }
    return path;
}
