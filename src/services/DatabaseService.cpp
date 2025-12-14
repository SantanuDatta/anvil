#include "services/DatabaseService.h"
#include "core/ConfigManager.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QRegularExpression>
#include <QDir>

namespace Anvil
{
    namespace Services
    {
        DatabaseService::DatabaseService(QObject *parent)
            : BaseService("database", parent), m_databaseType(DatabaseType::Unknown), m_port(3306), m_rootPassword("")
        {

            setDisplayName("Database Server");
            setDescription("MySQL/PostgreSQL/MariaDB database server");

            Core::ConfigManager &config = Core::ConfigManager::instance();
            m_port = config.databasePort();
            m_dataDir = Utils::FileSystem::joinPath(config.dataPath(), "database");
            m_logPath = Utils::FileSystem::joinPath(config.logsPath(), "database.log");

            // Detect package manager
            m_packageManager = detectPackageManager();

            // Detect installed database
            m_databaseType = detectInstalledDatabase();

            // Set database type from config if available
            QString dbType = config.databaseType();
            if (!dbType.isEmpty())
            {
                m_databaseType = stringToDatabaseType(dbType);
            }

            // Detect version
            auto versionResult = detectVersion();
            if (versionResult.isSuccess())
            {
                setVersion(versionResult.data);
            }
        }

        DatabaseService::~DatabaseService()
        {
        }

        ServiceResult<bool> DatabaseService::start()
        {
            LOG_INFO(QString("Starting %1").arg(databaseTypeString()));

            if (!isInstalled())
            {
                return ServiceResult<bool>::Err("Database server is not installed");
            }

            if (isRunning())
            {
                LOG_INFO("Database server is already running");
                return ServiceResult<bool>::Ok(true);
            }

            QString serviceName = (m_databaseType == DatabaseType::PostgreSQL)
                                      ? getPostgreSQLServiceName()
                                      : getMySQLServiceName();

            auto result = executeAsRoot("systemctl", QStringList() << "start" << serviceName);

            if (result.isSuccess())
            {
                updateStatus(Models::ServiceStatus::Running);
                LOG_INFO("Database server started successfully");
            }

            return result;
        }

        ServiceResult<bool> DatabaseService::stop()
        {
            LOG_INFO(QString("Stopping %1").arg(databaseTypeString()));

            if (!isRunning())
            {
                return ServiceResult<bool>::Ok(true);
            }

            QString serviceName = (m_databaseType == DatabaseType::PostgreSQL)
                                      ? getPostgreSQLServiceName()
                                      : getMySQLServiceName();

            auto result = executeAsRoot("systemctl", QStringList() << "stop" << serviceName);

            if (result.isSuccess())
            {
                updateStatus(Models::ServiceStatus::Stopped);
                LOG_INFO("Database server stopped successfully");
            }

            return result;
        }

        bool DatabaseService::isInstalled() const
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                return checkProgramExists("psql");
            }

