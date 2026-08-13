#pragma once

#include <QMargins>
#include <QSize>

class QWidget;

void configurePlatformIntegratedTitleBar(QWidget *window);
void configurePlatformWindowAspectRatio(QWidget *window, const QSize &aspectRatio);
[[nodiscard]] QMargins platformTitleBarControlMargins(QWidget *window);
