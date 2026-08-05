#pragma once

#include <QUrl>

enum class ExternalNavigationDisposition {
    Browse,
    Prompt,
    Block,
};

[[nodiscard]] ExternalNavigationDisposition externalNavigationDisposition(
    const QUrl &url,
    bool isMainFrame
);
