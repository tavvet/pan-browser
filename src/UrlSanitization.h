#pragma once

#include <QUrl>

namespace UrlSanitization {

[[nodiscard]] QUrl httpUrlForPersistence(QUrl url);

} // namespace UrlSanitization
