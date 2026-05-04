#include "RobotController.h"
#include <QJsonObject>
#include <QJsonDocument>

RobotController::RobotController(QObject *parent)
    : QObject(parent)
{
    connect(&client, &WebSocketClient::connected,
            this, &RobotController::connected);

    connect(&client, &WebSocketClient::disconnected,
            this, &RobotController::disconnected);
}

void RobotController::connectToRobot(const QString &ip)
{
    qDebug() << "Connecting to robot at:" << ip;
}

void RobotController::move(const QString &direction, int speed)
{
    QString msg = createMoveCommand(direction, speed);
    client.sendMessage(msg);
}

void RobotController::stop()
{
    QJsonObject obj;
    obj["action"] = "stop";

    QString msg = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    client.sendMessage(msg);
}

QString RobotController::createMoveCommand(const QString &direction, int speed)
{
    QJsonObject obj;
    obj["action"] = "move";
    obj["direction"] = direction;
    obj["speed"] = speed;

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}