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
#include "streamvideowidget.h"

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
    m_panoramaClient = new WebSocketClient(this);

    // =========================================================
    // 2. 初始化三摄分屏播放器
    // =========================================================
    
    // 【关键修复】如果 pageMultiCam 在UI里没设布局，这里补一个
    if (!ui->pageMultiCam->layout()) {
        QHBoxLayout *layout = new QHBoxLayout(ui->pageMultiCam);
        layout->setSpacing(20);
        layout->setContentsMargins(20, 20, 20, 20);
    }

    m_camClients[0] = nullptr;

    for(int i = 1; i <= 13; i++) {
        m_camClients[i] = new WebSocketClient(this);
    }

    for (int i = 0; i < 3; i++) {
        // 创建垂直容器 (上视频，下标签)
        QWidget *container = new QWidget(ui->pageMultiCam);
        QVBoxLayout *vbox = new QVBoxLayout(container);
        vbox->setContentsMargins(0, 0, 0, 0);
        vbox->setSpacing(5);

        // 创建视频窗口 (StreamVideoWidget)
        m_multiVideoWidgets[i] = new StreamVideoWidget(container);

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
    
    m_cameraActiveFlags.resize(14);
    m_cameraActiveFlags.fill(false);

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
    for (int i = 0; i < 3; i++) {
        if (m_multiVideoWidgets[i]) {
            m_multiVideoWidgets[i]->setAcceptFrames(false);
            m_multiVideoWidgets[i]->clearFrame();
        }
    }
    m_cameraActiveFlags.fill(false);
    if (pageIndex == 0) {
        // 全景模式：只有 ID 0 活跃
        m_cameraActiveFlags[0] = true;
    } else {
        // 三摄模式：计算当前页的 3 个相机 ID
        int startCamId = (pageIndex - 1) * 3 + 1;
        for (int i = 0; i < 3; i++) {
            int camId = startCamId + i;
            if (camId <= 13) {
                m_cameraActiveFlags[camId] = true;
            }
        }
    }

    if (m_cameraActiveFlags[0]) {
        m_isPanoramaMode = true;
        ui->stackVideoMode->setCurrentIndex(0);
        // 连接全景...
    } else{
        m_isPanoramaMode = false;
        this->setMinimumHeight(0);
        this->setMaximumHeight(QWIDGETSIZE_MAX);

        ui->stackVideoMode->setCurrentIndex(1);
        int startCamId = (pageIndex -1)*3+1;
        for(int i=0; i<3; i++)
        {
            int currentCamId = startCamId + i;

            if (currentCamId <= 13) {
                // 相机存在：显示窗口
                m_multiVideoWidgets[i]->parentWidget()->show();
                m_multiVideoWidgets[i]->show();
                m_multiLabels[i]->setText(QString("相机 %1").arg(currentCamId));

                m_multiVideoWidgets[i]->setAcceptFrames(true);
                m_multiVideoWidgets[i]->setFrameToken(++m_frameTokenCounter);

                // 【重要】在建立新连接前，先清空画面，防止上一页的残影
                // 只有当即将建立连接时，这一步才会被后面的视频流覆盖
                // 如果该相机没信号，这里就保持黑屏，非常合理
                QPixmap blackPix(m_multiVideoWidgets[i]->size());
                blackPix.fill(Qt::black);
                m_multiVideoWidgets[i]->setPixmap(blackPix);

            } else {
                // 相机不存在 (14, 15)：隐藏窗口
                m_multiVideoWidgets[i]->parentWidget()->hide();
            }
        }
        // 强制重置布局比例
        QHBoxLayout *layout = qobject_cast<QHBoxLayout*>(ui->pageMultiCam->layout());
        if (layout) {
            layout->setStretch(0, 1);
            layout->setStretch(1, 1);
            layout->setStretch(2, 1);
            layout->activate();
        }
    }
    for (int camId = 1; camId <= 13; camId++) {
        WebSocketClient *client = m_camClients[camId];
        if (!client) continue;

        if (m_cameraActiveFlags[camId]) {
            // ---> 情况 A: 该相机需要显示 <---

            // 1. 断开旧信号 (防止重复绑定)
            client->disconnect();

            // 2. 获取地址并连接
            // 优化：如果已经连接且 URL 没变，可以不调 connectToServer
            QString url = getCamWsUrl(camId);
            client->connectToServer(url);

            // 3. 计算它应该显示在哪个窗口 (0, 1, 2)
            // 算法：(camId - 1) % 3
            int widgetIndex = (camId - 1) % 3;

            if (widgetIndex >= 0 && widgetIndex < 3) {
                StreamVideoWidget *widget = m_multiVideoWidgets[widgetIndex];
                quint64 token = widget->frameToken();
                connect(client, &WebSocketClient::sendBynariesToPlayer, widget,
                        [widget, token](const QByteArray &data){
                            if (widget->frameToken() == token) {
                                widget->receiveFrameData(data);
                            }
                        });

            qDebug() << "相机" << camId << "绑定到窗口" << widgetIndex;
            }

        } else {
            client->disconnect();
            client->disconnectFromServer();
        }
    }

    // 触发重绘
    QResizeEvent event(size(), size());
    resizeEvent(&event);
}

void VideoPanorama::pauseFramesFor(int ms)
{
    for (int i = 0; i < 3; i++) {
        if (m_multiVideoWidgets[i]) {
            m_multiVideoWidgets[i]->pauseFramesFor(ms);
        }
    }
    if (m_videoWidget) {
        m_videoWidget->pauseFramesFor(ms);
    }
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

// 判断某个相机是否有视频源需要播放
bool VideoPanorama::isCameraAvailable(int camId)
{
    // --- 目前阶段：只有相机1可用 ---
    if (camId == 1) return true;

    // --- 未来阶段：所有都可用 ---
    // return true;
    return false;
}
