#include "BrowserPage.h"

#include "ExternalNavigationPolicy.h"

#include <QWebEngineSettings>

BrowserPage::BrowserPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    settings()->setUnknownUrlSchemePolicy(
        QWebEngineSettings::UnknownUrlSchemePolicy::AllowUnknownUrlSchemesFromUserInteraction
    );
}

bool BrowserPage::acceptNavigationRequest(
    const QUrl &url,
    NavigationType type,
    bool isMainFrame
)
{
    switch (externalNavigationDisposition(url, isMainFrame)) {
    case ExternalNavigationDisposition::Browse:
        if (isMainFrame)
            emit mainFrameNavigationRequested(url, static_cast<int>(type));
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    case ExternalNavigationDisposition::Prompt:
        if (type != NavigationTypeLinkClicked
            && type != NavigationTypeTyped
            && type != NavigationTypeFormSubmitted) {
            return false;
        }
        emit externalUrlRequested(url);
        return false;
    case ExternalNavigationDisposition::Block:
        return false;
    }
    return false;
}
