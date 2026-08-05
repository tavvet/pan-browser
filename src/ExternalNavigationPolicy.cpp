#include "ExternalNavigationPolicy.h"

ExternalNavigationDisposition externalNavigationDisposition(
    const QUrl &url,
    bool isMainFrame
)
{
    if (!url.isValid() || url.scheme().isEmpty())
        return ExternalNavigationDisposition::Block;

    const QString scheme = url.scheme().toLower();
    if (scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("about")
        || scheme == QStringLiteral("blob")
        || scheme == QStringLiteral("data")) {
        return ExternalNavigationDisposition::Browse;
    }

    if (!isMainFrame
        || scheme == QStringLiteral("file")
        || scheme == QStringLiteral("javascript")
        || scheme == QStringLiteral("vbscript")
        || scheme == QStringLiteral("qrc")
        || scheme == QStringLiteral("chrome")
        || scheme == QStringLiteral("chrome-extension")
        || scheme == QStringLiteral("devtools")) {
        return ExternalNavigationDisposition::Block;
    }

    return ExternalNavigationDisposition::Prompt;
}
