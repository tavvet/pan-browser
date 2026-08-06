#pragma once

#include "AddressSuggestion.h"

#include <QFrame>

class AddressLineEdit;
class QListWidget;
class QListWidgetItem;
class QHideEvent;

class AddressCompletionPopup final : public QFrame {
    Q_OBJECT

public:
    explicit AddressCompletionPopup(AddressLineEdit *addressBar, QWidget *parent = nullptr);
    ~AddressCompletionPopup() override;

    void showSuggestions(const QList<AddressSuggestion> &suggestions);

signals:
    void urlActivated(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void activateItem(const QListWidgetItem *item);
    void activateCurrent();
    void positionBelowAddressBar();
    void updatePlacementStyle(const QString &placement);

    AddressLineEdit *m_addressBar = nullptr;
    QListWidget *m_list = nullptr;
};
