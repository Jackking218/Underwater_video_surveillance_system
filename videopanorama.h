#ifndef VIDEOPANORAMA_H
#define VIDEOPANORAMA_H

#include <QWidget>
#include <QFile>
#include <QLabel>
#include <QTemporaryFile>
#include <QResizeEvent>

#include "websocketclient.h"
#include "streamvideowidget.h"

namespace Ui {
class VideoPanorama;
}

class VideoPanorama : public QWidget
{
    Q_OBJECT

public:
    explicit VideoPanorama(QWidget *parent = nullptr);
    ~VideoPanorama();

    // 根据宽度计算并更新高度
    void adjustHeightToWidth();
    bool isCameraAvailable(int camId);
    void pauseFramesFor(int ms);

public slots:
    // 接收模式切换信号 (0=全景, >0=相机组页码)
    void switchMode(int pageIndex);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    Ui::VideoPanorama *ui;

    //--定义播放器组件----
    StreamVideoWidget *m_videoWidget; // 全景视频显示窗口

    // 视频原始尺寸 (用于计算比例)
    const double VIDEO_WIDTH = 7680.0;
    const double VIDEO_HEIGHT = 1600.0;

    // 当前模式记录
    bool m_isPanoramaMode;

    StreamVideoWidget *m_multiVideoWidgets[3]; // 3个显示窗口
    QLabel *m_multiLabels[3];             // 3个相机号标签
    
    // WebSocket Clients
    // 索引 1-13 对应相机 1-13, 0 可用于全景或其他用途
    //WebSocketClient *m_camClients[14];
    WebSocketClient *m_camClients[14];
    WebSocketClient *m_panoramaClient; // 全景相机 (独立)

    // 辅助函数：获取相机的 WebSocket URL
    QString getCamWsUrl(int camId);
    //相机标志位
    QVector<bool> m_cameraActiveFlags;
    quint64 m_frameTokenCounter = 0;
};

#endif // VIDEOPANORAMA_H
