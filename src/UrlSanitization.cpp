#include "UrlSanitization.h"

namespace UrlSanitization {

QUrl httpUrlForPersistence(QUrl url)
{
    const QString scheme = url.scheme().toLower();
    if (!url.isValid()
        || url.host().isEmpty()
        || (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return {};
    }

    url.setScheme(scheme);
    url.setHost(url.host().toLower());
    url.setUserName(QString());
    url.setPassword(QString());
    return url;
}

} // namespace UrlSanitization
