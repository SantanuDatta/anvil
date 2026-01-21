#ifndef DISTRODETECTOR_H
#define DISTRODETECTOR_H

#include <QString>
#include <QMap>

namespace Anvil::Utils
{
    enum class DistroType
    {
        Ubuntu,
        Debian,
        Fedora,
        CentOS,
        Arch,
        OpenSUSE,
        Manjaro,
        PopOS,
        LinuxMint,
        ElementaryOS,
        Unknown
    };

    enum class PackageManager
    {
        APT,    // Debian, Ubuntu, Mint, Pop, Elementary
        DNF,    // Fedora, CentOS 8+
        YUM,    // CentOS 7
        Pacman, // Arch, Manjaro
        Zypper, // OpenSUSE
        Unknown
    };

    struct DistroInfo
    {
        DistroType type;
        QString name;
        QString version;
        QString codename;
        PackageManager packageManager;

        DistroInfo()
            : type(DistroType::Unknown), packageManager(PackageManager::Unknown) {}
    };

    class DistroDetector
    {
    public:
        static DistroInfo detect();

        // Detect package manager
        static PackageManager detectPackageManager();

        // Get install commands for each distro
        static QStringList getInstallCommand(PackageManager pm, const QString &package);
        static QStringList getInstallCommand(PackageManager pm, const QStringList &packages);

        // Check if PPA is needed (for Ubuntu/Debian)
        static bool needsPPA(const DistroInfo &distro, const QString &package);
        static QStringList addPPACommand(const QString &ppa);

        // Convert types to strings
        static QString distroTypeToString(DistroType type);
        static QString packageManagerToString(PackageManager pm);

    private:
        static DistroInfo parseOSRelease();
        static DistroInfo parseLSBRelease();
        static DistroType identifyDistro(const QString &id, const QString &idLike);
    };
}
#endif
