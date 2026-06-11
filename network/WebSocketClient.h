#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QAbstractSocket>

class WebSocketClient : public QObject {
    Q_OBJECT
public:
    explicit WebSocketClient(QObject *parent = nullptr);

    void startServer(quint16 port = 8080);
    void stopServer();
    QString listenAddress() const;
    void sendMessage(const QString &message);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);
    void connectionFailed();

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onReadyRead();

private:
    QTcpServer  tcpServer;
    QTcpSocket *clientSocket = nullptr;
    QString     readBuffer;
    static QHostAddress findWifiAddress();
};