            return checkProgramExists("mysql");
        }

        bool DatabaseService::isRunning() const
        {
            QString serviceName = (m_databaseType == DatabaseType::PostgreSQL)
                                      ? getPostgreSQLServiceName()
                                      : getMySQLServiceName();

            auto result = executeAndCapture("systemctl", QStringList() << "is-active" << serviceName);
            return result.isSuccess() && result.data.trimmed() == "active";
        }

        ServiceResult<bool> DatabaseService::install()
        {
            LOG_INFO(QString("Installing %1").arg(databaseTypeString()));

            if (isInstalled())
            {
                return ServiceResult<bool>::Err("Database server is already installed");
            }

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                return installPostgreSQL();
            }
            else
            {
                return installMySQL();
            }
        }

        ServiceResult<bool> DatabaseService::uninstall()
        {
            LOG_INFO(QString("Uninstalling %1").arg(databaseTypeString()));

            if (!isInstalled())
            {
                return ServiceResult<bool>::Ok(true);
            }

            // Stop database first
            if (isRunning())
            {
                stop();
            }

            if (m_packageManager == "apt")
            {
                QString package = (m_databaseType == DatabaseType::PostgreSQL)
                                      ? "postgresql"
                                      : "mysql-server";

                return executeAsRoot("apt", QStringList() << "remove" << "-y" << package);
            }

            return ServiceResult<bool>::Err("Unsupported package manager");
        }

        ServiceResult<bool> DatabaseService::configure()
        {
            LOG_INFO(QString("Configuring %1").arg(databaseTypeString()));

            Utils::FileSystem::ensureDirectoryExists(m_dataDir);

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                return configurePostgreSQL();
            }
            else
            {
                return configureMySQL();
            }
        }

        QString DatabaseService::configPath() const
        {
            return getConfigPath();
        }

        ServiceResult<QString> DatabaseService::detectVersion()
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                auto result = executeAndCapture("psql", QStringList() << "--version");

                if (result.isSuccess())
                {
                    QRegularExpression regex("PostgreSQL ([\\d.]+)");
                    QRegularExpressionMatch match = regex.match(result.data);

                    if (match.hasMatch())
                    {
                        return ServiceResult<QString>::Ok(match.captured(1));
                    }
                }
            }
            else
            {
                auto result = executeAndCapture("mysql", QStringList() << "--version");

                if (result.isSuccess())
                {
                    QRegularExpression regex("Ver ([\\d.]+)");
                    QRegularExpressionMatch match = regex.match(result.data);

                    if (match.hasMatch())
                    {
                        return ServiceResult<QString>::Ok(match.captured(1));
                    }
                }
            }

            return ServiceResult<QString>::Err("Could not detect database version");
        }

        void DatabaseService::setDatabaseType(DatabaseType type)
        {
            m_databaseType = type;

            // Update config
            Core::ConfigManager &config = Core::ConfigManager::instance();
            config.setDatabaseType(databaseTypeToString(type));

            // Update display name
            setDisplayName(databaseTypeString());
        }

        QString DatabaseService::databaseTypeString() const
        {
            return databaseTypeToString(m_databaseType);
        }

        QString DatabaseService::databaseTypeToString(DatabaseType type)
        {
            switch (type)
            {
            case DatabaseType::MySQL:
                return "MySQL";
            case DatabaseType::PostgreSQL:
                return "PostgreSQL";
            case DatabaseType::MariaDB:
                return "MariaDB";
            default:
                return "Unknown";
            }
        }

        DatabaseType DatabaseService::stringToDatabaseType(const QString &type)
        {
            QString lower = type.toLower();
            if (lower == "mysql")
                return DatabaseType::MySQL;
            if (lower == "postgresql" || lower == "postgres")
                return DatabaseType::PostgreSQL;
            if (lower == "mariadb")
                return DatabaseType::MariaDB;
            return DatabaseType::Unknown;
        }

        ServiceResult<bool> DatabaseService::createDatabase(const QString &name,
                                                            const QString &charset,
                                                            const QString &collation)
        {
            LOG_INFO(QString("Creating database: %1").arg(name));

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QString cmd = QString("CREATE DATABASE %1;").arg(name);
                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    emit databaseCreated(name);
                    LOG_INFO(QString("Database created: %1").arg(name));
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
            else
            {
                QString cmd = QString("CREATE DATABASE `%1` CHARACTER SET %2 COLLATE %3;")
                                  .arg(name, charset, collation);

                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    emit databaseCreated(name);
                    LOG_INFO(QString("Database created: %1").arg(name));
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
        }

        ServiceResult<bool> DatabaseService::dropDatabase(const QString &name)
        {
            LOG_INFO(QString("Dropping database: %1").arg(name));

            QString cmd = QString("DROP DATABASE IF EXISTS `%1`;").arg(name);
            auto result = executeSQLCommand(cmd);

            if (result.isSuccess())
            {
                emit databaseDropped(name);
                LOG_INFO(QString("Database dropped: %1").arg(name));
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> DatabaseService::databaseExists(const QString &name)
        {
            auto databases = listDatabases();

            if (databases.isError())
            {
                return ServiceResult<bool>::Err(databases.error);
            }

            for (const DatabaseInfo &db : databases.data)
            {
                if (db.name == name)
                {
                    return ServiceResult<bool>::Ok(true);
                }
            }

            return ServiceResult<bool>::Ok(false);
        }

        ServiceResult<QList<DatabaseInfo>> DatabaseService::listDatabases()
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QString cmd = "SELECT datname FROM pg_database WHERE datistemplate = false;";
                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    return ServiceResult<QList<DatabaseInfo>>::Ok(
                        parsePostgreSQLDatabases(result.data));
                }

                return ServiceResult<QList<DatabaseInfo>>::Err(result.error);
            }
            else
            {
                QString cmd = "SHOW DATABASES;";
                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    return ServiceResult<QList<DatabaseInfo>>::Ok(
                        parseMySQLDatabases(result.data));
                }

                return ServiceResult<QList<DatabaseInfo>>::Err(result.error);
            }
        }

        ServiceResult<DatabaseInfo> DatabaseService::getDatabaseInfo(const QString &name)
        {
            auto databases = listDatabases();

            if (databases.isError())
            {
                return ServiceResult<DatabaseInfo>::Err(databases.error);
            }

            for (const DatabaseInfo &db : databases.data)
            {
                if (db.name == name)
                {
                    return ServiceResult<DatabaseInfo>::Ok(db);
                }
            }

            return ServiceResult<DatabaseInfo>::Err("Database not found");
        }

        ServiceResult<bool> DatabaseService::createUser(const QString &username,
                                                        const QString &password,
                                                        const QString &host)
        {
            LOG_INFO(QString("Creating user: %1@%2").arg(username, host));

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QString cmd = QString("CREATE USER %1 WITH PASSWORD '%2';")
                                  .arg(username, password);

                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    emit userCreated(username);
                    LOG_INFO(QString("User created: %1").arg(username));
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
            else
            {
                QString cmd = QString("CREATE USER '%1'@'%2' IDENTIFIED BY '%3';")
                                  .arg(username, host, password);

                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    flushPrivileges();
                    emit userCreated(username);
                    LOG_INFO(QString("User created: %1@%2").arg(username, host));
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
        }

        ServiceResult<bool> DatabaseService::dropUser(const QString &username,
                                                      const QString &host)
        {
            LOG_INFO(QString("Dropping user: %1@%2").arg(username, host));

            QString cmd;
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                cmd = QString("DROP USER IF EXISTS %1;").arg(username);
            }
            else
            {
                cmd = QString("DROP USER IF EXISTS '%1'@'%2';").arg(username, host);
            }

            auto result = executeSQLCommand(cmd);

            if (result.isSuccess())
            {
                if (m_databaseType != DatabaseType::PostgreSQL)
                {
                    flushPrivileges();
                }
                emit userDropped(username);
                LOG_INFO(QString("User dropped: %1").arg(username));
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> DatabaseService::userExists(const QString &username,
                                                        const QString &host)
        {
            auto users = listUsers();

            if (users.isError())
            {
                return ServiceResult<bool>::Err(users.error);
            }

            for (const DatabaseUser &user : users.data)
            {
                if (user.username == username &&
                    (m_databaseType == DatabaseType::PostgreSQL || user.host == host))
                {
                    return ServiceResult<bool>::Ok(true);
                }
            }

            return ServiceResult<bool>::Ok(false);
        }

        ServiceResult<QList<DatabaseUser>> DatabaseService::listUsers()
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QString cmd = "SELECT usename FROM pg_user;";
                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    return ServiceResult<QList<DatabaseUser>>::Ok(
                        parsePostgreSQLUsers(result.data));
                }

                return ServiceResult<QList<DatabaseUser>>::Err(result.error);
            }
            else
            {
                QString cmd = "SELECT User, Host FROM mysql.user;";
                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    return ServiceResult<QList<DatabaseUser>>::Ok(
                        parseMySQLUsers(result.data));
                }

                return ServiceResult<QList<DatabaseUser>>::Err(result.error);
            }
        }

        ServiceResult<bool> DatabaseService::grantAllPrivileges(const QString &username,
                                                                const QString &database,
                                                                const QString &host)
        {
            QStringList privileges = {"ALL PRIVILEGES"};
            return grantPrivileges(username, database, privileges, host);
        }

        ServiceResult<bool> DatabaseService::grantPrivileges(const QString &username,
                                                             const QString &database,
                                                             const QStringList &privileges,
                                                             const QString &host)
        {
            LOG_INFO(QString("Granting privileges to %1 on %2").arg(username, database));

            QString privList = privileges.join(", ");
            QString cmd;

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                cmd = QString("GRANT %1 ON DATABASE %2 TO %3;")
                          .arg(privList, database, username);
            }
            else
            {
                cmd = QString("GRANT %1 ON `%2`.* TO '%3'@'%4';")
                          .arg(privList, database, username, host);
            }

            auto result = executeSQLCommand(cmd);

            if (result.isSuccess())
            {
                if (m_databaseType != DatabaseType::PostgreSQL)
                {
                    flushPrivileges();
                }
                LOG_INFO(QString("Privileges granted to %1").arg(username));
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> DatabaseService::revokePrivileges(const QString &username,
                                                              const QString &database,
                                                              const QString &host)
        {
            LOG_INFO(QString("Revoking privileges from %1 on %2").arg(username, database));

            QString cmd;
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                cmd = QString("REVOKE ALL PRIVILEGES ON DATABASE %1 FROM %2;")
                          .arg(database, username);
            }
            else
            {
                cmd = QString("REVOKE ALL PRIVILEGES ON `%1`.* FROM '%2'@'%3';")
                          .arg(database, username, host);
            }

            auto result = executeSQLCommand(cmd);

            if (result.isSuccess())
            {
                if (m_databaseType != DatabaseType::PostgreSQL)
                {
                    flushPrivileges();
                }
                LOG_INFO(QString("Privileges revoked from %1").arg(username));
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> DatabaseService::flushPrivileges()
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                return ServiceResult<bool>::Ok(true); // Not needed for PostgreSQL
            }

            QString cmd = "FLUSH PRIVILEGES;";
            auto result = executeSQLCommand(cmd);

            if (result.isSuccess())
            {
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }
        // services/DatabaseService.cpp (Part 2 - continued)

        // services/DatabaseService.cpp (Part 2 - continued)

        ServiceResult<bool> DatabaseService::testConnection(const QString &username,
                                                            const QString &password)
        {
            LOG_INFO("Testing database connection");

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QStringList args;
                args << "-U" << username << "-c" << "SELECT 1;";

                // For PostgreSQL, we need to use environment variable for password
                // We'll use executeCommand which doesn't support env vars easily,
                // so we'll do a simple connection test
                auto result = executeAndCapture("psql", args);
                return ServiceResult<bool>::Ok(result.isSuccess());
            }
            else
            {
                QStringList args;
                args << "-u" << username;

                if (!password.isEmpty())
                {
                    args << QString("-p%1").arg(password);
                }

                args << "-e" << "SELECT 1;";

                auto result = executeMySQLCommand(args);
                return ServiceResult<bool>::Ok(result.isSuccess());
            }
        }

        ServiceResult<QString> DatabaseService::executeQuery(const QString &query,
                                                             const QString &database)
        {
            return executeSQLCommand(query, database);
        }

        ServiceResult<QString> DatabaseService::backupDatabase(const QString &database,
                                                               const QString &outputPath)
        {
            LOG_INFO(QString("Backing up database: %1").arg(database));

            QString backupFile = outputPath;
            if (backupFile.isEmpty())
            {
                QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
                backupFile = Utils::FileSystem::joinPath(m_dataDir,
                                                         QString("%1-%2.sql").arg(database, timestamp));
            }

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QStringList args;
                args << "-U" << "postgres" << database << "-f" << backupFile;

                QString pgDump = getPostgreSQLDumpBinary();
                auto result = executeCommand(pgDump, args);

                if (result.isSuccess())
                {
                    LOG_INFO(QString("Backup created: %1").arg(backupFile));
                    return ServiceResult<QString>::Ok(backupFile);
                }

                return ServiceResult<QString>::Err(result.error);
            }
            else
            {
                QStringList args;
                args << database << "--result-file=" + backupFile;

                QString mysqldump = getMySQLDumpBinary();
                auto result = executeCommand(mysqldump, args);

                if (result.isSuccess())
                {
                    LOG_INFO(QString("Backup created: %1").arg(backupFile));
                    return ServiceResult<QString>::Ok(backupFile);
                }

                return ServiceResult<QString>::Err(result.error);
            }
        }

        ServiceResult<bool> DatabaseService::restoreDatabase(const QString &database,
                                                             const QString &inputPath)
        {
            LOG_INFO(QString("Restoring database: %1").arg(database));

            if (!Utils::FileSystem::fileExists(inputPath))
            {
                return ServiceResult<bool>::Err("Backup file does not exist");
            }

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QStringList args;
                args << "-U" << "postgres" << "-d" << database << "-f" << inputPath;

                auto result = executeCommand(getPostgreSQLBinary(), args);

                if (result.isSuccess())
                {
                    LOG_INFO(QString("Database restored: %1").arg(database));
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
            else
            {
                // Use shell command to pipe the backup file
                QString cmd = QString("%1 %2 < %3").arg(getMySQLBinary(), database, inputPath);

                QStringList args;
                args << "-c" << cmd;
                auto result = executeCommand("/bin/sh", args);

                if (result.isSuccess())
                {
                    LOG_INFO(QString("Database restored: %1").arg(database));
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
        }

        void DatabaseService::setPort(int port)
        {
            m_port = port;

            Core::ConfigManager &config = Core::ConfigManager::instance();
            config.setDatabasePort(port);
        }

        ServiceResult<bool> DatabaseService::setRootPassword(const QString &newPassword,
                                                             const QString &currentPassword)
        {
            LOG_INFO("Setting root password");

            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QString cmd = QString("ALTER USER postgres WITH PASSWORD '%1';").arg(newPassword);
                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    m_rootPassword = newPassword;
                    LOG_INFO("Root password updated");
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
            else
            {
                QString cmd;
                if (currentPassword.isEmpty())
                {
                    cmd = QString("ALTER USER 'root'@'localhost' IDENTIFIED BY '%1';")
                              .arg(newPassword);
                }
                else
                {
                    cmd = QString("SET PASSWORD FOR 'root'@'localhost' = PASSWORD('%1');")
                              .arg(newPassword);
                }

                auto result = executeSQLCommand(cmd);

                if (result.isSuccess())
                {
                    flushPrivileges();
                    m_rootPassword = newPassword;
                    LOG_INFO("Root password updated");
                    return ServiceResult<bool>::Ok(true);
                }

                return ServiceResult<bool>::Err(result.error);
            }
        }

        ServiceResult<bool> DatabaseService::hasRootPassword()
        {
            auto result = testConnection("root", "");
            return ServiceResult<bool>::Ok(!result.isSuccess());
        }

        // MySQL/MariaDB specific implementations
        ServiceResult<bool> DatabaseService::installMySQL()
        {
            LOG_INFO("Installing MySQL/MariaDB");

            if (m_packageManager == "apt")
            {
                auto result = executeAsRoot("apt",
                                            QStringList() << "install" << "-y" << "mysql-server");

                if (result.isSuccess())
                {
                    m_databaseType = DatabaseType::MySQL;
                    configure();
                    secureMySQLInstallation();
                    LOG_INFO("MySQL installed successfully");
                    return ServiceResult<bool>::Ok(true);
                }

                return result;
            }

            return ServiceResult<bool>::Err("Unsupported package manager");
        }

        ServiceResult<bool> DatabaseService::configureMySQL()
        {
            LOG_INFO("Configuring MySQL");

            // Create custom config file
            QString configContent = QString(R"(
[mysqld]
port = %1
socket = %2
datadir = %3
log_error = %4

# InnoDB settings
innodb_buffer_pool_size = 256M
innodb_log_file_size = 64M
innodb_flush_log_at_trx_commit = 2

# Connection settings
max_connections = 200
max_allowed_packet = 64M

# Character set
character-set-server = utf8mb4
collation-server = utf8mb4_unicode_ci

[client]
port = %1
socket = %2
                                                                                                                                                                                                                                                                )")
                                        .arg(m_port)
                                        .arg(getSocketPath(), m_dataDir, m_logPath);

            QString configPath = Utils::FileSystem::joinPath(m_dataDir, "my.cnf");
            auto result = Utils::FileSystem::writeFile(configPath, configContent);

            if (result.isError())
            {
                return ServiceResult<bool>::Err(result.error);
            }

            LOG_INFO("MySQL configuration created");
            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> DatabaseService::secureMySQLInstallation()
        {
            LOG_INFO("Securing MySQL installation");

            // Remove anonymous users
            executeSQLCommand("DELETE FROM mysql.user WHERE User='';");

            // Remove remote root
            executeSQLCommand("DELETE FROM mysql.user WHERE User='root' AND Host NOT IN ('localhost', '127.0.0.1', '::1');");

            // Remove test database
            executeSQLCommand("DROP DATABASE IF EXISTS test;");
            executeSQLCommand("DELETE FROM mysql.db WHERE Db='test' OR Db='test\\_%';");

            // Flush privileges
            flushPrivileges();

            LOG_INFO("MySQL installation secured");
            return ServiceResult<bool>::Ok(true);
        }

        QString DatabaseService::getMySQLBinary() const
        {
            return getProgramPath("mysql");
        }

        QString DatabaseService::getMySQLDumpBinary() const
        {
            return getProgramPath("mysqldump");
        }

        QString DatabaseService::getMySQLServiceName() const
        {
            if (checkProgramExists("mariadb"))
            {
                return "mariadb";
            }
            return "mysql";
        }

        // PostgreSQL specific implementations
        ServiceResult<bool> DatabaseService::installPostgreSQL()
        {
            LOG_INFO("Installing PostgreSQL");

            if (m_packageManager == "apt")
            {
                auto result = executeAsRoot("apt",
                                            QStringList() << "install" << "-y" << "postgresql" << "postgresql-contrib");

                if (result.isSuccess())
                {
                    m_databaseType = DatabaseType::PostgreSQL;
                    m_port = 5432;
                    configure();
                    LOG_INFO("PostgreSQL installed successfully");
                    return ServiceResult<bool>::Ok(true);
                }

                return result;
            }

            return ServiceResult<bool>::Err("Unsupported package manager");
        }

        ServiceResult<bool> DatabaseService::configurePostgreSQL()
        {
            LOG_INFO("Configuring PostgreSQL");

            // PostgreSQL configuration is more complex and typically managed by the package
            // For now, we'll just ensure the data directory exists
            Utils::FileSystem::ensureDirectoryExists(m_dataDir);

            LOG_INFO("PostgreSQL configuration checked");
            return ServiceResult<bool>::Ok(true);
        }

        QString DatabaseService::getPostgreSQLBinary() const
        {
            return getProgramPath("psql");
        }

        QString DatabaseService::getPostgreSQLDumpBinary() const
        {
            return getProgramPath("pg_dump");
        }

        QString DatabaseService::getPostgreSQLServiceName() const
        {
            return "postgresql";
        }

        // Command execution helpers
        ServiceResult<QString> DatabaseService::executeSQLCommand(const QString &command,
                                                                  const QString &database)
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                QStringList args;
                args << "-U" << "postgres";

                if (!database.isEmpty())
                {
                    args << "-d" << database;
                }

                args << "-c" << command;

                return executePostgreSQLCommand(args);
            }
            else
            {
                QStringList args;
                args << "-u" << "root";

                if (!m_rootPassword.isEmpty())
                {
                    args << QString("-p%1").arg(m_rootPassword);
                }

                if (!database.isEmpty())
                {
                    args << database;
                }

                args << "-e" << command;

                return executeMySQLCommand(args);
            }
        }

        ServiceResult<QString> DatabaseService::executeMySQLCommand(const QStringList &args)
        {
            auto result = executeAndCapture(getMySQLBinary(), args);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data);
            }

            return ServiceResult<QString>::Err(result.error);
        }

        ServiceResult<QString> DatabaseService::executePostgreSQLCommand(const QStringList &args)
        {
            auto result = executeAndCapture(getPostgreSQLBinary(), args);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data);
            }

            return ServiceResult<QString>::Err(result.error);
        }

        // Parsing helpers
        QList<DatabaseInfo> DatabaseService::parseMySQLDatabases(const QString &output)
        {
            QList<DatabaseInfo> databases;
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);

            for (const QString &line : lines)
            {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed == "Database" ||
                    trimmed.startsWith("+") || trimmed.startsWith("|"))
                {
                    continue;
                }

                // Skip system databases
                if (trimmed == "information_schema" ||
                    trimmed == "performance_schema" ||
                    trimmed == "mysql" ||
                    trimmed == "sys")
                {
                    continue;
                }

                DatabaseInfo info;
                info.name = trimmed;
                databases.append(info);
            }

            return databases;
        }

        QList<DatabaseInfo> DatabaseService::parsePostgreSQLDatabases(const QString &output)
        {
            QList<DatabaseInfo> databases;
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);

            for (const QString &line : lines)
            {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed == "datname")
                {
                    continue;
                }

                // Skip system databases
                if (trimmed == "postgres" ||
                    trimmed == "template0" ||
                    trimmed == "template1")
                {
                    continue;
                }

                DatabaseInfo info;
                info.name = trimmed;
                databases.append(info);
            }

            return databases;
        }

        QList<DatabaseUser> DatabaseService::parseMySQLUsers(const QString &output)
        {
            QList<DatabaseUser> users;
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);

            for (const QString &line : lines)
            {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed.contains("User") ||
                    trimmed.startsWith("+") || trimmed.startsWith("|"))
                {
                    continue;
                }

                QStringList parts = trimmed.split('\t', Qt::SkipEmptyParts);
                if (parts.size() >= 2)
                {
                    DatabaseUser user;
                    user.username = parts[0].trimmed();
                    user.host = parts[1].trimmed();
                    users.append(user);
                }
            }

            return users;
        }

        QList<DatabaseUser> DatabaseService::parsePostgreSQLUsers(const QString &output)
        {
            QList<DatabaseUser> users;
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);

            for (const QString &line : lines)
            {
                QString trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed == "usename")
                {
                    continue;
                }

                DatabaseUser user;
                user.username = trimmed;
                users.append(user);
            }

            return users;
        }

        // Path helpers
        QString DatabaseService::getConfigPath() const
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                return "/etc/postgresql/postgresql.conf";
            }
            return "/etc/mysql/my.cnf";
        }

        QString DatabaseService::getDataDir() const
        {
            return m_dataDir;
        }

        QString DatabaseService::getSocketPath() const
        {
            if (m_databaseType == DatabaseType::PostgreSQL)
            {
                return "/var/run/postgresql/.s.PGSQL.5432";
            }
            return "/var/run/mysqld/mysqld.sock";
        }

        QString DatabaseService::getLogPath() const
        {
            return m_logPath;
        }

        // Detection
        DatabaseType DatabaseService::detectInstalledDatabase() const
        {
            if (checkProgramExists("psql"))
            {
                return DatabaseType::PostgreSQL;
            }

            if (checkProgramExists("mariadb"))
            {
                return DatabaseType::MariaDB;
            }

            if (checkProgramExists("mysql"))
            {
                return DatabaseType::MySQL;
            }

            return DatabaseType::Unknown;
        }

        QString DatabaseService::detectPackageManager() const
        {
            if (checkProgramExists("apt"))
            {
                return "apt";
            }
            else if (checkProgramExists("dnf"))
            {
                return "dnf";
            }
            else if (checkProgramExists("yum"))
            {
                return "yum";
            }
            else if (checkProgramExists("pacman"))
            {
                return "pacman";
            }
            return QString();
        }
    }
}
