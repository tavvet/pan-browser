#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QToolButton;

class FindBar final : public QWidget {
    Q_OBJECT

public:
    explicit FindBar(QWidget *parent = nullptr);

    [[nodiscard]] QString query() const;
    void focusInput(const QString &initialText = QString());
    void setSearching();
    void setResults(int activeMatch, int numberOfMatches);
    void clearResults();

signals:
    void queryChanged(const QString &query);
    void navigationRequested(bool backward);
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateNavigationButtons(bool enabled);

    QLineEdit *m_query = nullptr;
    QLabel *m_result = nullptr;
    QToolButton *m_previous = nullptr;
    QToolButton *m_next = nullptr;
};
