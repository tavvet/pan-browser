#include "WindowChromePlatform.h"

#include <QWidget>

#import <AppKit/AppKit.h>

#include <algorithm>
#include <cmath>

namespace {

NSWindow *nativeWindow(QWidget *widget)
{
    if (!widget)
        return nil;
    auto *view = reinterpret_cast<NSView *>(widget->winId());
    return view ? [view window] : nil;
}

} // namespace

void configurePlatformIntegratedTitleBar(QWidget *widget)
{
    NSWindow *window = nativeWindow(widget);
    if (!window)
        return;

    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setStyleMask:[window styleMask] | NSWindowStyleMaskFullSizeContentView];
}

void configurePlatformWindowAspectRatio(QWidget *widget, const QSize &aspectRatio)
{
    if (aspectRatio.width() <= 0 || aspectRatio.height() <= 0)
        return;

    NSWindow *window = nativeWindow(widget);
    if (!window)
        return;
    [window setContentAspectRatio:NSMakeSize(
        static_cast<CGFloat>(aspectRatio.width()),
        static_cast<CGFloat>(aspectRatio.height())
    )];
}

QMargins platformTitleBarControlMargins(QWidget *widget)
{
    NSWindow *window = nativeWindow(widget);
    if (!window)
        return {};

    CGFloat rightEdge = 0.0;
    const NSWindowButton buttons[] = {
        NSWindowCloseButton,
        NSWindowMiniaturizeButton,
        NSWindowZoomButton,
    };
    for (const NSWindowButton buttonType : buttons) {
        NSButton *button = [window standardWindowButton:buttonType];
        if (!button)
            continue;
        const NSRect rectInWindow = [button convertRect:[button bounds] toView:nil];
        rightEdge = std::max(rightEdge, NSMaxX(rectInWindow));
    }

    constexpr CGFloat spacingAfterControls = 10.0;
    return QMargins(
        static_cast<int>(std::ceil(rightEdge + spacingAfterControls)),
        0,
        0,
        0
    );
}
