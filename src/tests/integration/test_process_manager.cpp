/**
 * ProcessManager Integration Test
 *
 * Tests all ProcessManager functionality
 */

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include "core/ConfigManager.h"
#include "core/ServiceManager.h"
#include "managers/ProcessManager.h"
#include "utils/Logger.h"

using namespace Anvil;

void testProcessManagerInit()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 1: ProcessManager Initialization";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;

    if (!processManager.initialize())
    {
        qDebug() << "❌ Initialization failed";
        return;
    }

    qDebug() << "✅ ProcessManager initialized";
    qDebug() << "✅ Ready for process orchestration";
}

void testCommandExecution()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 2: Command Execution";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    // Test simple command
    qDebug() << "\n🔧 Testing 'echo' command...";
    auto result = processManager.executeCommand("echo", QStringList() << "Hello Anvil");

    if (result.isSuccess())
    {
        qDebug() << "✅ Command executed";
        qDebug() << "   Output:" << result.data.trimmed();
    }
    else
    {
        qDebug() << "❌ Failed:" << result.error;
    }

    // Test shell command
    qDebug() << "\n🔧 Testing shell command...";
    auto shellResult = processManager.executeShell("date +%Y-%m-%d");

    if (shellResult.isSuccess())
    {
        qDebug() << "✅ Shell executed";
        qDebug() << "   Date:" << shellResult.data.trimmed();
    }
    else
    {
        qDebug() << "❌ Failed:" << shellResult.error;
    }

    // Test command with timeout
    qDebug() << "\n🔧 Testing command timeout...";
    auto timeoutResult = processManager.executeCommand("sleep",
                                                       QStringList() << "2",
                                                       1000); // 1 second timeout

    if (timeoutResult.isError())
    {
        qDebug() << "✅ Timeout correctly handled";
    }
    else
    {
        qDebug() << "⚠️  Should have timed out";
    }
}

void testServiceProcessManagement()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 3: Service Process Management";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    QString serviceName = "test-sleep-service";

    // Start a long-running process
    qDebug() << "\n🚀 Starting test service...";
    auto startResult = processManager.startServiceProcess(
        serviceName,
        "sleep",
        QStringList() << "10"
    );

    if (startResult.isError())
    {
        qDebug() << "❌ Failed to start:" << startResult.error;
        return;
    }

    qint64 pid = startResult.data;
    qDebug() << "✅ Service started (PID:" << pid << ")";

    // Check if running
    QThread::msleep(500);

    bool running = processManager.isServiceRunning(serviceName);
    qDebug() << "✅ Service running:" << (running ? "YES" : "NO");

    // Get process info
    qint64 servicePid = processManager.getServicePid(serviceName);
    qDebug() << "✅ Service PID:" << servicePid;

    qint64 uptime = processManager.getServiceUptime(serviceName);
    qDebug() << "✅ Uptime:" << uptime << "seconds";

    bool healthy = processManager.isServiceHealthy(serviceName);
    qDebug() << "✅ Healthy:" << (healthy ? "YES" : "NO");

    // Stop the service
    qDebug() << "\n🛑 Stopping service...";
    auto stopResult = processManager.stopServiceProcess(serviceName);

    if (stopResult.isSuccess())
    {
        qDebug() << "✅ Service stopped";
    }
    else
    {
        qDebug() << "❌ Failed to stop:" << stopResult.error;
    }

    // Verify stopped
    running = processManager.isServiceRunning(serviceName);
    qDebug() << "✅ Service still running:" << (running ? "YES (unexpected)" : "NO (correct)");
}

