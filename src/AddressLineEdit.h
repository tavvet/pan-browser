#pragma once

#include <QLineEdit>
#include <QUrl>

class AddressLineEdit final : public QLineEdit {
    Q_OBJECT

public:
    explicit AddressLineEdit(QWidget *parent = nullptr);

    void setGhostCompletion(const QString &completionText, const QUrl &url);
    void clearGhostCompletion();
    [[nodiscard]] bool hasGhostCompletion() const;
    [[nodiscard]] QUrl ghostCompletionUrl() const;
    bool acceptGhostCompletion();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_ghostText;
    QString m_ghostSuffix;
    QUrl m_ghostUrl;
};
