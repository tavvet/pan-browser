#pragma once

#include "CredentialStore.h"

#include <QWidget>

#include <memory>

class QLabel;
class QListWidget;
class QPushButton;
class QShowEvent;

class CredentialsSettingsPage final : public QWidget {
    Q_OBJECT

public:
    explicit CredentialsSettingsPage(
        CredentialStore *credentialStore = nullptr,
        QWidget *parent = nullptr
    );

signals:
    void destructiveOperationActiveChanged(bool active);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void reload();
    void applyListResult(CredentialStoreListResult result);
    void setBusy(bool busy, const QString &status = {});
    void setDestructiveOperationActive(bool active);
    void updateActions();
    void removeSelected();
    void removeAll();
    void removeTargets(const QList<CredentialTarget> &targets);
    [[nodiscard]] bool canRemoveAll() const;
    [[nodiscard]] QList<CredentialTarget> selectedTargets() const;

    std::shared_ptr<CredentialStore> m_credentialStore;
    QList<StoredCredentialSummary> m_summaries;
    QListWidget *m_credentials = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_refresh = nullptr;
    QPushButton *m_removeSelected = nullptr;
    QPushButton *m_removeAll = nullptr;
    CredentialStoreErrorCode m_listError = CredentialStoreErrorCode::None;
    bool m_loaded = false;
    bool m_busy = false;
    bool m_destructiveOperationActive = false;
};
