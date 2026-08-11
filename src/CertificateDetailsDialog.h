#pragma once

#include "TrustCertificateRepository.h"

#include <QDialog>

class CertificateDetailsDialog final : public QDialog {
public:
    explicit CertificateDetailsDialog(
        TrustCertificateInfo certificateInfo,
        QWidget *parent = nullptr
    );
};
