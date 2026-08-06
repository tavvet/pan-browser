#include "WebAppShortcutManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QXmlStreamWriter>

#include <utility>

namespace {

bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}

QString text(const char *source)
{
    return QCoreApplication::translate("WebAppShortcutManager", source);
}

bool isValidAppId(const QString &id)
{
    static const QRegularExpression expression(QStringLiteral("^[0-9a-f]{64}$"));
    return expression.match(id).hasMatch();
}

#ifdef Q_OS_MACOS
QString defaultHostBundlePath()
{
    QDir directory(QCoreApplication::applicationDirPath());
    if (directory.dirName() != QStringLiteral("MacOS") || !directory.cdUp())
        return QString();
    if (directory.dirName() != QStringLiteral("Contents") || !directory.cdUp())
        return QString();
    return directory.dirName().endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)
        ? directory.absolutePath()
        : QString();
}

bool writeFile(
    const QString &path,
    const QByteArray &contents,
    QFileDevice::Permissions permissions,
    QString *error
)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return fail(error, file.errorString());
    if (file.write(contents) != contents.size() || !file.commit())
        return fail(error, file.errorString());
    if (!QFile::setPermissions(path, permissions))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot set shortcut file permissions")));
    return true;
}

QByteArray shortcutInfoPlist(
    const WebApp &app,
    const QString &hostExecutable,
    bool hasIcon
)
{
    QByteArray contents;
    QXmlStreamWriter writer(&contents);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeDTD(QStringLiteral(
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
    ));
    writer.writeStartElement(QStringLiteral("plist"));
    writer.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    writer.writeStartElement(QStringLiteral("dict"));
    const auto stringEntry = [&writer](const QString &key, const QString &value) {
        writer.writeTextElement(QStringLiteral("key"), key);
        writer.writeTextElement(QStringLiteral("string"), value);
    };
    stringEntry(QStringLiteral("CFBundleInfoDictionaryVersion"), QStringLiteral("6.0"));
    stringEntry(QStringLiteral("CFBundlePackageType"), QStringLiteral("APPL"));
    stringEntry(QStringLiteral("CFBundleExecutable"), QStringLiteral("PanBrowserWebAppLauncher"));
    stringEntry(QStringLiteral("CFBundleIdentifier"), QStringLiteral("dev.panbrowser.webapp.%1").arg(app.id));
    stringEntry(
        QStringLiteral("CFBundleName"),
        WebAppShortcutManager::safeShortcutName(app.shortName.isEmpty() ? app.name : app.shortName)
    );
    stringEntry(
        QStringLiteral("CFBundleDisplayName"),
        WebAppShortcutManager::safeShortcutName(app.name)
    );
    stringEntry(QStringLiteral("CFBundleVersion"), QStringLiteral("1"));
    stringEntry(QStringLiteral("CFBundleShortVersionString"), QStringLiteral("1.0"));
    if (hasIcon)
        stringEntry(QStringLiteral("CFBundleIconFile"), QStringLiteral("AppIcon.icns"));
    writer.writeTextElement(QStringLiteral("key"), QStringLiteral("LSUIElement"));
    writer.writeEmptyElement(QStringLiteral("true"));
    writer.writeTextElement(QStringLiteral("key"), QStringLiteral("NSHighResolutionCapable"));
    writer.writeEmptyElement(QStringLiteral("true"));
    stringEntry(QStringLiteral("PanBrowserWebAppId"), app.id);
    stringEntry(QStringLiteral("PanBrowserHostExecutable"), hostExecutable);
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    return contents;
}

bool runProcess(const QString &program, const QStringList &arguments, QString *error)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForStarted(3000) || !process.waitForFinished(15000)) {
        process.kill();
        process.waitForFinished();
        return fail(error, process.errorString());
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        QString explanation = QString::fromUtf8(process.readAllStandardError()).trimmed();
        if (explanation.isEmpty())
            explanation = text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "The system tool returned an error"));
        return fail(error, explanation);
    }
    return true;
}

