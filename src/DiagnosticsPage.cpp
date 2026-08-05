#include "DiagnosticsPage.h"

#include "BrowserProfile.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSysInfo>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineSettings>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>

namespace {

QLabel *valueLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("diagnosticValue"));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

QString environmentValue(const char *name)
{
    const QString value = qEnvironmentVariable(name).trimmed();
    return value.isEmpty() ? QStringLiteral("Not set") : value;
}

QString enabledText(bool enabled)
{
    return enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled");
}

bool containsCommandLineFlag(const QString &arguments, const QString &flag)
{
    const QRegularExpression pattern(
        QStringLiteral("(?:^|\\s)%1(?:\\s|$)").arg(QRegularExpression::escape(flag))
    );
    return pattern.match(arguments).hasMatch();
}

bool environmentSwitchEnabled(const char *name)
{
    const QString value = qEnvironmentVariable(name).trimmed().toLower();
    return !value.isEmpty() && value != QStringLiteral("0") && value != QStringLiteral("false");
}

void addSectionLabel(QVBoxLayout *layout, const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("sectionLabel"));
    layout->addWidget(label);
}

QFormLayout *addCard(QVBoxLayout *layout, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName(QStringLiteral("settingsCard"));
    auto *form = new QFormLayout(card);
    form->setContentsMargins(18, 15, 18, 15);
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(10);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    layout->addWidget(card);
    return form;
}

} // namespace