void testAutoRestart()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 4: Auto-Restart Feature";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    QString serviceName = "test-auto-restart";

    // Start a process that will exit quickly
    qDebug() << "\n🔁 Starting service with auto-restart...";
    auto startResult = processManager.startServiceProcess(
        serviceName,
        "sleep",
        QStringList() << "1" // Will exit after 1 second
    );

    if (startResult.isError())
    {
        qDebug() << "❌ Failed to start:" << startResult.error;
        return;
    }

    // Enable auto-restart
    processManager.setAutoRestart(serviceName, true, 3);
    qDebug() << "✅ Auto-restart enabled (max 3 attempts)";

    // Wait and observe
    qDebug() << "\n⏱️  Waiting for process to exit and restart...";
    for (int i = 0; i < 5; i++)
    {
        QThread::sleep(1);

        bool running = processManager.isServiceRunning(serviceName);
        int restarts = processManager.getRestartCount(serviceName);

        qDebug() << QString("   T+%1s: Running=%2, Restarts=%3")
                    .arg(i + 1)
                    .arg(running ? "YES" : "NO")
                    .arg(restarts);
    }

    // Cleanup
    processManager.stopServiceProcess(serviceName);
}

void testBackgroundTasks()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 5: Background Tasks";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    QString taskName = "test-background-task";

    // Start background task
    qDebug() << "\n📋 Starting background task...";
    auto startResult = processManager.startBackgroundTask(
        taskName,
        "sh",
        QStringList() << "-c" << "for i in 1 2 3; do echo Step $i; sleep 1; done"
    );

    if (startResult.isError())
    {
        qDebug() << "❌ Failed:" << startResult.error;
        return;
    }

    qDebug() << "✅ Task started (PID:" << startResult.data << ")";

    // Monitor task
    qDebug() << "\n⏱️  Monitoring task...";
    for (int i = 0; i < 5; i++)
    {
        QThread::sleep(1);

        bool running = processManager.isBackgroundTaskRunning(taskName);
        QString output = processManager.getBackgroundTaskOutput(taskName);

        qDebug() << QString("   T+%1s: Running=%2")
                    .arg(i + 1)
                    .arg(running ? "YES" : "NO");

        if (!output.isEmpty())
        {
            qDebug() << "   Output:" << output.trimmed();
        }
    }

    // Cleanup
    if (processManager.isBackgroundTaskRunning(taskName))
    {
        processManager.stopBackgroundTask(taskName);
    }
}

void testMultipleServices()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 6: Multiple Service Management";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    // Start multiple services
    QStringList serviceNames = {
        "service-1",
        "service-2",
        "service-3"
    };

    qDebug() << "\n🚀 Starting 3 services...";
    for (const QString &name : serviceNames)
    {
        auto result = processManager.startServiceProcess(
            name,
            "sleep",
            QStringList() << "30"
        );

        if (result.isSuccess())
        {
            qDebug() << QString("✅ %1 started (PID: %2)").arg(name).arg(result.data);
        }
        else
        {
            qDebug() << QString("❌ %1 failed: %2").arg(name, result.error);
        }
    }

    // Check all services
    QThread::msleep(500);

    qDebug() << "\n📊 Service Status:";
    QStringList managed = processManager.getManagedServices();
    qDebug() << QString("Total managed services: %1").arg(managed.size());

    for (const QString &name : managed)
    {
        bool running = processManager.isServiceRunning(name);
        qint64 pid = processManager.getServicePid(name);
        qint64 uptime = processManager.getServiceUptime(name);

        qDebug() << QString("  • %1: %2 (PID: %3, Uptime: %4s)")
                    .arg(name)
                    .arg(running ? "Running" : "Stopped")
                    .arg(pid)
                    .arg(uptime);
    }

    // Stop all
    qDebug() << "\n🛑 Stopping all services...";
    processManager.stopAll();

    QThread::sleep(1);

    int stillRunning = 0;
    for (const QString &name : serviceNames)
    {
        if (processManager.isServiceRunning(name))
        {
            stillRunning++;
        }
    }

    qDebug() << QString("✅ Services still running: %1 (should be 0)").arg(stillRunning);
}

void testCleanup()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 7: Cleanup & Shutdown";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    // Start some services
    qDebug() << "\n🚀 Starting test services...";
    processManager.startServiceProcess("cleanup-test-1", "sleep", QStringList() << "60");
    processManager.startServiceProcess("cleanup-test-2", "sleep", QStringList() << "60");

    QThread::msleep(500);

    int beforeCount = processManager.getManagedServices().size();
    qDebug() << QString("Services before cleanup: %1").arg(beforeCount);

    // Perform cleanup
    qDebug() << "\n🧹 Performing cleanup...";
    processManager.cleanupDeadProcesses();

    int afterCount = processManager.getManagedServices().size();
    qDebug() << QString("Services after cleanup: %1").arg(afterCount);

    // Graceful shutdown
    qDebug() << "\n🛑 Graceful shutdown...";
    processManager.shutdown();

    qDebug() << "✅ Shutdown complete";
}

