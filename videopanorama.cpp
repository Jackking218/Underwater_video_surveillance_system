#include "videopanorama.h"
#include "ui_videopanorama.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>
#include <QShowEvent>
#include <QPoint>
#include <QDebug>
#include <QSettings>

VideoPanorama::VideoPanorama(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::VideoPanorama)
    , m_isPanoramaMode(true)
{
    ui->setupUi(this);

    // 0. 基础设置
    this->setMinimumHeight(200);

    // 初始化指针数组
    for(int i=0; i<3; i++) {
        m_multiVideoWidgets[i] = nullptr;
        m_multiLabels[i] = nullptr;
    }

    // =========================================================
    // 1. 初始化全景播放器 (StreamVideoWidget)
    // =========================================================
    m_videoWidget = new StreamVideoWidget(ui->pagePanorama);
    // m_videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio); // StreamVideoWidget 内部自适应
    m_videoWidget->show(); // 必须show

    // TODO: 全景视频 WebSocket Client 初始化
    // m_panoramaClient = new WebSocketClient(this);
    m_Client = new WebSocketClient(this);

    // =========================================================
    // 2. 初始化三摄分屏播放器
    // =========================================================
    
    // 【关键修复】如果 pageMultiCam 在UI里没设布局，这里补一个
    if (!ui->pageMultiCam->layout()) {
        QHBoxLayout *layout = new QHBoxLayout(ui->pageMultiCam);
        layout->setSpacing(20);
        layout->setContentsMargins(20, 20, 20, 20);
    }

    for (int i = 0; i < 3; i++) {
        // 创建垂直容器 (上视频，下标签)
        QWidget *container = new QWidget(ui->pageMultiCam);
        QVBoxLayout *vbox = new QVBoxLayout(container);
        vbox->setContentsMargins(0, 0, 0, 0);
        vbox->setSpacing(5);

        // 创建视频窗口 (StreamVideoWidget)
        m_multiVideoWidgets[i] = new StreamVideoWidget(container);
        // m_multiVideoWidgets[i]->setStyleSheet("background-color: black;"); // 默认已黑底

        // 创建标签
        m_multiLabels[i] = new QLabel(container);
        m_multiLabels[i]->setAlignment(Qt::AlignCenter);
        m_multiLabels[i]->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");
        m_multiLabels[i]->setFixedHeight(20);

        // 加入布局 (Stretch: 视频1，标签0)
        vbox->addWidget(m_multiVideoWidgets[i], 1);
        vbox->addWidget(m_multiLabels[i], 0);

        // 将容器放入页面
        ui->pageMultiCam->layout()->addWidget(container);
    }
    

    // 默认显示第一页
    ui->stackVideoMode->setCurrentIndex(0);
}

VideoPanorama::~VideoPanorama()
{
    delete ui;
}

QString VideoPanorama::getCamWsUrl(int camId)
{
    // 从 config.ini 读取配置
        // 路径：可执行文件同级目录下的 config.ini
        QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
        QSettings settings(configPath, QSettings::IniFormat);

        // 键名规则：Cameras/Cam1, Cameras/Cam2 ...
        QString key = QString("Cameras/Cam%1").arg(camId);

        // 默认地址 (使用 Demo 地址)
        QString defaultUrl = "ws://192.168.100.5:8020/ws/subscribe";

        // 读取配置，如果不存在则使用默认值
        QString url = settings.value(key, defaultUrl).toString();

        // qDebug() << "Get Cam" << camId << "URL:" << url;
        return url;
}

