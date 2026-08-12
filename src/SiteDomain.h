#pragma once

#include <QString>

class QUrl;

namespace SiteDomain {

[[nodiscard]] QString normalizeHost(const QString &host);
[[nodiscard]] QString registrableDomain(const QString &host);
[[nodiscard]] QString siteForUrl(const QUrl &url);
[[nodiscard]] bool sameSite(const QUrl &left, const QUrl &right);
[[nodiscard]] QUrl normalizedPageUrl(const QUrl &url);
[[nodiscard]] QString normalizeHostPattern(const QString &pattern);
[[nodiscard]] bool hostMatchesPattern(const QString &host, const QString &pattern);

} // namespace SiteDomain