bool createIconSet(const QImage &source, const QString &iconSetPath, QString *error)
{
    if (!QDir().mkpath(iconSetPath))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot create the shortcut icon directory")));
    const QList<QPair<QString, int>> images = {
        {QStringLiteral("icon_16x16.png"), 16},
        {QStringLiteral("icon_16x16@2x.png"), 32},
        {QStringLiteral("icon_32x32.png"), 32},
        {QStringLiteral("icon_32x32@2x.png"), 64},
        {QStringLiteral("icon_128x128.png"), 128},
        {QStringLiteral("icon_128x128@2x.png"), 256},
        {QStringLiteral("icon_256x256.png"), 256},
        {QStringLiteral("icon_256x256@2x.png"), 512},
        {QStringLiteral("icon_512x512.png"), 512},
        {QStringLiteral("icon_512x512@2x.png"), 1024},
    };
    for (const auto &[fileName, size] : images) {
        QImage canvas(size, size, QImage::Format_ARGB32_Premultiplied);
        canvas.fill(Qt::transparent);
        const QImage scaled = source.scaled(
            size,
            size,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );
        QPainter painter(&canvas);
        painter.drawImage((size - scaled.width()) / 2, (size - scaled.height()) / 2, scaled);
        painter.end();
        if (!canvas.save(QDir(iconSetPath).filePath(fileName), "PNG"))
            return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot write the shortcut icon")));
    }
    return true;
}

bool createShortcutIcon(
    const WebApp &app,
    const QString &bundlePath,
    const QString &hostBundlePath,
    QString *error
)
{
    const QString resourcesPath = QDir(bundlePath).filePath(QStringLiteral("Contents/Resources"));
    const QString iconPath = QDir(resourcesPath).filePath(QStringLiteral("AppIcon.icns"));
    QImage image;
    image.loadFromData(app.iconPng, "PNG");
    if (!image.isNull()) {
        const QString iconSetPath = QDir(resourcesPath).filePath(QStringLiteral("AppIcon.iconset"));
        QString iconError;
        if (createIconSet(image, iconSetPath, &iconError)
            && runProcess(
                QStringLiteral("/usr/bin/iconutil"),
                {QStringLiteral("-c"), QStringLiteral("icns"), QStringLiteral("-o"), iconPath, iconSetPath},
                &iconError
            )) {
            QDir(iconSetPath).removeRecursively();
            return true;
        }
        QDir(iconSetPath).removeRecursively();
    }

    const QString fallbackIcon = QDir(hostBundlePath).filePath(
        QStringLiteral("Contents/Resources/PanBrowser.icns")
    );
    if (QFileInfo(fallbackIcon).isFile() && QFile::copy(fallbackIcon, iconPath))
        return true;
    if (error)
        error->clear();
    return false;
}

#endif

} // namespace

WebAppShortcutManager::WebAppShortcutManager(
    QString shortcutRoot,
    QString hostBundlePath
)
    : m_shortcutRoot(std::move(shortcutRoot))
    , m_hostBundlePath(std::move(hostBundlePath))
{
#ifdef Q_OS_MACOS
    if (m_shortcutRoot.isEmpty()) {
        m_shortcutRoot = QDir::home().filePath(
            QStringLiteral("Applications/PanBrowser Apps")
        );
    }
    if (m_hostBundlePath.isEmpty())
        m_hostBundlePath = defaultHostBundlePath();
#endif
}

bool WebAppShortcutManager::isSupported() const
{
#ifdef Q_OS_MACOS
    const QString launcher = QDir(m_hostBundlePath).filePath(
        QStringLiteral("Contents/Resources/PanBrowserWebAppLauncher")
    );
    const QString hostExecutable = QDir(m_hostBundlePath).filePath(
        QStringLiteral("Contents/MacOS/PanBrowser")
    );
    return !m_hostBundlePath.isEmpty()
        && QFileInfo(launcher).isExecutable()
        && QFileInfo(hostExecutable).isExecutable();
#else
    return false;
#endif
}

