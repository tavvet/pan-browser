#pragma once

#include "HistoryStore.h"

#include <QFrame>

class QLineEdit;
class QListWidget;

class HistoryCompletionPopup final : public QFrame {
    Q_OBJECT

public:
    explicit HistoryCompletionPopup(QLineEdit *addressBar, QWidget *parent = nullptr);
    ~HistoryCompletionPopup() override;

    void showSuggestions(const QList<HistorySuggestion> &suggestions);

signals:
    void urlActivated(const QUrl &url);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void activateCurrent();
    void positionBelowAddressBar();

    QLineEdit *m_addressBar = nullptr;
    QListWidget *m_list = nullptr;
};
