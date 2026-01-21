#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "models/Site.h"

using namespace Anvil::Models;

class SiteTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsUrls();
    void validatesRequiredFields();
    void serializesAndDeserializes();
};

void SiteTest::buildsUrls()
{
    Site site("Example", "/tmp", "example.test");

    QCOMPARE(site.url(), QString("http://example.test"));
    QCOMPARE(site.secureUrl(), QString("https://example.test"));
}

void SiteTest::validatesRequiredFields()
{
    Site site;
    site.setName(QString());
    site.setPath(QString());
    site.setDomain(QString());

    const QStringList errors = site.validate();

    QVERIFY(errors.contains("Site name is required"));
    QVERIFY(errors.contains("Site path is required"));
    QVERIFY(errors.contains("Domain is required"));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    Site validSite("Valid", tempDir.path(), "valid.test");

    QVERIFY(validSite.isValid());
}

void SiteTest::serializesAndDeserializes()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    Site site("Serialized", tempDir.path(), "serialized.test");
    site.setPhpVersion("8.2");
    site.setSslEnabled(true);
    site.setIsParked(true);

    const QDateTime lastAccessed = QDateTime::fromString("2024-01-10T12:30:00", Qt::ISODate);
    site.setLastAccessed(lastAccessed);

    const QJsonObject json = site.toJson();
    const Site roundTrip = Site::fromJson(json);

    QCOMPARE(roundTrip.name(), site.name());
    QCOMPARE(roundTrip.path(), site.path());
    QCOMPARE(roundTrip.domain(), site.domain());
    QCOMPARE(roundTrip.phpVersion(), site.phpVersion());
    QCOMPARE(roundTrip.sslEnabled(), site.sslEnabled());
    QCOMPARE(roundTrip.isParked(), site.isParked());
    QCOMPARE(roundTrip.lastAccessed(), lastAccessed);
}

QTEST_MAIN(SiteTest)
#include "test_site.moc"
