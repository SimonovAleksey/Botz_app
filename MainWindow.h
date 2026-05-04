#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QTimer>
#include <QKeyEvent>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QStackedWidget *stack;

    QWidget *menuScreen;
    QWidget *tankScreen;
    QWidget *steeringScreen;

    // Tank
    QSlider *leftTrack;
    QSlider *rightTrack;

    // Steering
    QSlider *throttle;
    QSlider *steering;

    // Buttons
    QPushButton *lightBtn;
    QPushButton *extraBtn;

    QLabel *lightIndicator;
    QLabel *extraIndicator;

    QLabel *connectionStatus;

    QTimer *updateTimer;

    // keyboard state
    bool keyW = false, keyS = false;
    bool keyUp = false, keyDown = false;
    bool keyA = false, keyD = false;

    void setupMenu();
    void setupTankUI();
    void setupSteeringUI();

    void updateIndicator(QLabel *indicator, bool state);
};