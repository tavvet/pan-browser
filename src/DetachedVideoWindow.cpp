#include "DetachedVideoWindow.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QWebEngineView>

#include <utility>

namespace {

class ElidedCaptionLabel final : public QLabel {
public:
    explicit ElidedCaptionLabel(const QString &fullText, QWidget *parent = nullptr)
        : QLabel(parent)
        , m_fullText(fullText)
    {
        setToolTip(fullText);
        setAccessibleName(fullText);
        updateDisplayedText();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateDisplayedText();
    }

    void changeEvent(QEvent *event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
            updateDisplayedText();
    }

private:
    void updateDisplayedText()
    {
        const QMargins margins = contentsMargins();
        const int availableWidth = qMax(0, width() - margins.left() - margins.right());
        setText(fontMetrics().elidedText(m_fullText, Qt::ElideRight, availableWidth));
    }

    QString m_fullText;
};

class SecurityOriginLabel final : public QLabel {
public:
    SecurityOriginLabel(
        const QString &fullText,
        const QString &copyActionText,
        DetachedVideoWindow::CopyTextHandler copyTextHandler,
        QWidget *parent = nullptr
    )
        : QLabel(parent)
        , m_fullText(fullText)
        , m_copyActionText(copyActionText)
        , m_copyTextHandler(std::move(copyTextHandler))
    {
        setToolTip(fullText);
        setAccessibleName(fullText);
        updateDisplayedText();
    }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->matches(QKeySequence::Copy)) {
            copyFullText();
            event->accept();
            return;
        }
        QLabel::keyPressEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent *event) override
    {
        QMenu menu(this);
        QAction *copyAction = menu.addAction(m_copyActionText);
        connect(copyAction, &QAction::triggered, this, [this] {
            copyFullText();
        });
        menu.exec(event->globalPos());
        event->accept();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateDisplayedText();
    }

    void changeEvent(QEvent *event) override
    {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
            updateDisplayedText();
    }

private:
    void copyFullText() const
    {
        if (m_copyTextHandler)
            m_copyTextHandler(m_fullText);
    }

    void updateDisplayedText()
    {
        const QMargins margins = contentsMargins();
        const int availableWidth = qMax(0, width() - margins.left() - margins.right());
        const QFontMetrics metrics = fontMetrics();
        if (metrics.horizontalAdvance(m_fullText) <= availableWidth) {
            setText(m_fullText);
            return;
        }

        const qsizetype schemeDelimiter = m_fullText.indexOf(QStringLiteral("://"));
        if (schemeDelimiter < 0) {
            setText(metrics.elidedText(m_fullText, Qt::ElideMiddle, availableWidth));
            return;
        }

        const QString fixedPrefix = m_fullText.left(schemeDelimiter + 3);
        const int suffixWidth = availableWidth - metrics.horizontalAdvance(fixedPrefix);
        if (suffixWidth <= metrics.horizontalAdvance(QString(QChar(0x2026)))) {
            setText(metrics.elidedText(m_fullText, Qt::ElideMiddle, availableWidth));
            return;
        }
        setText(fixedPrefix + metrics.elidedText(
            m_fullText.mid(schemeDelimiter + 3),
            Qt::ElideLeft,
            suffixWidth
        ));
    }

    QString m_fullText;
    QString m_copyActionText;
    DetachedVideoWindow::CopyTextHandler m_copyTextHandler;
};

} // namespace

