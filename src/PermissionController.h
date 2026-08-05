#pragma once

#include "PermissionPolicy.h"

#include <QList>
#include <QObject>
#include <QPointer>
#include <QWebEnginePermission>

#include <optional>

class PermissionPrompt;
class QWebEngineView;

class PermissionController final : public QObject {
    Q_OBJECT

public:
    explicit PermissionController(PermissionPrompt *prompt, QObject *parent = nullptr);
    ~PermissionController() override;

    void request(QWebEngineView *webView, const QWebEnginePermission &permission);
    void currentViewChanged(QWebEngineView *webView);
    void cancelForView(QWebEngineView *webView);

private:
    struct PendingPermission {
        QPointer<QWebEngineView> webView;
        QWebEnginePermission permission;
        BrowserPermissionKind kind = BrowserPermissionKind::Other;
    };

    static BrowserPermissionKind kindForPermission(
        QWebEnginePermission::PermissionType type
    );
    void allowCurrent();
    void denyCurrent();
    void showNext();
    void denyAll();

    PermissionPrompt *m_prompt = nullptr;
    QPointer<QWebEngineView> m_currentView;
    QList<PendingPermission> m_queue;
    std::optional<PendingPermission> m_active;
};
