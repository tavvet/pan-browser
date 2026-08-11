#include "CertificateDetailsDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include <utility>

namespace {

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

CertificateDetailsDialog::CertificateDetailsDialog(
    TrustCertificateInfo certificateInfo,
    QWidget *parent
)
    : QDialog(parent)
{
    Q_ASSERT(!certificateInfo.certificates.isEmpty());
    setObjectName(QStringLiteral("certificateDetailsDialog"));
    setWindowTitle(QCoreApplication::translate("TrustRulesDialog", "Certificate Details"));
    setWindowIcon(QIcon(QStringLiteral(":/assets/icons/shield-check.svg")));
    resize(640, 510);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(
        QCoreApplication::translate("TrustRulesDialog", "Certificate Details"),
        this
    );
    title->setObjectName(QStringLiteral("dialogTitle"));
    layout->addWidget(title);

    auto *fileName = new QLabel(QFileInfo(certificateInfo.absolutePath).fileName(), this);
    fileName->setObjectName(QStringLiteral("dialogSubtitle"));
    fileName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileName->setToolTip(certificateInfo.absolutePath);
    layout->addWidget(fileName);

    QComboBox *certificateSelector = nullptr;
    if (certificateInfo.certificates.size() > 1) {
        certificateSelector = new QComboBox(this);
        for (qsizetype index = 0; index < certificateInfo.certificates.size(); ++index) {
            certificateSelector->addItem(
                QCoreApplication::translate("TrustRulesDialog", "Certificate %1 — %2")
                    .arg(index + 1)
                    .arg(TrustCertificateRepository::displayName(
                        certificateInfo.certificates.at(index)
                    ))
            );
        }
        layout->addWidget(certificateSelector);
    }

    auto *status = new QLabel(this);
    status->setObjectName(QStringLiteral("certificateStatus"));
    layout->addWidget(status);

    auto *form = new QFormLayout();
    form->setHorizontalSpacing(18);
    form->setVerticalSpacing(12);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const auto selectableValue = [this] {
        auto *label = new QLabel(this);
        label->setObjectName(QStringLiteral("certificateValue"));
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };

    auto *subject = selectableValue();
    auto *issuer = selectableValue();
    auto *serial = selectableValue();
    auto *validFrom = selectableValue();
    auto *validUntil = selectableValue();
    auto *fingerprint = selectableValue();
    fingerprint->setObjectName(QStringLiteral("certificateFingerprint"));

    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Subject"), subject);
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Issuer"), issuer);
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Serial number"), serial);
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Valid from"), validFrom);
    form->addRow(QCoreApplication::translate("TrustRulesDialog", "Valid until"), validUntil);
    form->addRow(QStringLiteral("SHA-256"), fingerprint);
    layout->addLayout(form, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *copyFingerprint = buttons->addButton(
        QCoreApplication::translate("TrustRulesDialog", "Copy fingerprint"),
        QDialogButtonBox::ActionRole
    );
    layout->addWidget(buttons);

    const auto showCertificate = [=, certificates = std::move(certificateInfo.certificates)](
                                     int index
                                 ) {
        const QSslCertificate &certificate = certificates.at(index);
        subject->setText(TrustCertificateRepository::displayName(certificate));

        QStringList issuerNames = certificate.issuerInfo(QSslCertificate::CommonName);
        if (issuerNames.isEmpty())
            issuerNames = certificate.issuerInfo(QSslCertificate::Organization);
        issuer->setText(
            issuerNames.isEmpty()
                ? QCoreApplication::translate("TrustRulesDialog", "Unknown")
                : issuerNames.join(QStringLiteral(", "))
        );
        serial->setText(QString::fromLatin1(certificate.serialNumber()).toUpper());
        validFrom->setText(certificate.effectiveDate().toLocalTime().toString(Qt::ISODate));
        validUntil->setText(certificate.expiryDate().toLocalTime().toString(Qt::ISODate));
        fingerprint->setText(TrustCertificateRepository::fingerprint(certificate));

        const QDateTime now = QDateTime::currentDateTimeUtc();
        if (now < certificate.effectiveDate().toUTC()) {
            status->setText(QCoreApplication::translate("TrustRulesDialog", "Not valid yet"));
            status->setProperty("state", QStringLiteral("warning"));
        } else if (now > certificate.expiryDate().toUTC()) {
            status->setText(QCoreApplication::translate("TrustRulesDialog", "Expired"));
            status->setProperty("state", QStringLiteral("error"));
        } else {
            status->setText(QCoreApplication::translate("TrustRulesDialog", "Valid"));
            status->setProperty("state", QStringLiteral("valid"));
        }
        repolish(status);
    };

    if (certificateSelector) {
        connect(
            certificateSelector,
            &QComboBox::currentIndexChanged,
            this,
            showCertificate
        );
    }
    connect(copyFingerprint, &QPushButton::clicked, this, [fingerprint] {
        QApplication::clipboard()->setText(fingerprint->text());
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    showCertificate(0);
}
