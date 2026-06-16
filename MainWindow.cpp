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
#include <QFile>
#include <QTextStream>
#include <QClipboard>
#include <QToolTip>

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

// ── Battery gauge: рисованная иконка + % + схематичный вольтметр ───────────────
class BatteryGaugeWidget : public QWidget
{
public:
    explicit BatteryGaugeWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        // Высота подогнана так, чтобы отступ сверху (до иконки) и снизу
        // (после строки вольтметра) внутри виджета совпадали — тогда центрирование
        // виджета в карточке (Qt::AlignCenter) даёт симметричные отступы и от краёв карточки.
        setFixedSize(120, 105);
    }

    void setLevel(double voltage, int percent)
    {
        m_voltage = voltage;
        m_percent = qBound(0, percent, 100);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QColor color;
        if      (m_percent <= 20) color = QColor("#FF3B30");
        else if (m_percent <= 40) color = QColor("#FF9500");
        else if (m_percent <= 70) color = QColor("#FFCC00");
        else                      color = QColor("#34C759");

        const int w = width();

        // Иконка батареи (корпус + ушко-терминал) — центрируем по их суммарной ширине,
        // чтобы вся фигура была симметрична относительно карточки
        const int iconW = 58, iconH = 28, iconY = 4, nubW = 4;
        const int iconTotalW = iconW + nubW;
        const int iconX = (w - iconTotalW) / 2;
        QRectF body(iconX, iconY, iconW, iconH);

        p.setPen(QPen(color, 2.2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(body, 5, 5);

        QRectF nub(iconX + iconW, iconY + iconH * 0.28, nubW, iconH * 0.44);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(nub, 1.5, 1.5);

        const double inset = 3.5;
        QRectF inner(body.x() + inset, body.y() + inset,
                     body.width() - inset * 2, body.height() - inset * 2);
        double fillW = inner.width() * (m_percent / 100.0);
        if (fillW > 0.5) {
            p.setBrush(color);
            p.drawRoundedRect(QRectF(inner.x(), inner.y(), fillW, inner.height()), 3, 3);
        }

        // Процент
        QFont pf(".AppleSystemUIFont");
        pf.setPixelSize(22);
        pf.setBold(true);
        p.setFont(pf);
        p.setPen(color);
        QRect pctRect(0, iconY + iconH + 6, w, 28);
        p.drawText(pctRect, Qt::AlignCenter, QString::number(m_percent) + "%");

        // Схематичный вольтметр: та же ширина, что у иконки (корпус + ушко),
        // и та же ось центрирования — для симметрии относительно карточки
        const int gaugeW = iconTotalW, gaugeH = 5;
        const int gaugeX = (w - gaugeW) / 2;
        const int gaugeY = pctRect.bottom() + 8;

        QRectF track(gaugeX, gaugeY, gaugeW, gaugeH);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E5E5EA"));
        p.drawRoundedRect(track, 2.5, 2.5);

        double frac = qBound(0.0, (m_voltage - kMinV) / (kMaxV - kMinV), 1.0);
        if (frac > 0.0) {
            p.setBrush(color);
            p.drawRoundedRect(QRectF(track.x(), track.y(), track.width() * frac, track.height()), 2.5, 2.5);
        }

        QFont vf(".AppleSystemUIFont");
        vf.setPixelSize(12);
        vf.setBold(false);
        p.setFont(vf);
        p.setPen(QColor("#8E8E93"));
        QRect vRect(0, gaugeY + gaugeH + 6, w, 16);
        p.drawText(vRect, Qt::AlignCenter, QString::number(m_voltage, 'f', 1) + " V");
    }

private:
    double m_voltage = 0.0;
    int    m_percent = 0;
    static constexpr double kMinV = 9.0;   // 3S Li-ion разряжен
    static constexpr double kMaxV = 12.6;  // 3S Li-ion заряжен
};

static QFrame* makeBatteryCard(BatteryGaugeWidget *&gaugeOut)
{
    QFrame *card = makeCard();
    card->setFixedSize(140, 152);
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(0, 0, 0, 0);
    gaugeOut = new BatteryGaugeWidget;
    l->addWidget(gaugeOut, 0, Qt::AlignCenter);
    return card;
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
    if (!on) {
        // Once faded out, drop the effect entirely instead of leaving a
        // zero-blur QGraphicsDropShadowEffect attached forever: a lingering
        // effect on the button clashes with switchTo()'s screen-level
        // opacity effect and makes the button flash at the wrong spot for a
        // frame the next time this screen is shown. Deferred via singleShot
        // so we never delete the effect from inside its own animation's
        // finished signal.
        QObject::connect(anim, &QPropertyAnimation::finished, btn, [btn]() {
            QTimer::singleShot(0, btn, [btn]() { btn->setGraphicsEffect(nullptr); });
        });
    }
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
    connectBtn->setText("Подключиться");
    connectBtn->setStyleSheet(
        QString("background:%1;border:none;border-radius:8px;"
                "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_BLUE)
    );
    menuStatusLabel->setText("Не в сети");
    menuStatusLabel->setStyleSheet(
        "font-size:13px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
    );
}

void MainWindow::setListening(bool on)
{
    isListening = on;
    if (on) {
        connectBtn->setText("Стоп");
        connectBtn->setStyleSheet(
            "background:#636366;border:none;border-radius:8px;"
            "color:white;font-size:13px;font-weight:600;padding:8px 20px;"
        );
        menuStatusLabel->setText("Ожидание подключения робота...");
        menuStatusLabel->setStyleSheet(
            "font-size:13px;font-weight:500;color:#0071E3;background:transparent;border:none;"
        );
    } else {
        connectBtn->setText("Подключиться");
        connectBtn->setStyleSheet(
            QString("background:%1;border:none;border-radius:8px;"
                    "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_BLUE)
        );
        menuStatusLabel->setText("Не в сети");
        menuStatusLabel->setStyleSheet(
            "font-size:13px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
        );
    }
}

void MainWindow::setBatteryLevel(double voltage, int percent)
{
    tankBatteryGauge->setLevel(voltage, percent);
    steerBatteryGauge->setLevel(voltage, percent);
}

void MainWindow::setConnected(bool on)
{
    isConnected = on;

    if (on) {
        flashWindow(QColor(C_GREEN), 500);
        showToast("Робот подключён");
    }

    const char *on_ss  = "font-size:12px;font-weight:600;color:#34C759;"
                         "background:transparent;border:none;";
    const char *off_ss = "font-size:12px;font-weight:500;color:#AEAEB2;"
                         "background:transparent;border:none;";
    for (QLabel *l : { tankStatusLabel, steerStatusLabel }) {
        l->setText(on ? "В сети" : "Не в сети");
        l->setStyleSheet(on ? on_ss : off_ss);
    }
    if (!on) setBatteryLevel(0.0, 0);

    if (on) {
        menuStatusLabel->setText("Робот подключён");
        menuStatusLabel->setStyleSheet(
            "font-size:13px;font-weight:600;color:#34C759;background:transparent;border:none;"
        );
        connectBtn->setText("Отключиться");
        connectBtn->setStyleSheet(
            QString("background:%1;border:none;border-radius:8px;"
                    "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_RED)
        );
    } else {
        // Robot disconnected but server keeps listening — go back to waiting state
        if (isListening) {
            menuStatusLabel->setText("Ожидание подключения робота...");
            menuStatusLabel->setStyleSheet(
                "font-size:13px;font-weight:500;color:#0071E3;background:transparent;border:none;"
            );
            connectBtn->setText("Стоп");
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

    connectBtn->setText("Не удалось подключиться");
    connectBtn->setStyleSheet(
        "background:#FF3B30;border:none;border-radius:8px;"
        "color:white;font-size:13px;font-weight:600;padding:8px 20px;"
    );
    bounceBtn(connectBtn);
    QTimer::singleShot(180, this, [=]() { if (!isConnected) bounceBtn(connectBtn); });

    menuStatusLabel->setText("Не удалось подключиться");
    menuStatusLabel->setStyleSheet(
        "font-size:13px;font-weight:500;color:#FF3B30;background:transparent;border:none;"
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

    // Ширина подогнана так, чтобы совпадать с шириной ряда карточек Tank/Steering
    // (172 + 16 spacing + 172 = 360) — см. modeRow ниже.
    const int kModeRowWidth = 172 + 16 + 172;

    QFrame *connCard = makeCard();
    connCard->setFixedWidth(kModeRowWidth);
    QVBoxLayout *ccl = new QVBoxLayout(connCard);
    ccl->setContentsMargins(20,18,20,18); ccl->setSpacing(12);

    connectBtn = new QPushButton("Подключиться");
    connectBtn->setStyleSheet(
        QString("background:%1;border:none;border-radius:8px;"
                "color:white;font-size:13px;font-weight:600;padding:8px 20px;").arg(C_BLUE)
    );

    menuStatusLabel = new QLabel("Не в сети");
    menuStatusLabel->setAlignment(Qt::AlignCenter);
    menuStatusLabel->setStyleSheet(
        "font-size:13px;font-weight:500;color:#AEAEB2;background:transparent;border:none;"
    );

    QFrame *divider = new QFrame;
    divider->setFixedHeight(1);
    divider->setStyleSheet("background:#E5E5EA;border:none;");

    QPushButton *programBtn = new QPushButton("Сгенерировать код для Arduino");
    programBtn->setCursor(Qt::PointingHandCursor);
    programBtn->setStyleSheet(
        "background:transparent;border:1px solid #D1D1D6;border-radius:8px;"
        "color:#1D1D1F;font-size:13px;font-weight:600;padding:8px 18px;"
    );

    ccl->addWidget(connectBtn); ccl->addWidget(menuStatusLabel);
    ccl->addWidget(divider);
    ccl->addWidget(programBtn);

    connect(programBtn, &QPushButton::clicked, [=]() {
        bounceBtn(programBtn);
        switchTo(programScreen);
    });

    QHBoxLayout *cardRow = new QHBoxLayout;
    cardRow->addStretch(); cardRow->addWidget(connCard); cardRow->addStretch();

    auto makeModeCard = [&](const QString &lbl, const QString &hint) -> QFrame* {
        QFrame *f = makeCard();
        f->setFixedSize(172, 110); f->setCursor(Qt::PointingHandCursor);
        QVBoxLayout *fl = new QVBoxLayout(f);
        fl->setContentsMargins(16,12,16,12); fl->setSpacing(4);
        QLabel *tl = new QLabel(lbl);
        tl->setAlignment(Qt::AlignCenter);
        tl->setWordWrap(true);
        tl->setStyleSheet("font-size:15px;font-weight:600;color:#1D1D1F;");
        QLabel *hl = new QLabel(hint);
        hl->setAlignment(Qt::AlignCenter);
        hl->setStyleSheet("font-size:11px;color:#8E8E93;");
        fl->addStretch(); fl->addWidget(tl); fl->addSpacing(2);
        fl->addWidget(hl); fl->addStretch();
        return f;
    };
    QFrame *tankCard  = makeModeCard("Танковое управление",     "W / S  ·  ↑ / ↓");
    QFrame *steerCard = makeModeCard("Рулевое управление", "W / S  ·  A / D");

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
                l->setText("Не в сети");
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
    QWidget *header = makeHeader("Танковое управление", backBtn, tankStatusLabel);
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
    QFrame *batteryCard = makeBatteryCard(tankBatteryGauge);

    tankLightBtn = makeToggleBtn("Light", "L", C_GREEN);
    tankExtraBtn = makeToggleBtn("Extra", "E", C_RED);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(10); btnRow->addStretch();
    btnRow->addWidget(tankLightBtn); btnRow->addWidget(tankExtraBtn); btnRow->addStretch();

    QLabel *hint = new QLabel("W / S  —  Левая гусеница          ↑ / ↓  —  Правая гусеница");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("font-size:11px;color:#C7C7CC;");

    QWidget *content = new QWidget; content->setObjectName("root");
    QVBoxLayout *cv = new QVBoxLayout(content);
    cv->setContentsMargins(24,18,24,16); cv->setSpacing(16);

    QHBoxLayout *tracksRow = new QHBoxLayout;
    tracksRow->addStretch(1); tracksRow->addWidget(leftCard);
    tracksRow->addStretch(1); tracksRow->addWidget(batteryCard, 0, Qt::AlignVCenter);
    tracksRow->addStretch(1); tracksRow->addWidget(rightCard); tracksRow->addStretch(1);

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
    QWidget *header = makeHeader("Рулевое управление", backBtn, steerStatusLabel);
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

    QLabel *hint = new QLabel("W / S  —  Движение          A / D  —  Рулевое управление");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("font-size:11px;color:#C7C7CC;");

    // Right panel: buttons at top, slider lower, hint at bottom
    QWidget *rightPanel = new QWidget; rightPanel->setObjectName("root");
    QVBoxLayout *rpl = new QVBoxLayout(rightPanel);
    rpl->setContentsMargins(0,0,0,0); rpl->setSpacing(0);
    QFrame *batteryCard = makeBatteryCard(steerBatteryGauge);

    rpl->addSpacing(16);
    rpl->addLayout(btnRow);
    rpl->addStretch(1);
    rpl->addWidget(batteryCard, 0, Qt::AlignHCenter);
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

// ── PROGRAM BOARD ──────────────────────────────────────────────────────────────
void MainWindow::setupProgramUI()
{
    programScreen = new QWidget;
    programScreen->setObjectName("root");
    QVBoxLayout *vl = new QVBoxLayout(programScreen);
    vl->setContentsMargins(0,0,0,0); vl->setSpacing(0);

    QPushButton *backBtn = nullptr;
    QLabel *dummyStatus  = nullptr;
    QWidget *header = makeHeader("Program Board", backBtn, dummyStatus);
    // Не hide() — скрытый виджет выпадает из расчёта layout, и заголовок
    // съезжает влево (backBtn слева не уравновешен ничем справа).
    // Делаем невидимым, но с шириной, которая компенсирует backBtn,
    // чтобы заголовок остался по центру, как на остальных экранах.
    dummyStatus->setText(QString());
    dummyStatus->setFixedWidth(32);
    dummyStatus->setStyleSheet("background:transparent;border:none;");
    connect(backBtn, &QPushButton::clicked, [=]() { bounceBtn(backBtn); switchTo(menuScreen); });

    QFrame *card = makeCard();
    card->setFixedWidth(340);
    QVBoxLayout *cl = new QVBoxLayout(card);
    cl->setContentsMargins(20,18,20,18); cl->setSpacing(12);

    QLabel *hint = new QLabel(
        "Введи имя Wi-Fi сети и пароль, которые будет использовать плата.\n"
        "Скрипт сгенерируется и сразу скопируется в буфер обмена — "
        "вставь его в Arduino IDE и загрузи на ESP32.");
    hint->setWordWrap(true);
    hint->setStyleSheet("font-size:12px;color:#8E8E93;");

    QLabel *ssidLabel = new QLabel("Имя Wi-Fi сети (SSID)");
    ssidLabel->setStyleSheet("font-size:11px;font-weight:600;letter-spacing:0.5px;color:#8E8E93;");
    programSsidInput = new QLineEdit;
    programSsidInput->setPlaceholderText("WiFi");

    QLabel *passLabel = new QLabel("Пароль");
    passLabel->setStyleSheet("font-size:11px;font-weight:600;letter-spacing:0.5px;color:#8E8E93;");
    programPassInput = new QLineEdit;
    programPassInput->setPlaceholderText("Password");

    QLabel *ipLabel = new QLabel("IP Вашего компьютера");
    ipLabel->setStyleSheet("font-size:11px;font-weight:600;letter-spacing:0.5px;color:#8E8E93;");

    QPushButton *ipHelpBtn = new QPushButton("?");
    ipHelpBtn->setCursor(Qt::PointingHandCursor);
    ipHelpBtn->setFixedSize(18, 18);
    ipHelpBtn->setStyleSheet(
        "QPushButton { background:#E5E5EA;border:none;border-radius:9px;"
        "  color:#636366;font-size:11px;font-weight:700;padding:0; }"
        "QPushButton:hover   { background:#D1D1D6; }"
        "QPushButton:pressed { background:#C7C7CC; }"
    );
    connect(ipHelpBtn, &QPushButton::clicked, [=]() {
        QToolTip::showText(
            ipHelpBtn->mapToGlobal(QPoint(ipHelpBtn->width() + 6, 0)),
            "Как узнать IP компьютера в локальной сети:\n\n"
            "macOS: меню  → Системные настройки → Wi-Fi →\n"
            "значок (i) рядом с активной сетью → IP-адрес.\n\n"
            "Либо в Терминале:\n"
            "ipconfig getifaddr en0\n\n"
            "Windows: Параметры → Сеть и Интернет → Wi-Fi →\n"
            "выбранная сеть → Свойства → IPv4-адрес.\n\n"
            "Либо в командной строке:\n"
            "ipconfig",
            ipHelpBtn, QRect(), 8000
        );
    });

    QHBoxLayout *ipLabelRow = new QHBoxLayout;
    ipLabelRow->setSpacing(6); ipLabelRow->setContentsMargins(0,0,0,0);
    ipLabelRow->addWidget(ipLabel); ipLabelRow->addWidget(ipHelpBtn); ipLabelRow->addStretch();

    programIpInput = new QLineEdit;
    programIpInput->setPlaceholderText("888.88.88.8");

    QLabel *portLabel = new QLabel("Порт");
    portLabel->setStyleSheet("font-size:11px;font-weight:600;letter-spacing:0.5px;color:#8E8E93;");
    programPortInput = new QLineEdit("8080");
    programPortInput->setPlaceholderText("8080");

    QPushButton *genBtn = new QPushButton("Сгенерировать и скопировать скрипт");
    genBtn->setCursor(Qt::PointingHandCursor);
    genBtn->setStyleSheet(
        QString("background:%1;border:none;border-radius:8px;"
                "color:white;font-size:13px;font-weight:600;padding:10px 16px;").arg(C_BLUE)
    );

    cl->addWidget(hint);
    cl->addWidget(ssidLabel); cl->addWidget(programSsidInput);
    cl->addWidget(passLabel); cl->addWidget(programPassInput);
    cl->addLayout(ipLabelRow); cl->addWidget(programIpInput);
    cl->addWidget(portLabel); cl->addWidget(programPortInput);
    cl->addSpacing(4);
    cl->addWidget(genBtn);

    QHBoxLayout *cardRow = new QHBoxLayout;
    cardRow->addStretch(); cardRow->addWidget(card); cardRow->addStretch();

    QVBoxLayout *content = new QVBoxLayout;
    content->addStretch(2); content->addLayout(cardRow); content->addStretch(3);

    vl->addWidget(header); vl->addLayout(content, 1);
    stack->addWidget(programScreen);

    connect(genBtn, &QPushButton::clicked, [=]() { bounceBtn(genBtn); generateAndCopySketch(); });
}

void MainWindow::generateAndCopySketch()
{
    const QString ssid = programSsidInput->text().trimmed();
    const QString pass = programPassInput->text();
    if (ssid.isEmpty()) {
        programSsidInput->setFocus();
        return;
    }

    QFile file(":/robot_esp32.ino");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        showToast("Не удалось прочитать шаблон скетча");
        return;
    }
    QString sketch = QTextStream(&file).readAll();

    // Escape so the values stay valid inside a C string literal
    auto escape = [](QString s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    };

    sketch.replace(QRegularExpression(R"(SSID\s*=\s*".*?";)"),
                    QString("SSID            = \"%1\";").arg(escape(ssid)));
    sketch.replace(QRegularExpression(R"(PASSWORD\s*=\s*".*?";)"),
                    QString("PASSWORD        = \"%1\";").arg(escape(pass)));

    const QString ip = programIpInput->text().trimmed();
    if (!ip.isEmpty()) {
        sketch.replace(QRegularExpression(R"(CONTROLLER_IP\s*=\s*".*?";)"),
                        QString("CONTROLLER_IP   = \"%1\";").arg(escape(ip)));
    }

    bool portOk = false;
    const int port = programPortInput->text().trimmed().toInt(&portOk);
    if (portOk && port > 0 && port <= 65535) {
        sketch.replace(QRegularExpression(R"(CONTROLLER_PORT\s*=\s*\d+;)"),
                        QString("CONTROLLER_PORT = %1;").arg(port));
    }

    QGuiApplication::clipboard()->setText(sketch);
    showToast("Скетч скопирован в буфер обмена");
}

// ── Toast: полупрозрачное уведомление с анимацией fade-in/hold/fade-out ────────
void MainWindow::showToast(const QString &text)
{
    auto *toast = new QLabel(text, this);
    toast->setAlignment(Qt::AlignCenter);
    toast->setWordWrap(true);
    toast->setStyleSheet(
        "background:rgba(29,29,31,0.80); color:white; font-size:12px; font-weight:600;"
        "border-radius:10px; padding:10px 16px;"
    );
    toast->setAttribute(Qt::WA_TransparentForMouseEvents);

    const QRect area = centralWidget()->geometry();
    const int margin = 24;
    toast->setFixedWidth(qMin(360, area.width() - margin * 2));
    toast->adjustSize();
    toast->move(area.x() + (area.width() - toast->width()) / 2,
                area.y() + area.height() - toast->height() - margin);
    toast->raise();
    toast->show();

    auto *effect = new QGraphicsOpacityEffect(toast);
    toast->setGraphicsEffect(effect);
    effect->setOpacity(0);

    auto *seq = new QSequentialAnimationGroup(toast);

    auto *in = new QPropertyAnimation(effect, "opacity");
    in->setDuration(160);
    in->setStartValue(0.0); in->setEndValue(1.0);
    in->setEasingCurve(QEasingCurve::OutQuad);

    auto *hold = new QPropertyAnimation(effect, "opacity");
    hold->setDuration(1600);
    hold->setStartValue(1.0); hold->setEndValue(1.0);

    auto *out = new QPropertyAnimation(effect, "opacity");
    out->setDuration(350);
    out->setStartValue(1.0); out->setEndValue(0.0);
    out->setEasingCurve(QEasingCurve::OutCubic);

    seq->addAnimation(in);
    seq->addAnimation(hold);
    seq->addAnimation(out);
    connect(seq, &QSequentialAnimationGroup::finished, toast, &QLabel::deleteLater);
    seq->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── CONSTRUCTOR ────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Robot Controller");
    setWindowIcon(QIcon(makeRcPixmap(64)));
    resize(820, 620);
    // High enough that the tank screen's track cards (label + 200px slider +
    // value readout, stacked) never get squeezed below their natural height —
    // otherwise the value label overlaps the slider instead of sitting under it.
    setMinimumSize(680, 600);
    setStyleSheet(GLOBAL_SS);

    stack = new QStackedWidget(this);
    setCentralWidget(stack);

    setupMenu();
    setupTankUI();
    setupSteeringUI();
    setupProgramUI();

    stack->setCurrentWidget(menuScreen);
    qApp->installEventFilter(this);

    connect(&robot, &RobotController::connected,        [=]() { setConnected(true);  });
    connect(&robot, &RobotController::disconnected,     [=]() { isConnected = false; setConnected(false); });
    connect(&robot, &RobotController::connectionFailed, [=]() { flashConnectError(); });
    connect(&robot, &RobotController::batteryUpdated,
            [=](double voltage, int percent) { setBatteryLevel(voltage, percent); });

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
