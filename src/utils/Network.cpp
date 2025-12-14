#include "utils/Network.h"
#include "utils/Logger.h"
#include "utils/FileSystem.h"
#include <QNetworkInterface>
#include <QHostInfo>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QDateTime>

namespace Anvil
{
    namespace Utils
    {

        // ============================================================================
        // Network Implementation
        // ============================================================================

        Network::Network(QObject *parent)
            : QObject(parent), m_networkManager(new QNetworkAccessManager(this)), m_currentReply(nullptr)
        {
        }

        Network::~Network()
        {
            cancelDownload();
        }

        bool Network::isPortAvailable(quint16 port, const QString &host)
        {
            QTcpServer server;
            bool result = server.listen(QHostAddress(host), port);
            if (result)
            {
                server.close();
            }
            return result;
        }

        quint16 Network::findAvailablePort(quint16 startPort, quint16 endPort)
        {
            for (quint16 port = startPort; port <= endPort; ++port)
            {
                if (isPortAvailable(port))
                {
                    return port;
                }
            }
            return 0; // No available port found
        }

        QList<quint16> Network::getUsedPorts()
        {
            QList<quint16> usedPorts;

            // This is a simplified version
            // In a real implementation, you'd parse /proc/net/tcp and /proc/net/tcp6
            for (quint16 port = 1; port != 0; ++port)
            {
                if (!isPortAvailable(port))
                {
                    usedPorts << port;
                }

                if (port > 10000)
                    break;
            }

            return usedPorts;
        }

        bool Network::isPortInUse(quint16 port)
        {
            return !isPortAvailable(port);
        }

        bool Network::isValidUrl(const QString &url)
        {
            QUrl qurl(url);
            return qurl.isValid() && !qurl.scheme().isEmpty() && !qurl.host().isEmpty();
        }

        bool Network::isValidDomain(const QString &domain)
        {
            // Simple regex for domain validation
            QRegularExpression regex("^([a-zA-Z0-9]([a-zA-Z0-9\\-]{0,61}[a-zA-Z0-9])?\\.)+[a-zA-Z]{2,}$");
            return regex.match(domain).hasMatch();
        }

        QString Network::extractDomain(const QString &url)
        {
            QUrl qurl(url);
            return qurl.host();
        }

        QString Network::extractProtocol(const QString &url)
        {
            QUrl qurl(url);
            return qurl.scheme();
        }

        bool Network::isLocalhost(const QString &host)
        {
            return host == "localhost" ||
                   host == "127.0.0.1" ||
                   host == "::1" ||
                   host == "0.0.0.0";
        }

        QString Network::getLocalIPv4()
        {
            QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

            for (const QHostAddress &address : addresses)
            {
                if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                    !address.isLoopback())
                {
                    return address.toString();
                }
            }

            return "127.0.0.1";
        }

        QString Network::getLocalIPv6()
        {
            QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

            for (const QHostAddress &address : addresses)
            {
                if (address.protocol() == QAbstractSocket::IPv6Protocol &&
                    !address.isLoopback())
                {
                    return address.toString();
                }
            }

            return "::1";
        }

        QStringList Network::getAllIPAddresses()
        {
            QStringList addresses;
            QList<QHostAddress> hostAddresses = QNetworkInterface::allAddresses();

            for (const QHostAddress &address : hostAddresses)
            {
                if (!address.isLoopback())
                {
                    addresses << address.toString();
                }
            }

            return addresses;
        }

        QString Network::getHostname()
        {
            return QHostInfo::localHostName();
        }

        bool Network::isNetworkAvailable()
        {
            QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

            for (const QNetworkInterface &iface : interfaces)
            {
                if (iface.flags().testFlag(QNetworkInterface::IsUp) &&
                    iface.flags().testFlag(QNetworkInterface::IsRunning) &&
                    !iface.flags().testFlag(QNetworkInterface::IsLoopBack))
                {
                    return true;
                }
            }

            return false;
        }

        void Network::downloadFile(const QUrl &url, const QString &destinationPath)
        {
            if (m_currentReply)
            {
                cancelDownload();
            }

            m_destinationPath = destinationPath;

            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

            m_currentReply = m_networkManager->get(request);

            connect(m_currentReply, &QNetworkReply::downloadProgress,
                    this, &Network::onDownloadProgress);
            connect(m_currentReply, &QNetworkReply::finished,
                    this, &Network::onDownloadFinished);
            connect(m_currentReply, &QNetworkReply::errorOccurred,
                    this, &Network::onDownloadError);

            LOG_INFO(QString("Downloading: %1 -> %2").arg(url.toString(), destinationPath));
        }

