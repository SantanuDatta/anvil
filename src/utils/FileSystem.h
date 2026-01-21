#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <QString>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <functional>

namespace Anvil::Utils
{
    template <typename T>
    struct FileResult
    {
        bool success;
        T data;
        QString error;

        static FileResult<T> Ok(const T &data)
        {
            return {true, data, QString()};
        }

        static FileResult<T> Err(const QString &error)
        {
            return {false, T(), error};
        }

        bool isSuccess() const { return success; }
        bool isError() const { return !success; }
    };

    class FileSystem
    {
    public:
        // File operations
        static FileResult<QString> readFile(const QString &path);
        static FileResult<QByteArray> readBinaryFile(const QString &path);
        static FileResult<QStringList> readLines(const QString &path);
        static FileResult<bool> writeFile(const QString &path, const QString &content);
        static FileResult<bool> writeBinaryFile(const QString &path, const QByteArray &data);
        static FileResult<bool> appendFile(const QString &path, const QString &content);

        // File information
        static bool fileExists(const QString &path);
        static bool isFile(const QString &path);
        static bool isDirectory(const QString &path);
        static bool isReadable(const QString &path);
        static bool isWritable(const QString &path);
        static bool isExecutable(const QString &path);
        static qint64 fileSize(const QString &path);
        static QDateTime lastModified(const QString &path);

        // File operations
        static bool copyFile(const QString &source, const QString &destination, bool overwrite = false);
        static bool moveFile(const QString &source, const QString &destination);
        static bool deleteFile(const QString &path);
        static bool renameFile(const QString &oldPath, const QString &newPath);
        static bool createSymlink(const QString &target, const QString &link);

        // Directory operations
        static bool createDirectory(const QString &path, bool createParents = true);
        static bool deleteDirectory(const QString &path, bool recursive = false);
        static bool copyDirectory(const QString &source, const QString &destination);
        static QStringList listDirectory(const QString &path, const QStringList &filters = QStringList());
        static QStringList listDirectoryRecursive(const QString &path, const QStringList &filters = QStringList());

        // Path operations
        static QString joinPath(const QString &base, const QString &path);
        static QString absolutePath(const QString &path);
        static QString relativePath(const QString &from, const QString &to);
        static QString parentDirectory(const QString &path);
        static QString fileName(const QString &path);
        static QString fileBaseName(const QString &path);
        static QString fileExtension(const QString &path);
        static QString cleanPath(const QString &path);

        // Permission operations
        static bool setPermissions(const QString &path, QFile::Permissions permissions);
        static bool makeExecutable(const QString &path);
        static QFile::Permissions getPermissions(const QString &path);

        // Template operations
        static FileResult<QString> processTemplate(const QString &templatePath,
                                                   const QMap<QString, QString> &variables);
        static QString replaceVariables(const QString &content,
                                        const QMap<QString, QString> &variables);

        // Search operations
        static QStringList findFiles(const QString &directory,
                                     const QString &pattern,
                                     bool recursive = true);
        static QStringList findFilesWithContent(const QString &directory,
                                                const QString &searchText,
                                                bool recursive = true);

        // Utility operations
        static bool ensureDirectoryExists(const QString &path);
        static QString getTempDirectory();
        static QString createTempFile(const QString &templateName = "temp-XXXXXX");
        static QString createTempDirectory(const QString &templateName = "temp-dir-XXXXXX");
        static qint64 getDirectorySize(const QString &path);
        static bool isEmpty(const QString &path);

        // Backup operations
        static FileResult<QString> createBackup(const QString &path);
        static bool restoreBackup(const QString &backupPath, const QString &originalPath);

        // Safe operations (with validation)
        static FileResult<QString> safeReadFile(const QString &path, qint64 maxSize = 10 * 1024 * 1024);
        static FileResult<bool> safeWriteFile(const QString &path, const QString &content);
        static bool isPathSafe(const QString &path);
        static bool isWithinDirectory(const QString &path, const QString &baseDir);

    private:
        // Internal helpers
        static bool copyDirectoryRecursive(const QString &source, const QString &destination);
        static void listDirectoryRecursiveInternal(const QString &path,
                                                   const QStringList &filters,
                                                   QStringList &result);
        static QString sanitizePath(const QString &path);
    };

    // RAII file handle wrapper
    class ScopedFile
    {
    public:
        explicit ScopedFile(const QString &path, QIODevice::OpenMode mode);
        ~ScopedFile();

        bool isOpen() const;
        QFile *file();
        QString errorString() const;

        ScopedFile(const ScopedFile &) = delete;
        ScopedFile &operator=(const ScopedFile &) = delete;

    private:
        QFile m_file;
    };

    // Directory iterator for efficient traversal
    class DirectoryIterator
    {
    public:
        explicit DirectoryIterator(const QString &path, bool recursive = false);

        bool hasNext();
        QString next();
        void reset();

        void setFilters(const QStringList &filters);
        void setNameFilters(const QStringList &nameFilters);

    private:
        QString m_path;
        bool m_recursive;
        QStringList m_filters;
        QStringList m_nameFilters;
        QStringList m_entries;
        int m_currentIndex;

        void loadEntries();
    };
}
#endif
