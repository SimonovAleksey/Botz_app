#include "RobotController.h"
#include <QJsonObject>
#include <QJsonDocument>

RobotController::RobotController(QObject *parent) : QObject(parent)
{
    connect(&client, &WebSocketClient::connected,       this, &RobotController::connected);
    connect(&client, &WebSocketClient::disconnected,    this, &RobotController::disconnected);
    connect(&client, &WebSocketClient::connectionFailed,this, &RobotController::connectionFailed);
}

void RobotController::startServer(quint16 port) { client.startServer(port); }
void RobotController::stopServer()              { client.stopServer(); }
QString RobotController::serverAddress() const  { return client.listenAddress(); }

void RobotController::sendTankState(int left, int right, bool light, bool extra)
{
    QJsonObject obj;
    obj["type"]  = "tank";
    obj["left"]  = left;
    obj["right"] = right;
    obj["light"] = light;
    obj["extra"] = extra;
    client.sendMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void RobotController::sendSteerState(int throttleVal, int angle, bool light, bool extra)
{
    QJsonObject obj;
    obj["type"]     = "steer";
    obj["throttle"] = throttleVal;
    obj["angle"]    = angle;
    obj["light"]    = light;
    obj["extra"]    = extra;
    client.sendMessage(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}
