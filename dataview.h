#ifndef DATAVIEW_H
#define DATAVIEW_H

#include <QWidget>
#include <QTimer>
#include <QtCharts>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QColorDialog>
#include "cameraclient.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DataView; }
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QtCharts;
QT_END_NAMESPACE

class DataView : public QWidget
{
    Q_OBJECT

public:
    explicit DataView(QWidget *parent = nullptr);
    ~DataView();
public slots:
    void switchTopage(int index);
signals:
    // mode: 0=全景, 1=相机分组1(1-3), 2=相机分组2(4-6) 3=相机分组3(7-9) 4=相机分组4(10-12) 5=相机分组5(13)
    void sigSwitchVideoMode(int mode);

private:
    Ui::DataView *ui;
    CameraClient *m_api;
    int m_currentVideoPageIndex = 0;
    // --- 模拟数据变量 ---
    double m_timeCount;     // 累计时间 (X轴)
    double m_velocity;      // 速度
    double m_acceleration;  // 加速度
    double m_displacement;  // 位移

    // --- 核心组件 ---
    QTimer *m_simTimer;     // 模拟定时器
    QTimer *m_saveTimer;    // 保存到数据库的定时器
    int m_saveIntervalSec;  // 保存间隔（秒）

    // --- 图表相关成员 ---
    // 左侧：速度曲线
    QChart *m_chartSpeed;
    QLineSeries *m_seriesSpeed;
    QValueAxis *m_axisX_Speed;
    QValueAxis *m_axisY_Speed;

    // 中间：位移曲线 (原frameAccel改为显示位移)
    QChart *m_chartDist;
    QLineSeries *m_seriesDist;
    QValueAxis *m_axisX_Dist;
    QValueAxis *m_axisY_Dist;

    // --- 初始化函数 ---
    void initChartStyles(); // 初始化图表样式
    void initTableStyles(); // 初始化表格样式

    // --- 数据库相关变量 ---
    QSqlDatabase m_db;          // 数据库连接对象

    // --- 历史记录分页变量 ---
    int m_historyCurrentPage;   // 当前第几页 (从1开始)
    int m_historyPageSize;      // 每页显示多少条 (例如 15条)
    int m_historyTotalPages;    // 总页数
    int m_historyTotalCount;    // 总数据量

    // --- 辅助函数声明 ---
    void initDatabase();        // 初始化数据库
    void recordDataToDb();      // 存数据
    void loadHistoryData();     // 读数据(刷新表格)
    void updateHistoryPageInfo(); // 更新页码显示

    QColor m_crosshairColor = Qt::red; // 默认红色
private slots:
    // --- 按钮槽函数 ---
    void on_btnSetFps_clicked();
    void on_btnSetExposure_clicked();
    void on_btnSetGain_clicked();
    void on_btnSetZoom_clicked();
    void on_btnSetFormat_clicked();

    // 触发类设置
    void on_btnSetTrigMode_clicked();
    void on_btnSetTrigSource_clicked();
    void on_btnSetTrigActive_clicked();

    // OSD类设置
    void on_btnSetCrosshair_clicked();
    void on_btnPickColor_clicked();
    void on_btnSnapshot_clicked();

    // --- 接口回调槽函数 ---
    void onApiResult(bool success, const QString &apiName, const QJsonObject &data, const QString &errorMsg);
    void onSnapshotReceived(const QPixmap &pixmap);
    void onServiceInfoReceived(const ServiceInfo &info);
    void onHealthInfoReceived(const HealthInfo &info);
};

#endif // DATAVIEW_H
