#pragma once
#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHostAddress>
#include <QNetworkInterface>

class WebSocketClient : public QObject {
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);
    ~WebSocketClient();

    void startServer(quint16 port = 8080);
    void stopServer();
    QString listenAddress() const;
    void sendMessage(const QString &message);

    // Exposed so other UI (e.g. the board-programming dialog) can show the
    // Mac's LAN IP before the server is even started.
    static QString detectLocalIp();

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);
    void connectionFailed();

private slots:
    void onNewConnection();
    void onClientDisconnected();

private:
    QWebSocketServer wsServer;
    QWebSocket      *clientSocket = nullptr;
    static QHostAddress findWifiAddressHost();
};
