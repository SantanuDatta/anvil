#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "utils/FileSystem.h"

using namespace Anvil::Utils;

class FileSystemIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void writesAndReadsFiles();
};

void FileSystemIntegrationTest::writesAndReadsFiles()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString rootPath = tempDir.path();
    const QString subDirPath = FileSystem::joinPath(rootPath, "nested");
    const QString filePath = FileSystem::joinPath(subDirPath, "sample.txt");

    QVERIFY(FileSystem::createDirectory(subDirPath));
    QVERIFY(FileSystem::isDirectory(subDirPath));

    const auto writeResult = FileSystem::writeFile(filePath, "hello anvil");
    QVERIFY(writeResult.isSuccess());
    QVERIFY(FileSystem::fileExists(filePath));

    const auto readResult = FileSystem::readFile(filePath);
    QVERIFY(readResult.isSuccess());
    QCOMPARE(readResult.data.trimmed(), QString("hello anvil"));

    const QStringList entries = FileSystem::listDirectory(subDirPath);
    QVERIFY(entries.contains("sample.txt"));
}

QTEST_MAIN(FileSystemIntegrationTest)
#include "test_filesystem_integration.moc"
