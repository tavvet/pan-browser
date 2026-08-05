#pragma once

#include <QWidget>

class BrowserProfile;

class DiagnosticsPage final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsPage(BrowserProfile *profile, QWidget *parent = nullptr);

private:
    [[nodiscard]] QString diagnosticReport() const;

    BrowserProfile *m_profile = nullptr;
};
