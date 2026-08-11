#include "TrustCertificateRepository.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <utility>

namespace {

QList<QSslCertificate> readCertificates(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const QByteArray data = file.readAll();
    QList<QSslCertificate> certificates = QSslCertificate::fromData(data, QSsl::Pem);
    if (certificates.isEmpty())
        certificates = QSslCertificate::fromData(data, QSsl::Der);
    return certificates;
}

QByteArray fileContents(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString uniqueDestination(const QString &sourcePath, const QDir &directory)
{
    const QFileInfo source(sourcePath);
    const QString baseName = source.completeBaseName().isEmpty()
        ? QStringLiteral("certificate")
        : source.completeBaseName();
    const QString suffix = source.suffix();

    for (int index = 0; ; ++index) {
        const QString numberedName = index == 0
            ? baseName
            : baseName + QStringLiteral("-%1").arg(index);
        const QString fileName = suffix.isEmpty()
            ? numberedName
            : numberedName + QLatin1Char('.') + suffix;
        const QString candidate = directory.filePath(fileName);
        if (!QFile::exists(candidate) || fileContents(candidate) == fileContents(sourcePath))
            return candidate;
    }
}

bool removeFileIfPresent(const QString &path)
{
    return !QFile::exists(path) || QFile::remove(path) || !QFile::exists(path);
}

} // namespace

TrustCertificateRepository::TrustCertificateRepository(QString configurationPath)
    : m_configurationPath(std::move(configurationPath))
{
}

TrustCertificateRepository::~TrustCertificateRepository()
{
    const QStringList failures = rollback();
    if (!failures.isEmpty()) {
        qWarning().noquote()
            << "Could not roll back imported certificate files:"
            << failures.join(QStringLiteral(", "));
    }
}

TrustCertificateInfo TrustCertificateRepository::inspect(const QString &configuredPath) const
{
    TrustCertificateInfo result;
    result.configuredPath = configuredPath;
    result.absolutePath = resolvePath(configuredPath);
    result.certificates = readCertificates(result.absolutePath);
    return result;
}

TrustCertificateImportResult TrustCertificateRepository::importFiles(
    const QStringList &sourcePaths
)
{
    TrustCertificateImportResult result;
    if (sourcePaths.isEmpty())
        return result;

    const QDir configurationDirectory = QFileInfo(m_configurationPath).absoluteDir();
    const QString certificateDirectoryPath = configurationDirectory.filePath(
        QStringLiteral("Certificates")
    );
    if (!QDir().mkpath(certificateDirectoryPath)) {
        result.certificateDirectoryError = certificateDirectoryPath;
        return result;
    }
    const QDir certificateDirectory(certificateDirectoryPath);

    for (const QString &sourcePath : sourcePaths) {
        if (readCertificates(sourcePath).isEmpty()) {
            result.failures.append({
                sourcePath,
                TrustCertificateImportFailureReason::InvalidCertificate,
            });
            continue;
        }

        const QString destination = uniqueDestination(sourcePath, certificateDirectory);
        const bool alreadyThere = QFileInfo(sourcePath).canonicalFilePath()
            == QFileInfo(destination).canonicalFilePath();
        const bool sameExistingFile = QFile::exists(destination)
            && fileContents(destination) == fileContents(sourcePath);

        if (!alreadyThere && !sameExistingFile) {
            if (!QFile::copy(sourcePath, destination)) {
                result.failures.append({
                    sourcePath,
                    TrustCertificateImportFailureReason::CopyFailed,
                });
                continue;
            }
            m_pendingFiles.append(destination);
        }

        result.anchors.append(QDir::fromNativeSeparators(
            configurationDirectory.relativeFilePath(destination)
        ));
    }

    return result;
}

QStringList TrustCertificateRepository::finalize(
    const QList<TrustRuleSettings> &rules
)
{
    QSet<QString> referencedFiles;
    for (const TrustRuleSettings &rule : rules) {
        for (const QString &anchor : rule.anchors)
            referencedFiles.insert(QFileInfo(resolvePath(anchor)).absoluteFilePath());
    }

    QStringList failures;
    QStringList remainingFiles;
    for (const QString &path : std::as_const(m_pendingFiles)) {
        if (referencedFiles.contains(QFileInfo(path).absoluteFilePath()))
            continue;
        if (!removeFileIfPresent(path)) {
            failures.append(path);
            remainingFiles.append(path);
        }
    }
    m_pendingFiles = remainingFiles;
    return failures;
}

QStringList TrustCertificateRepository::rollback()
{
    QStringList failures;
    QStringList remainingFiles;
    for (const QString &path : std::as_const(m_pendingFiles)) {
        if (!removeFileIfPresent(path)) {
            failures.append(path);
            remainingFiles.append(path);
        }
    }
    m_pendingFiles = remainingFiles;
    return failures;
}

QString TrustCertificateRepository::displayName(const QSslCertificate &certificate)
{
    QStringList names = certificate.subjectInfo(QSslCertificate::CommonName);
    if (names.isEmpty())
        names = certificate.subjectInfo(QSslCertificate::Organization);
    return names.isEmpty()
        ? QCoreApplication::translate("TrustRulesDialog", "Unnamed certificate")
        : names.join(QStringLiteral(", "));
}

QString TrustCertificateRepository::fingerprint(const QSslCertificate &certificate)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(certificate.toDer(), QCryptographicHash::Sha256)
            .toHex(':')
            .toUpper()
    );
}

QString TrustCertificateRepository::resolvePath(const QString &configuredPath) const
{
    if (QFileInfo(configuredPath).isAbsolute())
        return configuredPath;
    return QFileInfo(m_configurationPath).absoluteDir().filePath(configuredPath);
}
