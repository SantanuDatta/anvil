#ifndef DATABASESERVICE_H
#define DATABASESERVICE_H

#include "services/BaseService.h"
#include <QMap>
#include <QStringList>

namespace Anvil::Services
{
    enum class DatabaseType
    {
        MySQL,
        PostgreSQL,
        MariaDB,
        Unknown
    };

    struct DatabaseInfo
    {
        QString name;
        QString charset;
        QString collation;
        qint64 size;

        DatabaseInfo() : size(0) {}
    };

    struct DatabaseUser
    {
        QString username;
        QString host;
        QStringList privileges;

        DatabaseUser() : host("localhost") {}
        DatabaseUser(const QString &user, const QString &h = "localhost")
            : username(user), host(h) {}
    };

    class DatabaseService : public BaseService
    {
        Q_OBJECT

    public:
        explicit DatabaseService(QObject *parent = nullptr);
        ~DatabaseService() override;

        // BaseService implementation
        ServiceResult<bool> start() override;
        ServiceResult<bool> stop() override;
        bool isInstalled() const override;
        bool isRunning() const override;
        ServiceResult<bool> install() override;
        ServiceResult<bool> uninstall() override;
        ServiceResult<bool> configure() override;
        QString configPath() const override;
        ServiceResult<QString> detectVersion() override;

        // Database type
        DatabaseType databaseType() const { return m_databaseType; }
        void setDatabaseType(DatabaseType type);
        QString databaseTypeString() const;
        static QString databaseTypeToString(DatabaseType type);
        static DatabaseType stringToDatabaseType(const QString &type);

        // Database operations
        ServiceResult<bool> createDatabase(const QString &name,
                                           const QString &charset = "utf8mb4",
                                           const QString &collation = "utf8mb4_unicode_ci");
        ServiceResult<bool> dropDatabase(const QString &name);
        ServiceResult<bool> databaseExists(const QString &name);
        ServiceResult<QList<DatabaseInfo>> listDatabases();
        ServiceResult<DatabaseInfo> getDatabaseInfo(const QString &name);

        // User operations
        ServiceResult<bool> createUser(const QString &username,
                                       const QString &password,
                                       const QString &host = "localhost");
        ServiceResult<bool> dropUser(const QString &username,
                                     const QString &host = "localhost");
        ServiceResult<bool> userExists(const QString &username,
                                       const QString &host = "localhost");
        ServiceResult<QList<DatabaseUser>> listUsers();

        // Privilege operations
        ServiceResult<bool> grantAllPrivileges(const QString &username,
                                               const QString &database,
                                               const QString &host = "localhost");
        ServiceResult<bool> grantPrivileges(const QString &username,
                                            const QString &database,
                                            const QStringList &privileges,
                                            const QString &host = "localhost");
        ServiceResult<bool> revokePrivileges(const QString &username,
                                             const QString &database,
                                             const QString &host = "localhost");
        ServiceResult<bool> flushPrivileges();

        // Connection testing
        ServiceResult<bool> testConnection(const QString &username = "root",
                                           const QString &password = "");
        ServiceResult<QString> executeQuery(const QString &query,
                                            const QString &database = "");

        // Backup and restore
        ServiceResult<QString> backupDatabase(const QString &database,
                                              const QString &outputPath);
        ServiceResult<bool> restoreDatabase(const QString &database,
                                            const QString &inputPath);

        // Configuration
        int port() const { return m_port; }
        void setPort(int port);
        QString socket() const { return m_socket; }
        QString dataDir() const { return m_dataDir; }

        // Root password management
        ServiceResult<bool> setRootPassword(const QString &newPassword,
                                            const QString &currentPassword = "");
        ServiceResult<bool> hasRootPassword();

    signals:
        void databaseCreated(const QString &name);
        void databaseDropped(const QString &name);
        void userCreated(const QString &username);
        void userDropped(const QString &username);

    private:
        // MySQL/MariaDB specific
        ServiceResult<bool> installMySQL();
        ServiceResult<bool> configureMySQL();
        ServiceResult<bool> secureMySQLInstallation();
        QString getMySQLBinary() const;
        QString getMySQLDumpBinary() const;
        QString getMySQLServiceName() const;

        // PostgreSQL specific
        ServiceResult<bool> installPostgreSQL();
        ServiceResult<bool> configurePostgreSQL();
        QString getPostgreSQLBinary() const;
        QString getPostgreSQLDumpBinary() const;
        QString getPostgreSQLServiceName() const;

        // Command execution helpers
        ServiceResult<QString> executeSQLCommand(const QString &command,
                                                 const QString &database = "");
        ServiceResult<QString> executeMySQLCommand(const QStringList &args);
        ServiceResult<QString> executePostgreSQLCommand(const QStringList &args);

        // Parsing helpers
        QList<DatabaseInfo> parseMySQLDatabases(const QString &output);
        QList<DatabaseInfo> parsePostgreSQLDatabases(const QString &output);
        QList<DatabaseUser> parseMySQLUsers(const QString &output);
        QList<DatabaseUser> parsePostgreSQLUsers(const QString &output);

        // Path helpers
        QString getConfigPath() const;
        QString getDataDir() const;
        QString getSocketPath() const;
        QString getLogPath() const;

        // Detection
        DatabaseType detectInstalledDatabase() const;
        QString detectPackageManager() const;

        DatabaseType m_databaseType;
        int m_port;
        QString m_socket;
        QString m_dataDir;
        QString m_logPath;
        QString m_packageManager;
        QString m_rootPassword;
    };
}
#endif
