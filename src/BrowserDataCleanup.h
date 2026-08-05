#pragma once

#include <QString>

bool removeManagedDataDirectory(
    const QString &directoryPath,
    const QString &managedRoot,
    QString *error = nullptr
);
