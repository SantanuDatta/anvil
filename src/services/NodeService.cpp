#include "services/NodeService.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QDir>
#include <QProcess>
#include <QRegularExpression>

namespace Anvil
{
    namespace Services
    {
        NodeService::NodeService(QObject *parent)
            : BaseService("node", parent), m_hasNvm(false)
        {

            setDisplayName("Node.js");
            setDescription("Node.js runtime environment");

            // Detect nvm
            detectNvm();

            // Detect current version
            if (isInstalled())
            {
                auto versionResult = detectVersion();
                if (versionResult.isSuccess())
                {
                    m_currentVersion = versionResult.data;
                    setVersion(m_currentVersion);
                }

                scanInstalledVersions();
            }
        }

        NodeService::~NodeService()
        {
        }

        ServiceResult<bool> NodeService::start()
        {
            // Node doesn't have a persistent service
            return ServiceResult<bool>::Ok(true);
        }

        ServiceResult<bool> NodeService::stop()
        {
            return ServiceResult<bool>::Ok(true);
        }

        bool NodeService::isInstalled() const
        {
            return checkProgramExists("node") || m_hasNvm;
        }

        bool NodeService::isRunning() const
        {
            return isInstalled();
        }

        ServiceResult<bool> NodeService::install()
        {
            LOG_INFO("Installing nvm");

            if (m_hasNvm)
            {
                return ServiceResult<bool>::Err("nvm is already installed");
            }

            return installNvm();
        }

        ServiceResult<bool> NodeService::uninstall()
        {
            return ServiceResult<bool>::Err("Please uninstall nvm manually");
        }

        ServiceResult<bool> NodeService::configure()
        {
            return ServiceResult<bool>::Ok(true);
        }

        QString NodeService::configPath() const
        {
            return Utils::FileSystem::joinPath(getNvmDir(), ".nvmrc");
        }

        ServiceResult<QString> NodeService::detectVersion()
        {
            auto result = executeAndCapture("node", QStringList() << "--version");

            if (result.isSuccess())
            {
                QString version = result.data.trimmed();
                version.remove('v'); // Remove leading 'v'
                return ServiceResult<QString>::Ok(version);
            }

            return ServiceResult<QString>::Err("Node not found");
        }

        bool NodeService::hasNvm() const
        {
            return m_hasNvm;
        }

        QString NodeService::nvmPath() const
        {
            return m_nvmDir;
        }

        ServiceResult<QList<NodeVersion>> NodeService::listAvailableVersions()
        {
            if (!m_hasNvm)
            {
                return ServiceResult<QList<NodeVersion>>::Err("nvm not installed");
            }

            auto result = executeNvm(QStringList() << "ls-remote" << "--lts");

            if (result.isSuccess())
            {
                return ServiceResult<QList<NodeVersion>>::Ok(parseVersionList(result.data));
            }

            return ServiceResult<QList<NodeVersion>>::Err(result.error);
        }

        ServiceResult<QList<NodeVersion>> NodeService::listInstalledVersions()
        {
            QList<NodeVersion> versions;

            for (auto it = m_installedVersions.begin(); it != m_installedVersions.end(); ++it)
            {
                versions.append(it.value());
            }

            return ServiceResult<QList<NodeVersion>>::Ok(versions);
        }

