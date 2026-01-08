#ifndef VIDEOPANORAMA_H
#define VIDEOPANORAMA_H

#include <QWidget>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QFile>
#include <QLabel>
#include <QTemporaryFile>
#include <QResizeEvent>


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
    QMediaPlayer *m_player;
    QVideoWidget *m_videoWidget; // 视频显示窗口 (黑窗口)
    QAudioOutput *m_audioOutput; // 音频输出

    // 视频原始尺寸 (用于计算比例)
    const double VIDEO_WIDTH = 7680.0;
    const double VIDEO_HEIGHT = 1600.0;

    // 辅助函数：将资源文件复制到本地临时目录
    QString copyResourceToTemp(QString resourcePath);

    // 当前模式记录
    bool m_isPanoramaMode;
    // bool m_rulerChecked; // 已移除

    QMediaPlayer *m_multiPlayers[3];      // 3个播放器
    QVideoWidget *m_multiVideoWidgets[3]; // 3个显示窗口
    QLabel *m_multiLabels[3];             // 3个相机号标签
    QAudioOutput *m_multiAudio[3];        // 3个音频输出
};

#endif // VIDEOPANORAMA_H
