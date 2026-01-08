#ifndef RULERWIDGET_H
#define RULERWIDGET_H

#include <QWidget>
#include <QMouseEvent>
#include <QPainter>

class RulerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit RulerWidget(QWidget *parent = nullptr);
    // 设置量程（默认26米）
    void setRange(double meters);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    double m_maxMeters;    // 最大量程 (26)
    bool m_isLocked;       // 是否已锁定位置
    int m_currentMouseX;   // 当前鼠标X坐标
    int m_lockedX;         // 锁定的X坐标

    // 辅助：将像素转换为米（用于显示）
    double pixelsToMeters(int px);

signals:
};

#endif // RULERWIDGET_H
