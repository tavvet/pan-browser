#pragma once

#include "ReaderSettings.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class BrowserPage;
class QJsonObject;

class ReaderModeController final : public QObject {
    Q_OBJECT

public:
    enum class Availability {
        Unknown,
        Unavailable,
        Available,
    };
    Q_ENUM(Availability)

    explicit ReaderModeController(
        BrowserPage *page,
        ReaderSettings *settings,
        QObject *parent = nullptr
    );

    [[nodiscard]] Availability availability() const;
    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool isActivationPending() const;
    [[nodiscard]] static bool supportsUrl(const QUrl &url);

public slots:
    void toggle();
    void activate();
    void deactivate();
    void refreshAppearance();

signals:
    void stateChanged();
    void appearanceChanged();
    void errorOccurred(const QString &message);

private:
    void destroyPagePresentation();
    void resetForNavigation();
    void probe();
    void handleMessage(const QJsonObject &message);
    void applyAppearance();
    void saveSettings();
    [[nodiscard]] QString activationScript() const;
    [[nodiscard]] QString appearanceScript() const;

    QPointer<BrowserPage> m_page;
    ReaderSettings *m_settings = nullptr;
    Availability m_availability = Availability::Unknown;
    bool m_active = false;
    bool m_activationPending = false;
    int m_probeAttempts = 0;
    QTimer m_probeTimer;
    QUrl m_observedUrl;
    quint64 m_probeRequest = 0;
    quint64 m_generation = 0;
};
