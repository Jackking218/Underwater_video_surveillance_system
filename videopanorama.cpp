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
        m_multiPlayers[i] = nullptr;
        m_multiAudio[i] = nullptr;
        m_multiLabels[i] = nullptr;
    }

    // =========================================================
    // 1. 初始化全景播放器
    // =========================================================
    m_videoWidget = new QVideoWidget(ui->pagePanorama);
    m_videoWidget->setAspectRatioMode(Qt::IgnoreAspectRatio);
    m_videoWidget->show(); // 必须show

    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setVideoOutput(m_videoWidget);
    m_player->setAudioOutput(m_audioOutput);

    // 获取路径
    QString appDir = QCoreApplication::applicationDirPath();

    // 加载全景视频
    QString panoramaPath = appDir + "/videos/testVideo_32_9.mp4";
    if (QFile::exists(panoramaPath)) {
        m_player->setSource(QUrl::fromLocalFile(panoramaPath));
        m_audioOutput->setVolume(0);
        m_player->setLoops(QMediaPlayer::Infinite);
        m_player->play();
    } else {
        qDebug() << "[严重错误] 找不到全景视频：" << panoramaPath;
    }

    // =========================================================
    // 2. 初始化三摄分屏播放器
    // =========================================================
    QString camVideoPath = appDir + "/videos/test-2048_2048.mp4"; // 检查这里的文件名！

    // 检查分屏视频是否存在
    bool camVideoExists = QFile::exists(camVideoPath);
    if(!camVideoExists) qDebug() << "[严重错误] 找不到相机测试视频：" << camVideoPath;

    // 【关键修复】如果 pageMultiCam 在UI里没设布局，这里补一个，防止崩溃或显示错误
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

        // 创建视频窗口
        m_multiVideoWidgets[i] = new QVideoWidget(container);
        m_multiVideoWidgets[i]->setAspectRatioMode(Qt::KeepAspectRatio); // 保持正方形比例
        // m_multiVideoWidgets[i]->setStyleSheet("background-color: black;"); // 可选：强制黑底

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

        // 创建播放器
        m_multiPlayers[i] = new QMediaPlayer(this);
        m_multiAudio[i] = new QAudioOutput(this);
        m_multiPlayers[i]->setVideoOutput(m_multiVideoWidgets[i]);
        m_multiPlayers[i]->setAudioOutput(m_multiAudio[i]);
        m_multiAudio[i]->setVolume(0);

        // 设置源并预备播放
        if (camVideoExists) {
            m_multiPlayers[i]->setSource(QUrl::fromLocalFile(camVideoPath));
            m_multiPlayers[i]->setLoops(QMediaPlayer::Infinite);
            m_multiPlayers[i]->play(); // 默认播放，依靠 StackWidget 隐藏
        }
    }

    // // =========================================================
    // // 3. 初始化标尺
    // // =========================================================
    // m_ruler = new RulerWidget(ui->pagePanorama); // 确保父对象正确
    // m_ruler->setAttribute(Qt::WA_NativeWindow, false); // 确保关闭原生窗口属性
    // m_ruler->setRange(26.0);
    // m_ruler->hide();
    // m_ruler->raise();

    // 默认显示第一页
    ui->stackVideoMode->setCurrentIndex(0);
}

VideoPanorama::~VideoPanorama()
{
    delete ui;
}

void VideoPanorama::switchMode(int pageIndex)
{
    if (pageIndex == 0) {
        // --- 全景模式 ---
        m_isPanoramaMode = true;
        ui->stackVideoMode->setCurrentIndex(0);
        m_player->play();

        for(int i=0; i<3; i++) m_multiPlayers[i]->pause();

    } else {
        // --- 三摄模式 ---
        m_isPanoramaMode = false;

        // 恢复高度限制
        this->setMinimumHeight(0);
        this->setMaximumHeight(QWIDGETSIZE_MAX);

        ui->stackVideoMode->setCurrentIndex(1);
        m_player->pause();


        // 计算相机ID
        int startCamId = (pageIndex - 1) * 3 + 1;

        for(int i = 0; i < 3; i++)
        {
            int currentCamId = startCamId + i;

            if (currentCamId <= 13) {
                m_multiVideoWidgets[i]->parentWidget()->show(); // 显示整个容器
                m_multiLabels[i]->setText(QString("相机 %1").arg(currentCamId));
                m_multiPlayers[i]->play();
            } else {
                m_multiVideoWidgets[i]->parentWidget()->hide(); // 隐藏整个容器
                m_multiPlayers[i]->pause();
            }
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

// (辅助函数 copyResourceToTemp 已不再使用，若需要可保留)
QString VideoPanorama::copyResourceToTemp(QString resourcePath) { return ""; }
