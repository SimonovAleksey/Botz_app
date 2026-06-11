#include "MainWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QParallelAnimationGroup>
#include <QApplication>
#include <QRegularExpression>

static const char *C_BLUE  = "#0071E3";
static const char *C_GREEN = "#34C759";
static const char *C_RED   = "#FF3B30";

// ── Global stylesheet ──────────────────────────────────────────────────────────
static const char *GLOBAL_SS = R"(
QWidget#root { background-color: #F5F5F7; }
* { font-family: ".AppleSystemUIFont","SF Pro Text","Helvetica Neue",Arial,sans-serif; }
QLabel { background:transparent; color:#1D1D1F; border:none; }
QLineEdit {
    background:#FFFFFF; border:1px solid #D1D1D6; border-radius:8px;
    color:#1D1D1F; padding:7px 11px; font-size:13px;
    selection-background-color:#0071E3; selection-color:white;
}
QLineEdit:focus { border-color:#0071E3; }
QSlider::groove:vertical   { background:#E5E5EA; width:4px;  border-radius:2px; }
QSlider::groove:horizontal { background:#E5E5EA; height:4px; border-radius:2px; }
QSlider::handle:vertical {
    background:#FFFFFF; border:1px solid #C7C7CC;
    width:22px; height:22px; margin:0 -9px; border-radius:11px;
}
QSlider::handle:horizontal {
    background:#FFFFFF; border:1px solid #C7C7CC;
    width:22px; height:22px; margin:-9px 0; border-radius:11px;
}
QSlider::sub-page:vertical   { background:#E5E5EA; border-radius:2px; }
QSlider::add-page:vertical    { background:#E5E5EA; border-radius:2px; }
QSlider::sub-page:horizontal  { background:#E5E5EA; border-radius:2px; }
QSlider::add-page:horizontal  { background:#E5E5EA; border-radius:2px; }
)";

// ── RC logo pixmap (window icon) ───────────────────────────────────────────────
static QPixmap makeRcPixmap(int size)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 113, 227));
    p.drawEllipse(1, 1, size-2, size-2);
    p.setPen(QPen(QColor(255,255,255,45), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(4, 4, size-8, size-8);
    QFont f(".AppleSystemUIFont");
    f.setPixelSize(qRound(size * 0.36));
    f.setBold(true);
    f.setLetterSpacing(QFont::AbsoluteSpacing, -0.5);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0,1,size,size), Qt::AlignCenter, "RC");
    p.end();
    return pix;
}

// ── Shadow helper (returns pointer for later mutation) ─────────────────────────
static QGraphicsDropShadowEffect* addShadow(QWidget *w,
                                             int blur=16, int yOff=2, int alpha=20)
{
    auto *fx = new QGraphicsDropShadowEffect;
    fx->setBlurRadius(blur);
    fx->setOffset(0, yOff);
    fx->setColor(QColor(0,0,0,alpha));
    w->setGraphicsEffect(fx);
    return fx;
}

// ── Card ───────────────────────────────────────────────────────────────────────
static QFrame* makeCard()
{
    QFrame *f = new QFrame;
    f->setObjectName("card");
    f->setStyleSheet(
        "QFrame#card {"
        "  background:rgba(255,255,255,0.82);"
        "  border:1px solid #E5E5EA;"
        "  border-radius:12px;"
        "}"
    );
    addShadow(f);
    return f;
}

// ── Accent slider ──────────────────────────────────────────────────────────────
// No QGraphicsEffect on the slider itself — would break mouse hit-testing.
// Card shadow mutates on value change to create the glow.
static void accentSlider(QSlider *s, QFrame *card, const QColor &accent)
{
    bool vert   = (s->orientation() == Qt::Vertical);
    QString ori  = vert ? "vertical"   : "horizontal";
    QString dim  = vert ? "width"      : "height";
    QString marg = vert ? "margin:0 -9px;" : "margin:-9px 0;";
    QString track = QString("rgba(%1,%2,%3,38)")
        .arg(accent.red()).arg(accent.green()).arg(accent.blue());

    s->setStyleSheet(QString(
        "QSlider::groove:%1 { background:%2; %3:4px; border-radius:2px; }"
        "QSlider::handle:%1 {"
        "  background:white; border:1.5px solid %4;"
        "  width:22px; height:22px; %5 border-radius:11px;"
        "}"
        "QSlider::handle:%1:hover { border-width:2px; }"
        "QSlider::sub-page:%1 { background:%2; border-radius:2px; }"
        "QSlider::add-page:%1  { background:%2; border-radius:2px; }"
    ).arg(ori, track, dim, accent.name(), marg));

    auto *shadow = qobject_cast<QGraphicsDropShadowEffect*>(card->graphicsEffect());
    if (!shadow) return;

    QObject::connect(s, &QSlider::valueChanged, s, [shadow, accent](int v) {
        bool active = qAbs(v) > 3;
        shadow->setBlurRadius(active ? 28 : 16);
        shadow->setOffset(0, active ? 0 : 2);
        shadow->setColor(active
            ? QColor(accent.red(), accent.green(), accent.blue(), 90)
            : QColor(0, 0, 0, 20));
    });
}

// ── Track mouse hold so timer doesn't fight the mouse ─────────────────────────
void MainWindow::trackSliderHold(QSlider *s)
{
    connect(s, &QSlider::sliderPressed,
            this, [this, s]() { heldSlider = s; });
    connect(s, &QSlider::sliderReleased,
            this, [this, s]() { if (heldSlider == s) heldSlider = nullptr; });
}

// ── Button glow toggle ────────────────────────────────────────────────────────
// Animates a QGraphicsDropShadowEffect on the button.
// On: blurRadius 0→22 (OutCubic, 280ms)   Off: 22→0 (InQuad, 360ms) + brief pulse
static void animateButtonGlow(QPushButton *btn, bool on, const QColor &glowColor)
{
    auto *fx = qobject_cast<QGraphicsDropShadowEffect*>(btn->graphicsEffect());
    if (!fx) {
        fx = new QGraphicsDropShadowEffect(btn);
        fx->setOffset(0, 0);
        fx->setBlurRadius(0);
        fx->setColor(glowColor);
        btn->setGraphicsEffect(fx);
    }
    // Kill any running animation on this effect to avoid conflicts
    for (auto *child : fx->children()) {
        if (auto *a = qobject_cast<QPropertyAnimation*>(child)) a->stop();
    }
    fx->setColor(glowColor);

    auto *anim = new QPropertyAnimation(fx, "blurRadius", fx);
    anim->setDuration(on ? 280 : 360);
    anim->setStartValue(fx->blurRadius());
    anim->setEndValue(on ? 22.0 : 0.0);
    anim->setEasingCurve(on ? QEasingCurve::OutCubic : QEasingCurve::InQuad);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── Button bounce ──────────────────────────────────────────────────────────────
static void bounceBtn(QPushButton *btn)
{
    const int h = qMax(qMax(btn->height(), btn->minimumHeight()), 20);
    auto *seq = new QSequentialAnimationGroup(btn);
    auto *par1 = new QParallelAnimationGroup;
    for (const QByteArray &prop : {"minimumHeight","maximumHeight"}) {
        auto *a = new QPropertyAnimation(btn, prop);
        a->setDuration(65); a->setStartValue(h); a->setEndValue(h-4);
        a->setEasingCurve(QEasingCurve::InQuad);
        par1->addAnimation(a);
    }
    auto *par2 = new QParallelAnimationGroup;
    for (const QByteArray &prop : {"minimumHeight","maximumHeight"}) {
        auto *a = new QPropertyAnimation(btn, prop);
        a->setDuration(160); a->setStartValue(h-4); a->setEndValue(h);
        a->setEasingCurve(QEasingCurve::OutBack);
        par2->addAnimation(a);
    }
    seq->addAnimation(par1);
    seq->addAnimation(par2);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── Full-window color flash ────────────────────────────────────────────────────
// Creates a transparent overlay over the central widget: quick fade-in, slow fade-out.
void MainWindow::flashWindow(const QColor &color, int fadeOutMs)
{
    auto *overlay = new QWidget(this);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setGeometry(centralWidget()->geometry());
    overlay->setStyleSheet(
        QString("background:rgba(%1,%2,%3,85);")
            .arg(color.red()).arg(color.green()).arg(color.blue())
    );
    overlay->raise();
    overlay->show();

    auto *effect = new QGraphicsOpacityEffect(overlay);
    overlay->setGraphicsEffect(effect);
    effect->setOpacity(0);

    auto *seq = new QSequentialAnimationGroup(overlay);

    auto *in = new QPropertyAnimation(effect, "opacity");
    in->setDuration(90);
    in->setStartValue(0.0); in->setEndValue(1.0);
    in->setEasingCurve(QEasingCurve::OutQuad);

    auto *out = new QPropertyAnimation(effect, "opacity");
    out->setDuration(fadeOutMs);
    out->setStartValue(1.0); out->setEndValue(0.0);
    out->setEasingCurve(QEasingCurve::OutCubic);

    seq->addAnimation(in);
    seq->addAnimation(out);
    connect(seq, &QSequentialAnimationGroup::finished, overlay, &QWidget::deleteLater);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── Screen fade transition ─────────────────────────────────────────────────────
void MainWindow::switchTo(QWidget *target)
{
    if (stack->currentWidget() == target) return;
    stack->setCurrentWidget(target);
    auto *effect = new QGraphicsOpacityEffect(target);
    target->setGraphicsEffect(effect);
    auto *anim = new QPropertyAnimation(effect, "opacity", this);
    anim->setDuration(180); anim->setStartValue(0.0); anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished,
            [target]() { target->setGraphicsEffect(nullptr); });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── Widget factories ───────────────────────────────────────────────────────────
QLabel* MainWindow::makeValueLabel()
{
    QLabel *l = new QLabel("0");
    l->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    l->setStyleSheet(
        "font-size:28px;font-weight:700;letter-spacing:-1px;"
        "color:#AEAEB2;background:transparent;border:none;min-width:72px;"
    );
    return l;
}
QSlider* MainWindow::makeVertSlider()
{
    QSlider *s = new QSlider(Qt::Vertical);
    s->setRange(-100, 100); s->setValue(0); s->setMinimumHeight(200);
    s->setFocusPolicy(Qt::NoFocus);
    return s;
}
QSlider* MainWindow::makeHorizSlider()
{
    QSlider *s = new QSlider(Qt::Horizontal);
    s->setRange(-100, 100); s->setValue(0); s->setMinimumWidth(260);
    s->setFocusPolicy(Qt::NoFocus);
    return s;
}
QPushButton* MainWindow::makeToggleBtn(const QString &text, const QString &key,
                                        const QString &activeColor)
{
    QPushButton *btn = new QPushButton(text + "   " + key);
    btn->setCheckable(true);
    btn->setMinimumSize(138, 42);
    QString darker = QColor(activeColor).darker(112).name();
    btn->setStyleSheet(
        "QPushButton {"
        "  background:#F5F5F7;border:1px solid #D1D1D6;border-radius:9px;"
        "  color:#1D1D1F;font-size:13px;font-weight:500;padding:0 16px;"
        "}"
        "QPushButton:hover   { background:#EBEBF0; }"
        "QPushButton:pressed { background:#DCDCE0; }"
        + QString(
            "QPushButton:checked { background:%1;border-color:%2;color:white;font-weight:600; }"
            "QPushButton:checked:hover { background:%2; }"
        ).arg(activeColor, darker)
    );
    return btn;
}
QWidget* MainWindow::makeHeader(const QString &title, QPushButton *&backBtn, QLabel *&statusOut)
{
    QWidget *bar = new QWidget;
    bar->setFixedHeight(52);
    bar->setStyleSheet(
        "background:rgba(255,255,255,0.90);"
        "border-bottom:1px solid rgba(0,0,0,0.08);"
    );
    backBtn = new QPushButton("←");
    backBtn->setFixedSize(36, 36);
    backBtn->setStyleSheet(
        "QPushButton { background:transparent;border:none;color:#0071E3;"
        "  font-size:20px;border-radius:8px;padding:0; }"
        "QPushButton:hover   { background:rgba(0,113,227,0.10); }"
        "QPushButton:pressed { background:rgba(0,113,227,0.20); }"
    );
    QLabel *titleLbl = new QLabel(title);
    titleLbl->setStyleSheet(
        "font-size:13px;font-weight:600;letter-spacing:0.2px;"
        "color:#1D1D1F;background:transparent;border:none;"
    );
    statusOut = new QLabel("Offline");
    statusOut->setStyleSheet(
        "font-size:12px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
    );
    QHBoxLayout *hl = new QHBoxLayout(bar);
    hl->setContentsMargins(12, 0, 16, 0);
    hl->addWidget(backBtn);
    hl->addStretch();
    hl->addWidget(titleLbl);
    hl->addStretch();
    hl->addWidget(statusOut);
    return bar;
}

// ── Connection state ───────────────────────────────────────────────────────────
void MainWindow::resetConnectError()
{
    if (isListening || isConnected) return;
    connectBtn->setText("Connect");
    connectBtn->setStyleSheet(
        QString("background:%1;border:none;border-radius:8px;"
                "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_BLUE)
    );
    menuStatusLabel->setText("Offline");
    menuStatusLabel->setStyleSheet(
        "font-size:13px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
    );
    ipInput->setStyleSheet(QString());
}

void MainWindow::setListening(bool on)
{
    isListening = on;
    if (on) {
        const QString addr = robot.serverAddress();
        ipInput->setText(addr);
        ipInput->setReadOnly(true);
        ipInput->setStyleSheet(
            "background:#F0F9FF;border:1.5px solid #0071E3;border-radius:8px;"
            "color:#0071E3;padding:7px 11px;font-size:13px;font-weight:500;"
        );
        connectBtn->setText("Stop");
        connectBtn->setStyleSheet(
            "background:#636366;border:none;border-radius:8px;"
            "color:white;font-size:13px;font-weight:600;padding:8px 20px;"
        );
        menuStatusLabel->setText("Waiting for robot...");
        menuStatusLabel->setStyleSheet(
            "font-size:13px;font-weight:500;color:#0071E3;background:transparent;border:none;"
        );
    } else {
        ipInput->setReadOnly(false);
        ipInput->clear();
        ipInput->setStyleSheet(QString());
        connectBtn->setText("Connect");
        connectBtn->setStyleSheet(
            QString("background:%1;border:none;border-radius:8px;"
                    "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_BLUE)
        );
        menuStatusLabel->setText("Offline");
        menuStatusLabel->setStyleSheet(
            "font-size:13px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
        );
    }
}

void MainWindow::setConnected(bool on)
{
    isConnected = on;

    if (on) flashWindow(QColor(C_GREEN), 500);

    const char *on_ss  = "font-size:12px;font-weight:600;color:#34C759;"
                         "background:transparent;border:none;";
    const char *off_ss = "font-size:12px;font-weight:500;color:#AEAEB2;"
                         "background:transparent;border:none;";
    for (QLabel *l : { tankStatusLabel, steerStatusLabel }) {
        l->setText(on ? "Online" : "Offline");
        l->setStyleSheet(on ? on_ss : off_ss);
    }

    if (on) {
        menuStatusLabel->setText("Connected");
        menuStatusLabel->setStyleSheet(
            "font-size:13px;font-weight:600;color:#34C759;background:transparent;border:none;"
        );
        connectBtn->setText("Disconnect");
        connectBtn->setStyleSheet(
            QString("background:%1;border:none;border-radius:8px;"
                    "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_RED)
        );
    } else {
        // Robot disconnected but server keeps listening — go back to waiting state
        if (isListening) {
            menuStatusLabel->setText("Waiting for robot...");
            menuStatusLabel->setStyleSheet(
                "font-size:13px;font-weight:500;color:#0071E3;background:transparent;border:none;"
            );
            connectBtn->setText("Stop");
            connectBtn->setStyleSheet(
                "background:#636366;border:none;border-radius:8px;"
                "color:white;font-size:13px;font-weight:600;padding:8px 20px;"
            );
        } else {
            setListening(false);
        }
    }
    if (connectBtn->height() > 0) bounceBtn(connectBtn);
}

void MainWindow::flashConnectError()
{
    if (isConnected) return;
    isListening = false;

    flashWindow(QColor(C_RED), 700);

    connectBtn->setText("Failed");
    connectBtn->setStyleSheet(
        "background:#FF3B30;border:none;border-radius:8px;"
        "color:white;font-size:13px;font-weight:600;padding:8px 20px;"
    );
    bounceBtn(connectBtn);
    QTimer::singleShot(180, this, [=]() { if (!isConnected) bounceBtn(connectBtn); });

    menuStatusLabel->setText("Server failed to start");
    menuStatusLabel->setStyleSheet(
        "font-size:13px;font-weight:500;color:#FF3B30;background:transparent;border:none;"
    );

    ipInput->setReadOnly(false);
    ipInput->clear();
    ipInput->setStyleSheet(
        "background:#FFF5F5;border:1.5px solid #FF3B30;border-radius:8px;"
        "color:#1D1D1F;padding:7px 11px;font-size:13px;"
    );
    QTimer::singleShot(2500, this, [=]() { if (!isListening) resetConnectError(); });
}

void MainWindow::setLightState(bool on)
{
    lightOn = on;
    tankLightBtn->setChecked(on);
    steerLightBtn->setChecked(on);
    animateButtonGlow(tankLightBtn,  on, QColor(C_GREEN));
    animateButtonGlow(steerLightBtn, on, QColor(C_GREEN));
}
void MainWindow::setExtraState(bool on)
{
    extraOn = on;
    tankExtraBtn->setChecked(on);
    steerExtraBtn->setChecked(on);
    animateButtonGlow(tankExtraBtn,  on, QColor(C_RED));
    animateButtonGlow(steerExtraBtn, on, QColor(C_RED));
}

// ── MENU ───────────────────────────────────────────────────────────────────────
void MainWindow::setupMenu()
{
    menuScreen = new QWidget;
    menuScreen->setObjectName("root");
    QVBoxLayout *vl = new QVBoxLayout(menuScreen);
    vl->setContentsMargins(40, 0, 40, 0);
    vl->setSpacing(0);

    QLabel *title = new QLabel("Robot Controller");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size:28px;font-weight:700;letter-spacing:-0.5px;color:#1D1D1F;");
    QLabel *sub = new QLabel("Arduino WebSocket Interface");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("font-size:13px;color:#8E8E93;");

    QFrame *connCard = makeCard();
    connCard->setMaximumWidth(400);
    QVBoxLayout *ccl = new QVBoxLayout(connCard);
    ccl->setContentsMargins(20,18,20,18); ccl->setSpacing(12);

    QLabel *srvLabel = new QLabel("Controller IP — enter this in ESP32 sketch:");
    srvLabel->setStyleSheet("font-size:11px;font-weight:600;letter-spacing:0.5px;color:#8E8E93;");

    ipInput = new QLineEdit;
    ipInput->setPlaceholderText("Press Connect to start server");

    connectBtn = new QPushButton("Connect");
    connectBtn->setStyleSheet(
        QString("background:%1;border:none;border-radius:8px;"
                "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_BLUE)
    );

    menuStatusLabel = new QLabel("Offline");
    menuStatusLabel->setAlignment(Qt::AlignCenter);
    menuStatusLabel->setStyleSheet(
        "font-size:13px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
    );

    QHBoxLayout *ipRow = new QHBoxLayout;
    ipRow->setSpacing(8); ipRow->addWidget(ipInput, 1); ipRow->addWidget(connectBtn);
    ccl->addWidget(srvLabel); ccl->addLayout(ipRow); ccl->addWidget(menuStatusLabel);

    QHBoxLayout *cardRow = new QHBoxLayout;
    cardRow->addStretch(); cardRow->addWidget(connCard); cardRow->addStretch();

    auto makeModeCard = [&](const QString &lbl, const QString &hint) -> QFrame* {
        QFrame *f = makeCard();
        f->setFixedSize(172, 110); f->setCursor(Qt::PointingHandCursor);
        QVBoxLayout *fl = new QVBoxLayout(f);
        fl->setContentsMargins(16,12,16,12); fl->setSpacing(4);
        QLabel *tl = new QLabel(lbl);
        tl->setAlignment(Qt::AlignCenter);
        tl->setStyleSheet("font-size:15px;font-weight:600;color:#1D1D1F;");
        QLabel *hl = new QLabel(hint);
        hl->setAlignment(Qt::AlignCenter);
        hl->setStyleSheet("font-size:11px;color:#8E8E93;");
        fl->addStretch(); fl->addWidget(tl); fl->addSpacing(2);
        fl->addWidget(hl); fl->addStretch();
        return f;
    };
    QFrame *tankCard  = makeModeCard("Tank",     "W / S  ·  ↑ / ↓");
    QFrame *steerCard = makeModeCard("Steering", "W / S  ·  A / D");

    QHBoxLayout *modeRow = new QHBoxLayout;
    modeRow->setSpacing(16); modeRow->addStretch();
    modeRow->addWidget(tankCard); modeRow->addWidget(steerCard); modeRow->addStretch();

    vl->addStretch(2); vl->addWidget(title); vl->addSpacing(4); vl->addWidget(sub);
    vl->addSpacing(26); vl->addLayout(cardRow);
    vl->addSpacing(22); vl->addLayout(modeRow); vl->addStretch(3);
    stack->addWidget(menuScreen);

    auto makeOverlay = [](QFrame *card) -> QPushButton* {
        QPushButton *ov = new QPushButton(card);
        ov->setGeometry(0, 0, card->width(), card->height());
        ov->setStyleSheet("background:transparent;border:none;border-radius:12px;");
        ov->raise();
        return ov;
    };
    QPushButton *tankOv  = makeOverlay(tankCard);
    QPushButton *steerOv = makeOverlay(steerCard);
    connect(tankOv,  &QPushButton::clicked, [=]() { bounceBtn(tankOv);  switchTo(tankScreen);     });
    connect(steerOv, &QPushButton::clicked, [=]() { bounceBtn(steerOv); switchTo(steeringScreen); });
    connect(connectBtn, &QPushButton::clicked, [=]() {
        bounceBtn(connectBtn);
        if (isConnected || isListening) {
            robot.stopServer();
            isConnected = false;
            setListening(false);
            for (QLabel *l : { tankStatusLabel, steerStatusLabel }) {
                l->setText("Offline");
                l->setStyleSheet("font-size:12px;font-weight:500;color:#AEAEB2;"
                                 "background:transparent;border:none;");
            }
            return;
        }
        robot.startServer(8080);
        // connectionFailed fires if listen() fails; otherwise show waiting state
        if (!robot.serverAddress().isEmpty())
            setListening(true);
    });
}

// ── TANK ───────────────────────────────────────────────────────────────────────
void MainWindow::setupTankUI()
{
    tankScreen = new QWidget;
    tankScreen->setObjectName("root");
    QVBoxLayout *vl = new QVBoxLayout(tankScreen);
    vl->setContentsMargins(0,0,0,0); vl->setSpacing(0);

    QPushButton *backBtn = nullptr;
    QWidget *header = makeHeader("Tank", backBtn, tankStatusLabel);
    connect(backBtn, &QPushButton::clicked, [=]() { bounceBtn(backBtn); switchTo(menuScreen); });

    auto makeTrackCard = [this](const QString &label, QSlider *&so, QLabel *&vo) -> QFrame* {
        QFrame *card = makeCard();
        card->setFixedWidth(116);
        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(14,18,14,18); cl->setAlignment(Qt::AlignHCenter); cl->setSpacing(14);
        QLabel *lbl = new QLabel(label);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("font-size:10px;font-weight:600;letter-spacing:1.2px;color:#8E8E93;");
        so = makeVertSlider();
        vo = makeValueLabel();
        accentSlider(so, card, QColor(C_BLUE));
        trackSliderHold(so);
        cl->addWidget(lbl);
        cl->addWidget(so, 1, Qt::AlignHCenter);
        cl->addWidget(vo);
        return card;
    };

    QFrame *leftCard  = makeTrackCard("LEFT",  leftTrack,  leftValLabel);
    QFrame *rightCard = makeTrackCard("RIGHT", rightTrack, rightValLabel);

    tankLightBtn = makeToggleBtn("Light", "L", C_GREEN);
    tankExtraBtn = makeToggleBtn("Extra", "E", C_RED);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10); btnRow->addStretch();
    btnRow->addWidget(tankLightBtn); btnRow->addWidget(tankExtraBtn); btnRow->addStretch();

    QLabel *hint = new QLabel("W / S  —  Left track          ↑ / ↓  —  Right track");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("font-size:11px;color:#C7C7CC;");

    QWidget *content = new QWidget; content->setObjectName("root");
    QVBoxLayout *cv = new QVBoxLayout(content);
    cv->setContentsMargins(24,18,24,16); cv->setSpacing(16);

    QHBoxLayout *tracksRow = new QHBoxLayout;
    tracksRow->addStretch(1); tracksRow->addWidget(leftCard);
    tracksRow->addStretch(3); tracksRow->addWidget(rightCard); tracksRow->addStretch(1);

    cv->addLayout(tracksRow, 1);
    cv->addSpacing(22);
    cv->addLayout(btnRow);
    cv->addSpacing(10);
    cv->addWidget(hint);
    vl->addWidget(header); vl->addWidget(content, 1);
    stack->addWidget(tankScreen);

    connect(tankLightBtn, &QPushButton::clicked, [=]() { bounceBtn(tankLightBtn); });
    connect(tankExtraBtn, &QPushButton::clicked, [=]() { bounceBtn(tankExtraBtn); });
    connect(tankLightBtn, &QPushButton::toggled, this, &MainWindow::setLightState);
    connect(tankExtraBtn, &QPushButton::toggled, this, &MainWindow::setExtraState);
}

// ── STEERING ──────────────────────────────────────────────────────────────────
void MainWindow::setupSteeringUI()
{
    steeringScreen = new QWidget; steeringScreen->setObjectName("root");
    QVBoxLayout *vl = new QVBoxLayout(steeringScreen);
    vl->setContentsMargins(0,0,0,0); vl->setSpacing(0);

    QPushButton *backBtn = nullptr;
    QWidget *header = makeHeader("Steering", backBtn, steerStatusLabel);
    connect(backBtn, &QPushButton::clicked, [=]() { bounceBtn(backBtn); switchTo(menuScreen); });

    // Throttle (left, vertical, green)
    QFrame *throttleCard = makeCard(); throttleCard->setFixedWidth(116);
    QVBoxLayout *thl = new QVBoxLayout(throttleCard);
    thl->setContentsMargins(14,18,14,18); thl->setAlignment(Qt::AlignHCenter); thl->setSpacing(14);
    QLabel *throttleLbl = new QLabel("THROTTLE"); throttleLbl->setAlignment(Qt::AlignCenter);
    throttleLbl->setStyleSheet("font-size:10px;font-weight:600;letter-spacing:1.2px;color:#8E8E93;");
    throttle = makeVertSlider(); throttleValLabel = makeValueLabel();
    accentSlider(throttle, throttleCard, QColor(C_GREEN));
    trackSliderHold(throttle);
    thl->addWidget(throttleLbl); thl->addWidget(throttle, 1, Qt::AlignHCenter);
    thl->addWidget(throttleValLabel);

    // Steering (horizontal, green)
    QFrame *steerCard = makeCard();
    QVBoxLayout *scl = new QVBoxLayout(steerCard);
    scl->setContentsMargins(20,16,20,16); scl->setSpacing(10);
    QLabel *steerLbl = new QLabel("STEERING"); steerLbl->setAlignment(Qt::AlignCenter);
    steerLbl->setStyleSheet("font-size:10px;font-weight:600;letter-spacing:1.2px;color:#8E8E93;");
    steering = makeHorizSlider(); steerValLabel = makeValueLabel();
    accentSlider(steering, steerCard, QColor(C_GREEN));
    trackSliderHold(steering);
    scl->addWidget(steerLbl); scl->addWidget(steering); scl->addWidget(steerValLabel, 0, Qt::AlignCenter);

    steerLightBtn = makeToggleBtn("Light", "L", C_GREEN);
    steerExtraBtn = makeToggleBtn("Extra", "E", C_RED);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10); btnRow->addStretch();
    btnRow->addWidget(steerLightBtn); btnRow->addWidget(steerExtraBtn); btnRow->addStretch();

    QLabel *hint = new QLabel("W / S  —  Throttle          A / D  —  Steering");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("font-size:11px;color:#C7C7CC;");

    // Right panel: buttons at top, slider lower, hint at bottom
    QWidget *rightPanel = new QWidget; rightPanel->setObjectName("root");
    QVBoxLayout *rpl = new QVBoxLayout(rightPanel);
    rpl->setContentsMargins(0,0,0,0); rpl->setSpacing(0);
    rpl->addSpacing(16);
    rpl->addLayout(btnRow);
    rpl->addStretch(1);          // pushes slider toward bottom
    rpl->addWidget(steerCard);
    rpl->addSpacing(8);
    rpl->addWidget(hint, 0, Qt::AlignCenter);
    rpl->addSpacing(16);

    QWidget *content = new QWidget; content->setObjectName("root");
    QHBoxLayout *hl = new QHBoxLayout(content);
    hl->setContentsMargins(24,18,24,18); hl->setSpacing(16);
    hl->addWidget(throttleCard); hl->addWidget(rightPanel, 1);

    vl->addWidget(header); vl->addWidget(content, 1);
    stack->addWidget(steeringScreen);

    connect(steerLightBtn, &QPushButton::clicked, [=]() { bounceBtn(steerLightBtn); });
    connect(steerExtraBtn, &QPushButton::clicked, [=]() { bounceBtn(steerExtraBtn); });
    connect(steerLightBtn, &QPushButton::toggled, this, &MainWindow::setLightState);
    connect(steerExtraBtn, &QPushButton::toggled, this, &MainWindow::setExtraState);
}

// ── CONSTRUCTOR ────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Robot Controller");
    setWindowIcon(QIcon(makeRcPixmap(64)));
    resize(820, 560);
    setMinimumSize(680, 460);
    setStyleSheet(GLOBAL_SS);

    stack = new QStackedWidget(this);
    setCentralWidget(stack);

    setupMenu();
    setupTankUI();
    setupSteeringUI();

    stack->setCurrentWidget(menuScreen);
    qApp->installEventFilter(this);

    connect(&robot, &RobotController::connected,        [=]() { setConnected(true);  });
    connect(&robot, &RobotController::disconnected,     [=]() { isConnected = false; setConnected(false); });
    connect(&robot, &RobotController::connectionFailed, [=]() { flashConnectError(); });

    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, [=]() {

        // ── Smooth return-to-zero ──────────────────────────────────────────────
        // Skip if slider is mouse-held (avoids fighting the mouse).
        // Clamp-at-zero prevents overshoot oscillation: if step would cross
        // zero (e.g. v=3, step=6 → -3), snap to 0 instead.
        auto updateAxis = [this](QSlider *s, bool plus, bool minus) {
            if (s == heldSlider) return;
            int v = s->value();
            if (plus)  v += 4;
            if (minus) v -= 4;
            if (!plus && !minus) {
                if (v > 0) { v -= 6; if (v < 0) v = 0; }
                else if (v < 0) { v += 6; if (v > 0) v = 0; }
            }
            s->setValue(qBound(-100, v, 100));
        };

        // ── Label update: per-screen accent colour ─────────────────────────────
        auto setValLabel = [](QLabel *l, int v, const char *col) {
            l->setText(QString::number(v));
            l->setStyleSheet(
                v != 0
                ? QString("font-size:28px;font-weight:700;letter-spacing:-1px;"
                          "color:%1;background:transparent;border:none;min-width:72px;").arg(col)
                : "font-size:28px;font-weight:700;letter-spacing:-1px;"
                  "color:#AEAEB2;background:transparent;border:none;min-width:72px;"
            );
        };

        if (stack->currentWidget() == tankScreen) {
            updateAxis(leftTrack,  keyW,  keyS);
            updateAxis(rightTrack, keyUp, keyDown);
            setValLabel(leftValLabel,  leftTrack->value(),  C_BLUE);
            setValLabel(rightValLabel, rightTrack->value(), C_BLUE);
        }
        if (stack->currentWidget() == steeringScreen) {
            updateAxis(throttle, keyW, keyS);
            updateAxis(steering, keyD, keyA);
            setValLabel(throttleValLabel, throttle->value(), C_GREEN);
            setValLabel(steerValLabel,    steering->value(), C_GREEN);
        }

        if (isConnected && ++sendTick >= 5) {
            sendTick = 0;
            if (stack->currentWidget() == tankScreen)
                robot.sendTankState(leftTrack->value(), rightTrack->value(), lightOn, extraOn);
            else if (stack->currentWidget() == steeringScreen)
                robot.sendSteerState(throttle->value(), steering->value(), lightOn, extraOn);
        }
    });
    updateTimer->start(20);
}

// ── KEYBOARD ──────────────────────────────────────────────────────────────────
// eventFilter intercepts arrow keys before child widgets (QPushButton focus
// navigation) can consume them. Only Up/Down are intercepted here; all other
// game keys reach keyPressEvent normally.
bool MainWindow::eventFilter(QObject *, QEvent *event)
{
    const auto type = event->type();
    if (type != QEvent::KeyPress && type != QEvent::KeyRelease) return false;
    auto *ke = static_cast<QKeyEvent*>(event);
    if (ke->isAutoRepeat()) return false;
    const bool press = (type == QEvent::KeyPress);
    switch (ke->key()) {
    case Qt::Key_Up:   keyUp   = press; return true;
    case Qt::Key_Down: keyDown = press; return true;
    default: return false;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;
    switch (event->key()) {
    case Qt::Key_Escape:
        if (stack->currentWidget() != menuScreen) switchTo(menuScreen);
        break;
    case Qt::Key_W:    keyW    = true;          break;
    case Qt::Key_S:    keyS    = true;          break;
    case Qt::Key_A:    keyA    = true;          break;
    case Qt::Key_D:    keyD    = true;          break;
    case Qt::Key_L:    setLightState(!lightOn); break;
    case Qt::Key_E:    setExtraState(!extraOn); break;
    default: break;
    }
}
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;
    switch (event->key()) {
    case Qt::Key_W:    keyW    = false; break;
    case Qt::Key_S:    keyS    = false; break;
    case Qt::Key_A:    keyA    = false; break;
    case Qt::Key_D:    keyD    = false; break;
    default: break;
    }
}
