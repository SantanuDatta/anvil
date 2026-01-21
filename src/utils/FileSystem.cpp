#include "utils/FileSystem.h"
#include "utils/Logger.h"
#include <QFileInfo>
#include <QDirIterator>
#include <QTextStream>
#include <QTemporaryFile>
#include <QDateTime>
#include <QRegularExpression>

namespace Anvil::Utils
{
    FileResult<QString> FileSystem::readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return FileResult<QString>::Err(
                QString("Cannot open file for reading: %1").arg(file.errorString()));
        }

        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();

        return FileResult<QString>::Ok(content);
    }

    FileResult<QByteArray> FileSystem::readBinaryFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return FileResult<QByteArray>::Err(
                QString("Cannot open file for reading: %1").arg(file.errorString()));
        }

        QByteArray data = file.readAll();
        file.close();

        return FileResult<QByteArray>::Ok(data);
    }

    FileResult<QStringList> FileSystem::readLines(const QString &path)
    {
        auto result = readFile(path);
        if (result.isError())
        {
            return FileResult<QStringList>::Err(result.error);
        }

        QStringList lines = result.data.split('\n');
        return FileResult<QStringList>::Ok(lines);
    }

    FileResult<bool> FileSystem::writeFile(const QString &path, const QString &content)
    {
        // Ensure parent directory exists
        QFileInfo fileInfo(path);
        if (!ensureDirectoryExists(fileInfo.absolutePath()))
        {
            return FileResult<bool>::Err("Cannot create parent directory");
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        {
            return FileResult<bool>::Err(
                QString("Cannot open file for writing: %1").arg(file.errorString()));
        }

        QTextStream stream(&file);
        stream << content;
        file.close();

        if (file.error() != QFile::NoError)
        {
            return FileResult<bool>::Err(file.errorString());
        }

        return FileResult<bool>::Ok(true);
    }

    FileResult<bool> FileSystem::writeBinaryFile(const QString &path, const QByteArray &data)
    {
        QFileInfo fileInfo(path);
        if (!ensureDirectoryExists(fileInfo.absolutePath()))
        {
            return FileResult<bool>::Err("Cannot create parent directory");
        }

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return FileResult<bool>::Err(
                QString("Cannot open file for writing: %1").arg(file.errorString()));
        }

        qint64 written = file.write(data);
        file.close();

        if (written != data.size())
        {
            return FileResult<bool>::Err("Failed to write all data");
        }

        return FileResult<bool>::Ok(true);
    }

    FileResult<bool> FileSystem::appendFile(const QString &path, const QString &content)
    {
        QFile file(path);
        if (!file.open(QIODevice::Append | QIODevice::Text))
        {
            return FileResult<bool>::Err(
                QString("Cannot open file for appending: %1").arg(file.errorString()));
        }

        QTextStream stream(&file);
        stream << content;
        file.close();

        return FileResult<bool>::Ok(true);
    }

    // ============================================================================
    // File Information
    // ============================================================================

    bool FileSystem::fileExists(const QString &path)
    {
        return QFileInfo::exists(path);
    }

    bool FileSystem::isFile(const QString &path)
    {
        return QFileInfo(path).isFile();
    }

    bool FileSystem::isDirectory(const QString &path)
    {
        return QFileInfo(path).isDir();
    }

    bool FileSystem::isReadable(const QString &path)
    {
        return QFileInfo(path).isReadable();
    }

    bool FileSystem::isWritable(const QString &path)
    {
        return QFileInfo(path).isWritable();
    }

    bool FileSystem::isExecutable(const QString &path)
    {
        return QFileInfo(path).isExecutable();
    }

    qint64 FileSystem::fileSize(const QString &path)
    {
        return QFileInfo(path).size();
    }

    QDateTime FileSystem::lastModified(const QString &path)
    {
        return QFileInfo(path).lastModified();
    }

    // ============================================================================
    // File Operations
    // ============================================================================

    bool FileSystem::copyFile(const QString &source, const QString &destination, bool overwrite)
    {
        if (!fileExists(source))
        {
            LOG_ERROR(QString("Source file does not exist: %1").arg(source));
            return false;
        }

        if (fileExists(destination) && !overwrite)
        {
            LOG_WARNING(QString("Destination file already exists: %1").arg(destination));
            return false;
        }

        if (fileExists(destination))
        {
            QFile::remove(destination);
        }

        QFileInfo destInfo(destination);
        ensureDirectoryExists(destInfo.absolutePath());

        return QFile::copy(source, destination);
    }

    bool FileSystem::moveFile(const QString &source, const QString &destination)
    {
        if (!fileExists(source))
        {
            LOG_ERROR(QString("Source file does not exist: %1").arg(source));
            return false;
        }

        QFileInfo destInfo(destination);
        ensureDirectoryExists(destInfo.absolutePath());

        return QFile::rename(source, destination);
    }

    bool FileSystem::deleteFile(const QString &path)
    {
        if (!fileExists(path))
        {
            return true; // Already deleted
        }

        return QFile::remove(path);
    }

    bool FileSystem::renameFile(const QString &oldPath, const QString &newPath)
    {
        return moveFile(oldPath, newPath);
    }

    bool FileSystem::createSymlink(const QString &target, const QString &link)
    {
        if (fileExists(link))
        {
            QFile::remove(link);
        }

        return QFile::link(target, link);
    }

    // ============================================================================
    // Directory Operations
    // ============================================================================

    bool FileSystem::createDirectory(const QString &path, bool createParents)
    {
        QDir dir;
        if (createParents)
        {
            return dir.mkpath(path);
        }
        return dir.mkdir(path);
    }

    bool FileSystem::deleteDirectory(const QString &path, bool recursive)
    {
        QDir dir(path);

        if (!dir.exists())
        {
            return true; // Already deleted
        }

        if (!recursive)
        {
            return dir.rmdir(path);
        }

        return dir.removeRecursively();
    }

    bool FileSystem::copyDirectory(const QString &source, const QString &destination)
    {
        QDir sourceDir(source);
        if (!sourceDir.exists())
        {
            LOG_ERROR(QString("Source directory does not exist: %1").arg(source));
            return false;
        }

        if (!createDirectory(destination))
        {
            LOG_ERROR(QString("Cannot create destination directory: %1").arg(destination));
            return false;
        }

        return copyDirectoryRecursive(source, destination);
    }

    QStringList FileSystem::listDirectory(const QString &path, const QStringList &filters)
    {
        QDir dir(path);
        if (!dir.exists())
        {
            return QStringList();
        }

        QDir::Filters dirFilters = QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot;

        if (filters.isEmpty())
        {
            return dir.entryList(dirFilters);
        }

        return dir.entryList(filters, dirFilters);
    }

    QStringList FileSystem::listDirectoryRecursive(const QString &path, const QStringList &filters)
    {
        QStringList result;
        listDirectoryRecursiveInternal(path, filters, result);
        return result;
    }

    // ============================================================================
    // Path Operations
    // ============================================================================

    QString FileSystem::joinPath(const QString &base, const QString &path)
    {
        return QDir(base).filePath(path);
    }

    QString FileSystem::absolutePath(const QString &path)
    {
        return QFileInfo(path).absoluteFilePath();
    }

    QString FileSystem::relativePath(const QString &from, const QString &to)
    {
        return QDir(from).relativeFilePath(to);
    }

    QString FileSystem::parentDirectory(const QString &path)
    {
        return QFileInfo(path).absolutePath();
    }

    QString FileSystem::fileName(const QString &path)
    {
        return QFileInfo(path).fileName();
    }

    QString FileSystem::fileBaseName(const QString &path)
    {
        return QFileInfo(path).baseName();
    }

    QString FileSystem::fileExtension(const QString &path)
    {
        return QFileInfo(path).suffix();
    }

    QString FileSystem::cleanPath(const QString &path)
    {
        return QDir::cleanPath(path);
    }

    // ============================================================================
    // Permission Operations
    // ============================================================================

    bool FileSystem::setPermissions(const QString &path, QFile::Permissions permissions)
    {
        return QFile::setPermissions(path, permissions);
    }

    bool FileSystem::makeExecutable(const QString &path)
    {
        QFile::Permissions perms = QFile::permissions(path);
        perms |= QFile::ExeOwner | QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther;
        return QFile::setPermissions(path, perms);
    }

    QFile::Permissions FileSystem::getPermissions(const QString &path)
    {
        return QFileInfo(path).permissions();
    }

    // ============================================================================
    // Template Operations
    // ============================================================================

    FileResult<QString> FileSystem::processTemplate(const QString &templatePath,
                                                    const QMap<QString, QString> &variables)
    {
        auto result = readFile(templatePath);
        if (result.isError())
        {
            return result;
        }

        QString processed = replaceVariables(result.data, variables);
        return FileResult<QString>::Ok(processed);
    }

    QString FileSystem::replaceVariables(const QString &content,
                                         const QMap<QString, QString> &variables)
    {
        QString result = content;

        for (auto it = variables.begin(); it != variables.end(); ++it)
        {
            QString placeholder = QString("{{%1}}").arg(it.key());
            result.replace(placeholder, it.value());
        }

        return result;
    }

    // ============================================================================
    // Search Operations
    // ============================================================================

    QStringList FileSystem::findFiles(const QString &directory,
                                      const QString &pattern,
                                      bool recursive)
    {
        QStringList result;

        QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags;
        if (recursive)
        {
            flags = QDirIterator::Subdirectories;
        }

        QDirIterator it(directory, QStringList() << pattern, QDir::Files, flags);

        while (it.hasNext())
        {
            result << it.next();
        }

        return result;
    }

    QStringList FileSystem::findFilesWithContent(const QString &directory,
                                                 const QString &searchText,
                                                 bool recursive)
    {
        QStringList result;
        QStringList files = findFiles(directory, "*", recursive);

        for (const QString &file : files)
        {
            auto content = readFile(file);
            if (content.isSuccess() && content.data.contains(searchText))
            {
                result << file;
            }
        }

        return result;
    }

    // ============================================================================
    // Utility Operations
    // ============================================================================

    bool FileSystem::ensureDirectoryExists(const QString &path)
    {
        if (isDirectory(path))
        {
            return true;
        }
        return createDirectory(path, true);
    }

    QString FileSystem::getTempDirectory()
    {
        return QDir::tempPath();
    }

    QString FileSystem::createTempFile(const QString &templateName)
    {
        QTemporaryFile temp(joinPath(getTempDirectory(), templateName));
        temp.setAutoRemove(false);

        if (!temp.open())
        {
            return QString();
        }

        QString path = temp.fileName();
        temp.close();

        return path;
    }

    QString FileSystem::createTempDirectory(const QString &templateName)
    {
        QString tempDir = joinPath(getTempDirectory(), templateName);
        tempDir.replace("XXXXXX", QString::number(QDateTime::currentMSecsSinceEpoch()));

        if (createDirectory(tempDir))
        {
            return tempDir;
        }

        return QString();
    }

    qint64 FileSystem::getDirectorySize(const QString &path)
    {
        qint64 totalSize = 0;

        QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            totalSize += QFileInfo(it.next()).size();
        }

        return totalSize;
    }

    bool FileSystem::isEmpty(const QString &path)
    {
        if (isFile(path))
        {
            return fileSize(path) == 0;
        }

        if (isDirectory(path))
        {
            QDir dir(path);
            return dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty();
        }

        return true;
    }

    // ============================================================================
    // Backup Operations
    // ============================================================================

    FileResult<QString> FileSystem::createBackup(const QString &path)
    {
        if (!fileExists(path))
        {
            return FileResult<QString>::Err("File does not exist");
        }

        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
        QString backupPath = QString("%1.backup-%2").arg(path, timestamp);

        if (copyFile(path, backupPath))
        {
            return FileResult<QString>::Ok(backupPath);
        }

        return FileResult<QString>::Err("Failed to create backup");
    }

    bool FileSystem::restoreBackup(const QString &backupPath, const QString &originalPath)
    {
        return copyFile(backupPath, originalPath, true);
    }

    // ============================================================================
    // Safe Operations
    // ============================================================================

    FileResult<QString> FileSystem::safeReadFile(const QString &path, qint64 maxSize)
    {
        if (!isPathSafe(path))
        {
            return FileResult<QString>::Err("Path is not safe");
        }

        qint64 size = fileSize(path);
        if (size > maxSize)
        {
            return FileResult<QString>::Err(
                QString("File too large: %1 bytes (max: %2)").arg(size).arg(maxSize));
        }

        return readFile(path);
    }

    FileResult<bool> FileSystem::safeWriteFile(const QString &path, const QString &content)
    {
        if (!isPathSafe(path))
        {
            return FileResult<bool>::Err("Path is not safe");
        }

        return writeFile(path, content);
    }

    bool FileSystem::isPathSafe(const QString &path)
    {
        QString cleaned = cleanPath(path);

        // Check for path traversal attempts
        if (cleaned.contains(".."))
        {
            return false;
        }

        // Check for null bytes
        if (cleaned.contains(QChar('\0')))
        {
            return false;
        }

        return true;
    }

    bool FileSystem::isWithinDirectory(const QString &path, const QString &baseDir)
    {
        QString absPath = absolutePath(path);
        QString absBase = absolutePath(baseDir);

        return absPath.startsWith(absBase);
    }

    // ============================================================================
    // Private Helper Methods
    // ============================================================================

    bool FileSystem::copyDirectoryRecursive(const QString &source, const QString &destination)
    {
        QDir sourceDir(source);

        QStringList entries = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &entry : entries)
        {
            QString srcPath = joinPath(source, entry);
            QString dstPath = joinPath(destination, entry);

            if (isDirectory(srcPath))
            {
                if (!createDirectory(dstPath))
                {
                    return false;
                }
                if (!copyDirectoryRecursive(srcPath, dstPath))
                {
                    return false;
                }
            }
            else
            {
                if (!copyFile(srcPath, dstPath))
                {
                    return false;
                }
            }
        }

        return true;
    }

    void FileSystem::listDirectoryRecursiveInternal(const QString &path,
                                                    const QStringList &filters,
                                                    QStringList &result)
    {
        QDir dir(path);
        QStringList entries = dir.entryList(filters, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &entry : entries)
        {
            QString fullPath = joinPath(path, entry);

            if (isDirectory(fullPath))
            {
                listDirectoryRecursiveInternal(fullPath, filters, result);
            }
            else
            {
                result << fullPath;
            }
        }
    }

    QString FileSystem::sanitizePath(const QString &path)
    {
        return cleanPath(path);
    }

    // ============================================================================
    // ScopedFile Implementation
    // ============================================================================

    ScopedFile::ScopedFile(const QString &path, QIODevice::OpenMode mode)
        : m_file(path)
    {
        m_file.open(mode);
    }

    ScopedFile::~ScopedFile()
    {
        if (m_file.isOpen())
        {
            m_file.close();
        }
    }

    bool ScopedFile::isOpen() const
    {
        return m_file.isOpen();
    }

    QFile *ScopedFile::file()
    {
        return &m_file;
    }

    QString ScopedFile::errorString() const
    {
        return m_file.errorString();
    }

    // ============================================================================
    // DirectoryIterator Implementation
    // ============================================================================

    DirectoryIterator::DirectoryIterator(const QString &path, bool recursive)
        : m_path(path), m_recursive(recursive), m_currentIndex(0)
    {
        loadEntries();
    }

    bool DirectoryIterator::hasNext()
    {
        return m_currentIndex < m_entries.size();
    }

    QString DirectoryIterator::next()
    {
        if (!hasNext())
        {
            return QString();
        }
        return m_entries[m_currentIndex++];
    }

    void DirectoryIterator::reset()
    {
        m_currentIndex = 0;
    }

    void DirectoryIterator::setFilters(const QStringList &filters)
    {
        m_filters = filters;
        loadEntries();
    }

    void DirectoryIterator::setNameFilters(const QStringList &nameFilters)
    {
        m_nameFilters = nameFilters;
        loadEntries();
    }

    void DirectoryIterator::loadEntries()
    {
        m_entries.clear();
        m_currentIndex = 0;

        if (m_recursive)
        {
            m_entries = FileSystem::listDirectoryRecursive(m_path, m_nameFilters);
        }
        else
        {
            m_entries = FileSystem::listDirectory(m_path, m_nameFilters);

            // Convert to absolute paths
            for (QString &entry : m_entries)
            {
                entry = FileSystem::joinPath(m_path, entry);
            }
        }
    }
}