        void Network::downloadToMemory(const QUrl &url)
        {
            if (m_currentReply)
            {
                cancelDownload();
            }

            m_destinationPath.clear();

            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

            m_currentReply = m_networkManager->get(request);

            connect(m_currentReply, &QNetworkReply::downloadProgress,
                    this, &Network::onDownloadProgress);
            connect(m_currentReply, &QNetworkReply::finished,
                    this, &Network::onDownloadFinished);
            connect(m_currentReply, &QNetworkReply::errorOccurred,
                    this, &Network::onDownloadError);
        }

        void Network::cancelDownload()
        {
            if (m_currentReply)
            {
                m_currentReply->abort();
                m_currentReply->deleteLater();
                m_currentReply = nullptr;
            }
        }

        void Network::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
        {
            emit downloadProgress(bytesReceived, bytesTotal);
        }

        void Network::onDownloadFinished()
        {
            if (!m_currentReply)
                return;

            if (m_currentReply->error() != QNetworkReply::NoError)
            {
                return; // Error handled in onDownloadError
            }

            QByteArray data = m_currentReply->readAll();

            if (m_destinationPath.isEmpty())
            {
                // Download to memory
                emit dataReady(data);
                emit downloadFinished(true, QString());
            }
            else
            {
                // Save to file
                QFile file(m_destinationPath);
                if (file.open(QIODevice::WriteOnly))
                {
                    file.write(data);
                    file.close();
                    LOG_INFO(QString("Download completed: %1").arg(m_destinationPath));
                    emit downloadFinished(true, m_destinationPath);
                }
                else
                {
                    QString error = QString("Cannot write to file: %1").arg(file.errorString());
                    LOG_ERROR(error);
                    emit downloadError(error);
                    emit downloadFinished(false, m_destinationPath);
                }
            }

            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        void Network::onDownloadError(QNetworkReply::NetworkError error)
        {
            Q_UNUSED(error);

            if (!m_currentReply)
                return;

            QString errorString = m_currentReply->errorString();
            LOG_ERROR(QString("Download error: %1").arg(errorString));

            emit downloadError(errorString);
            emit downloadFinished(false, m_destinationPath);

            m_currentReply->deleteLater();
            m_currentReply = nullptr;
        }

        // ============================================================================
        // NetworkSync Implementation
        // ============================================================================

        DownloadResult NetworkSync::download(const QUrl &url, int timeoutMs)
        {
            QNetworkAccessManager manager;
            QNetworkRequest request(url);
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                                 QNetworkRequest::NoLessSafeRedirectPolicy);

            return executeRequest(manager, request, QByteArray(), "GET", timeoutMs);
        }

        DownloadResult NetworkSync::downloadFile(const QUrl &url,
                                                 const QString &destinationPath,
                                                 int timeoutMs)
        {
            auto result = download(url, timeoutMs);

            if (!result.success)
            {
                return result;
            }

            QFile file(destinationPath);
            if (!file.open(QIODevice::WriteOnly))
            {
                return DownloadResult::Err(QString("Cannot write to file: %1").arg(file.errorString()));
            }

            file.write(result.data);
            file.close();

            return result;
        }

        DownloadResult NetworkSync::get(const QUrl &url,
                                        const QMap<QString, QString> &headers,
                                        int timeoutMs)
        {
            QNetworkAccessManager manager;
            QNetworkRequest request(url);

            for (auto it = headers.begin(); it != headers.end(); ++it)
            {
                request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
            }

            return executeRequest(manager, request, QByteArray(), "GET", timeoutMs);
        }

        DownloadResult NetworkSync::post(const QUrl &url,
                                         const QByteArray &data,
                                         const QMap<QString, QString> &headers,
                                         int timeoutMs)
        {
            QNetworkAccessManager manager;
            QNetworkRequest request(url);

            for (auto it = headers.begin(); it != headers.end(); ++it)
            {
                request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
            }

            return executeRequest(manager, request, data, "POST", timeoutMs);
        }

        bool NetworkSync::canConnect(const QString &host, quint16 port, int timeoutMs)
        {
            QTcpSocket socket;
            socket.connectToHost(host, port);

            return socket.waitForConnected(timeoutMs);
        }

        bool NetworkSync::pingHost(const QString &host, int timeoutMs)
        {
            // Simple TCP connection test on port 80
            return canConnect(host, 80, timeoutMs);
        }

        int NetworkSync::getResponseTime(const QString &host, quint16 port)
        {
            QTcpSocket socket;

            qint64 startTime = QDateTime::currentMSecsSinceEpoch();
            socket.connectToHost(host, port);

            if (!socket.waitForConnected(5000))
            {
                return -1;
            }

            qint64 endTime = QDateTime::currentMSecsSinceEpoch();
            socket.close();

            return static_cast<int>(endTime - startTime);
        }

