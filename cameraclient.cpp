#include "cameraclient.h"
#include <QJsonArray>

CameraClient::CameraClient(QObject *parent)
    : QObject{parent}
{
    m_manager = new QNetworkAccessManager(this);
}

CameraClient::~CameraClient()
{}

/**
* @brief 获取 MJPEG 流的完整 URL
* @return 适合直接塞给 QWebEngineView 或 VLC 的 URL
*/
QString CameraClient::getMjpegStreamUrl() const
{
    return m_baseUrl + "/mjpeg";
}

/**
 * @brief 设置API的基础地址
 * @param url 例如 "http://192.168.1.100:8080"
 */
void CameraClient::setBaseUrl(const QString &url)
{
    m_baseUrl = url;
    if(m_baseUrl.endsWith("/"))
    {
        m_baseUrl.chop(1);
    }
}

// ==========================================
// 辅助函数
// ==========================================
QJsonObject CameraClient::createBaseJson(bool isGlobal, int cameraId)
{
    QJsonObject json;
    if(isGlobal){
        json["scope"] = "global";
    }else{
        json["scope"] = "camera";
        json["camera_id"] = cameraId;
    }
    return json;
}

void CameraClient::sendPostRequest(const QString &endpoint, const QJsonObject &json)
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonDocument doc(json);
    QByteArray postData = doc.toJson();

    QNetworkReply *reply = m_manager->post(request, postData);

    // 使用 Lambda 处理响应
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater(); // 稍后自动释放

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray respData = reply->readAll();
            QJsonDocument respDoc = QJsonDocument::fromJson(respData);
            emit controlResult(true, endpoint, respDoc.object(), "OK");
        } else {
            // 处理 HTTP 错误 (400, 404, 500, 503)
            QString errStr = QString("HTTP Error %1: %2").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(reply->errorString());
            emit controlResult(false, endpoint, QJsonObject(), errStr);
        }
    });
}

// ==========================================
// GET 接口实现
// ==========================================
/**
 * @brief GET / 获取服务信息与当前配置
 */
void CameraClient::getServiceConfig()
{
    QNetworkRequest request(QUrl(m_baseUrl + "/"));
    QNetworkReply *reply = m_manager->get(request);

    connect(reply, &QNetworkReply::finished, this ,[=](){
        reply->deleteLater();
        if(reply->error() == QNetworkReply::NoError){
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            emit serviceConfigReceived(doc.object());
        }else{
            emit controlResult(false, "/", QJsonObject(), reply->errorString());
        }
    });

}

/**
 * @brief GET /health 获取运行状态与统计
 */
void CameraClient::getHealthStatus()
{
    QNetworkRequest request(QUrl(m_baseUrl + "/health"));
    QNetworkReply *reply = m_manager->get(request);

    connect(reply, &QNetworkReply::finished ,this ,[=](){
        reply->deleteLater();
        if(reply->error() == QNetworkReply::NoError){
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            emit healthStatusReceived(doc.object());
        }else{
            emit controlResult(false, "/health", QJsonObject(), reply->errorString());
        }
    });
}

/**
 * @brief GET /snapshot 获取最新一帧 JPEG 抓图
 */
void CameraClient::getSnapshot()
{
    QNetworkRequest request(QUrl(m_baseUrl + "/snapshot"));
    QNetworkReply *reply = m_manager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data, "JPEG")) {
                emit snapshotReceived(pixmap);
            } else {
                emit controlResult(false, "/snapshot", QJsonObject(), "Failed to load JPEG data");
            }
        } else {
            emit controlResult(false, "/snapshot", QJsonObject(), reply->errorString());
        }
    });
}

// ==========================================
// POST 控制接口实现
// ==========================================
/**
 * @brief POST /control/frame_rate 设置帧率
 */
void CameraClient::setFrameRate(bool isGlobal, int cameraId, double fps)
{
    QJsonObject json = createBaseJson(isGlobal,cameraId);
    json["fps"] = fps;
    sendPostRequest("/control/frame_rate", json);
}

/**
 * @brief POST /control/exposure 设置曝光时间 (us)
 */
void CameraClient::setExposure(bool isGlobal, int cameraId, float exposureUs)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["exposure_us"] = exposureUs;
    sendPostRequest("/control/exposure", json);
}

/**
 * @brief POST /control/gain 设置增益
 */
void CameraClient::setGain(bool isGlobal, int cameraId, float gain)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["gain"] = gain;
    sendPostRequest("/control/gain", json);
}

/**
 * @brief POST /control/pixel_format 设置像素格式
 * @param symbol 如 "Mono8", "BayerRG8", "RGB8"
 */
void CameraClient::setPixelFormat(bool isGlobal, int cameraId, const QString &symbol)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["symbol"] = symbol;
    sendPostRequest("/control/pixel_format", json);
}

/**
 * @brief POST /control/trigger_mode 设置触发模式
 * @param symbol "Off" 或 "On"
 */
void CameraClient::setTriggerMode(bool isGlobal, int cameraId, const QString &symbol)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["symbol"] = symbol; // "Off", "On"
    sendPostRequest("/control/trigger_mode", json);
}

/**
 * @brief POST /control/trigger_source 设置触发源
 * @param symbol "Software", "Line0", "Line1"
 */
void CameraClient::setTriggerSource(bool isGlobal, int cameraId, const QString &symbol)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["symbol"] = symbol; // "Software", "Line0", "Line1"
    sendPostRequest("/control/trigger_source", json);
}

/**
 * @brief POST /control/trigger_activation 设置触发沿
 * @param symbol "RisingEdge", "FallingEdge"
 */
void CameraClient::setTriggerActivation(bool isGlobal, int cameraId, const QString &symbol)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["symbol"] = symbol; // "RisingEdge", "FallingEdge"
    sendPostRequest("/control/trigger_activation", json);
}

/**
 * @brief POST /control/zoom 设置缩放因子
 * @param factor >0, 建议 0.1~3.0
*/
void CameraClient::setZoom(bool isGlobal, int cameraId, float factor)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["factor"] = factor;
    sendPostRequest("/control/zoom", json);
}

/**
 * @brief POST /control/crosshair 设置十字光标
 * @param color 文档提示BGR格式，本函数内部会自动转换
 */
void CameraClient::setCrosshair(bool isGlobal, int cameraId, bool enabled, int x, int y, int size, const QColor &color, int thickness)
{
    QJsonObject json = createBaseJson(isGlobal, cameraId);
    json["enabled"] = enabled;
    if(enabled){
        json["x"] = x;
        json["y"] = y;
        json["size"] = size;
        json["thickness"] = thickness;

        QJsonArray colorArr;
        colorArr.append(color.blue());
        colorArr.append(color.green());
        colorArr.append(color.red());
        json["color"] = colorArr;
    }

    sendPostRequest("/control/crosshair", json);
}


void CameraClient::saveSnapshotToServer(int cameraId, const QString &path)
{
    QJsonObject json;
    json["cameraId"] = cameraId;
    json["path"] = path;

    sendPostRequest("control/snapshot", json);
}