DetachedVideoPlaceholder::DetachedVideoPlaceholder(
    QWebEngineView *sourceView,
    const QString &message,
    const QString &returnButtonText
)
    : QFrame(sourceView)
    , m_sourceView(sourceView)
{
    setObjectName(QStringLiteral("detachedVideoPlaceholder"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(14);
    layout->addStretch(1);

    auto *label = new QLabel(message, this);
    label->setObjectName(QStringLiteral("detachedVideoMessage"));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *returnButton = new QPushButton(returnButtonText, this);
    returnButton->setObjectName(QStringLiteral("returnDetachedVideoButton"));
    returnButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    layout->addWidget(returnButton, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    connect(returnButton, &QPushButton::clicked, this, &DetachedVideoPlaceholder::returnRequested);

    if (m_sourceView) {
        m_sourceView->installEventFilter(this);
        setGeometry(m_sourceView->rect());
    }
    show();
    raise();
    setFocus(Qt::OtherFocusReason);
}

DetachedVideoPlaceholder::~DetachedVideoPlaceholder()
{
    if (m_sourceView)
        m_sourceView->removeEventFilter(this);
}

bool DetachedVideoPlaceholder::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_sourceView
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        setGeometry(m_sourceView->rect());
        raise();
    }
    return QFrame::eventFilter(watched, event);
}

DetachedVideoWindow::DetachedVideoWindow(
    QWebEngineView *sourceView,
    const QString &windowTitle,
    const QString &sourceCaption,
    const QString &sourceOrigin,
    QWidget *parent,
    CopyTextHandler copyTextHandler
)
    : QMainWindow(parent, Qt::Window)
    , m_sourceView(sourceView)
    , m_sourceOriginText(sourceOrigin)
{
    if (!copyTextHandler) {
        copyTextHandler = [](const QString &text) {
            if (QClipboard *clipboard = QGuiApplication::clipboard())
                clipboard->setText(text);
        };
    }

    setObjectName(QStringLiteral("detachedVideoWindow"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    setWindowTitle(windowTitle);
    if (sourceView)
        setWindowIcon(sourceView->window()->windowIcon());

    auto *container = new QWidget(this);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    auto *originBar = new QFrame(container);
    originBar->setObjectName(QStringLiteral("detachedVideoOriginBar"));
    originBar->setAttribute(Qt::WA_StyledBackground, true);
    auto *originLayout = new QHBoxLayout(originBar);
    originLayout->setContentsMargins(12, 7, 12, 7);
    originLayout->setSpacing(8);
    auto *originIcon = new QLabel(originBar);
    originIcon->setPixmap(
        QIcon(QStringLiteral(":/assets/icons/info.svg")).pixmap(QSize(15, 15))
    );
    originLayout->addWidget(originIcon, 0, Qt::AlignVCenter);
    auto *originTextLayout = new QVBoxLayout;
    originTextLayout->setContentsMargins(0, 0, 0, 0);
    originTextLayout->setSpacing(1);
    auto *originCaption = new ElidedCaptionLabel(sourceCaption, originBar);
    originCaption->setObjectName(QStringLiteral("detachedVideoOriginCaption"));
    originCaption->setMinimumWidth(0);
    originCaption->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    originTextLayout->addWidget(originCaption);
    m_originLabel = new SecurityOriginLabel(
        sourceOrigin,
        tr("Copy Source Address"),
        std::move(copyTextHandler),
        originBar
    );
    m_originLabel->setObjectName(QStringLiteral("detachedVideoOrigin"));
    m_originLabel->setMinimumWidth(0);
    m_originLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_originLabel->setTextInteractionFlags(
        Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
    );
    originTextLayout->addWidget(m_originLabel);
    originLayout->addLayout(originTextLayout, 1);
    containerLayout->addWidget(originBar);

    m_webView = new QWebEngineView(container);
    containerLayout->addWidget(m_webView, 1);
    setCentralWidget(container);

    m_webView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        m_webView,
        &QWidget::customContextMenuRequested,
        this,
        &DetachedVideoWindow::contextMenuRequested
    );

    if (sourceView)
        m_webView->setPage(sourceView->page());

    const QScreen *targetScreen = sourceView && sourceView->window()->screen()
        ? sourceView->window()->screen()
        : QGuiApplication::primaryScreen();
    const QRect available = targetScreen
        ? targetScreen->availableGeometry()
        : QRect(0, 0, 1280, 720);
    QSize initialSize(960, 540);
    const QSize availableSize(
        qMax(1, available.width() * 4 / 5),
        qMax(1, available.height() * 4 / 5)
    );
    if (initialSize.width() > availableSize.width()
        || initialSize.height() > availableSize.height()) {
        initialSize.scale(availableSize, Qt::KeepAspectRatio);
    }
    QRect initialGeometry(QPoint(), initialSize);
    const QPoint center = sourceView
        ? sourceView->window()->frameGeometry().center()
        : available.center();
    initialGeometry.moveCenter(center);
    if (!available.contains(initialGeometry)) {
        initialGeometry.moveLeft(qBound(
            available.left(),
            initialGeometry.left(),
            available.right() - initialGeometry.width() + 1
        ));
        initialGeometry.moveTop(qBound(
            available.top(),
            initialGeometry.top(),
            available.bottom() - initialGeometry.height() + 1
        ));
    }
    setGeometry(initialGeometry);

    qApp->installEventFilter(this);
}

DetachedVideoWindow::~DetachedVideoWindow()
{
    if (qApp)
        qApp->removeEventFilter(this);
    restorePage();
}

QWebEngineView *DetachedVideoWindow::webView() const
{
    return m_webView;
}

QString DetachedVideoWindow::sourceOriginText() const
{
    return m_sourceOriginText;
}

void DetachedVideoWindow::restorePage()
{
    if (m_pageRestored)
        return;
    m_pageRestored = true;
    if (m_sourceView && m_webView && m_webView->page())
        m_sourceView->setPage(m_webView->page());
}

void DetachedVideoWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    emit returnRequested();
}

bool DetachedVideoWindow::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);
    if (isActiveWindow() && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape && keyEvent->modifiers() == Qt::NoModifier) {
            emit returnRequested();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}
