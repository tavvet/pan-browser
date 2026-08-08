#pragma once

class QString;

namespace PrivateData {

bool ensureDirectory(const QString &path, QString *error = nullptr);
bool restrictFile(const QString &path, QString *error = nullptr);
bool restrictDatabaseFiles(const QString &path, QString *error = nullptr);

} // namespace PrivateData
