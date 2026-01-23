#include "cameraclient.h"
#include <QJsonArray>
#include <QVariant>

static PipelineStatus parsePipelineStatus(const QJsonObject &json) {
    PipelineStatus status;
    status.running = json["running"].toBool();
    status.last_seq = json["last_seq"].toVariant().toLongLong();
    status.last_ts_ns = json["last_ts_ns"].toVariant().toLongLong();
    // last_error can be null, toString() handles it gracefully by returning empty string
    status.last_error = json["last_error"].isNull() ? "" : json["last_error"].toString();
    return status;
}

static ServiceConfig parseServiceConfig(const QJsonObject &json) {
    ServiceConfig config;
    config.camera_count = json["camera_count"].toInt();
    config.target_fps = json["target_fps"].toInt();
    config.jpeg_quality = json["jpeg_quality"].toInt();
    config.pause_when_no_clients = json["pause_when_no_clients"].toBool();
    config.source_video = json["source_video"].isNull() ? "" : json["source_video"].toString();
    return config;
}

static ServiceInfo parseServiceInfo(const QJsonObject &json) {
    ServiceInfo info;
    info.service = json["service"].toString();
    QJsonArray routes = json["routes"].toArray();
    for (const auto &route : routes) {
        info.routes.append(route.toString());
    }
    info.clients = json["clients"].toInt();
    info.fps_ema = json["fps_ema"].toDouble();
    info.encoded_seq = json["encoded_seq"].toVariant().toLongLong();
    info.pipeline = parsePipelineStatus(json["pipeline"].toObject());
    info.config = parseServiceConfig(json["config"].toObject());
    return info;
}

static HealthInfo parseHealthInfo(const QJsonObject &json) {
    HealthInfo info;
    info.clients = json["clients"].toInt();
    info.fps_ema = json["fps_ema"].toDouble();
    info.encoded_seq = json["encoded_seq"].toVariant().toLongLong();
    info.encoded_ts_ns = json["encoded_ts_ns"].toVariant().toLongLong();
    info.last_error = json["last_error"].isNull() ? "" : json["last_error"].toString();
    info.pipeline = parsePipelineStatus(json["pipeline"].toObject());
    return info;
}

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
        json["camera_id"] = cameraId-1;
    }
    return json;
}

void CameraClient::sendPostRequest(const QString &endpoint, const QJsonObject &json)
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    qDebug()<<"url:"<<url;
    QJsonDocument doc(json);
    qDebug() << "发送 POST 数据:" << doc.toJson(QJsonDocument::Compact); // 加上这一行
    QByteArray postData = doc.toJson();
    qDebug()<<"json wei"<<"/n"<<postData;
    QNetworkReply *reply = m_manager->post(request, postData);

    // 使用 Lambda 处理响应
    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater(); // 稍后自动释放

        // 获取 HTTP 状态码
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray respData = reply->readAll();
            QJsonDocument respDoc = QJsonDocument::fromJson(respData);
            emit controlResult(true, endpoint, respDoc.object(), "OK");
        } else {
            // 1. 读取服务器返回的报错详情 (这是解决 422 的关键)
            QByteArray serverResp = reply->readAll();
            qDebug() << "--------------------------------------------------";
            qDebug() << "POST 请求失败: " << endpoint;
            qDebug() << "HTTP 状态码: " << statusCode;
            qDebug() << "服务器返回详情: " << serverResp; // <--- 重点看这行输出！
            qDebug() << "--------------------------------------------------";

            // 2. 构造错误信息
            QString errStr = QString("HTTP Error %1: %2").arg(statusCode).arg(reply->errorString());

            // 3. 发送信号通知 UI
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
            QJsonObject obj = doc.object();
            emit serviceInfoReceived(parseServiceInfo(obj));
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
            QJsonObject obj = doc.object();
            emit healthInfoReceived(parseHealthInfo(obj));
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
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError && statusCode == 200) {
            QByteArray data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(data, "JPEG")) {
                emit snapshotReceived(pixmap);
            } else {
                emit controlResult(false, "/snapshot", QJsonObject(), "Failed to load JPEG data");
            }
        } else {
            QString errStr = (statusCode == 503) ? "Service Unavailable (No Frame)" : reply->errorString();
            emit controlResult(false, "/snapshot", QJsonObject(), errStr);
        }
    });
}

