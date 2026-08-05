#pragma once

#include "BookmarkStore.h"

#include <QDialog>

#include <optional>

class QLineEdit;

class BookmarkDialog final : public QDialog {
    Q_OBJECT

public:
    BookmarkDialog(
        BookmarkStore *store,
        const QUrl &url,
        const QString &suggestedTitle,
        const std::optional<Bookmark> &existing,
        QWidget *parent = nullptr
    );

private:
    void saveBookmark();
    void removeBookmark();

    BookmarkStore *m_store = nullptr;
    std::optional<Bookmark> m_existing;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_url = nullptr;
};
