#pragma once

#include <QDialog>
#include <QUrl>

class BookmarkStore;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTimer;

class BookmarksDialog final : public QDialog {
    Q_OBJECT

public:
    explicit BookmarksDialog(BookmarkStore *store, QWidget *parent = nullptr);

signals:
    void openRequested(const QUrl &url, bool newTab);

private:
    void reload();
    void updateActions();
    void editSelected();
    void removeSelected();
    void clearAll();
    void openSelected(bool newTab);

    BookmarkStore *m_store = nullptr;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_bookmarks = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_open = nullptr;
    QPushButton *m_openNewTab = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_remove = nullptr;
    QTimer *m_filterTimer = nullptr;
};