        DownloadResult NetworkSync::executeRequest(QNetworkAccessManager &manager,
                                                   QNetworkRequest &request,
                                                   const QByteArray &data,
                                                   const QString &method,
                                                   int timeoutMs)
        {
            QNetworkReply *reply = nullptr;

            if (method == "GET")
            {
                reply = manager.get(request);
            }
            else if (method == "POST")
            {
                reply = manager.post(request, data);
            }
            else
            {
                return DownloadResult::Err("Unsupported HTTP method");
            }

            QEventLoop loop;
            QTimer timer;

            timer.setSingleShot(true);
            timer.setInterval(timeoutMs);

            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

            timer.start();
            loop.exec();

            if (!timer.isActive())
            {
                reply->abort();
                reply->deleteLater();
                return DownloadResult::Err("Request timeout");
            }

            timer.stop();

            if (reply->error() != QNetworkReply::NoError)
            {
                QString error = reply->errorString();
                reply->deleteLater();
                return DownloadResult::Err(error);
            }

            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray responseData = reply->readAll();
            reply->deleteLater();

            return DownloadResult::Ok(responseData, statusCode);
        }

        // ============================================================================
        // PortChecker Implementation
        // ============================================================================

        PortChecker::PortChecker(QObject *parent)
            : QObject(parent), m_server(new QTcpServer(this))
        {

            connect(m_server, &QTcpServer::newConnection,
                    this, &PortChecker::onNewConnection);
        }

        PortChecker::~PortChecker()
        {
            stop();
        }

        bool PortChecker::listen(quint16 port, const QString &address)
        {
            return m_server->listen(QHostAddress(address), port);
        }

        void PortChecker::stop()
        {
            if (m_server->isListening())
            {
                m_server->close();
            }
        }

        bool PortChecker::isListening() const
        {
            return m_server->isListening();
        }

        quint16 PortChecker::port() const
        {
            return m_server->serverPort();
        }

        void PortChecker::onNewConnection()
        {
            QTcpSocket *socket = m_server->nextPendingConnection();
            if (socket)
            {
                emit connectionReceived();
                socket->close();
                socket->deleteLater();
            }
        }

        // ============================================================================
        // DnsManager Implementation
        // ============================================================================

        QString DnsManager::m_hostsFilePath = "/etc/hosts";
        QString DnsManager::m_hostsBackupPath = "/etc/hosts.backup";

        QString DnsManager::getHostsFilePath()
        {
            return m_hostsFilePath;
        }

        bool DnsManager::addHostEntry(const QString &domain, const QString &ip)
        {
            QMap<QString, QString> entries;
            if (!parseHostsFile(entries))
            {
                LOG_ERROR("Failed to parse hosts file");
                return false;
            }

            QString cleanDomain = sanitizeDomain(domain);
            entries[cleanDomain] = ip;

            if (!writeHostsFile(entries))
            {
                LOG_ERROR("Failed to write hosts file");
                return false;
            }

            LOG_INFO(QString("Added host entry: %1 -> %2").arg(cleanDomain, ip));
            return true;
        }

        bool DnsManager::removeHostEntry(const QString &domain)
        {
            QMap<QString, QString> entries;
            if (!parseHostsFile(entries))
            {
                return false;
            }

            QString cleanDomain = sanitizeDomain(domain);
            entries.remove(cleanDomain);

            if (!writeHostsFile(entries))
            {
                return false;
            }

            LOG_INFO(QString("Removed host entry: %1").arg(cleanDomain));
            return true;
        }

        bool DnsManager::hasHostEntry(const QString &domain)
        {
            QMap<QString, QString> entries;
            if (!parseHostsFile(entries))
            {
                return false;
            }

            return entries.contains(sanitizeDomain(domain));
        }

        QMap<QString, QString> DnsManager::getAllHostEntries()
        {
            QMap<QString, QString> entries;
            parseHostsFile(entries);
            return entries;
        }

        bool DnsManager::backupHostsFile()
        {
            return FileSystem::copyFile(m_hostsFilePath, m_hostsBackupPath, true);
        }

        bool DnsManager::restoreHostsFile()
        {
            return FileSystem::copyFile(m_hostsBackupPath, m_hostsFilePath, true);
        }

        QString DnsManager::resolveDomain(const QString &domain)
        {
            QHostInfo info = QHostInfo::fromName(domain);

            if (info.error() != QHostInfo::NoError)
            {
                return QString();
            }

            QList<QHostAddress> addresses = info.addresses();
            if (addresses.isEmpty())
            {
                return QString();
            }

            return addresses.first().toString();
        }