        ServiceResult<bool> NodeService::installVersion(const QString &version)
        {
            LOG_INFO(QString("Installing Node.js %1").arg(version));

            if (!m_hasNvm)
            {
                return ServiceResult<bool>::Err("nvm not installed. Click 'Install Node/nvm' first.");
            }

            auto result = executeNvm(QStringList() << "install" << version);

            if (result.isSuccess())
            {
                scanInstalledVersions();
                emit versionInstalled(version);
                LOG_INFO(QString("Node.js %1 installed").arg(version));
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> NodeService::uninstallVersion(const QString &version)
        {
            LOG_INFO(QString("Uninstalling Node.js %1").arg(version));

            if (!m_hasNvm)
            {
                return ServiceResult<bool>::Err("nvm not installed");
            }

            auto result = executeNvm(QStringList() << "uninstall" << version);

            if (result.isSuccess())
            {
                m_installedVersions.remove(version);
                emit versionUninstalled(version);
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> NodeService::useVersion(const QString &version)
        {
            LOG_INFO(QString("Switching to Node.js %1").arg(version));

            if (!m_hasNvm)
            {
                return ServiceResult<bool>::Err("nvm not installed");
            }

            auto result = executeNvm(QStringList() << "use" << version);

            if (result.isSuccess())
            {
                m_currentVersion = version;
                setVersion(version);
                emit versionSwitched(version);
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        ServiceResult<bool> NodeService::setDefaultVersion(const QString &version)
        {
            LOG_INFO(QString("Setting Node.js %1 as default").arg(version));

            if (!m_hasNvm)
            {
                return ServiceResult<bool>::Err("nvm not installed");
            }

            auto result = executeNvm(QStringList() << "alias" << "default" << version);

            if (result.isSuccess())
            {
                m_defaultVersion = version;
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err(result.error);
        }

        QString NodeService::defaultVersion() const
        {
            return m_defaultVersion;
        }

        ServiceResult<QStringList> NodeService::listLTSVersions()
        {
            auto result = executeNvm(QStringList() << "ls-remote" << "--lts");

            if (result.isSuccess())
            {
                QStringList ltsVersions;
                QStringList lines = result.data.split('\n');

                for (const QString &line : lines)
                {
                    if (line.contains("(Latest LTS:") || line.contains("(LTS:"))
                    {
                        QString version = line.trimmed().split(' ').first();
                        version.remove('v');
                        ltsVersions << version;
                    }
                }

                return ServiceResult<QStringList>::Ok(ltsVersions);
            }

            return ServiceResult<QStringList>::Err(result.error);
        }

        QString NodeService::latestLTS() const
        {
            return "20"; // Node 20 is current LTS
        }

        ServiceResult<QString> NodeService::npmVersion()
        {
            auto result = executeNpm(QStringList() << "--version");

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data.trimmed());
            }

            return ServiceResult<QString>::Err(result.error);
        }

        ServiceResult<bool> NodeService::updateNpm()
        {
            LOG_INFO("Updating npm");

            auto result = executeNpm(QStringList() << "install" << "-g" << "npm@latest");

            return ServiceResult<bool>::Ok(result.isSuccess());
        }

        ServiceResult<bool> NodeService::npmInstall(const QString &projectPath)
        {
            LOG_INFO(QString("Running npm install in %1").arg(projectPath));

            QStringList args;
            args << "-c" << QString("cd %1 && npm install").arg(projectPath);

            auto result = executeCommand("/bin/sh", args);
            return ServiceResult<bool>::Ok(result.isSuccess());
        }

        ServiceResult<bool> NodeService::npmUpdate(const QString &projectPath)
        {
            QStringList args;
            args << "-c" << QString("cd %1 && npm update").arg(projectPath);

            auto result = executeCommand("/bin/sh", args);
            return ServiceResult<bool>::Ok(result.isSuccess());
        }

        ServiceResult<bool> NodeService::npmRun(const QString &projectPath, const QString &script)
        {
            LOG_INFO(QString("Running npm run %1 in %2").arg(script, projectPath));

            QStringList args;
            args << "-c" << QString("cd %1 && npm run %2").arg(projectPath, script);

            auto result = executeCommand("/bin/sh", args);
            return ServiceResult<bool>::Ok(result.isSuccess());
        }

        ServiceResult<QString> NodeService::readNvmrc(const QString &projectPath)
        {
            QString nvmrcPath = Utils::FileSystem::joinPath(projectPath, ".nvmrc");

            if (!Utils::FileSystem::fileExists(nvmrcPath))
            {
                return ServiceResult<QString>::Err(".nvmrc not found");
            }

            auto result = Utils::FileSystem::readFile(nvmrcPath);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data.trimmed());
            }

            return ServiceResult<QString>::Err(result.error);
        }

        ServiceResult<bool> NodeService::useNvmrc(const QString &projectPath)
        {
            auto versionResult = readNvmrc(projectPath);

            if (versionResult.isError())
            {
                return ServiceResult<bool>::Err(versionResult.error);
            }

            return useVersion(versionResult.data);
        }

        ServiceResult<QString> NodeService::executeNvm(const QStringList &args)
        {
            QString nvmScript = Utils::FileSystem::joinPath(m_nvmDir, "nvm.sh");

            QString command = QString("source %1 && nvm %2")
                                  .arg(nvmScript, args.join(' '));

            QStringList shellArgs;
            shellArgs << "-c" << command;

            auto result = executeAndCapture("/bin/bash", shellArgs);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data);
            }

            return ServiceResult<QString>::Err(result.error);
        }

        ServiceResult<QString> NodeService::executeNode(const QStringList &args)
        {
            auto result = executeAndCapture("node", args);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data);
            }

