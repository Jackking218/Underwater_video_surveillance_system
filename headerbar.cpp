#include "headerbar.h"
#include "ui_headerbar.h"
#include <QTimer>
#include <QDateTime>
#include <QButtonGroup>

HeaderBar::HeaderBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HeaderBar)
{
    ui->setupUi(this);
    //设置定时器更新时间
    QTimer* timer = new QTimer(this);
    connect(timer,&QTimer::timeout,this,[=](){
        QDateTime current = QDateTime::currentDateTime();
        ui->lbDate->setText(current.toString("yyyy年MM月dd日 dddd"));
    });
    timer->start(1000);

    //初始化按钮互斥
    QButtonGroup *group = new QButtonGroup(this);
    group->addButton(ui->btnPreview,0);
    group->addButton(ui->btnPlayback,1);
    group->addButton(ui->btnSettings,2);
    group->addButton(ui->btnHistory,3);
    group->setExclusive(true); // 开启互斥
    ui->btnPreview->setChecked(true); // 默认选中第一个

    connect(group, &QButtonGroup::idClicked, this, [=](int id){
       emit sigPageChanged(id);
    });

}

HeaderBar::~HeaderBar()
{
    delete ui;
}
