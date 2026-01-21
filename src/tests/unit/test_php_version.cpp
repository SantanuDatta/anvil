#include <QtTest/QtTest>

#include "models/PHPVersion.h"

using namespace Anvil::Models;

class PHPVersionTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesVersionComponents();
    void validatesVersionStrings();
    void comparesVersions();
    void parsesVersionListOutput();
};

void PHPVersionTest::parsesVersionComponents()
{
    PHPVersion version("8.3.12");

    QCOMPARE(version.majorVersion(), 8);
    QCOMPARE(version.minorVersion(), 3);
    QCOMPARE(version.patchVersion(), 12);
    QCOMPARE(version.shortVersion(), QString("8.3"));
    QCOMPARE(version.fullVersion(), QString("8.3.12"));
}

void PHPVersionTest::validatesVersionStrings()
{
    QVERIFY(PHPVersion::isValidVersion("8.3"));
    QVERIFY(PHPVersion::isValidVersion("8.3.12"));
    QVERIFY(!PHPVersion::isValidVersion("8"));
    QVERIFY(!PHPVersion::isValidVersion("8.3.beta"));
}

void PHPVersionTest::comparesVersions()
{
    PHPVersion older("8.2.9");
    PHPVersion newer("8.3.1");
    PHPVersion same("8.3.1");

    QVERIFY(older < newer);
    QVERIFY(newer > older);
    QVERIFY(newer == same);
    QVERIFY(older != newer);
}

void PHPVersionTest::parsesVersionListOutput()
{
    const QString output = QStringLiteral("PHP 8.3.12 (cli)\ninvalid\n8.2.5\n");
    const QList<PHPVersion> versions = PHPVersion::parseVersionList(output);

    QCOMPARE(versions.size(), 2);
    QCOMPARE(versions.at(0).fullVersion(), QString("8.3.12"));
    QCOMPARE(versions.at(1).fullVersion(), QString("8.2.5"));
}

QTEST_MAIN(PHPVersionTest)
#include "test_php_version.moc"
