#include "MacApplicationReopen.h"

#import <AppKit/AppKit.h>

@interface PanBrowserApplicationDelegateProxy : NSObject <NSApplicationDelegate> {
@public
    MacApplicationReopenHandler *handler;
    id originalDelegate;
}

- (instancetype)initWithOriginalDelegate:(id)original
                                  handler:(MacApplicationReopenHandler *)reopenHandler;

@end

@implementation PanBrowserApplicationDelegateProxy

- (instancetype)initWithOriginalDelegate:(id)original
                                  handler:(MacApplicationReopenHandler *)reopenHandler
{
    self = [super init];
    if (self) {
        originalDelegate = [original retain];
        handler = reopenHandler;
    }
    return self;
}

- (void)dealloc
{
    [originalDelegate release];
    [super dealloc];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender
                     hasVisibleWindows:(BOOL)hasVisibleWindows
{
    Q_UNUSED(sender)
    Q_UNUSED(hasVisibleWindows)
    if (handler)
        handler->notifyReopen();
    return NO;
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return [super respondsToSelector:selector]
        || [originalDelegate respondsToSelector:selector];
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    if ([originalDelegate respondsToSelector:selector])
        return originalDelegate;
    return [super forwardingTargetForSelector:selector];
}

@end

MacApplicationReopenHandler::MacApplicationReopenHandler(QObject *parent)
    : QObject(parent)
{
    id original = [NSApp delegate];
    auto *proxy = [[PanBrowserApplicationDelegateProxy alloc]
        initWithOriginalDelegate:original
                         handler:this];
    [NSApp setDelegate:proxy];
    m_proxy = proxy;
}

MacApplicationReopenHandler::~MacApplicationReopenHandler()
{
    auto *proxy = static_cast<PanBrowserApplicationDelegateProxy *>(m_proxy);
    if (!proxy)
        return;
    proxy->handler = nullptr;
    if ([NSApp delegate] == proxy)
        [NSApp setDelegate:proxy->originalDelegate];
    [proxy release];
    m_proxy = nullptr;
}

void MacApplicationReopenHandler::notifyReopen()
{
    emit reopenRequested();
}
