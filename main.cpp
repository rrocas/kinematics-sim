#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QElapsedTimer>
#include <QTimer>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLayout>
#include "qcustomplot.h"  // third-party lib for the live graphs at the bottom

// The simulation box (where the ball lives) is smaller than the window
// the rest of the space is taken by the side panel and graphs
constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

constexpr int BOX_WIDTH = 500;
constexpr int BOX_HEIGHT = 300;

constexpr int BALL_RADIUS = 8;

class Ball {
public:
  float x, y;     // position (pixels, top-left origin)
  float radius;
  float vx, vy;   // velocity (pixels/second)
  float ax, ay;   // acceleration (pixels/second²) — reset and re-applied every frame by input

  Ball(float x, float y, float r)
    : x(x), y(y), radius(r), vx(0), vy(0), ax(0), ay(0) {}

  void update(float dt, float w, float h){
    // Euler integration — each frame, velocity changes by acceleration, position changes by velocity
    // simple and slightly inaccurate at large dt, but fine for this
    vx += ax * dt;
    vy += ay * dt;
    x += vx * dt;
    y += vy * dt;

    // Wall collisions: perfectly elastic (no energy loss), just flip the relevant velocity component
    if (x >= w - radius) { x = w - radius; vx = -vx; }
    if (x <= radius)     { x = radius;     vx = -vx; }
    if (y >= h - radius) { y = h - radius; vy = -vy; }
    if (y <= radius)     { y = radius;     vy = -vy; }
  }
};

class BoxWidget : public QWidget {
public:
  QElapsedTimer clock;  // measures real wall time since start, used to compute dt
  float lastTime = 0;

  QTimer *timer;  // drives the simulation loop at ~60fps

  Ball ball;
  float a_rate = 300;  // acceleration magnitude in px/s², adjustable via slider

  // Key state — true while the key is held down, false otherwise
  bool w = false;
  bool a = false;
  bool s = false;
  bool d = false;
  bool q = false;      // brake: decelerate along current direction
  bool e = false;      // boost: accelerate along current direction
  bool space = false;  // instant stop

  BoxWidget(QWidget *parent = nullptr)
    : QWidget(parent), ball(BOX_WIDTH/2.0f, BOX_HEIGHT/2.0f, BALL_RADIUS) {
      setFixedSize(BOX_WIDTH, BOX_HEIGHT);
      setFocusPolicy(Qt::StrongFocus);  // required to receive keyboard events
      clock.start();

      timer = new QTimer(this);

      connect(timer, &QTimer::timeout, this, [this]() {

        // Reset acceleration — input re-applies it from scratch each frame
        ball.ax = 0;
        ball.ay = 0;

        // WASD: cardinal direction acceleration
        if (w) ball.ay = -a_rate;
        if (s) ball.ay = a_rate;
        if (a) ball.ax = -a_rate;
        if (d) ball.ax = a_rate;

        // Q: brake — force pointing opposite to current velocity
        // normalize velocity to get direction, then flip it
        if (q) {
          float length = sqrt(pow(ball.vx, 2) + pow(ball.vy, 2));
          if (length != 0) {
            ball.ax += -(ball.vx / length) * a_rate;
            ball.ay += -(ball.vy / length) * a_rate;
          }
        }

        // E: boost — same as brake but in the same direction as velocity
        if (e) {
          float length = sqrt(pow(ball.vx, 2) + pow(ball.vy, 2));
          if (length != 0) {
            ball.ax += (ball.vx / length) * a_rate;
            ball.ay += (ball.vy / length) * a_rate;
          }
        }

        // Space: bypass physics entirely and zero out velocity immediately
        if (space) {
          ball.vx = 0;
          ball.vy = 0;
        }

        // dt = time since last frame in seconds (real, not hardcoded)
        float current = clock.elapsed() / 1000.0f;
        float dt = current - lastTime;
        lastTime = current;

        ball.update(dt, width(), height());

        update();  // schedules a repaint → calls paintEvent
      });

      timer->start(16);  // ~16ms per tick = ~60fps target
  }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter p(this);

    p.fillRect(rect(), Qt::black);

    // Box border
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#D0D0D0"), 1));
    p.drawRect(0, 0, width()-1, height()-1);

