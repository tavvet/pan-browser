#include "OpenSourceNoticesDialog.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTextCursor>
#include <QUrl>
#include <QVBoxLayout>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineView>

#include <algorithm>

namespace {

constexpr qint64 maximumNoticeSize = 16 * 1024 * 1024;

struct NoticeEntry {
    QString relativePath;
    QString absolutePath;
};

class ChromiumCreditsPage final : public QWebEnginePage {
public:
    explicit ChromiumCreditsPage(QWebEngineProfile *profile, QObject *parent)
        : QWebEnginePage(profile, parent)
    {
    }

protected:
    bool acceptNavigationRequest(
        const QUrl &url,
        NavigationType type,
        bool isMainFrame
    ) override
    {
        if (!isMainFrame)
            return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
        const bool creditsPage = url.scheme() == QStringLiteral("chrome")
            && url.host() == QStringLiteral("credits");
        const bool initialBlank = url == QUrl(QStringLiteral("about:blank"));
        return (creditsPage || initialBlank)
            && QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

QList<NoticeEntry> noticeEntries(const QString &documentationDirectory)
{
    QList<NoticeEntry> entries;
    if (documentationDirectory.isEmpty())
        return entries;

    const QDir root(documentationDirectory);
    QDirIterator iterator(
        documentationDirectory,
        QDir::Files | QDir::Readable | QDir::NoSymLinks,
        QDirIterator::Subdirectories
    );
    while (iterator.hasNext()) {
        const QString absolutePath = iterator.next();
        const QFileInfo info(absolutePath);
        if (info.size() > maximumNoticeSize)
            continue;
        entries.append({root.relativeFilePath(absolutePath), absolutePath});
    }
    std::sort(entries.begin(), entries.end(), [](const NoticeEntry &left, const NoticeEntry &right) {
        return left.relativePath.compare(right.relativePath, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

} // namespace

OpenSourceNoticesDialog::OpenSourceNoticesDialog(
    QWebEngineProfile *profile,
    QWidget *parent
)
    : QDialog(parent)
    , m_profile(profile)
    , m_documentationDirectory(documentationDirectory())
{
    setObjectName(QStringLiteral("openSourceNoticesDialog"));
    setWindowTitle(tr("Open-source notices"));
    resize(920, 640);
    setMinimumSize(700, 480);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(tr("Open-source notices"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Review licenses and attribution notices for PanBrowser and bundled components."),
        this
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("noticeSplitter"));
    m_noticeFiles = new QListWidget(splitter);
    m_noticeFiles->setObjectName(QStringLiteral("noticeFiles"));
    m_noticeFiles->setMinimumWidth(230);
    m_noticeText = new QPlainTextEdit(splitter);
    m_noticeText->setObjectName(QStringLiteral("noticeText"));
    m_noticeText->setReadOnly(true);
    m_noticeText->setLineWrapMode(QPlainTextEdit::NoWrap);
    splitter->addWidget(m_noticeFiles);
    splitter->addWidget(m_noticeText);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    layout->addWidget(splitter, 1);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("fieldHint"));
    m_status->setWordWrap(true);
    layout->addWidget(m_status);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *openDirectory = buttons->addButton(
        tr("Open documentation folder"),
        QDialogButtonBox::ActionRole
    );
    auto *chromiumCredits = buttons->addButton(
        tr("Chromium licenses"),
        QDialogButtonBox::ActionRole
    );
    openDirectory->setEnabled(!m_documentationDirectory.isEmpty());
    chromiumCredits->setEnabled(m_profile != nullptr);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(openDirectory, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_documentationDirectory));
    });
    connect(
        chromiumCredits,
        &QPushButton::clicked,
        this,
        &OpenSourceNoticesDialog::showChromiumCredits
    );
    layout->addWidget(buttons);

    const QList<NoticeEntry> entries = noticeEntries(m_documentationDirectory);
    for (const NoticeEntry &entry : entries) {
        auto *item = new QListWidgetItem(entry.relativePath, m_noticeFiles);
        item->setData(Qt::UserRole, entry.absolutePath);
        item->setToolTip(entry.relativePath);
    }

    if (entries.isEmpty()) {
        m_status->setText(tr("No packaged notices were found."));
        m_noticeFiles->setEnabled(false);
        m_noticeText->setPlainText(tr("The documentation directory is unavailable."));
    } else {
        m_status->setText(tr("%n notice file(s)", nullptr, entries.size()));
        connect(
            m_noticeFiles,
            &QListWidget::currentRowChanged,
            this,
            &OpenSourceNoticesDialog::loadSelectedNotice
        );
        m_noticeFiles->setCurrentRow(0);
    }
}

QString OpenSourceNoticesDialog::documentationDirectory()
{
    const QDir applicationDirectory(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        applicationDirectory.absoluteFilePath(QStringLiteral("../Resources/Documentation")),
        applicationDirectory.absoluteFilePath(QStringLiteral("Documentation")),
        applicationDirectory.absoluteFilePath(QStringLiteral("../share/doc/PanBrowser")),
    };
    for (const QString &candidate : candidates) {
        const QFileInfo info(QDir::cleanPath(candidate));
        if (info.isDir() && info.isReadable())
            return info.canonicalFilePath();
    }
    return {};
}

void OpenSourceNoticesDialog::loadSelectedNotice(int row)
{
    const QListWidgetItem *item = m_noticeFiles->item(row);
    if (!item)
        return;

    const QString path = item->data(Qt::UserRole).toString();
    const QFileInfo info(path);
    const QDir root(m_documentationDirectory);
    const QString canonicalPath = info.canonicalFilePath();
    const QString canonicalRoot = QFileInfo(root.absolutePath()).canonicalFilePath();
    const QString relativePath = QDir(canonicalRoot).relativeFilePath(canonicalPath);
    if (canonicalRoot.isEmpty()
        || canonicalPath.isEmpty()
        || relativePath == QStringLiteral("..")
        || relativePath.startsWith(QStringLiteral("../"))) {
        m_noticeText->setPlainText(tr("The selected notice is outside the documentation directory."));
        return;
    }

    QFile file(canonicalPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_noticeText->setPlainText(
            tr("Cannot read %1: %2").arg(item->text(), file.errorString())
        );
        return;
    }
    const QByteArray contents = file.read(maximumNoticeSize + 1);
    if (contents.size() > maximumNoticeSize) {
        m_noticeText->setPlainText(tr("The selected notice is too large to display."));
        return;
    }
    m_noticeText->setPlainText(QString::fromUtf8(contents));
    m_noticeText->moveCursor(QTextCursor::Start);
}

void OpenSourceNoticesDialog::showChromiumCredits()
{
    if (!m_profile)
        return;

    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("chromiumCreditsDialog"));
    dialog.setWindowTitle(tr("Chromium licenses"));
    dialog.resize(920, 680);
    dialog.setMinimumSize(700, 480);

    auto *layout = new QVBoxLayout(&dialog);
    auto *subtitle = new QLabel(
        tr("These notices are generated from the Chromium components embedded in this Qt WebEngine build."),
        &dialog
    );
    subtitle->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *view = new QWebEngineView(&dialog);
    view->setPage(new ChromiumCreditsPage(m_profile, view));
    view->setUrl(QUrl(QStringLiteral("chrome://credits")));
    layout->addWidget(view, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.exec();
}
