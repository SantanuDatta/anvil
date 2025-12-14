#ifndef NETWORK_H
#define NETWORK_H

#include <QString>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <functional>

namespace Anvil {
    namespace Utils {
        struct DownloadResult {
            bool success;
            QByteArray data;
            QString error;
            int httpStatusCode;

            static DownloadResult Ok(const QByteArray& data, int statusCode = 200) {
                return {true, data, QString(), statusCode};
            }

            static DownloadResult Err(const QString& error, int statusCode = 0) {
                return {false, QByteArray(), error, statusCode};
            }
        };

        class Network : public QObject {
            Q_OBJECT

        public:
            explicit Network(QObject* parent = nullptr);
            ~Network();

            // Port management
            static bool isPortAvailable(quint16 port, const QString& host = "127.0.0.1");
            static quint16 findAvailablePort(quint16 startPort = 8000, quint16 endPort = 9000);
            static QList<quint16> getUsedPorts();
            static bool isPortInUse(quint16 port);

            // URL operations
            static bool isValidUrl(const QString& url);
            static bool isValidDomain(const QString& domain);
            static QString extractDomain(const QString& url);
            static QString extractProtocol(const QString& url);
            static bool isLocalhost(const QString& host);

            // Network interface operations
            static QString getLocalIPv4();
            static QString getLocalIPv6();
            static QStringList getAllIPAddresses();
            static QString getHostname();
            static bool isNetworkAvailable();

            // Download operations (async)
            void downloadFile(const QUrl& url, const QString& destinationPath);
            void downloadToMemory(const QUrl& url);
            void cancelDownload();

        signals:
            void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
            void downloadFinished(bool success, const QString& path);
            void downloadError(const QString& error);
            void dataReady(const QByteArray& data);

        private slots:
            void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
            void onDownloadFinished();
            void onDownloadError(QNetworkReply::NetworkError error);

        private:
            QNetworkAccessManager* m_networkManager;
            QNetworkReply* m_currentReply;
            QString m_destinationPath;
        };

        // Synchronous network operations
        class NetworkSync {
        public:
            // Synchronous download
            static DownloadResult download(const QUrl& url, int timeoutMs = 30000);
            static DownloadResult downloadFile(const QUrl& url,
                                               const QString& destinationPath,
                                               int timeoutMs = 60000);

            // HTTP operations
            static DownloadResult get(const QUrl& url,
                                      const QMap<QString, QString>& headers = QMap<QString, QString>(),
                                      int timeoutMs = 30000);
            static DownloadResult post(const QUrl& url,
                                       const QByteArray& data,
                                       const QMap<QString, QString>& headers = QMap<QString, QString>(),
                                       int timeoutMs = 30000);

            // Connection testing
            static bool canConnect(const QString& host, quint16 port, int timeoutMs = 5000);
            static bool pingHost(const QString& host, int timeoutMs = 5000);
            static int getResponseTime(const QString& host, quint16 port);

        private:
            static DownloadResult executeRequest(QNetworkAccessManager& manager,
                                                 QNetworkRequest& request,
                                                 const QByteArray& data,
                                                 const QString& method,
                                                 int timeoutMs);
        };

        // Simple TCP server for checking connectivity
        class PortChecker : public QObject {
            Q_OBJECT

        public:
            explicit PortChecker(QObject* parent = nullptr);
            ~PortChecker();

            bool listen(quint16 port, const QString& address = "127.0.0.1");
            void stop();
            bool isListening() const;
            quint16 port() const;

        signals:
            void connectionReceived();

        private slots:
            void onNewConnection();

        private:
            QTcpServer* m_server;
        };

        // DNS/Hosts file management
        class DnsManager {
        public:
            // Hosts file operations
            static QString getHostsFilePath();
            static bool addHostEntry(const QString& domain, const QString& ip = "127.0.0.1");
            static bool removeHostEntry(const QString& domain);
            static bool hasHostEntry(const QString& domain);
            static QMap<QString, QString> getAllHostEntries();
            static bool backupHostsFile();
            static bool restoreHostsFile();

            // DNS resolution
            static QString resolveDomain(const QString& domain);
            static QStringList resolveAllAddresses(const QString& domain);
            static bool isDomainResolvable(const QString& domain);

        private:
            static QString m_hostsFilePath;
            static QString m_hostsBackupPath;

            static bool parseHostsFile(QMap<QString, QString>& entries);
            static bool writeHostsFile(const QMap<QString, QString>& entries);
            static QString sanitizeDomain(const QString& domain);
        };

        // URL builder helper
        class UrlBuilder {
        public:
            explicit UrlBuilder(const QString& baseUrl = QString());

            UrlBuilder& setScheme(const QString& scheme);
            UrlBuilder& setHost(const QString& host);
            UrlBuilder& setPort(quint16 port);
            UrlBuilder& setPath(const QString& path);
            UrlBuilder& addQueryParam(const QString& key, const QString& value);
            UrlBuilder& setFragment(const QString& fragment);

            QUrl build() const;
            QString toString() const;

        private:
            QUrl m_url;
        };

        // Network speed tester
        class NetworkSpeedTest : public QObject {
            Q_OBJECT

        public:
            explicit NetworkSpeedTest(QObject* parent = nullptr);

            void start(const QUrl& testUrl);
            void stop();

        signals:
            void speedCalculated(double mbps);
            void testFinished();
            void testError(const QString& error);

        private slots:
            void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
            void onFinished();

        private:
            QNetworkAccessManager* m_manager;
            QNetworkReply* m_reply;
            qint64 m_startTime;
            qint64 m_bytesReceived;
        };
    }
}
#endif