bool WebAppShortcutManager::shortcutExists(const WebApp &app) const
{
    return isValidAppId(app.id)
        && !matchingShortcutPaths(app.id).isEmpty();
}

QString WebAppShortcutManager::shortcutPath(const WebApp &app) const
{
    return QDir(m_shortcutRoot).filePath(
        QStringLiteral("%1 — %2.app").arg(safeShortcutName(app.name), app.id.left(8))
    );
}

QString WebAppShortcutManager::shortcutRoot() const
{
    return m_shortcutRoot;
}

bool WebAppShortcutManager::createOrUpdate(const WebApp &app, QString *error) const
{
    if (error)
        error->clear();
#ifndef Q_OS_MACOS
    Q_UNUSED(app)
    return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "System shortcuts are not supported on this platform yet")));
#else
    if (!isValidAppId(app.id) || app.name.trimmed().isEmpty())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "The web app shortcut data is invalid")));
    if (!isSupported())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "PanBrowser must run from a complete macOS application bundle to create shortcuts")));
    if (QFileInfo(m_shortcutRoot).isSymLink())
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "The web app shortcuts directory must not be a symbolic link")));
    if (!QDir().mkpath(m_shortcutRoot))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot create the web app shortcuts directory")));

    const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString temporaryPath = QDir(m_shortcutRoot).filePath(
        QStringLiteral(".panbrowser-%1.app").arg(token)
    );
    const QString contentsPath = QDir(temporaryPath).filePath(QStringLiteral("Contents"));
    const QString executablePath = QDir(contentsPath).filePath(
        QStringLiteral("MacOS/PanBrowserWebAppLauncher")
    );
    const QString resourcesPath = QDir(contentsPath).filePath(QStringLiteral("Resources"));
    if (!QDir().mkpath(QFileInfo(executablePath).absolutePath())
        || !QDir().mkpath(resourcesPath)) {
        QDir(temporaryPath).removeRecursively();
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot create the macOS shortcut bundle")));
    }

    const QString sourceLauncher = QDir(m_hostBundlePath).filePath(
        QStringLiteral("Contents/Resources/PanBrowserWebAppLauncher")
    );
    if (!QFile::copy(sourceLauncher, executablePath)
        || !QFile::setPermissions(
            executablePath,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
                | QFileDevice::ReadGroup | QFileDevice::ExeGroup
                | QFileDevice::ReadOther | QFileDevice::ExeOther
        )) {
        QDir(temporaryPath).removeRecursively();
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot install the macOS shortcut launcher")));
    }

    QString iconError;
    const bool hasIcon = createShortcutIcon(app, temporaryPath, m_hostBundlePath, &iconError);
    const QString hostExecutable = QDir(m_hostBundlePath).filePath(
        QStringLiteral("Contents/MacOS/PanBrowser")
    );
    const QByteArray infoPlist = shortcutInfoPlist(
        app,
        hostExecutable,
        hasIcon
    );
    const QFileDevice::Permissions regularPermissions = QFileDevice::ReadOwner
        | QFileDevice::WriteOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther;
    QString writeError;
    if (!writeFile(QDir(contentsPath).filePath(QStringLiteral("Info.plist")), infoPlist, regularPermissions, &writeError)
        || !writeFile(QDir(contentsPath).filePath(QStringLiteral("PkgInfo")), QByteArrayLiteral("APPL????"), regularPermissions, &writeError)
        || !writeFile(QDir(resourcesPath).filePath(QStringLiteral("PanBrowserWebAppId")), app.id.toLatin1() + '\n', regularPermissions, &writeError)) {
        QDir(temporaryPath).removeRecursively();
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot write the macOS shortcut bundle: %1")).arg(writeError));
    }

    QString signingError;
    if (!runProcess(
            QStringLiteral("/usr/bin/codesign"),
            {QStringLiteral("--force"), QStringLiteral("--sign"), QStringLiteral("-"), QStringLiteral("--timestamp=none"), temporaryPath},
            &signingError
        )) {
        QDir(temporaryPath).removeRecursively();
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot sign the macOS shortcut: %1")).arg(signingError));
    }

    const QString destination = shortcutPath(app);
    QString backupPath;
    if (QFileInfo::exists(destination)) {
        if (!pathBelongsToApp(destination, app.id)) {
            QDir(temporaryPath).removeRecursively();
            return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Another application already uses the shortcut path: %1")).arg(destination));
        }
        backupPath = destination + QStringLiteral(".backup-%1").arg(token);
        if (!QDir().rename(destination, backupPath)) {
            QDir(temporaryPath).removeRecursively();
            return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot replace the existing web app shortcut")));
        }
    }
    if (!QDir().rename(temporaryPath, destination)) {
        if (!backupPath.isEmpty())
            QDir().rename(backupPath, destination);
        QDir(temporaryPath).removeRecursively();
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot move the web app shortcut into place")));
    }
    if (!backupPath.isEmpty())
        QDir(backupPath).removeRecursively();

    for (const QString &path : matchingShortcutPaths(app.id)) {
        if (path != destination)
            QDir(path).removeRecursively();
    }
    return true;