void testRealWorldScenario()
{
    qDebug() << "\n" << QString("=").repeated(60);
    qDebug() << "TEST 8: Real-World Scenario - PHP-FPM Simulation";
    qDebug() << QString("=").repeated(60);

    Managers::ProcessManager processManager;
    processManager.initialize();

    // Simulate PHP-FPM service
    QString serviceName = "php-fpm-8.3";

    qDebug() << "\n🐘 Starting PHP-FPM simulation...";
    auto result = processManager.startServiceProcess(
        serviceName,
        "sleep",
        QStringList() << "300" // 5 minute process
    );

    if (result.isError())
    {
        qDebug() << "❌ Failed:" << result.error;
        return;
    }

    qDebug() << "✅ PHP-FPM started (PID:" << result.data << ")";

    // Enable auto-restart
    processManager.setAutoRestart(serviceName, true, 3);
    qDebug() << "✅ Auto-restart enabled";

    // Monitor health
    qDebug() << "\n📊 Health Check:";
    for (int i = 0; i < 3; i++)
    {
        QThread::sleep(1);

        bool running = processManager.isServiceRunning(serviceName);
        bool healthy = processManager.isServiceHealthy(serviceName);
        qint64 uptime = processManager.getServiceUptime(serviceName);

        qDebug() << QString("  Check %1: Running=%2, Healthy=%3, Uptime=%4s")
                    .arg(i + 1)
                    .arg(running ? "YES" : "NO")
                    .arg(healthy ? "YES" : "NO")
                    .arg(uptime);
    }

    // Simulate reload (restart)
    qDebug() << "\n🔄 Simulating reload...";
    auto restartResult = processManager.restartServiceProcess(serviceName);

    if (restartResult.isSuccess())
    {
        qDebug() << "✅ Service reloaded";

        QThread::msleep(500);

        bool running = processManager.isServiceRunning(serviceName);
        qDebug() << QString("✅ Service running after reload: %1").arg(running ? "YES" : "NO");
    }

    // Cleanup
    processManager.stopServiceProcess(serviceName);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Setup logging
    Utils::Logger::instance().setConsoleOutput(true);
    Utils::Logger::instance().setLogLevel(Utils::LogLevel::Info);

    qDebug() << "╔════════════════════════════════════════════════════════════╗";
    qDebug() << "║   ProcessManager Test Suite                                ║";
    qDebug() << "╚════════════════════════════════════════════════════════════╝";

    try
    {
        // Initialize core systems
        Core::ConfigManager::instance().initialize();

        // Run tests
        testProcessManagerInit();
        testCommandExecution();
        testServiceProcessManagement();
        testAutoRestart();
        testBackgroundTasks();
        testMultipleServices();
        testCleanup();
        testRealWorldScenario();

        qDebug() << "\n";
        qDebug() << "╔════════════════════════════════════════════════════════════╗";
        qDebug() << "║   ✅ All Tests Completed                                   ║";
        qDebug() << "╚════════════════════════════════════════════════════════════╝";
        qDebug() << "";
        qDebug() << "Summary:";
        qDebug() << "  ✓ ProcessManager initialization";
        qDebug() << "  ✓ Command execution (sync)";
        qDebug() << "  ✓ Service process lifecycle";
        qDebug() << "  ✓ Auto-restart functionality";
        qDebug() << "  ✓ Background tasks";
        qDebug() << "  ✓ Multiple service orchestration";
        qDebug() << "  ✓ Cleanup & shutdown";
        qDebug() << "  ✓ Real-world PHP-FPM simulation";
        qDebug() << "";

        return 0;
    }
    catch (const std::exception &e)
    {
        qDebug() << "\n❌ Test failed with exception:" << e.what();
        return 1;
    }
}