        QStringList DnsManager::resolveAllAddresses(const QString &domain)
        {
            QHostInfo info = QHostInfo::fromName(domain);

            if (info.error() != QHostInfo::NoError)
            {
                return QStringList();
            }

            QStringList addresses;
            for (const QHostAddress &addr : info.addresses())
            {
                addresses << addr.toString();
            }

            return addresses;
        }

        bool DnsManager::isDomainResolvable(const QString &domain)
        {
            return !resolveDomain(domain).isEmpty();
        }

        bool DnsManager::parseHostsFile(QMap<QString, QString> &entries)
        {
            auto result = FileSystem::readLines(m_hostsFilePath);
            if (result.isError())
            {
                return false;
            }

            QRegularExpression commentRegex("^\\s*#");
            QRegularExpression entryRegex("^\\s*(\\S+)\\s+(\\S+)");

            for (const QString &line : result.data)
            {
                if (commentRegex.match(line).hasMatch() || line.trimmed().isEmpty())
                {
                    continue;
                }

                QRegularExpressionMatch match = entryRegex.match(line);
                if (match.hasMatch())
                {
                    QString ip = match.captured(1);
                    QString domain = match.captured(2);
                    entries[domain] = ip;
                }
            }

            return true;
        }

        bool DnsManager::writeHostsFile(const QMap<QString, QString> &entries)
        {
            QStringList lines;
            lines << "# Host entries managed by Anvil";
            lines << "";

            for (auto it = entries.begin(); it != entries.end(); ++it)
            {
                lines << QString("%1\t%2").arg(it.value(), it.key());
            }

            auto result = FileSystem::writeFile(m_hostsFilePath, lines.join('\n'));
            return result.isSuccess();
        }

        QString DnsManager::sanitizeDomain(const QString &domain)
        {
            return domain.toLower().trimmed();
        }

        // ============================================================================
        // UrlBuilder Implementation
        // ============================================================================

        UrlBuilder::UrlBuilder(const QString &baseUrl)
        {
            if (!baseUrl.isEmpty())
            {
                m_url = QUrl(baseUrl);
            }
        }

        UrlBuilder &UrlBuilder::setScheme(const QString &scheme)
        {
            m_url.setScheme(scheme);
            return *this;
        }

        UrlBuilder &UrlBuilder::setHost(const QString &host)
        {
            m_url.setHost(host);
            return *this;
        }

        UrlBuilder &UrlBuilder::setPort(quint16 port)
        {
            m_url.setPort(port);
            return *this;
        }

        UrlBuilder &UrlBuilder::setPath(const QString &path)
        {
            m_url.setPath(path);
            return *this;
        }

        UrlBuilder &UrlBuilder::addQueryParam(const QString &key, const QString &value)
        {
            QUrlQuery query(m_url);
            query.addQueryItem(key, value);
            m_url.setQuery(query);
            return *this;
        }

        UrlBuilder &UrlBuilder::setFragment(const QString &fragment)
        {
            m_url.setFragment(fragment);
            return *this;
        }

        QUrl UrlBuilder::build() const
        {
            return m_url;
        }

        QString UrlBuilder::toString() const
        {
            return m_url.toString();
        }

        // ============================================================================
        // NetworkSpeedTest Implementation
        // ============================================================================

        NetworkSpeedTest::NetworkSpeedTest(QObject *parent)
            : QObject(parent), m_manager(new QNetworkAccessManager(this)), m_reply(nullptr), m_startTime(0), m_bytesReceived(0)
        {
        }

        void NetworkSpeedTest::start(const QUrl &testUrl)
        {
            if (m_reply)
            {
                stop();
            }

            QNetworkRequest request(testUrl);
            m_reply = m_manager->get(request);

            m_startTime = QDateTime::currentMSecsSinceEpoch();
            m_bytesReceived = 0;

            connect(m_reply, &QNetworkReply::downloadProgress,
                    this, &NetworkSpeedTest::onDownloadProgress);
            connect(m_reply, &QNetworkReply::finished,
                    this, &NetworkSpeedTest::onFinished);
        }

        void NetworkSpeedTest::stop()
        {
            if (m_reply)
            {
                m_reply->abort();
                m_reply->deleteLater();
                m_reply = nullptr;
            }
        }

        void NetworkSpeedTest::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
        {
            m_bytesReceived = bytesReceived;

            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
            qint64 elapsed = currentTime - m_startTime;

            if (elapsed > 0)
            {
                double seconds = elapsed / 1000.0;
                double mbps = (bytesReceived * 8.0) / (seconds * 1000000.0);
                emit speedCalculated(mbps);
            }
        }

        void NetworkSpeedTest::onFinished()
        {
            if (m_reply->error() != QNetworkReply::NoError)
            {
                emit testError(m_reply->errorString());
            }

            emit testFinished();

            m_reply->deleteLater();
            m_reply = nullptr;
        }
    }
}
