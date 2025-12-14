#include "utils/DistroDetector.h"
#include "utils/FileSystem.h"
#include "utils/Logger.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>

namespace Anvil
{
    namespace Utils
    {
        DistroInfo DistroDetector::detect()
        {
            DistroInfo info;

            // Try /etc/os-release first (standard on most modern distros)
            info = parseOSRelease();

            if (info.type == DistroType::Unknown)
            {
                // Fallback to lsb_release
                info = parseLSBRelease();
            }

            // Detect package manager
            info.packageManager = detectPackageManager();

            LOG_INFO(QString("Detected distro: %1 %2 (%3)")
                         .arg(info.name, info.version, packageManagerToString(info.packageManager)));

            return info;
        }

        PackageManager DistroDetector::detectPackageManager()
        {
            // Check for package managers in order of preference
            QStringList managers = {
                "/usr/bin/apt",
                "/usr/bin/dnf",
                "/usr/bin/yum",
                "/usr/bin/pacman",
                "/usr/bin/zypper"};

            for (const QString &path : managers)
            {
                if (QFile::exists(path))
                {
                    if (path.contains("apt"))
                        return PackageManager::APT;
                    if (path.contains("dnf"))
                        return PackageManager::DNF;
                    if (path.contains("yum"))
                        return PackageManager::YUM;
                    if (path.contains("pacman"))
                        return PackageManager::Pacman;
                    if (path.contains("zypper"))
                        return PackageManager::Zypper;
                }
            }

            return PackageManager::Unknown;
        }

        QStringList DistroDetector::getInstallCommand(PackageManager pm, const QString &package)
        {
            return getInstallCommand(pm, QStringList() << package);
        }

        QStringList DistroDetector::getInstallCommand(PackageManager pm, const QStringList &packages)
        {
            QStringList cmd;

            switch (pm)
            {
            case PackageManager::APT:
                cmd << "apt" << "install" << "-y";
                cmd.append(packages);
                break;

            case PackageManager::DNF:
                cmd << "dnf" << "install" << "-y";
                cmd.append(packages);
                break;

            case PackageManager::YUM:
                cmd << "yum" << "install" << "-y";
                cmd.append(packages);
                break;

            case PackageManager::Pacman:
                cmd << "pacman" << "-S" << "--noconfirm";
                cmd.append(packages);
                break;

            case PackageManager::Zypper:
                cmd << "zypper" << "install" << "-y";
                cmd.append(packages);
                break;

            default:
                break;
            }

            return cmd;
        }

        bool DistroDetector::needsPPA(const DistroInfo &distro, const QString &package)
        {
            // Check if this is a PHP version that needs Ondřej's PPA
            if ((distro.type == DistroType::Ubuntu ||
                 distro.type == DistroType::Debian ||
                 distro.type == DistroType::PopOS ||
                 distro.type == DistroType::LinuxMint) &&
                package.contains("php"))
            {
                return true;
            }
            return false;
        }

        QStringList DistroDetector::addPPACommand(const QString &ppa)
        {
            QStringList cmd;
            cmd << "add-apt-repository" << "-y" << ppa;
            return cmd;
        }

        QString DistroDetector::distroTypeToString(DistroType type)
        {
            switch (type)
            {
            case DistroType::Ubuntu:
                return "Ubuntu";
            case DistroType::Debian:
                return "Debian";
            case DistroType::Fedora:
                return "Fedora";
            case DistroType::CentOS:
                return "CentOS";
            case DistroType::Arch:
                return "Arch Linux";
            case DistroType::OpenSUSE:
                return "openSUSE";
            case DistroType::Manjaro:
                return "Manjaro";
            case DistroType::PopOS:
                return "Pop!_OS";
            case DistroType::LinuxMint:
                return "Linux Mint";
            case DistroType::ElementaryOS:
                return "elementary OS";
            default:
                return "Unknown";
            }
        }

        QString DistroDetector::packageManagerToString(PackageManager pm)
        {
            switch (pm)
            {
            case PackageManager::APT:
                return "apt";
            case PackageManager::DNF:
                return "dnf";
            case PackageManager::YUM:
                return "yum";
            case PackageManager::Pacman:
                return "pacman";
            case PackageManager::Zypper:
                return "zypper";
            default:
                return "unknown";
            }
        }

        DistroInfo DistroDetector::parseOSRelease()
        {
            DistroInfo info;

            QFile file("/etc/os-release");
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                return info;
            }

            QTextStream in(&file);
            QMap<QString, QString> values;

            while (!in.atEnd())
            {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#'))
                    continue;

                int pos = line.indexOf('=');
                if (pos > 0)
                {
                    QString key = line.left(pos);
                    QString value = line.mid(pos + 1).remove('"');
                    values[key] = value;
                }
            }

            info.name = values.value("NAME");
            info.version = values.value("VERSION_ID");
            info.codename = values.value("VERSION_CODENAME");

            QString id = values.value("ID").toLower();
            QString idLike = values.value("ID_LIKE").toLower();

            info.type = identifyDistro(id, idLike);

            return info;
        }

        DistroInfo DistroDetector::parseLSBRelease()
        {
            DistroInfo info;

            QProcess process;
            process.start("lsb_release", QStringList() << "-a");

            if (!process.waitForFinished(3000))
            {
                return info;
            }

            QString output = process.readAllStandardOutput();
            QStringList lines = output.split('\n');

            for (const QString &line : lines)
            {
                if (line.contains("Distributor ID:"))
                {
                    info.name = line.split(':').last().trimmed();
                }
                else if (line.contains("Release:"))
                {
                    info.version = line.split(':').last().trimmed();
                }
                else if (line.contains("Codename:"))
                {
                    info.codename = line.split(':').last().trimmed();
                }
            }

            info.type = identifyDistro(info.name.toLower(), "");

            return info;
        }

        DistroType DistroDetector::identifyDistro(const QString &id, const QString &idLike)
        {
            QString search = (id + " " + idLike).toLower();

            if (search.contains("ubuntu"))
                return DistroType::Ubuntu;
            if (search.contains("pop"))
                return DistroType::PopOS;
            if (search.contains("mint"))
                return DistroType::LinuxMint;
            if (search.contains("elementary"))
                return DistroType::ElementaryOS;
            if (search.contains("debian"))
                return DistroType::Debian;
            if (search.contains("fedora"))
                return DistroType::Fedora;
            if (search.contains("centos"))
                return DistroType::CentOS;
            if (search.contains("rhel"))
                return DistroType::CentOS;
            if (search.contains("arch"))
                return DistroType::Arch;
            if (search.contains("manjaro"))
                return DistroType::Manjaro;
            if (search.contains("opensuse") || search.contains("suse"))
                return DistroType::OpenSUSE;

            return DistroType::Unknown;
        }
    }
}