#endif
}

bool WebAppShortcutManager::remove(const WebApp &app, QString *error) const
{
    if (error)
        error->clear();
    if (!isValidAppId(app.id))
        return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "The web app shortcut data is invalid")));
#ifndef Q_OS_MACOS
    return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "System shortcuts are not supported on this platform yet")));
#else
    for (const QString &path : matchingShortcutPaths(app.id)) {
        if (!QDir(path).removeRecursively())
            return fail(error, text(QT_TRANSLATE_NOOP("WebAppShortcutManager", "Cannot remove the web app shortcut: %1")).arg(path));
    }
    return true;
#endif
}

QString WebAppShortcutManager::safeShortcutName(const QString &name)
{
    QString result = name;
    static const QRegularExpression unsafe(
        QStringLiteral("[\\x{0000}-\\x{001F}\\x{007F}\\p{Cf}/\\\\:]")
    );
    result.replace(unsafe, QStringLiteral(" "));
    result = result.simplified();
    while (result.startsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char('.')))
        result = result.mid(result.startsWith(QLatin1Char('.')) ? 1 : 0, result.size() - 1).trimmed();
    if (result.size() > 80) {
        qsizetype limit = 80;
        if (result.at(limit - 1).isHighSurrogate() && result.at(limit).isLowSurrogate())
            --limit;
        result.truncate(limit);
    }
    result = result.trimmed();
    return result.isEmpty() ? QStringLiteral("Web App") : result;
}

QStringList WebAppShortcutManager::matchingShortcutPaths(const QString &appId) const
{
    QStringList paths;
    if (m_shortcutRoot.isEmpty() || !isValidAppId(appId))
        return paths;
    const QFileInfo rootInfo(m_shortcutRoot);
    if (!rootInfo.isDir() || rootInfo.isSymLink())
        return paths;
    const QDir root(m_shortcutRoot);
    const QFileInfoList entries = root.entryInfoList(
        {QStringLiteral("*.app")},
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks
    );
    for (const QFileInfo &entry : entries) {
        if (pathBelongsToApp(entry.absoluteFilePath(), appId))
            paths.append(entry.absoluteFilePath());
    }
    return paths;
}

bool WebAppShortcutManager::pathBelongsToApp(
    const QString &path,
    const QString &appId
) const
{
    const QFileInfo bundle(path);
    if (!bundle.isDir() || bundle.isSymLink() || bundle.absolutePath() != QDir(m_shortcutRoot).absolutePath())
        return false;
    const QString markerPath = QDir(path).filePath(
        QStringLiteral("Contents/Resources/PanBrowserWebAppId")
    );
    const QFileInfo markerInfo(markerPath);
    if (!markerInfo.isFile() || markerInfo.isSymLink())
        return false;
    QFile marker(markerPath);
    if (!marker.open(QIODevice::ReadOnly) || marker.size() > 80)
        return false;
    return QString::fromLatin1(marker.readAll()).trimmed() == appId;
}
