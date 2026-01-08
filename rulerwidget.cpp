#include "rulerwidget.h"

RulerWidget::RulerWidget(QWidget *parent)
    : QWidget{parent}
{
    m_maxMeters = 26.0;
    m_isLocked = false;
    m_currentMouseX = 0;
    m_lockedX = 0;

    setMouseTracking(true);
    setFixedHeight(50);

    // this->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    // 改为普通子窗口，去掉 ToolTip 标志
    this->setWindowFlags(Qt::WindowDoesNotAcceptFocus);

    setAttribute(Qt::WA_TranslucentBackground, true); // 允许背景透明
    setAutoFillBackground(false);
}

void RulerWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // painter.fillRect(rect(), QColor(0, 0, 0, 100));

    // 2. 绘制基准线 (白色)
    QPen basePen(Qt::white, 2);
    painter.setPen(basePen);
    painter.drawLine(0, 0, w, 0); // 顶线

    // 3. 绘制刻度
    double pixelsPerMeter = (double)w / m_maxMeters;
    QFont font("Microsoft YaHei", 9, QFont::Bold);
    painter.setFont(font);

    for (int i = 0; i <= m_maxMeters; ++i) {
        int x = (int)(i * pixelsPerMeter);

        if (i % 5 == 0) {
            // 长刻度 (亮青色)
            painter.setPen(QPen(QColor(0, 255, 255), 2));
            painter.drawLine(x, 0, x, 25);
            // 文字 (白色)
            painter.setPen(Qt::white);
            QString text = QString::number(i);
            int textW = painter.fontMetrics().horizontalAdvance(text);
            // 文字稍微往下挪一点
            painter.drawText(x - textW/2, 42, text);
        } else {
            // 短刻度 (白色半透明)
            painter.setPen(QPen(QColor(255, 255, 255, 180), 1));
            painter.drawLine(x, 0, x, 12);
        }
    }

    // ---------------------------------------------------------
    // 3. 绘制红色光标线 (保持红色高亮)
    // ---------------------------------------------------------
    // (这部分逻辑保持不变，只需确保颜色鲜艳)
    int drawX = m_isLocked ? m_lockedX : m_currentMouseX;

    if (m_isLocked || rect().contains(mapFromGlobal(QCursor::pos()))) {
        QPen redPen(Qt::red, 2); // 红色，2像素
        painter.setPen(redPen);
        painter.drawLine(drawX, 0, drawX, h);

        // 数值显示背景框 (为了看清字，给文字加个半透明黑底)
        double currentMeters = pixelsToMeters(drawX);
        QString valStr = QString::number(currentMeters, 'f', 1) + "m";

        // 计算文字区域
        int textW = painter.fontMetrics().horizontalAdvance(valStr) + 10;
        QRect textRect(drawX + 5, h - 20, textW, 20);

        // 边界检查：防止文字画出屏幕右侧
        if (drawX + 5 + textW > w) {
            textRect.moveLeft(drawX - 5 - textW);
        }

        painter.fillRect(textRect, QColor(0, 0, 0, 200)); // 深黑底
        painter.setPen(Qt::white);
        painter.drawText(textRect, Qt::AlignCenter, valStr);
    }
}

void RulerWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_isLocked) {
        m_currentMouseX = event->pos().x();
        update(); // 触发重绘
    }
    QWidget::mouseMoveEvent(event);
}

void RulerWidget::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        if(m_isLocked)
        {
            // 如果已经锁定了，再次点击则更新锁定位置
            m_lockedX = event->pos().x();
        }else{
            m_isLocked = true;
            m_lockedX = event->pos().x();
        }
        update();
    }else if(event->button() == Qt::RightButton)
    {
        m_isLocked = false;
        update();
    }
    QWidget::mousePressEvent(event);
}

void RulerWidget::setRange(double meters)
{
    m_maxMeters = meters;
    update(); // 触发重绘，更新刻度
}

double RulerWidget::pixelsToMeters(int px)
{
    return (double)px / width() * m_maxMeters;
}
