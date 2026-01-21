#include <QtTest/QtTest>

#include "utils/Process.h"

using namespace Anvil::Utils;

class ProcessExecutorIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void executesCommand();
};

void ProcessExecutorIntegrationTest::executesCommand()
{
    ProcessExecutor executor;
    const ProcessResult result = executor.execute("echo", {"hello"});

    QVERIFY(result.isSuccess());
    QCOMPARE(result.exitCode, 0);
    QVERIFY(result.output.contains("hello"));
}

QTEST_MAIN(ProcessExecutorIntegrationTest)
#include "test_process_executor_integration.moc"