            return ServiceResult<QString>::Err(result.error);
        }

        ServiceResult<QString> NodeService::executeNpm(const QStringList &args)
        {
            auto result = executeAndCapture("npm", args);

            if (result.isSuccess())
            {
                return ServiceResult<QString>::Ok(result.data);
            }

            return ServiceResult<QString>::Err(result.error);
        }

        ServiceResult<bool> NodeService::installNvm()
        {
            LOG_INFO("Installing nvm");

            QString installCmd = "curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash";

            QStringList args;
            args << "-c" << installCmd;

            auto result = executeCommand("/bin/bash", args);

            if (result.isSuccess())
            {
                detectNvm();
                LOG_INFO("nvm installed successfully");
                return ServiceResult<bool>::Ok(true);
            }

            return ServiceResult<bool>::Err("nvm installation failed");
        }

        ServiceResult<bool> NodeService::detectNvm()
        {
            m_nvmDir = getNvmDir();

            QString nvmScript = Utils::FileSystem::joinPath(m_nvmDir, "nvm.sh");
            m_hasNvm = Utils::FileSystem::fileExists(nvmScript);

            if (m_hasNvm)
            {
                LOG_INFO(QString("nvm detected at: %1").arg(m_nvmDir));
            }

            return ServiceResult<bool>::Ok(m_hasNvm);
        }

        void NodeService::scanInstalledVersions()
        {
            m_installedVersions.clear();

            if (!m_hasNvm)
            {
                return;
            }

            auto result = executeNvm(QStringList() << "list");

            if (result.isSuccess())
            {
                QStringList lines = result.data.split('\n');

                for (const QString &line : lines)
                {
                    NodeVersion version = parseVersionString(line);
                    if (!version.version.isEmpty())
                    {
                        m_installedVersions[version.version] = version;

                        if (version.isDefault)
                        {
                            m_defaultVersion = version.version;
                        }
                    }
                }
            }

            LOG_INFO(QString("Found %1 Node.js versions").arg(m_installedVersions.size()));
        }

        QString NodeService::detectCurrentVersion()
        {
            auto result = detectVersion();
            return result.isSuccess() ? result.data : QString();
        }

        QString NodeService::getNodePath(const QString &version) const
        {
            return Utils::FileSystem::joinPath(m_nvmDir,
                                               QString("versions/node/v%1/bin/node").arg(version));
        }

        QString NodeService::getNpmPath(const QString &version) const
        {
            return Utils::FileSystem::joinPath(m_nvmDir,
                                               QString("versions/node/v%1/bin/npm").arg(version));
        }

        QString NodeService::getNvmDir() const
        {
            QString nvmDir = qgetenv("NVM_DIR");

            if (nvmDir.isEmpty())
            {
                nvmDir = QDir::homePath() + "/.nvm";
            }

            return nvmDir;
        }

        QList<NodeVersion> NodeService::parseVersionList(const QString &output)
        {
            QList<NodeVersion> versions;
            QStringList lines = output.split('\n');

            for (const QString &line : lines)
            {
                NodeVersion version = parseVersionString(line);
                if (!version.version.isEmpty())
                {
                    versions.append(version);
                }
            }

            return versions;
        }

        NodeVersion NodeService::parseVersionString(const QString &line)
        {
            NodeVersion version;

            QString trimmed = line.trimmed();
            if (trimmed.isEmpty())
            {
                return version;
            }

            // Parse: "->     v20.11.0  (default)"
            QRegularExpression regex("(->)?\\s*v?([\\d.]+)\\s*(\\(.*\\))?");
            QRegularExpressionMatch match = regex.match(trimmed);

            if (match.hasMatch())
            {
                version.isDefault = match.captured(1) == "->";
                version.version = match.captured(2);
                version.alias = match.captured(3);
                version.isInstalled = true;
                version.path = getNodePath(version.version);
            }

            return version;
        }
    }
}
