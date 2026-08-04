#include "MainWindow.h"

#include "CertificateTrustValidator.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QIcon>
#include <QProgressBar>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QWebEngineCertificateError>
#include <QWebEnginePage>
#include <QWebEngineView>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    createInterface();
    connectBrowserSignals();
    reloadRules();

    const QStringList arguments = QApplication::arguments();
    const QUrl initialUrl = arguments.size() > 1
        ? QUrl::fromUserInput(arguments.at(1))
        : m_trustPolicy.startPage();
    m_webView->setUrl(initialUrl);
}

void MainWindow::createInterface()
{
    QFile themeFile(QStringLiteral(":/assets/theme.qss"));
    if (themeFile.open(QIODevice::ReadOnly))
        qApp->setStyleSheet(QString::fromUtf8(themeFile.readAll()));

    resize(1180, 760);
    setWindowTitle(QStringLiteral("PanBrowser"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/app-icon.svg")));

    m_webView = new QWebEngineView(this);
    setCentralWidget(m_webView);

    QToolBar *toolbar = addToolBar(QStringLiteral("Navigation"));
    toolbar->setObjectName(QStringLiteral("navigationBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(19, 19));
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);

    QAction *back = m_webView->pageAction(QWebEnginePage::Back);
    back->setIcon(QIcon(QStringLiteral(":/assets/icons/arrow-left.svg")));
    back->setText(QStringLiteral("Back"));
    toolbar->addAction(back);

    QAction *forward = m_webView->pageAction(QWebEnginePage::Forward);
    forward->setIcon(QIcon(QStringLiteral(":/assets/icons/arrow-right.svg")));
    forward->setText(QStringLiteral("Forward"));
    toolbar->addAction(forward);

    QAction *reload = m_webView->pageAction(QWebEnginePage::Reload);
    reload->setIcon(QIcon(QStringLiteral(":/assets/icons/rotate-cw.svg")));
    reload->setText(QStringLiteral("Reload"));
    toolbar->addAction(reload);

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

    connect(go, &QAction::triggered, this, &MainWindow::navigateFromAddressBar);
    connect(m_address, &QLineEdit::returnPressed, this, &MainWindow::navigateFromAddressBar);

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("PanBrowser"));
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

void MainWindow::connectBrowserSignals()
{
    connect(m_webView, &QWebEngineView::urlChanged, this, [this](const QUrl &url) {
        m_address->setText(url.toString());
    });
    connect(m_webView, &QWebEngineView::titleChanged, this, [this](const QString &title) {
        setWindowTitle(title.isEmpty() ? QStringLiteral("PanBrowser") : title + QStringLiteral(" — PanBrowser"));
    });
    connect(m_webView, &QWebEngineView::loadStarted, this, [this] {
        m_lastAcceptedRule.clear();
        m_progress->show();
        setTrustStatus(QStringLiteral("Loading…"));
    });
    connect(m_webView, &QWebEngineView::loadProgress, m_progress, &QProgressBar::setValue);
    connect(m_webView, &QWebEngineView::loadFinished, this, [this](bool ok) {
        m_progress->hide();
        if (ok) {
            if (m_lastAcceptedRule.isEmpty())
                setTrustStatus(QStringLiteral("Secure · Chromium system trust"));
        } else if (m_lastAcceptedRule.isEmpty()) {
            setTrustStatus(QStringLiteral("Page loading failed"), true);
        }
    });
    connect(
        m_webView->page(),
        &QWebEnginePage::certificateError,
        this,
        &MainWindow::handleCertificateError
    );
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
    m_webView->setUrl(url);
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

void MainWindow::handleCertificateError(const QWebEngineCertificateError &error)
{
    QWebEngineCertificateError decision(error);
    const QString host = error.url().host();
    const TrustRule *rule = m_trustPolicy.ruleForHost(host);

    if (!rule) {
        qWarning().noquote() << "[PanBrowser TLS] rejected unconfigured host" << host
                             << error.description();
        decision.rejectCertificate();
        setTrustStatus(
            QStringLiteral("Blocked %1: no matching trust rule").arg(host),
            true
        );
        return;
    }

    if (!error.isOverridable()) {
        decision.rejectCertificate();
        setTrustStatus(
            QStringLiteral("Blocked %1: non-overridable certificate error").arg(host),
            true
        );
        return;
    }

    if (error.type() != QWebEngineCertificateError::CertificateAuthorityInvalid) {
        decision.rejectCertificate();
        setTrustStatus(
            QStringLiteral("Blocked %1: only an unknown CA may be overridden").arg(host),
            true
        );
        return;
    }

    if (rule->mode == TrustMode::SystemOnly) {
        decision.rejectCertificate();
        setTrustStatus(
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
        m_lastAcceptedRule = rule->name;
        qInfo().noquote() << "[PanBrowser TLS] accepted" << host << "using rule" << rule->name;
        setTrustStatus(
            QStringLiteral("Secure · %1 · %2").arg(rule->name, result.explanation)
        );
    } else {
        decision.rejectCertificate();
        qWarning().noquote() << "[PanBrowser TLS] rejected" << host << result.explanation;
        setTrustStatus(
            QStringLiteral("Blocked %1: %2").arg(host, result.explanation),
            true
        );
    }
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
