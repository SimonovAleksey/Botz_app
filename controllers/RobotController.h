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
    void batteryUpdated(double voltage, int percent);

private slots:
    void onMessageReceived(const QString &message);

private:
    WebSocketClient client;
};
