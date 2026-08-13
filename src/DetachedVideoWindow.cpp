#include "DetachedVideoWindow.h"

#include "WindowChromePlatform.h"

#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <QWindow>

namespace {

constexpr int closeButtonSize = 36;
constexpr int closeButtonMargin = 8;
constexpr int resizeBorderWidth = 6;
constexpr int minimumVideoLongSide = 240;
constexpr int minimumVideoShortSide = 135;
constexpr double minimumVideoAspectRatio = 1.0 / 4.0;
constexpr double maximumVideoAspectRatio = 4.0;

bool requiresSystemWindowMove()
{
    return QGuiApplication::platformName().startsWith(
        QStringLiteral("wayland"),
        Qt::CaseInsensitive
    );
}

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
    const QSize &videoSize,
    QWidget *parent
)
    : QMainWindow(
          parent,
          Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
      )
    , m_sourceView(sourceView)
{
    if (videoSize.width() > 0 && videoSize.height() > 0) {
        const double requestedAspectRatio =
            static_cast<double>(videoSize.width()) / videoSize.height();
        if (requestedAspectRatio < minimumVideoAspectRatio)
            m_videoAspectSize = QSize(1, 4);
        else if (requestedAspectRatio > maximumVideoAspectRatio)
            m_videoAspectSize = QSize(4, 1);
        else
            m_videoAspectSize = videoSize;
        m_videoAspectRatio =
            static_cast<double>(m_videoAspectSize.width()) / m_videoAspectSize.height();
    }
    setObjectName(QStringLiteral("detachedVideoWindow"));
    setAttribute(Qt::WA_DeleteOnClose, false);
    QSize minimumSize;
    if (m_videoAspectRatio >= 1.0) {
        const int minimumWidth = qMax(
            minimumVideoLongSide,
            qRound(minimumVideoShortSide * m_videoAspectRatio)
        );
        minimumSize = QSize(
            minimumWidth,
            qRound(minimumWidth / m_videoAspectRatio)
        );
    } else {
        const int minimumHeight = qMax(
            minimumVideoLongSide,
            qRound(minimumVideoShortSide / m_videoAspectRatio)
        );
        minimumSize = QSize(
            qRound(minimumHeight * m_videoAspectRatio),
            minimumHeight
        );
    }
    setMinimumSize(minimumSize);
    setWindowTitle(windowTitle);
    if (sourceView)
        setWindowIcon(sourceView->window()->windowIcon());

    auto *container = new QWidget(this);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(0, 0, 0, 0);
    containerLayout->setSpacing(0);

    m_webView = new QWebEngineView(container);
    m_webView->setMouseTracking(true);
    containerLayout->addWidget(m_webView, 1);
    setCentralWidget(container);

    m_closeButton = new QToolButton(this);
    m_closeButton->setObjectName(QStringLiteral("closeDetachedVideoButton"));
    m_closeButton->setIcon(QIcon(QStringLiteral(":/assets/icons/x.svg")));
    m_closeButton->setIconSize(QSize(19, 19));
    m_closeButton->setFixedSize(closeButtonSize, closeButtonSize);
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setToolTip(tr("Close Video Window"));
    m_closeButton->setAccessibleName(tr("Close Video Window"));
    m_closeButton->hide();
    connect(m_closeButton, &QToolButton::clicked, this, &DetachedVideoWindow::returnRequested);

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
    QSize initialSize = constrainedSize(960, 540, m_videoAspectRatio >= 16.0 / 9.0);
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
    configurePlatformWindowAspectRatio(
        this,
        m_videoAspectSize
    );
    updateCloseButtonGeometry();

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

void DetachedVideoWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateCloseButtonGeometry();
}

