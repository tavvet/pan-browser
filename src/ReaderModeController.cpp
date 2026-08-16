#include "ReaderModeController.h"

#include "BrowserPage.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QVariant>
#include <QWebEngineScript>

#include <array>

namespace {

constexpr int maximumElements = 120000;
constexpr int minimumArticleLength = 300;
constexpr int maximumTextLength = 5000000;
constexpr int maximumMarkupLength = 6000000;
constexpr int maximumTitleLength = 1024;
constexpr int maximumMetadataLength = 512;
constexpr int maximumLanguageLength = 128;
constexpr int maximumUrlLength = 16384;
constexpr int maximumDataImageLength = 1000000;
constexpr int maximumSrcsetLength = 1100000;
constexpr int maximumSrcsetCandidates = 128;
constexpr std::array<int, 3> probeRetryDelaysMs{600, 1800, 4000};

QString resourceText(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

QString readerableSource()
{
    static const QString source = resourceText(
        QStringLiteral(":/assets/reader/third_party/readability/Readability-readerable.js")
    );
    return source;
}

QString readabilitySource()
{
    static const QString source = resourceText(
        QStringLiteral(":/assets/reader/third_party/readability/Readability.js")
    );
    return source;
}

QString domPurifySource()
{
    static const QString source = resourceText(
        QStringLiteral(":/assets/reader/third_party/dompurify/purify.min.js")
    );
    return source;
}

QString readerModeSource()
{
    static const QString source = resourceText(
        QStringLiteral(":/assets/reader/reader-mode.js")
    );
    return source;
}

QString readerModeCss()
{
    static const QString source = resourceText(
        QStringLiteral(":/assets/reader/reader-mode.css")
    );
    return source;
}

QJsonObject appearanceObject(const ReaderSettings &settings)
{
    return {
        {QStringLiteral("theme"), settings.themeName()},
        {QStringLiteral("typeface"), settings.typefaceName()},
        {QStringLiteral("textSize"), settings.textSize()},
        {QStringLiteral("contentWidth"), settings.contentWidth()},
    };
}

QString compactJson(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace

ReaderModeController::ReaderModeController(
    BrowserPage *page,
    ReaderSettings *settings,
    QObject *parent
)
    : QObject(parent)
    , m_page(page)
    , m_settings(settings)
    , m_loading(page && page->isLoading())
    , m_observedUrl(page ? page->url() : QUrl())
{
    Q_ASSERT(page);
    Q_ASSERT(settings);
    m_probeTimer.setSingleShot(true);
    connect(&m_probeTimer, &QTimer::timeout, this, &ReaderModeController::probe);
    connect(page, &QWebEnginePage::loadStarted, this, [this] {
        m_loading = true;
        resetForNavigation();
    });
    connect(page, &QWebEnginePage::loadFinished, this, [this](bool ok) {
        m_loading = false;
        m_probeTimer.stop();
        ++m_probeRequest;
        if (ok) {
            m_probeAttempts = 0;
            probe();
        } else {
            m_availability = Availability::Unavailable;
            emit stateChanged();
        }
    });
    connect(page, &QWebEnginePage::urlChanged, this, [this](const QUrl &url) {
        if (url == m_observedUrl)
            return;
        m_observedUrl = url;
        if (m_loading || (m_page && m_page->isLoading()))
            return;
        resetForNavigation();
        m_probeTimer.start(probeRetryDelaysMs.front());
    });
    connect(page, &BrowserPage::readerModeMessage, this, &ReaderModeController::handleMessage);
}

ReaderModeController::Availability ReaderModeController::availability() const
{
    return m_availability;
}

bool ReaderModeController::isActive() const
{
    return m_active;
}

bool ReaderModeController::isActivationPending() const
{
    return m_activationPending;
}

bool ReaderModeController::supportsUrl(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    return url.isValid()
        && !url.host().isEmpty()
        && (scheme == QStringLiteral("http") || scheme == QStringLiteral("https"));
}

void ReaderModeController::toggle()
{
    if (m_active)
        deactivate();
    else
        activate();
}

void ReaderModeController::activate()
{
    if (!m_page
        || m_active
        || m_activationPending
        || m_availability != Availability::Available) {
        return;
    }

    const QString script = activationScript();
    if (script.isEmpty()) {
        emit errorOccurred(tr("Reader mode resources are unavailable"));
        return;
    }

    m_activationPending = true;
    emit stateChanged();
    const quint64 generation = m_generation;
    const QPointer<ReaderModeController> guard(this);
    m_page->runJavaScript(
        script,
        QWebEngineScript::ApplicationWorld,
        [guard, generation](const QVariant &result) {
            if (!guard || generation != guard->m_generation)
                return;
            guard->m_activationPending = false;
            const QVariantMap response = result.toMap();
            if (response.value(QStringLiteral("ok")).toBool()) {
                guard->m_active = true;
            } else {
                guard->m_availability = Availability::Unavailable;
                qWarning().noquote()
                    << "[PanBrowser reader mode] activation failed:"
                    << response.value(QStringLiteral("error")).toString();
                emit guard->errorOccurred(QCoreApplication::translate(
                    "ReaderModeController",
                    "Reader mode is unavailable for this page"
                ));
            }
            emit guard->stateChanged();
        }
    );
}

void ReaderModeController::deactivate()
{
    if (!m_page || (!m_active && !m_activationPending))
        return;

    ++m_generation;
    m_activationPending = false;
    m_active = false;
    destroyPagePresentation();
    emit stateChanged();
}

void ReaderModeController::destroyPagePresentation()
{
    if (!m_page)
        return;

    m_page->runJavaScript(
        QStringLiteral(R"JS(
(() => {
    const reader = globalThis.__panBrowserReader;
    if (!reader || typeof reader.destroy !== "function")
        return false;
    reader.destroy();
    return true;
})()
)JS"),
        QWebEngineScript::ApplicationWorld
    );
}

void ReaderModeController::refreshAppearance()
{
    applyAppearance();
}

void ReaderModeController::resetForNavigation()
{
    if (m_active || m_activationPending)
        destroyPagePresentation();
    m_probeTimer.stop();
    ++m_probeRequest;
    ++m_generation;
    const bool changed = m_availability != Availability::Unknown
        || m_active
        || m_activationPending;
    m_availability = Availability::Unknown;
    m_active = false;
    m_activationPending = false;
    m_probeAttempts = 0;
    if (changed)
        emit stateChanged();
}

void ReaderModeController::probe()
{
    m_probeTimer.stop();
    if (!m_page || !supportsUrl(m_page->url())) {
        m_availability = Availability::Unavailable;
        emit stateChanged();
        return;
    }

    const QString library = readerableSource();
    if (library.isEmpty()) {
        m_availability = Availability::Unavailable;
        emit errorOccurred(tr("Reader mode resources are unavailable"));
        emit stateChanged();
        return;
    }

    const QString script = library + QStringLiteral(R"JS(
;(() => {
    if (document.contentType !== "text/html"
        && document.contentType !== "application/xhtml+xml") {
        return false;
    }
    if (document.getElementsByTagName("*").length > %1)
        return false;
    try {
        return isProbablyReaderable(document, {
            minContentLength: 120,
            minScore: 20
        });
    } catch (_) {
        return false;
    }
})()
)JS").arg(maximumElements);

    m_availability = Availability::Unknown;
    ++m_probeAttempts;
    const quint64 request = ++m_probeRequest;
    emit stateChanged();
    const quint64 generation = m_generation;
    const QPointer<ReaderModeController> guard(this);
    m_page->runJavaScript(
        script,
        QWebEngineScript::ApplicationWorld,
        [guard, generation, request](const QVariant &result) {
            if (!guard
                || generation != guard->m_generation
                || request != guard->m_probeRequest) {
                return;
            }
            if (!result.toBool()
                && guard->m_probeAttempts <= static_cast<int>(probeRetryDelaysMs.size())) {
                const int delay = probeRetryDelaysMs.at(
                    static_cast<std::size_t>(guard->m_probeAttempts - 1)
                );
                guard->m_probeTimer.start(delay);
                return;
            }
            guard->m_availability = result.toBool()
                ? Availability::Available
                : Availability::Unavailable;
            emit guard->stateChanged();
        }
    );
}

void ReaderModeController::handleMessage(const QJsonObject &message)
{
    if (!m_active || !m_settings)
        return;

    const QString action = message.value(QStringLiteral("action")).toString();
    const QJsonValue value = message.value(QStringLiteral("value"));
    if (action == QStringLiteral("close")) {
        deactivate();
        return;
    }
    if (action == QStringLiteral("theme")) {
        const QString name = value.toString();
        if (name == QStringLiteral("system"))
            m_settings->setTheme(ReaderTheme::System);
        else if (name == QStringLiteral("light"))
            m_settings->setTheme(ReaderTheme::Light);
        else if (name == QStringLiteral("sepia"))
            m_settings->setTheme(ReaderTheme::Sepia);
        else if (name == QStringLiteral("dark"))
            m_settings->setTheme(ReaderTheme::Dark);
        else
            return;
    } else if (action == QStringLiteral("typeface")) {
        const QString name = value.toString();
        if (name != QStringLiteral("serif") && name != QStringLiteral("sans"))
            return;
        m_settings->setTypeface(
            name == QStringLiteral("sans")
                ? ReaderTypeface::SansSerif
                : ReaderTypeface::Serif
        );
    } else if (action == QStringLiteral("text-size")) {
        if (!value.isDouble())
            return;
        m_settings->setTextSize(value.toInt());
    } else if (action == QStringLiteral("content-width")) {
        if (!value.isDouble())
            return;
        m_settings->setContentWidth(value.toInt());
    } else {
        return;
    }

    applyAppearance();
    saveSettings();
    emit appearanceChanged();
}

void ReaderModeController::applyAppearance()
{
    if (!m_page || !m_active)
        return;
    m_page->runJavaScript(appearanceScript(), QWebEngineScript::ApplicationWorld);
}

void ReaderModeController::saveSettings()
{
    QString error;
    if (!m_settings->save(&error))
        emit errorOccurred(error);
}

QString ReaderModeController::activationScript() const
{
    const QString readability = readabilitySource();
    const QString purifier = domPurifySource();
    const QString reader = readerModeSource();
    const QString css = readerModeCss();
    if (readability.isEmpty() || purifier.isEmpty() || reader.isEmpty() || css.isEmpty())
        return {};

    QJsonObject labels;
    labels.insert(QStringLiteral("readerMode"), tr("Reader Mode"));
    labels.insert(QStringLiteral("appearance"), tr("Reading appearance"));
    labels.insert(QStringLiteral("theme"), tr("Theme"));
    labels.insert(QStringLiteral("typeface"), tr("Switch typeface"));
    labels.insert(QStringLiteral("smallerText"), tr("Smaller text"));
    labels.insert(QStringLiteral("largerText"), tr("Larger text"));
    labels.insert(QStringLiteral("narrower"), tr("Narrower content"));
    labels.insert(QStringLiteral("wider"), tr("Wider content"));
    labels.insert(QStringLiteral("close"), tr("Exit Reader Mode"));
    labels.insert(QStringLiteral("untitled"), tr("Untitled article"));
    labels.insert(QStringLiteral("themes"), QJsonArray{
        QJsonObject{
            {QStringLiteral("value"), QStringLiteral("system")},
            {QStringLiteral("label"), tr("System")},
        },
        QJsonObject{
            {QStringLiteral("value"), QStringLiteral("light")},
            {QStringLiteral("label"), tr("Light")},
        },
        QJsonObject{
            {QStringLiteral("value"), QStringLiteral("sepia")},
            {QStringLiteral("label"), tr("Sepia")},
        },
        QJsonObject{
            {QStringLiteral("value"), QStringLiteral("dark")},
            {QStringLiteral("label"), tr("Dark")},
        },
    });

    QJsonObject options;
    options.insert(QStringLiteral("token"), m_page->readerModeToken());
    options.insert(QStringLiteral("messagePrefix"), BrowserPage::readerModeMessagePrefix());
    options.insert(QStringLiteral("css"), css);
    options.insert(QStringLiteral("labels"), labels);
    options.insert(QStringLiteral("appearance"), appearanceObject(*m_settings));
    options.insert(QStringLiteral("maximumElements"), maximumElements);
    options.insert(QStringLiteral("minimumArticleLength"), minimumArticleLength);
    options.insert(QStringLiteral("maximumTextLength"), maximumTextLength);
    options.insert(QStringLiteral("maximumMarkupLength"), maximumMarkupLength);
    options.insert(QStringLiteral("maximumTitleLength"), maximumTitleLength);
    options.insert(QStringLiteral("maximumMetadataLength"), maximumMetadataLength);
    options.insert(QStringLiteral("maximumLanguageLength"), maximumLanguageLength);
    options.insert(QStringLiteral("maximumUrlLength"), maximumUrlLength);
    options.insert(QStringLiteral("maximumDataImageLength"), maximumDataImageLength);
    options.insert(QStringLiteral("maximumSrcsetLength"), maximumSrcsetLength);
    options.insert(QStringLiteral("maximumSrcsetCandidates"), maximumSrcsetCandidates);
    options.insert(QStringLiteral("minimumTextSize"), ReaderSettings::minimumTextSize);
    options.insert(QStringLiteral("maximumTextSize"), ReaderSettings::maximumTextSize);
    options.insert(QStringLiteral("minimumContentWidth"), ReaderSettings::minimumContentWidth);
    options.insert(QStringLiteral("maximumContentWidth"), ReaderSettings::maximumContentWidth);

    return readability
        + QLatin1Char('\n')
        + purifier
        + QStringLiteral("\n;globalThis.__panBrowserReaderOptions = ")
        + compactJson(options)
        + QStringLiteral(";\n")
        + reader;
}

QString ReaderModeController::appearanceScript() const
{
    return QStringLiteral(R"JS(
(() => {
    const reader = globalThis.__panBrowserReader;
    if (!reader || typeof reader.applyAppearance !== "function")
        return false;
    reader.applyAppearance(%1);
    return true;
})()
)JS").arg(compactJson(appearanceObject(*m_settings)));
}