DiagnosticsPage::DiagnosticsPage(BrowserProfile *profile, QWidget *parent)
    : QWidget(parent)
    , m_profile(profile)
{
    setObjectName(QStringLiteral("diagnosticsSettingsPage"));
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("diagnosticsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("diagnosticsContent"));
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(30, 24, 30, 24);
    layout->setSpacing(14);
    scroll->setWidget(content);
    rootLayout->addWidget(scroll);

    auto *title = new QLabel(QStringLiteral("Diagnostics"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);
    auto *subtitle = new QLabel(
        QStringLiteral("Runtime versions, graphics configuration, and profile locations."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    layout->addWidget(subtitle);

    addSectionLabel(layout, QStringLiteral("RUNTIME"), this);
    QFormLayout *runtime = addCard(layout, this);
    runtime->addRow(
        QStringLiteral("PanBrowser"),
        valueLabel(QCoreApplication::applicationVersion(), this)
    );
    runtime->addRow(QStringLiteral("Qt"), valueLabel(QString::fromLatin1(qVersion()), this));
    runtime->addRow(
        QStringLiteral("Qt WebEngine"),
        valueLabel(QString::fromLatin1(qWebEngineVersion()), this)
    );
    runtime->addRow(
        QStringLiteral("Chromium"),
        valueLabel(QString::fromLatin1(qWebEngineChromiumVersion()), this)
    );
    runtime->addRow(
        QStringLiteral("Chromium security patch"),
        valueLabel(QString::fromLatin1(qWebEngineChromiumSecurityPatchVersion()), this)
    );
    runtime->addRow(QStringLiteral("Operating system"), valueLabel(QSysInfo::prettyProductName(), this));
    runtime->addRow(QStringLiteral("Architecture"), valueLabel(QSysInfo::currentCpuArchitecture(), this));

    addSectionLabel(layout, QStringLiteral("GRAPHICS & SECURITY"), this);
    QFormLayout *graphics = addCard(layout, this);
    const QString chromiumFlags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS");
    const QString applicationArguments = QCoreApplication::arguments().join(QLatin1Char(' '));
    const bool gpuDisabled = containsCommandLineFlag(chromiumFlags, QStringLiteral("--disable-gpu"))
        || containsCommandLineFlag(applicationArguments, QStringLiteral("--disable-gpu"));
    const bool sandboxDisabled = environmentSwitchEnabled("QTWEBENGINE_DISABLE_SANDBOX")
        || containsCommandLineFlag(chromiumFlags, QStringLiteral("--no-sandbox"))
        || containsCommandLineFlag(applicationArguments, QStringLiteral("--no-sandbox"));
    graphics->addRow(
        QStringLiteral("GPU acceleration"),
        valueLabel(
            gpuDisabled
                ? QStringLiteral("Forced off by runtime flag")
                : QStringLiteral("Automatic — hardware preferred, software fallback"),
            this
        )
    );
    graphics->addRow(
        QStringLiteral("WebGL"),
        valueLabel(enabledText(m_profile->settings()->testAttribute(
            QWebEngineSettings::WebGLEnabled
        )), this)
    );
    graphics->addRow(
        QStringLiteral("Accelerated 2D canvas"),
        valueLabel(enabledText(m_profile->settings()->testAttribute(
            QWebEngineSettings::Accelerated2dCanvasEnabled
        )), this)
    );
    graphics->addRow(
        QStringLiteral("Chromium sandbox"),
        valueLabel(sandboxDisabled ? QStringLiteral("Disabled by runtime flag")
                                   : QStringLiteral("Enabled"), this)
    );
    graphics->addRow(
        QStringLiteral("RHI backend override"),
        valueLabel(environmentValue("QSG_RHI_BACKEND"), this)
    );
    graphics->addRow(
        QStringLiteral("Chromium flags"),
        valueLabel(environmentValue("QTWEBENGINE_CHROMIUM_FLAGS"), this)
    );

    auto *graphicsHint = new QLabel(
        QStringLiteral("“Automatic” means Chromium attempts to use the GPU and falls back when necessary. For the exact active GPU and ANGLE/RHI backend, launch PanBrowser with QT_LOGGING_RULES=\"qt.webenginecontext=true;qt.webengine.compositor=true\"."),
        this
    );
    graphicsHint->setObjectName(QStringLiteral("fieldHint"));
    graphicsHint->setWordWrap(true);
    layout->addWidget(graphicsHint);

    addSectionLabel(layout, QStringLiteral("PROFILE"), this);
    QFormLayout *paths = addCard(layout, this);
    paths->addRow(
        QStringLiteral("Persistent storage"),
        valueLabel(m_profile->persistentStoragePath(), this)
    );
    paths->addRow(QStringLiteral("HTTP cache"), valueLabel(m_profile->cachePath(), this));

    auto *actions = new QHBoxLayout();
    auto *copy = new QPushButton(QStringLiteral("Copy diagnostic report"), this);
    auto *copied = new QLabel(QStringLiteral("Copied."), this);
    copied->setObjectName(QStringLiteral("dataActionStatus"));
    copied->hide();
    actions->addWidget(copy);
    actions->addWidget(copied);
    actions->addStretch();
    layout->addLayout(actions);
    layout->addStretch();

    connect(copy, &QPushButton::clicked, this, [this, copied] {
        QApplication::clipboard()->setText(diagnosticReport());
        copied->show();
        QTimer::singleShot(1800, copied, &QWidget::hide);
    });
}

QString DiagnosticsPage::diagnosticReport() const
{
    const QString chromiumFlags = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS");
    const QString applicationArguments = QCoreApplication::arguments().join(QLatin1Char(' '));
    const bool gpuDisabled = containsCommandLineFlag(chromiumFlags, QStringLiteral("--disable-gpu"))
        || containsCommandLineFlag(applicationArguments, QStringLiteral("--disable-gpu"));
    const bool sandboxDisabled = environmentSwitchEnabled("QTWEBENGINE_DISABLE_SANDBOX")
        || containsCommandLineFlag(chromiumFlags, QStringLiteral("--no-sandbox"))
        || containsCommandLineFlag(applicationArguments, QStringLiteral("--no-sandbox"));
    return QStringLiteral(
        "PanBrowser: %1\n"
        "Qt: %2\n"
        "Qt WebEngine: %3\n"
        "Chromium: %4\n"
        "Chromium security patch: %5\n"
        "Operating system: %6\n"
        "Architecture: %7\n"
        "GPU acceleration: %8\n"
        "WebGL: %9\n"
        "Accelerated 2D canvas: %10\n"
        "Chromium sandbox: %11\n"
        "QSG_RHI_BACKEND: %12\n"
        "QTWEBENGINE_CHROMIUM_FLAGS: %13\n"
        "Persistent storage: %14\n"
        "HTTP cache: %15\n"
    ).arg(
        QCoreApplication::applicationVersion(),
        QString::fromLatin1(qVersion()),
        QString::fromLatin1(qWebEngineVersion()),
        QString::fromLatin1(qWebEngineChromiumVersion()),
        QString::fromLatin1(qWebEngineChromiumSecurityPatchVersion()),
        QSysInfo::prettyProductName(),
        QSysInfo::currentCpuArchitecture(),
        gpuDisabled ? QStringLiteral("Forced off") : QStringLiteral("Automatic"),
        enabledText(m_profile->settings()->testAttribute(QWebEngineSettings::WebGLEnabled)),
        enabledText(m_profile->settings()->testAttribute(
            QWebEngineSettings::Accelerated2dCanvasEnabled
        )),
        sandboxDisabled ? QStringLiteral("Disabled") : QStringLiteral("Enabled"),
        environmentValue("QSG_RHI_BACKEND"),
        environmentValue("QTWEBENGINE_CHROMIUM_FLAGS"),
        m_profile->persistentStoragePath(),
        m_profile->cachePath()
    );
}