void VideoPanorama::switchMode(int pageIndex)
{
    // 1. 【先断开旧连接】防止信号重复叠加
    // 断开 m_Client 的所有信号连接
    m_Client->disconnect();

    if (pageIndex == 0) {
        // --- 全景模式 ---
        m_isPanoramaMode = true;
        ui->stackVideoMode->setCurrentIndex(0);

    } else {
        // --- 三摄模式 ---
        m_isPanoramaMode = false;

        // 恢复高度
        this->setMinimumHeight(0);
        this->setMaximumHeight(QWIDGETSIZE_MAX);

        ui->stackVideoMode->setCurrentIndex(1);

        // 计算当前页显示的相机ID
        int startCamId = (pageIndex - 1) * 3 + 1;

        for(int i = 0; i < 3; i++)
        {
            int currentCamId = startCamId + i;

            if (currentCamId <= 13) {
                // 显示窗口
                m_multiVideoWidgets[i]->parentWidget()->show();
                m_multiVideoWidgets[i]->show(); // 【确保 Widget 本身也 Show】
                m_multiLabels[i]->setText(QString("相机 %1").arg(currentCamId));

                // 红色高亮演示
                m_multiLabels[i]->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");
            } else {
                m_multiVideoWidgets[i]->parentWidget()->hide();
            }
        }

        // ==========================================================
        // 【核心修改】连接 WebSocket 数据流 -> 视频窗口
        // ==========================================================

        // 假设后端只有 1 个测试视频流，我们把它连到当前页的第 1 个窗口上 (即 m_multiVideoWidgets[0])
        // 这样点击"相机1"时，相机1会有画面。点击"相机4"时，相机4会有画面。

        // 1. 连接服务器 (测试地址)
        // 这里的 url 根据您的后端实际情况填写
        QString wsUrl = QString("ws://127.0.0.1:8020/ws/subscribe/camera/%1").arg(pageIndex);
        // 或者固定测试地址:
        // QString wsUrl = "ws://192.168.100.5:8020/ws/subscribe";

        m_Client->connectToServer(wsUrl);

        // 1. 先断开旧连接 (防止多次 connect 导致闪烁或崩溃)
        m_Client->disconnect(this);
        for(int i=0; i<3; i++) {
        m_Client->disconnect(m_multiVideoWidgets[i]);
        }

        // 2. 建立新连接 (直接连到 Widget 的槽函数，不要用 Lambda)
        // 将数据流连到第一个窗口 (相机1)
        connect(m_Client, &WebSocketClient::sendBynariesToPlayer,
             m_multiVideoWidgets[0], &StreamVideoWidget::receiveFrameData);

        // 设置标签颜色
        m_multiLabels[0]->setStyleSheet("color: red; font-size: 14px; font-weight: bold;");
        // 强制刷新布局
        if (ui->pageMultiCam->layout()) {
            ui->pageMultiCam->layout()->activate();
        }
    }

    // 强制重绘
    QResizeEvent event(size(), size());
    resizeEvent(&event);
}


void VideoPanorama::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    int containerW = ui->stackVideoMode->width();
    int containerH = ui->stackVideoMode->height();
    if(containerW <= 0) return;

    if (m_isPanoramaMode) {
        // === 全景模式布局 ===

        // 2. 计算视频安全区域
        QRect safeArea(0, 0, containerW, containerH);
        QSize originalSize(VIDEO_WIDTH, VIDEO_HEIGHT);
        QSize fittedSize = originalSize.scaled(safeArea.size(), Qt::KeepAspectRatio);

        int videoX = (safeArea.width() - fittedSize.width()) / 2;
        int videoY = (safeArea.height() - fittedSize.height()) / 2;

        if(m_videoWidget) {
            m_videoWidget->setGeometry(videoX, videoY, fittedSize.width(), fittedSize.height());
        }

    } else {
        // === 多摄模式布局 ===
        // 由 HBoxLayout 自动管理，无需代码干预
    }
}

void VideoPanorama::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    // 延时刷新布局，解决刚启动尺寸不对的问题
    QTimer::singleShot(50, this, [=](){
        if (this->isVisible()) {
            QResizeEvent re(size(), size());
            resizeEvent(&re);
        }
    });
}

void VideoPanorama::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

void VideoPanorama::adjustHeightToWidth()
{
    // 如果需要维持比例，可以在这里实现
    // 目前保持原样
}