bool DetachedVideoWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (isActiveWindow() && event->type() == QEvent::KeyPress) {
        const auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape && keyEvent->modifiers() == Qt::NoModifier) {
            emit returnRequested();
            return true;
        }
    }

    const QEvent::Type type = event->type();
    if (type != QEvent::MouseMove
        && type != QEvent::MouseButtonPress
        && type != QEvent::MouseButtonRelease) {
        return QMainWindow::eventFilter(watched, event);
    }

    auto *mouseEvent = static_cast<QMouseEvent *>(event);
    const QPoint globalPosition = mouseEvent->globalPosition().toPoint();
    const bool pointerInside = frameGeometry().contains(globalPosition);
    setCloseButtonVisible(pointerInside);

    QWidget *watchedWidget = qobject_cast<QWidget *>(watched);
    const bool eventBelongsToWindow = watchedWidget && watchedWidget->window() == this;
    const bool closeButtonEvent = watchedWidget
        && (watchedWidget == m_closeButton || m_closeButton->isAncestorOf(watchedWidget));

    if (type == QEvent::MouseButtonPress) {
        m_dragCandidate = false;
        m_dragging = false;
        m_systemMoving = false;
        m_resizing = false;
        m_resizeEdges = {};
        if (!eventBelongsToWindow
            || closeButtonEvent
            || mouseEvent->button() != Qt::LeftButton
            || mouseEvent->modifiers() != Qt::NoModifier) {
            return QMainWindow::eventFilter(watched, event);
        }

        const QPoint localPosition = mapFromGlobal(globalPosition);
        Qt::Edges resizeEdges;
        if (localPosition.x() < resizeBorderWidth)
            resizeEdges |= Qt::LeftEdge;
        else if (localPosition.x() >= width() - resizeBorderWidth)
            resizeEdges |= Qt::RightEdge;
        if (localPosition.y() < resizeBorderWidth)
            resizeEdges |= Qt::TopEdge;
        else if (localPosition.y() >= height() - resizeBorderWidth)
            resizeEdges |= Qt::BottomEdge;
        if (resizeEdges != Qt::Edges()) {
            m_resizing = true;
            m_resizeEdges = resizeEdges;
            m_dragPressGlobal = globalPosition;
            m_resizeStartGeometry = geometry();
            mouseEvent->accept();
            return true;
        }

        m_dragCandidate = true;
        m_dragPressGlobal = globalPosition;
        m_dragStartPosition = pos();
        return QMainWindow::eventFilter(watched, event);
    }

    if (type == QEvent::MouseMove) {
        if (m_resizing && (mouseEvent->buttons() & Qt::LeftButton)) {
            setGeometry(constrainedResizeGeometry(globalPosition));
            mouseEvent->accept();
            return true;
        }
        if (!m_dragCandidate || !(mouseEvent->buttons() & Qt::LeftButton)) {
            if (!(mouseEvent->buttons() & Qt::LeftButton)) {
                m_dragCandidate = false;
                m_dragging = false;
                m_systemMoving = false;
                m_resizing = false;
                m_resizeEdges = {};
            }
            return QMainWindow::eventFilter(watched, event);
        }

        const QPoint delta = globalPosition - m_dragPressGlobal;
        if (!m_dragging
            && delta.manhattanLength() >= QApplication::startDragDistance()) {
            if (requiresSystemWindowMove()) {
                QWindow *nativeWindow = windowHandle();
                if (nativeWindow && nativeWindow->startSystemMove()) {
                    m_systemMoving = true;
                    mouseEvent->accept();
                    return true;
                }
            }
            m_dragging = true;
        }
        if (m_systemMoving) {
            mouseEvent->accept();
            return true;
        }
        if (m_dragging) {
            move(m_dragStartPosition + delta);
            mouseEvent->accept();
            return true;
        }
        return QMainWindow::eventFilter(watched, event);
    }

    const bool consumed = (m_dragging || m_systemMoving || m_resizing)
        && mouseEvent->button() == Qt::LeftButton;
    m_dragCandidate = false;
    m_dragging = false;
    m_systemMoving = false;
    m_resizing = false;
    m_resizeEdges = {};
    if (consumed) {
        mouseEvent->accept();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

QRect DetachedVideoWindow::constrainedResizeGeometry(const QPoint &globalPosition) const
{
    const QPoint delta = globalPosition - m_dragPressGlobal;
    const bool horizontal = m_resizeEdges.testFlag(Qt::LeftEdge)
        || m_resizeEdges.testFlag(Qt::RightEdge);
    const bool vertical = m_resizeEdges.testFlag(Qt::TopEdge)
        || m_resizeEdges.testFlag(Qt::BottomEdge);

    int requestedWidth = m_resizeStartGeometry.width();
    int requestedHeight = m_resizeStartGeometry.height();
    if (m_resizeEdges.testFlag(Qt::LeftEdge))
        requestedWidth -= delta.x();
    else if (m_resizeEdges.testFlag(Qt::RightEdge))
        requestedWidth += delta.x();
    if (m_resizeEdges.testFlag(Qt::TopEdge))
        requestedHeight -= delta.y();
    else if (m_resizeEdges.testFlag(Qt::BottomEdge))
        requestedHeight += delta.y();

    bool widthDriven = horizontal && !vertical;
    if (horizontal && vertical) {
        const double relativeWidthChange = qAbs(
            static_cast<double>(requestedWidth - m_resizeStartGeometry.width())
            / qMax(1, m_resizeStartGeometry.width())
        );
        const double relativeHeightChange = qAbs(
            static_cast<double>(requestedHeight - m_resizeStartGeometry.height())
            / qMax(1, m_resizeStartGeometry.height())
        );
        widthDriven = relativeWidthChange >= relativeHeightChange;
    }

    const QSize resizedSize = constrainedSize(
        requestedWidth,
        requestedHeight,
        widthDriven
    );
    QPoint topLeft = m_resizeStartGeometry.topLeft();
    if (m_resizeEdges.testFlag(Qt::LeftEdge)) {
        topLeft.setX(
            m_resizeStartGeometry.x()
            + m_resizeStartGeometry.width()
            - resizedSize.width()
        );
    } else if (!m_resizeEdges.testFlag(Qt::RightEdge)) {
        topLeft.setX(
            m_resizeStartGeometry.x()
            + (m_resizeStartGeometry.width() - resizedSize.width()) / 2
        );
    }
    if (m_resizeEdges.testFlag(Qt::TopEdge)) {
        topLeft.setY(
            m_resizeStartGeometry.y()
            + m_resizeStartGeometry.height()
            - resizedSize.height()
        );
    } else if (!m_resizeEdges.testFlag(Qt::BottomEdge)) {
        topLeft.setY(
            m_resizeStartGeometry.y()
            + (m_resizeStartGeometry.height() - resizedSize.height()) / 2
        );
    }
    return QRect(topLeft, resizedSize);
}

QSize DetachedVideoWindow::constrainedSize(
    int width,
    int height,
    bool widthDriven
) const
{
    int constrainedWidth = width;
    int constrainedHeight = height;
    if (widthDriven) {
        constrainedWidth = qMax(minimumWidth(), constrainedWidth);
        constrainedHeight = qRound(constrainedWidth / m_videoAspectRatio);
        if (constrainedHeight < minimumHeight()) {
            constrainedHeight = minimumHeight();
            constrainedWidth = qRound(constrainedHeight * m_videoAspectRatio);
        }
    } else {
        constrainedHeight = qMax(minimumHeight(), constrainedHeight);
        constrainedWidth = qRound(constrainedHeight * m_videoAspectRatio);
        if (constrainedWidth < minimumWidth()) {
            constrainedWidth = minimumWidth();
            constrainedHeight = qRound(constrainedWidth / m_videoAspectRatio);
        }
    }
    return QSize(qMax(1, constrainedWidth), qMax(1, constrainedHeight));
}

void DetachedVideoWindow::updateCloseButtonGeometry()
{
    if (!m_closeButton)
        return;
    m_closeButton->move(
        qMax(0, width() - closeButtonSize - closeButtonMargin),
        closeButtonMargin
    );
    m_closeButton->raise();
}

void DetachedVideoWindow::setCloseButtonVisible(bool visible)
{
    if (!m_closeButton || m_closeButton->isVisible() == visible)
        return;
    m_closeButton->setVisible(visible);
    if (visible)
        m_closeButton->raise();
}
