#pragma once

#include <QDialog>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QWebEngineProfile;

class OpenSourceNoticesDialog final : public QDialog {
    Q_OBJECT

public:
    explicit OpenSourceNoticesDialog(
        QWebEngineProfile *profile,
        QWidget *parent = nullptr
    );

    [[nodiscard]] static QString documentationDirectory();

private:
    void loadSelectedNotice(int row);
    void showChromiumCredits();

    QWebEngineProfile *m_profile = nullptr;
    QString m_documentationDirectory;
    QListWidget *m_noticeFiles = nullptr;
    QPlainTextEdit *m_noticeText = nullptr;
    QLabel *m_status = nullptr;
};
