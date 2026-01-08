#ifndef CAMERACLIENT_H
#define CAMERACLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QPixmap>

class CameraClient : public QObject
{
    Q_OBJECT
public:
    explicit CameraClient(QObject *parent = nullptr);
    virtual ~CameraClient();
    void setBaseUrl(const QString &url); // 设置服务地址

    // --- 基础接口 ---
    void getServiceConfig(); // GET
    void getHealthStatus();      // GET /health
    void getSnapshot();      // GET /snapshot
    QString getMjpegStreamUrl() const;  // GET /mjpeg

    // --- 参数控制接口 (封装通用的 POST 请求) ---
    // 设置帧率
    void setFrameRate(bool isGlobal, int cameraId, double fps);
    // 设置曝光
    void setExposure(bool isGlobal, int cameraId, float exposureUs);
    // 设置增益
    void setGain(bool isGlobal, int cameraId, float gain);
    // 设置十字光标
    void setCrosshair(bool isGlobal, int cameraId, bool enabled,
                      int x = 0, int y = 0, int size = 20,
                      const QColor &color = Qt::red, int thickness = 2);

    // 设置像素格式
    void setPixelFormat(bool isGlobal, int cameraId, const QString &symbol);
    // 设置触发格式
    void setTriggerMode(bool isGlobal, int cameraId, const QString &symbol);
    // 设置触发源
    void setTriggerSource(bool isGlobal, int cameraId, const QString &symbol);
    // 设置触发沿
    void setTriggerActivation(bool isGlobal, int cameraId, const QString &symbol);
    // 设置触发因子
    void setZoom(bool isGlobal, int cameraId, float factor);
    // 指定相机抓拍到服务器路径
    void saveSnapshotToServer(int cameraId, const QString &path);

signals:
    void serviceConfigReceived(const QJsonObject &config); // GET / 响应
    void healthStatusReceived(const QJsonObject &status);  // GET /health 响应
    void snapshotReceived(const QPixmap &pixmap);          // GET /snapshot 响应 (图片)

    // --- 控制结果信号 ---
    // success: 请求是否成功
    // apiName: 调用的接口名 (用于区分是哪个设置返回的)
    // resultData: 服务器返回的JSON数据 (包含 applied, failed 等信息)
    // errorMsg: 如果失败，具体的错误信息
    void controlResult(bool success, const QString &apiName, const QJsonObject &resultData, const QString &errorMsg);
private:
    QNetworkAccessManager *m_manager;
    QString m_baseUrl;
    // 辅助函数：构造基础 JSON (包含 scope 和 camera_id)
    QJsonObject createBaseJson(bool isGlobal, int cameraId);
    // 辅助函数：统一发送 POST 请求
    void sendPostRequest(const QString &endpoint, const QJsonObject &json);
};

#endif // CAMERACLIENT_H