/**
 * @brief GET /mjpeg 获取最新一帧 JPEG 抓图
 */

void CameraClient::startMjpegStream()
{
    if (m_mjpegReply) {
        return;
    }
    QUrl url(getMjpegStreamUrl());
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "multipart/x-mixed-replace");
    m_mjpegBuffer.clear();
    m_mjpegReply = m_manager->get(request);

    connect(m_mjpegReply, &QNetworkReply::readyRead, this, &CameraClient::handleMjpegReadyRead);
    connect(m_mjpegReply, &QNetworkReply::errorOccurred, this, [=](QNetworkReply::NetworkError error){
        emit controlResult(false, "/mjpeg", QJsonObject(), m_mjpegReply ? m_mjpegReply->errorString() : QStringLiteral("stream error"));
    });
    connect(m_mjpegReply, &QNetworkReply::finished, this, [=]() {
        if (m_mjpegReply) {
            m_mjpegReply->deleteLater();
            m_mjpegReply = nullptr;
        }
    });
}

void CameraClient::stopMjpegStream()
{
    if (!m_mjpegReply) {
        return;
    }
    disconnect(m_mjpegReply, nullptr, this, nullptr);
    m_mjpegReply->abort();
    m_mjpegReply->deleteLater();
    m_mjpegReply = nullptr;
    m_mjpegBuffer.clear();
}

void CameraClient::handleMjpegReadyRead()
{
    if (!m_mjpegReply) {
        return;
    }
    m_mjpegBuffer.append(m_mjpegReply->readAll());

    while (true) {
        int boundaryIdx = m_mjpegBuffer.indexOf("--frame");
        if (boundaryIdx < 0) {
            break;
        }
        if (boundaryIdx > 0) {
            m_mjpegBuffer.remove(0, boundaryIdx);
        }

        int boundaryLineEnd = m_mjpegBuffer.indexOf("\r\n");
        if (boundaryLineEnd < 0) {
            break;
        }
        int headersStart = boundaryLineEnd + 2;
        int headersEnd = m_mjpegBuffer.indexOf("\r\n\r\n", headersStart);
        if (headersEnd < 0) {
            break;
        }

        QByteArray headersBytes = m_mjpegBuffer.mid(headersStart, headersEnd - headersStart);
        QString headersStr = QString::fromLatin1(headersBytes);
        int contentLength = -1;
        const auto lines = headersStr.split("\r\n");
        for (const QString &line : lines) {
            if (line.startsWith("Content-Length:", Qt::CaseInsensitive)) {
                contentLength = line.section(':', 1, 1).trimmed().toInt();
                break;
            }
        }

        if (contentLength <= 0) {
            emit controlResult(false, "/mjpeg", QJsonObject(), QStringLiteral("missing Content-Length"));
            // 丢弃到头结束，尝试继续
            m_mjpegBuffer.remove(0, headersEnd + 4);
            continue;
        }

        // 跳过头部
        m_mjpegBuffer.remove(0, headersEnd + 4);

        if (m_mjpegBuffer.size() < contentLength) {
            // 数据未到齐，等待下一次 readyRead
            break;
        }

        QByteArray imageData = m_mjpegBuffer.left(contentLength);
        m_mjpegBuffer.remove(0, contentLength);

        // 可选跳过帧末尾 CRLF
        if (m_mjpegBuffer.startsWith("\r\n")) {
            m_mjpegBuffer.remove(0, 2);
        }

        QPixmap pixmap;
        if (pixmap.loadFromData(imageData, "JPEG")) {
            emit mjpegFrameReceived(pixmap);
        } else {
            emit controlResult(false, "/mjpeg", QJsonObject(), QStringLiteral("failed to decode JPEG frame"));
        }
        // 继续循环解析可能已到达的下一帧
    }
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
    qDebug() << "Sending POST POST /control/gain";
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
