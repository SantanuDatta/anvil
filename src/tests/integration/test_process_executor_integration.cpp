#include <QtTest/QtTest>
#include <QFileInfo>

#include "utils/Process.h"

using namespace Anvil::Utils;

class ProcessExecutorIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void executesCommand();
    void resolvesExecutablePathWithoutShellingOut();
    void supportsEnvironmentValuesContainingEqualsSigns();
    void resolvesExecutablePathWhenPathIsEmpty();
};

void ProcessExecutorIntegrationTest::executesCommand()
{
    ProcessExecutor executor;
    const ProcessResult result = executor.execute("echo", {"hello"});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.output.contains("hello"));
}

void ProcessExecutorIntegrationTest::resolvesExecutablePathWithoutShellingOut()
{
    ProcessExecutor executor;

    const QString path = executor.programPath("echo");

    QVERIFY2(!path.isEmpty(), "Expected to find executable path for 'echo'");
    QVERIFY2(QFileInfo::exists(path), "Returned executable path should exist");
}

void ProcessExecutorIntegrationTest::supportsEnvironmentValuesContainingEqualsSigns()
{
    ProcessExecutor executor;
    executor.addEnvironmentVariable("ANVIL_COMPLEX_VALUE", "alpha=beta=gamma");

    const ProcessResult result = executor.execute(
        "python3",
        {"-c", "import os;print(os.getenv('ANVIL_COMPLEX_VALUE', ''))"});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.output.trimmed(), QString("alpha=beta=gamma"));
}

void ProcessExecutorIntegrationTest::resolvesExecutablePathWhenPathIsEmpty()
{
    ProcessExecutor executor;

    const QByteArray originalPath = qgetenv("PATH");
    qputenv("PATH", QByteArray());

    const QString path = executor.programPath("sh");

    if (originalPath.isEmpty())
    {
        qunsetenv("PATH");
    }
    else
    {
        qputenv("PATH", originalPath);
    }

    QVERIFY2(!path.isEmpty(), "Expected fallback path resolution for 'sh' even when PATH is empty");
    QVERIFY2(QFileInfo::exists(path), "Fallback-resolved executable path should exist");
}

QTEST_MAIN(ProcessExecutorIntegrationTest)
#include "test_process_executor_integration.moc"