    // Ball — drawEllipse takes top-left corner so we offset by radius to center it
    p.setBrush(Qt::green);
    p.setPen(QPen(Qt::green, 2));
    p.drawEllipse(
        ball.x - ball.radius,
        ball.y - ball.radius,
        ball.radius * 2,
        ball.radius * 2
    );
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_W) w = true;
    if (event->key() == Qt::Key_S) s = true;
    if (event->key() == Qt::Key_A) a = true;
    if (event->key() == Qt::Key_D) d = true;
    if (event->key() == Qt::Key_Q) q = true;
    if (event->key() == Qt::Key_E) e = true;
    if (event->key() == Qt::Key_Space) space = true;
  }

  void keyReleaseEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_W) w = false;
    if (event->key() == Qt::Key_S) s = false;
    if (event->key() == Qt::Key_A) a = false;
    if (event->key() == Qt::Key_D) d = false;
    if (event->key() == Qt::Key_Q) q = false;
    if (event->key() == Qt::Key_E) e = false;
    if (event->key() == Qt::Key_Space) space = false;
  }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setStyleSheet("background-color: #161B22;");  // dark GitHub-ish theme

    // Main layout: top row (box + side panel) stacked above bottom row (graphs)
    QVBoxLayout *mainLayout = new QVBoxLayout(&window);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    QWidget *topRow = new QWidget();
    QHBoxLayout *topLayout = new QHBoxLayout(topRow);
    topLayout->setContentsMargins(10, 10, 10, 10);
    topLayout->setSpacing(10);

    BoxWidget *box = new BoxWidget();

    // Side panel: live numeric readouts + controls hint + sliders
    QWidget *sidePanel = new QWidget();
    sidePanel->setStyleSheet("color: #D0D0D0; font-family: monospace;");
    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);

    QLabel *speed = new QLabel("speed: 0");
    QLabel *vx    = new QLabel("vx: 0");
    QLabel *vy    = new QLabel("vy: 0");
    QLabel *fps   = new QLabel("fps: 0");
    QLabel *controls = new QLabel("WASD: move\nQ: brake\nE: boost\nSPACE: stop");

    sideLayout->addWidget(fps);
    sideLayout->addWidget(speed);
    sideLayout->addWidget(vx);
    sideLayout->addWidget(vy);
    sideLayout->addWidget(controls);

    QLabel *lblAcc = new QLabel("acceleration: 300");
    QSlider *sliderAcc = new QSlider(Qt::Horizontal);
    sliderAcc->setRange(50, 1000);
    sliderAcc->setValue(300);

    QLabel *lblRadius = new QLabel("ball radius: 8");
    QSlider *sliderRadius = new QSlider(Qt::Horizontal);
    sliderRadius->setRange(2, 50);
    sliderRadius->setValue(8);

    // FPS counter: averages over 0.5s to avoid the number flickering every frame
    static int frameCount = 0;
    static double lastFpsTime = 0;

    // Piggyback on the simulation timer to update all the side panel labels
    QObject::connect(box->timer, &QTimer::timeout, [=]() mutable {
        double s = sqrt(pow(box->ball.vx, 2) + pow(box->ball.vy, 2));
        speed->setText("speed: " + QString::number(s, 'f', 1));
        vx->setText("vx: " + QString::number(box->ball.vx, 'f', 1));
        vy->setText("vy: " + QString::number(box->ball.vy, 'f', 1));

        frameCount++;
        double now = box->clock.elapsed() / 1000.0;
        if (now - lastFpsTime >= 0.5) {
            fps->setText("fps: " + QString::number(frameCount / (now - lastFpsTime), 'f', 0));
            frameCount = 0;
            lastFpsTime = now;
        }
    });

    // Sliders directly mutate the live simulation state
    QObject::connect(sliderAcc, &QSlider::valueChanged, [=](int val) {
        box->a_rate = val;
        lblAcc->setText("acceleration: " + QString::number(val));
    });

    QObject::connect(sliderRadius, &QSlider::valueChanged, [=](int val) {
        box->ball.radius = val;
        lblRadius->setText("ball radius: " + QString::number(val));
    });

    sideLayout->addWidget(lblAcc);
    sideLayout->addWidget(sliderAcc);
    sideLayout->addWidget(lblRadius);
    sideLayout->addWidget(sliderRadius);

    topLayout->addWidget(box);
    topLayout->addWidget(sidePanel, 1);  // stretch factor 1 = takes remaining width

    // === BOTTOM: four live scrolling graphs, all sharing the same timer ===
    QWidget *bottom = new QWidget();
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottom);

    // --- Graph 1: scalar speed |v| over time ---
    QCustomPlot *speed_plot = new QCustomPlot(bottom);
    speed_plot->addGraph();
    speed_plot->setBackground(QBrush(QColor("#161B22")));
    speed_plot->axisRect()->setBackground(QBrush(QColor("#161B22")));
    speed_plot->graph(0)->setPen(QPen(QColor("#D0D0D0"), 2));
    speed_plot->xAxis->setTickLabelColor(QColor("#8B949E"));
    speed_plot->yAxis->setTickLabelColor(QColor("#8B949E"));
    speed_plot->xAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::DotLine));
    speed_plot->yAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::DotLine));
    speed_plot->xAxis->setLabel("t (s)");
    speed_plot->yAxis->setLabel("speed (px)");
    speed_plot->xAxis->setLabelColor(QColor("#D0D0D0"));
    speed_plot->yAxis->setLabelColor(QColor("#D0D0D0"));
    speed_plot->xAxis->setRange(0, 10);
    speed_plot->yAxis->setRange(0, 600);
    speed_plot->setMinimumHeight(150);
    speed_plot->setMinimumWidth(200);

    QObject::connect(box->timer, &QTimer::timeout, [=]() {
        // t comes from the real clock now, not t += 0.016
        double t = box->clock.elapsed() / 1000.0;
        double speed = sqrt(pow(box->ball.vx, 2) + pow(box->ball.vy, 2));
        speed_plot->graph(0)->addData(t, speed);
        speed_plot->graph(0)->data()->removeBefore(t - 10);  // keep a 10s sliding window
        speed_plot->xAxis->setRange(t, 10, Qt::AlignRight);  // scroll x axis with time
        speed_plot->graph(0)->rescaleValueAxis();
        speed_plot->replot();
    });

    // --- Graph 2: ball X position over time ---
    QCustomPlot *x_pos_plot = new QCustomPlot(bottom);
    x_pos_plot->addGraph();
    x_pos_plot->setBackground(QBrush(QColor("#161B22")));
    x_pos_plot->axisRect()->setBackground(QBrush(QColor("#161B22")));
    x_pos_plot->graph(0)->setPen(QPen(QColor("#58A6FF"), 2));  // blue
    x_pos_plot->xAxis->setTickLabelColor(QColor("#8B949E"));
    x_pos_plot->yAxis->setTickLabelColor(QColor("#8B949E"));
    x_pos_plot->xAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::DotLine));
    x_pos_plot->yAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::DotLine));
    x_pos_plot->xAxis->setRange(0, 10);
    x_pos_plot->yAxis->setRange(0, BOX_WIDTH);
    x_pos_plot->setMinimumHeight(150);
    x_pos_plot->setMinimumWidth(200);
    x_pos_plot->xAxis->setLabel("t (s)");
    x_pos_plot->yAxis->setLabel("Position in X axis (px)");
    x_pos_plot->xAxis->setLabelColor(QColor("#D0D0D0"));
    x_pos_plot->yAxis->setLabelColor(QColor("#D0D0D0"));

    QObject::connect(box->timer, &QTimer::timeout, [=]() {
        double t = box->clock.elapsed() / 1000.0;
        x_pos_plot->graph(0)->addData(t, box->ball.x);
        x_pos_plot->graph(0)->data()->removeBefore(t - 10);
        x_pos_plot->xAxis->setRange(t, 10, Qt::AlignRight);
        x_pos_plot->replot();
    });

    // --- Graph 3: ball Y position over time ---
    // Y is flipped (*-1) because screen coords go downward, this makes "up" look like up on the graph
    QCustomPlot *y_pos_plot = new QCustomPlot(bottom);
    y_pos_plot->addGraph();
    y_pos_plot->setBackground(QBrush(QColor("#161B22")));
    y_pos_plot->axisRect()->setBackground(QBrush(QColor("#161B22")));
    y_pos_plot->graph(0)->setPen(QPen(QColor("#3FB950"), 2));  // green
    y_pos_plot->xAxis->setTickLabelColor(QColor("#8B949E"));
    y_pos_plot->yAxis->setTickLabelColor(QColor("#8B949E"));
    y_pos_plot->xAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::DotLine));
    y_pos_plot->yAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::DotLine));
    y_pos_plot->xAxis->setRange(0, 10);
    y_pos_plot->yAxis->setRange(0, -BOX_HEIGHT);
    y_pos_plot->setMinimumHeight(150);
    y_pos_plot->setMinimumWidth(200);
    y_pos_plot->xAxis->setLabel("t (s)");
    y_pos_plot->yAxis->setLabel("Position in Y axis (px)");
    y_pos_plot->xAxis->setLabelColor(QColor("#D0D0D0"));
    y_pos_plot->yAxis->setLabelColor(QColor("#D0D0D0"));

    QObject::connect(box->timer, &QTimer::timeout, [=]() {
        double t = box->clock.elapsed() / 1000.0;
        y_pos_plot->graph(0)->addData(t, box->ball.y * -1);  // flip Y
        y_pos_plot->graph(0)->data()->removeBefore(t - 10);
        y_pos_plot->xAxis->setRange(t, 10, Qt::AlignRight);
        y_pos_plot->replot();
    });

    // --- Graph 4: vx and vy together — blue=vx, green=vy ---
    // vy is also flipped for the same reason as the Y position graph
    QCustomPlot *vel_plot = new QCustomPlot(bottom);
    vel_plot->addGraph();  // graph(0) = vx
    vel_plot->addGraph();  // graph(1) = vy (flipped)
    vel_plot->setBackground(QBrush(QColor("#161B22")));
    vel_plot->axisRect()->setBackground(QBrush(QColor("#161B22")));
    vel_plot->graph(0)->setPen(QPen(QColor("#58A6FF"), 2));  // blue = vx
    vel_plot->graph(1)->setPen(QPen(QColor("#3FB950"), 2));  // green = vy
    vel_plot->xAxis->setTickLabelColor(QColor("#8B949E"));
    vel_plot->yAxis->setTickLabelColor(QColor("#8B949E"));
    vel_plot->xAxis->setLabelColor(QColor("#8B949E"));
    vel_plot->yAxis->setLabelColor(QColor("#8B949E"));
    vel_plot->xAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::SolidLine));
    vel_plot->yAxis->grid()->setPen(QPen(QColor("#21262D"), 1, Qt::SolidLine));
    vel_plot->xAxis->setLabel("t (s)");
    vel_plot->yAxis->setLabel("v (px/s)");
    vel_plot->yAxis->setRange(-600, 600);
    vel_plot->setMinimumHeight(150);
    vel_plot->setMinimumWidth(200);

    QObject::connect(box->timer, &QTimer::timeout, [=]() {
        double t = box->clock.elapsed() / 1000.0;
        vel_plot->graph(0)->addData(t, box->ball.vx);
        vel_plot->graph(1)->addData(t, box->ball.vy * -1);  // flip vy to match visual intuition
        vel_plot->graph(0)->data()->removeBefore(t - 10);
        vel_plot->graph(1)->data()->removeBefore(t - 10);
        vel_plot->xAxis->setRange(t, 10, Qt::AlignRight);
        vel_plot->graph(0)->rescaleValueAxis(true);
        vel_plot->replot();
    });

    bottomLayout->addWidget(speed_plot);
    bottomLayout->addWidget(x_pos_plot);
    bottomLayout->addWidget(y_pos_plot);
    bottomLayout->addWidget(vel_plot);

    mainLayout->addWidget(topRow);
    mainLayout->addWidget(bottom, 1);  // stretch factor 1 = graphs take all remaining height

    window.resize(WINDOW_WIDTH, WINDOW_HEIGHT);
    window.show();
    return app.exec();  // hands control to Qt's event loop — everything runs from here
}