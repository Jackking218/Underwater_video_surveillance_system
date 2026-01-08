#ifndef HEADERBAR_H
#define HEADERBAR_H

#include <QWidget>

namespace Ui {
class HeaderBar;
}

class HeaderBar : public QWidget
{
    Q_OBJECT

public:
    explicit HeaderBar(QWidget *parent = nullptr);
    ~HeaderBar();

signals:
    void sigPageChanged(int index); // 发送页面索引信号 (0=预览, 1=回放, 2=设置, 3=历史)
private:
    Ui::HeaderBar *ui;
};

#endif // HEADERBAR_H
