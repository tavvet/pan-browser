#pragma once

#include <QObject>

class MacApplicationReopenHandler final : public QObject {
    Q_OBJECT

public:
    explicit MacApplicationReopenHandler(QObject *parent = nullptr);
    ~MacApplicationReopenHandler() override;

    void notifyReopen();

signals:
    void reopenRequested();

private:
    void *m_proxy = nullptr;
};
