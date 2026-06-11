#pragma once

#include <QObject>
#include "network/WebSocketClient.h"

class RobotController : public QObject
{
    Q_OBJECT

public:
    explicit RobotController(QObject *parent = nullptr);

    void startServer(quint16 port = 8080);
    void stopServer();
    QString serverAddress() const;

    void sendTankState(int left, int right, bool light, bool extra);
    void sendSteerState(int throttle, int angle, bool light, bool extra);

signals:
    void connected();
    void disconnected();
    void connectionFailed();

private:
    WebSocketClient client;
};
