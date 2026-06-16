#include "WebSocketClient.h"
#include <QDebug>

WebSocketClient::WebSocketClient(QObject *parent)
    : QObject(parent)
    , wsServer("RobotController", QWebSocketServer::NonSecureMode, this)
{
    connect(&wsServer, &QWebSocketServer::newConnection,
            this, &WebSocketClient::onNewConnection);
}

WebSocketClient::~WebSocketClient()
{
    stopServer();
}

void WebSocketClient::startServer(quint16 port)
{
    if (clientSocket) {
        clientSocket->close();
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
    wsServer.close();

    if (!wsServer.listen(QHostAddress::AnyIPv4, port)) {
        qDebug() << "[WS] listen failed:" << wsServer.errorString();
        emit connectionFailed();
        return;
    }
    qDebug() << "[WS] listening on" << listenAddress();
}

void WebSocketClient::stopServer()
{
    if (clientSocket) {
        clientSocket->close();
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
    wsServer.close();
}

QString WebSocketClient::listenAddress() const
{
    if (!wsServer.isListening()) return {};
    QHostAddress addr = findWifiAddressHost();
    QString ip = addr.isNull() ? QString("0.0.0.0") : addr.toString();
    return ip + ":" + QString::number(wsServer.serverPort());
}

void WebSocketClient::sendMessage(const QString &message)
{
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState)
        clientSocket->sendTextMessage(message);
}

void WebSocketClient::onNewConnection()
{
    QWebSocket *pending = wsServer.nextPendingConnection();
    if (!pending) return;

    // Accept only one robot at a time
    if (clientSocket && clientSocket->state() == QAbstractSocket::ConnectedState) {
        pending->close();
        pending->deleteLater();
        return;
    }
    if (clientSocket) clientSocket->deleteLater();
    clientSocket = pending;

    connect(clientSocket, &QWebSocket::disconnected,
            this, &WebSocketClient::onClientDisconnected);
    connect(clientSocket, &QWebSocket::textMessageReceived,
            this, &WebSocketClient::messageReceived);

    qDebug() << "[WS] robot connected from" << clientSocket->peerAddress().toString();
    emit connected();
}

void WebSocketClient::onClientDisconnected()
{
    qDebug() << "[WS] robot disconnected";
    if (clientSocket) {
        clientSocket->deleteLater();
        clientSocket = nullptr;
    }
    emit disconnected();
}

QString WebSocketClient::detectLocalIp()
{
    QHostAddress addr = findWifiAddressHost();
    return addr.isNull() ? QString() : addr.toString();
}

QHostAddress WebSocketClient::findWifiAddressHost()
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp))      continue;
        if (  iface.flags() & QNetworkInterface::IsLoopBack) continue;
        const QString name = iface.name();
        if (name.startsWith("utun") || name.startsWith("tun") || name.startsWith("ppp")) continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            quint32 ip    = entry.ip().toIPv4Address();
            quint32 first = ip >> 24;
            quint32 second = (ip >> 16) & 0xFF;
            if (first == 10 || first == 192 || (first == 172 && second >= 16 && second <= 31))
                return entry.ip();
        }
    }
    return {};
}
