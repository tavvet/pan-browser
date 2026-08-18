#pragma once

#include <QList>

class QAction;
class AddressCompletionPopup;
class AddressLineEdit;
class BrowserTabBar;
class CrossDomainPrompt;
class DownloadButton;
class DownloadsPanel;
class FindBar;
class QLabel;
class QMenu;
class PermissionPrompt;
class QProgressBar;
class QStackedWidget;
class QToolBar;
class WindowChromeController;

struct BrowserWindowUi final {
    DownloadsPanel *downloadsPanel = nullptr;
    DownloadButton *downloadButton = nullptr;
    FindBar *findBar = nullptr;
    QToolBar *findToolbar = nullptr;
    AddressCompletionPopup *addressCompletionPopup = nullptr;
    PermissionPrompt *permissionPrompt = nullptr;
    CrossDomainPrompt *crossDomainPrompt = nullptr;
    BrowserTabBar *tabBar = nullptr;
    QStackedWidget *tabStack = nullptr;
    AddressLineEdit *address = nullptr;
    QAction *backAction = nullptr;
    QAction *forwardAction = nullptr;
    QAction *reloadAction = nullptr;
    QAction *newTabAction = nullptr;
    QAction *closeTabAction = nullptr;
    QAction *reopenClosedTabAction = nullptr;
    QAction *nextTabAction = nullptr;
    QAction *previousTabAction = nullptr;
    QList<QAction *> numberedTabActions;
    QAction *securityIndicator = nullptr;
    QAction *bookmarkAction = nullptr;
    QAction *readerModeAction = nullptr;
    QAction *closeFindAction = nullptr;
    QAction *installWebAppAction = nullptr;
    QAction *developerToolsAction = nullptr;
    QAction *zoomLevelAction = nullptr;
    QAction *zoomInAction = nullptr;
    QAction *zoomOutAction = nullptr;
    QAction *resetZoomAction = nullptr;
    QMenu *webAppsMenu = nullptr;
    QMenu *switchToTabMenu = nullptr;
    QMenu *zoomMenu = nullptr;
    QLabel *trustStatus = nullptr;
    QLabel *ruleCount = nullptr;
    QProgressBar *progress = nullptr;
    WindowChromeController *windowChromeController = nullptr;
};
