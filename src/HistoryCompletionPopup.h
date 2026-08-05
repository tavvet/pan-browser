#pragma once

#include "HistoryStore.h"

#include <QFrame>

class AddressLineEdit;
class QListWidget;
class QHideEvent;

class HistoryCompletionPopup final : public QFrame {
    Q_OBJECT

public:
    explicit HistoryCompletionPopup(AddressLineEdit *addressBar, QWidget *parent = nullptr);
    ~HistoryCompletionPopup() override;

    void showSuggestions(const QList<HistorySuggestion> &suggestions);

signals:
    void urlActivated(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void activateCurrent();
    void positionBelowAddressBar();
    void updatePlacementStyle(const QString &placement);

    AddressLineEdit *m_addressBar = nullptr;
    QListWidget *m_list = nullptr;
};
