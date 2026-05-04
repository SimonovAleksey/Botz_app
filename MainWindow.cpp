#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

// ================= INIT =================

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(700, 500);

    stack = new QStackedWidget(this);
    setCentralWidget(stack);

    setStyleSheet("background-color: #2b2b2b; color: white;");

    setupMenu();
    setupTankUI();
    setupSteeringUI();

    stack->setCurrentWidget(menuScreen);

    // 🔄 Таймер управления
    updateTimer = new QTimer(this);

    connect(updateTimer, &QTimer::timeout, [=]() {

        auto updateAxis = [](QSlider *s, bool plus, bool minus) {

            int v = s->value();

            if (plus) v += 4;
            if (minus) v -= 4;

            // возврат к 0 (быстрее и без дрожания)
            if (!plus && !minus) {
                if (v > 0) v -= 6;
                else if (v < 0) v += 6;
            }

            if (abs(v) < 3) v = 0;

            s->setValue(qBound(-100, v, 100));
        };

        if (stack->currentWidget() == tankScreen) {
            updateAxis(leftTrack, keyW, keyS);
            updateAxis(rightTrack, keyUp, keyDown);
        }

        if (stack->currentWidget() == steeringScreen) {
            updateAxis(throttle, keyW, keyS);
            updateAxis(steering, keyD, keyA);
        }
    });

    updateTimer->start(20);
}

// ================= MENU =================

void MainWindow::setupMenu()
{
    menuScreen = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(menuScreen);

    QPushButton *tankBtn = new QPushButton("Танковое управление");
    QPushButton *steerBtn = new QPushButton("Рулевая рейка");

    tankBtn->setMinimumHeight(50);
    steerBtn->setMinimumHeight(50);

    layout->addStretch();
    layout->addWidget(tankBtn);
    layout->addWidget(steerBtn);
    layout->addStretch();

    connect(tankBtn, &QPushButton::clicked, [=]() {
        stack->setCurrentWidget(tankScreen);
    });

    connect(steerBtn, &QPushButton::clicked, [=]() {
        stack->setCurrentWidget(steeringScreen);
    });

    stack->addWidget(menuScreen);
}

// ================= COMMON BUTTON STYLE =================

QPushButton* makeRoundButton(QString text)
{
    QPushButton *btn = new QPushButton(text);
    btn->setCheckable(true);
    btn->setFixedSize(60, 60);

    btn->setStyleSheet(
        "QPushButton {"
        "border-radius: 30px;"
        "background-color: #444;"
        "}"
        "QPushButton:checked {"
        "background-color: #777;"
        "}"
    );

    return btn;
}

// ================= TANK =================

void MainWindow::setupTankUI()
{
    tankScreen = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(tankScreen);

    // Назад
    QVBoxLayout *center = new QVBoxLayout();
    QPushButton *backBtn = new QPushButton("←");

    connect(backBtn, &QPushButton::clicked, [=]() {
        stack->setCurrentWidget(menuScreen);
    });

    // Слайдеры
    leftTrack = new QSlider(Qt::Vertical);
    rightTrack = new QSlider(Qt::Vertical);

    leftTrack->setRange(-100, 100);
    rightTrack->setRange(-100, 100);

    // Кнопки
    lightBtn = makeRoundButton("L");
    extraBtn = makeRoundButton("E");

    lightIndicator = new QLabel();
    extraIndicator = new QLabel();

    lightIndicator->setFixedSize(14,14);
    extraIndicator->setFixedSize(14,14);

    updateIndicator(lightIndicator, false);
    updateIndicator(extraIndicator, false);

    connect(lightBtn, &QPushButton::toggled, [=](bool s){
        updateIndicator(lightIndicator, s);
    });

    connect(extraBtn, &QPushButton::toggled, [=](bool s){
        updateIndicator(extraIndicator, s);
    });

    connectionStatus = new QLabel("🔴 OFFLINE");

    QVBoxLayout *btns = new QVBoxLayout();
    btns->addWidget(lightBtn);
    btns->addWidget(lightIndicator);
    btns->addWidget(extraBtn);
    btns->addWidget(extraIndicator);
    btns->addWidget(connectionStatus);

    center->addWidget(backBtn);
    center->addLayout(btns);

    layout->addWidget(leftTrack);
    layout->addLayout(center);
    layout->addWidget(rightTrack);

    stack->addWidget(tankScreen);
}

// ================= STEERING =================

void MainWindow::setupSteeringUI()
{
    steeringScreen = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(steeringScreen);

    QPushButton *backBtn = new QPushButton("←");

    connect(backBtn, &QPushButton::clicked, [=]() {
        stack->setCurrentWidget(menuScreen);
    });

    throttle = new QSlider(Qt::Vertical);
    steering = new QSlider(Qt::Horizontal);

    throttle->setRange(-100, 100);
    steering->setRange(-100, 100);

    // Кнопки
    lightBtn = makeRoundButton("L");
    extraBtn = makeRoundButton("E");

    lightIndicator = new QLabel();
    extraIndicator = new QLabel();

    lightIndicator->setFixedSize(14,14);
    extraIndicator->setFixedSize(14,14);

    updateIndicator(lightIndicator, false);
    updateIndicator(extraIndicator, false);

    connect(lightBtn, &QPushButton::toggled, [=](bool s){
        updateIndicator(lightIndicator, s);
    });

    connect(extraBtn, &QPushButton::toggled, [=](bool s){
        updateIndicator(extraIndicator, s);
    });

    connectionStatus = new QLabel("🔴 OFFLINE");

    layout->addWidget(backBtn);
    layout->addWidget(throttle);
    layout->addWidget(steering);
    layout->addWidget(lightBtn);
    layout->addWidget(lightIndicator);
    layout->addWidget(extraBtn);
    layout->addWidget(extraIndicator);
    layout->addWidget(connectionStatus);

    stack->addWidget(steeringScreen);
}

// ================= INDICATOR =================

void MainWindow::updateIndicator(QLabel *indicator, bool state)
{
    if (state)
        indicator->setStyleSheet("background-color: green; border-radius: 7px;");
    else
        indicator->setStyleSheet("background-color: red; border-radius: 7px;");
}

// ================= KEYBOARD =================

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_W) keyW = true;
    if (event->key() == Qt::Key_S) keyS = true;
    if (event->key() == Qt::Key_Up) keyUp = true;
    if (event->key() == Qt::Key_Down) keyDown = true;
    if (event->key() == Qt::Key_A) keyA = true;
    if (event->key() == Qt::Key_D) keyD = true;

    if (event->key() == Qt::Key_L) lightBtn->toggle();
    if (event->key() == Qt::Key_E) extraBtn->toggle();
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;

    if (event->key() == Qt::Key_W) keyW = false;
    if (event->key() == Qt::Key_S) keyS = false;
    if (event->key() == Qt::Key_Up) keyUp = false;
    if (event->key() == Qt::Key_Down) keyDown = false;
    if (event->key() == Qt::Key_A) keyA = false;
    if (event->key() == Qt::Key_D) keyD = false;
}