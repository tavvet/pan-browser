#pragma once

#include <QMargins>

class QWidget;

void configurePlatformIntegratedTitleBar(QWidget *window);
[[nodiscard]] QMargins platformTitleBarControlMargins(QWidget *window);
