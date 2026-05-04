#pragma once

#include <QObject>
#include <QWebSocket>

class WebSocketClient : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketClient(QObject *parent = nullptr);

    void connectTo(const QString &url);
    void sendMessage(const QString &message);

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString &message);

private:
    QWebSocket socket;

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
};