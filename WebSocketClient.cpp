#include "WebSocketClient.h"
#include <QDebug>

WebSocketClient::WebSocketClient(QObject *parent)
    : QObject(parent)
{
    connect(&socket, &QWebSocket::connected,
            this, &WebSocketClient::onConnected);

    connect(&socket, &QWebSocket::disconnected,
            this, &WebSocketClient::onDisconnected);

    connect(&socket, &QWebSocket::textMessageReceived,
            this, &WebSocketClient::onTextMessageReceived);
}

void WebSocketClient::connectTo(const QString &url)
{
    socket.open(QUrl(url));
}

void WebSocketClient::sendMessage(const QString &message)
{
    socket.sendTextMessage(message);
}

void WebSocketClient::onConnected()
{
    qDebug() << "WebSocket connected";
    emit connected();
}

void WebSocketClient::onDisconnected()
{
    qDebug() << "WebSocket disconnected";
    emit disconnected();
}

void WebSocketClient::onTextMessageReceived(const QString &message)
{
    qDebug() << "Received:" << message;
    emit messageReceived(message);
}