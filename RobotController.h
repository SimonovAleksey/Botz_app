#pragma once

#include <QObject>
#include "network/WebSocketClient.h"

class RobotController : public QObject
{
    Q_OBJECT

public:
    void connectToRobot(const QString &ip);
    explicit RobotController(QObject *parent = nullptr);

    void move(const QString &direction, int speed);
    void stop();

signals:
    void connected();
    void disconnected();

private:
    WebSocketClient client;

    QString createMoveCommand(const QString &direction, int speed);
};