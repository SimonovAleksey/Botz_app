#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>
#include <QLineEdit>
#include <QEvent>
#include "controllers/RobotController.h"

class BatteryGaugeWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QStackedWidget *stack;
    QWidget *menuScreen;
    QWidget *tankScreen;
    QWidget *steeringScreen;
    QWidget *programScreen;

    QLineEdit *programSsidInput;
    QLineEdit *programPassInput;
    QLineEdit *programIpInput;
    QLineEdit *programPortInput;

    QSlider *leftTrack;
    QSlider *rightTrack;
    QLabel  *leftValLabel;
    QLabel  *rightValLabel;

    QSlider *throttle;
    QSlider *steering;
    QLabel  *throttleValLabel;
    QLabel  *steerValLabel;

    QPushButton *tankLightBtn;
    QPushButton *tankExtraBtn;
    QPushButton *steerLightBtn;
    QPushButton *steerExtraBtn;

    QLabel *tankStatusLabel;
    QLabel *steerStatusLabel;
    QLabel *menuStatusLabel;

    BatteryGaugeWidget *tankBatteryGauge  = nullptr;
    BatteryGaugeWidget *steerBatteryGauge = nullptr;

    QPushButton *connectBtn;

    QTimer *updateTimer;
    RobotController robot;

    // Which slider is currently mouse-held (timer must not override it)
    QSlider *heldSlider = nullptr;

    bool lightOn     = false;
    bool extraOn     = false;
    bool isConnected = false;
    bool isListening = false;
    int  sendTick    = 0;

    bool keyW = false, keyS = false;
    bool keyUp = false, keyDown = false;
    bool keyA = false, keyD = false;

    void setupMenu();
    void setupTankUI();
    void setupSteeringUI();
    void setupProgramUI();
    void generateAndCopySketch();
    void showToast(const QString &text);

    void setLightState(bool on);
    void setExtraState(bool on);
    void setConnected(bool on);
    void setListening(bool on);
    void setBatteryLevel(double voltage, int percent);
    void flashConnectError();
    void resetConnectError();
    void flashWindow(const QColor &color, int fadeOutMs = 600);
    void switchTo(QWidget *target);

    // Wire press/release signals so the timer respects mouse holds
    void trackSliderHold(QSlider *s);

    static QLabel*      makeValueLabel();
    static QSlider*     makeVertSlider();
    static QSlider*     makeHorizSlider();
    static QPushButton* makeToggleBtn(const QString &text, const QString &key,
                                       const QString &activeColor = "#34C759");
    static QWidget*     makeHeader(const QString &title,
                                    QPushButton *&backBtn,
                                    QLabel      *&statusOut);
};
