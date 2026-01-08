#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //启动全屏
    this->showFullScreen();
    this->setMinimumSize(1280, 720);

    connect(ui->widgetHeader,&HeaderBar::sigPageChanged,this,[=](int index){
        ui->widgetDataView->switchTopage(index);

    });

    connect(ui->widgetDataView, &DataView::sigSwitchVideoMode,
            ui->widgetVide0panorama, &VideoPanorama::switchMode);
    
    ui->widgetRuler->setRange(26.0);
    ui->widgetRuler->show();
}

MainWindow::~MainWindow()
{
    delete ui;
}


