#include "dataview.h"
#include "ui_dataview.h"
#include <QRandomGenerator>
#include <QTimer>
#include <QtCharts/QChartView>
#include <QDebug>
#include "Database/dbmanager.h"
#include <QSettings>
#include <QCoreApplication>

DataView::DataView(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DataView)
    , m_timeCount(0)
    , m_velocity(0)
    , m_acceleration(0)
    , m_displacement(0)
{
    ui->setupUi(this);

    initTableStyles();  // 准备表格
    initChartStyles();  // 准备图表

    // 读取保存间隔（秒）默认 5s
    {
        QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";
        QSettings settings(configPath, QSettings::IniFormat);
        m_saveIntervalSec = settings.value("Database/SaveIntervalSeconds", 5).toInt();
        if (m_saveIntervalSec <= 0) m_saveIntervalSec = 5;
    }

    // 2. 设置定时器
    // 2.1 模拟刷新：每500毫秒刷新一次数据用于图表
    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, [=](){

        // --- A. 模拟物理计算 ---
        // 随机生成加速度 (-2 到 2 之间)
        double randomAcc = QRandomGenerator::global()->bounded(-20, 20) / 10.0;
        m_acceleration = randomAcc;

        // 速度 v = v0 + at
        m_velocity += m_acceleration;
        if(m_velocity < 0) m_velocity = 0;
        if(m_velocity > 100) m_velocity = 100;

        // 位移 s = s0 + vt
        double deltaS = m_velocity * 0.5; // 0.5秒的时间片
        m_displacement += deltaS;
        // m_position = m_displacement + 0; // 假设初始位置是10

        m_timeCount += 0.5; // X轴时间增加

        // --- B. 更新表格数据 ---
        // 假设表格顺序：速度、加速度、位移、位置
        if(ui->tableWidget->rowCount() >= 3) {
            ui->tableWidget->item(0, 1)->setText(QString::number(m_velocity, 'f', 1));
            ui->tableWidget->item(1, 1)->setText(QString::number(m_acceleration, 'f', 1));
            ui->tableWidget->item(2, 1)->setText(QString::number(m_displacement, 'f', 1));
        }

        // --- C. 更新图表数据 ---
        // 1. 速度图表
        m_seriesSpeed->append(m_timeCount, m_velocity);
        if(m_timeCount > 20) {
            m_axisX_Speed->setMax(m_timeCount); // 让最大值跟随时间增加
            m_axisX_Speed->setMin(0);           // 强制最小值永远保持 0
        }

        // 2. 位移图表
        m_seriesDist->append(m_timeCount, m_displacement);
        if(m_timeCount > 20) {
            m_axisX_Dist->setMax(m_timeCount);  // 让最大值跟随时间增加
            m_axisX_Dist->setMin(0);            // 强制最小值永远保持 0
        }
        // 动态调整Y轴范围 (因为位移是一直增加的)
        if(m_displacement > m_axisY_Dist->max()) {
            m_axisY_Dist->setMax(m_displacement + 50);
        }

        // 刷新仅更新图表与表格展示，不直接入库
    });

    // 2.2 入库定时器：每 m_saveIntervalSec 秒写数据库一次
    m_saveTimer = new QTimer(this);
    m_saveTimer->setInterval(m_saveIntervalSec * 1000);
    connect(m_saveTimer, &QTimer::timeout, this, [=](){
        recordDataToDb();
    });

    // 3. 启动定时器
    m_simTimer->start(500);
    m_saveTimer->start();

    /**********************历史信息统计***************************************/
    // 初始化分页变量
    m_historyCurrentPage = 1;
    m_historyPageSize = 10; // 假设一页显示10行，根据您UI的高度调整

    // 启动数据库
    initDatabase();

    // 连接历史翻页按钮信号
    connect(ui->btnHisPrev, &QPushButton::clicked, this, [=](){
        if(m_historyCurrentPage > 1) {
            m_historyCurrentPage--;
            loadHistoryData();
        }
    });

    connect(ui->btnHisNext, &QPushButton::clicked, this, [=](){
        if(m_historyCurrentPage < m_historyTotalPages) {
            m_historyCurrentPage++;
            loadHistoryData();
        }
    });


    /**********************CambtnPage***************************************/
    // 1. 创建按钮组 (方便管理互斥和ID)
    QButtonGroup *camGroup = new QButtonGroup(this);

    // 2. 添加按钮并分配 ID
    // ID 规则：0=全景，1=相机1，2=相机2...
    camGroup->addButton(ui->btnPanorama, 0);
    camGroup->addButton(ui->btnCam1, 1);
    camGroup->addButton(ui->btnCam2, 2);
    camGroup->addButton(ui->btnCam3, 3);
    camGroup->addButton(ui->btnCam4, 4);
    camGroup->addButton(ui->btnCam5, 5);
    camGroup->addButton(ui->btnCam6, 6);
    camGroup->addButton(ui->btnCam7, 7);
    camGroup->addButton(ui->btnCam8, 8);
    camGroup->addButton(ui->btnCam9, 9);
    camGroup->addButton(ui->btnCam10, 10);
    camGroup->addButton(ui->btnCam11, 11);
    camGroup->addButton(ui->btnCam12, 12);
    camGroup->addButton(ui->btnCam13, 13);
    camGroup->setExclusive(true); // 互斥
    ui->btnPanorama->setChecked(true); // 默认全景

    // 3. 连接信号
    connect(camGroup, &QButtonGroup::idClicked, this, [=](int id){
        if (id == 0) {
            emit sigSwitchVideoMode(0);
        } else {
            // 计算页码 (1-3 -> 页1, 4-6 -> 页2 ...)  算法： (id - 1) / 3 + 1
            int pageIndex = (id - 1) / 3 + 1;
            emit sigSwitchVideoMode(pageIndex);
        }
    });


    /**********************settingPage***************************************/
    // ===============================================
    // 1. 初始化相机接口客户端
    // ===============================================
    m_api = new CameraClient(this);
    m_api->setBaseUrl("http://127.0.0.1:8080");

    // 2. 连接通用结果信号 -> 处理回调函数
    connect(m_api, &CameraClient::controlResult, this, &DataView::onApiResult);

    // 3. 连接抓拍图片信号 -> 显示图片
    connect(m_api, &CameraClient::snapshotReceived, this, &DataView::onSnapshotReceived);

    // 4. 初始化UI控件的默认值 (可选，提升体验)
    ui->dsbFps->setRange(1, 120); ui->dsbFps->setValue(25);
    ui->dsbExposure->setRange(100, 1000000); ui->dsbExposure->setValue(20000);
    ui->dsbGain->setRange(0, 24); ui->dsbGain->setValue(0);
    ui->comboPixelFormat->addItems({"Mono8", "BayerRG8", "RGB8"});
    ui->lineEdit->setPlaceholderText("D:/img/1.jpg");
    ui->sbTargetCamId->setRange(1, 13);
    ui->sbTargetCamId->setValue(1);
}

DataView::~DataView()
{
    delete ui;
}

// 初始化表格 (保持您之前的样式，这里只做数据结构准备)
void DataView::initTableStyles()
{
    ui->tableWidget->horizontalHeader()->setVisible(false);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->setShowGrid(false);
    ui->tableWidget->setFocusPolicy(Qt::NoFocus);
    ui->tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 预设4行数据
    QStringList names = {"速度 (m/s)", "加速度 (m/s²)", "位移 (m)"};
    ui->tableWidget->setRowCount(3);
    ui->tableWidget->setColumnCount(2);

    for(int i=0; i<3; i++) {
        QTableWidgetItem *nameItem = new QTableWidgetItem(names[i]);
        nameItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        nameItem->setForeground(QBrush(QColor(0, 230, 255))); // 青色文字
        ui->tableWidget->setItem(i, 0, nameItem);

        QTableWidgetItem *valItem = new QTableWidgetItem("0.0");
        valItem->setTextAlignment(Qt::AlignCenter);
        valItem->setForeground(QBrush(Qt::white)); // 白色数值
        ui->tableWidget->setItem(i, 1, valItem);
    }
}

// 初始化图表 (核心部分)
void DataView::initChartStyles()
{
    // ================= 创建速度图表 (左) =================
    m_seriesSpeed = new QLineSeries();
    m_seriesSpeed->setName("速度");

    m_chartSpeed = new QChart();
    m_chartSpeed->addSeries(m_seriesSpeed);
    m_chartSpeed->legend()->hide(); // 隐藏图例，显得简洁
    m_chartSpeed->setBackgroundRoundness(0);
    m_chartSpeed->setMargins(QMargins(0,0,0,0)); // 填满
    m_chartSpeed->setBackgroundVisible(false); // 透明背景，透出Frame的颜色

    // 配置X轴
    m_axisX_Speed = new QValueAxis();
    m_axisX_Speed->setRange(0, 20);
    m_axisX_Speed->setTitleText("时间（s）");
    m_axisX_Speed->setTitleBrush(Qt::cyan);    // 设置标题颜色
    m_axisX_Speed->setTitleVisible(true);      // 确保可见
    m_axisX_Speed->setLabelFormat("%.0f"); // 不显示小数
    m_axisX_Speed->setLabelsColor(Qt::white);
    m_axisX_Speed->setGridLineColor(QColor(255, 255, 255, 30)); // 淡淡的网格
    m_chartSpeed->addAxis(m_axisX_Speed, Qt::AlignBottom);
    m_seriesSpeed->attachAxis(m_axisX_Speed);

    // 配置Y轴
    m_axisY_Speed = new QValueAxis();
    m_axisY_Speed->setRange(0, 100);
    m_axisY_Speed->setTitleText("速度 (m/s)");  // 设置标题文字
    m_axisY_Speed->setTitleBrush(Qt::cyan);    // 设置标题颜色
    m_axisY_Speed->setTitleVisible(true);      // 确保可见
    m_axisY_Speed->setLabelsColor(Qt::cyan);
    m_axisY_Speed->setGridLineColor(QColor(255, 255, 255, 30));
    m_chartSpeed->addAxis(m_axisY_Speed, Qt::AlignLeft);
    m_seriesSpeed->attachAxis(m_axisY_Speed);

    // 放入界面 (frameSpeed 里的 chartPlaceholder1)
    QChartView *chartView1 = new QChartView(m_chartSpeed);
    chartView1->setRenderHint(QPainter::Antialiasing); // 抗锯齿
    chartView1->setStyleSheet("background: transparent"); // 视图也透明

    // 将图表视图加入到 frameSpeed 的布局中
    QVBoxLayout *layout1 = new QVBoxLayout(ui->chartPlaceholder1); // 这里替换为您实际的占位符名字
    layout1->setContentsMargins(0,0,0,0);
    layout1->addWidget(chartView1);


    // ================= 创建位移图表  =================
    m_seriesDist = new QLineSeries();
    m_seriesDist->setColor(QColor(255, 0, 255)); // 位移曲线换个紫色

    m_chartDist = new QChart();
    m_chartDist->addSeries(m_seriesDist);
    m_chartDist->legend()->hide();
    m_chartDist->setBackgroundVisible(false);
    m_chartDist->setMargins(QMargins(0,0,0,0));

    // X轴
    m_axisX_Dist = new QValueAxis();
    m_axisX_Dist->setRange(0, 20);
    m_axisX_Dist->setTitleText("时间（s）");     // 设置标题文字
    m_axisX_Dist->setTitleBrush(Qt::cyan);
    m_axisX_Dist->setTitleVisible(true);
    m_axisX_Dist->setLabelsColor(Qt::white);
    m_axisX_Dist->setGridLineColor(QColor(255, 255, 255, 30));
    m_chartDist->addAxis(m_axisX_Dist, Qt::AlignBottom);
    m_seriesDist->attachAxis(m_axisX_Dist);

    // Y轴
    m_axisY_Dist = new QValueAxis();
    m_axisY_Dist->setRange(0, 100); // 初始范围，后面会动态变大
    m_axisY_Dist->setTitleText("位移 (m)");     // 设置标题文字
    m_axisY_Dist->setTitleBrush(Qt::cyan);
    m_axisY_Dist->setTitleVisible(true);
    m_axisY_Dist->setLabelsColor(Qt::cyan);
    m_axisY_Dist->setGridLineColor(QColor(255, 255, 255, 30));
    m_chartDist->addAxis(m_axisY_Dist, Qt::AlignLeft);
    m_seriesDist->attachAxis(m_axisY_Dist);

    // 放入界面 (frameAccel 里的 chartPlaceholder2)
    QChartView *chartView2 = new QChartView(m_chartDist);
    chartView2->setRenderHint(QPainter::Antialiasing);
    chartView2->setStyleSheet("background: transparent");

    QVBoxLayout *layout2 = new QVBoxLayout(ui->chartPlaceholder2); // 这里替换为您中间占位符的名字
    layout2->setContentsMargins(0,0,0,0);
    layout2->addWidget(chartView2);
}

void DataView::switchTopage(int index)
{
    ui->stackeContent->setCurrentIndex(index);

    QString titleText;

    switch(index)
    {
    case 0:
        titleText = "船只信息统计";
            break;
    case 1:
        titleText = "视频回放";
        break;
    case 2:
        titleText = "系统总体参数";
        break;
    case 3:
        titleText = "历史数据查询";
        break;
    default:
        titleText = "船只统计信息";
        break;
    }

    ui->label_2->setText(titleText);

    if(index == 3) {
        // 重置到第一页并加载
        m_historyCurrentPage = 1;
        loadHistoryData();
    }
}

void DataView::initDatabase()
{
    if (!DBManager::instance().connectToDb()) {
        qDebug() << "数据库连接失败，请检查配置";
        // 这里可以弹窗提示用户
    }
}

void DataView::recordDataToDb()
{
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    bool ok = DBManager::instance().insertLog(timeStr, m_velocity, m_acceleration, m_displacement);
    // 如果当前处于“历史数据查询”页，则在成功插入后刷新表格
    if (ok && ui->stackeContent->currentIndex() == 3) {
        loadHistoryData();
    }
}

void DataView::loadHistoryData()
{
    // 1. 获取总数
    m_historyTotalCount = DBManager::instance().getTotalCount();

    // 计算总页数 (向上取整)
    m_historyTotalPages = (m_historyTotalCount + m_historyPageSize - 1) / m_historyPageSize;
    if(m_historyTotalPages == 0) m_historyTotalPages = 1;

    int offset = (m_historyCurrentPage - 1) * m_historyPageSize;
    QSqlQuery query = DBManager::instance().getHistoryLogs(m_historyPageSize, offset);

    // 3. 填充表格
    ui->tableHistory->setRowCount(0); // 清空旧数据
    // 设置列头 (只需要设置一次，也可以放在初始化里)
    ui->tableHistory->setColumnCount(4);
    ui->tableHistory->setHorizontalHeaderLabels(QStringList() << "时间" << "速度" << "加速度" << "位移" );
    // 让列宽自适应
    ui->tableHistory->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    int row = 0;
    while(query.next()) {
        ui->tableHistory->insertRow(row);

        // 逐列填入
        QDateTime dt = query.value(0).toDateTime();
        QString dtStr = dt.isValid() ? dt.toLocalTime().toString("yyyy-MM-dd HH:mm:ss")
                                     : query.value(0).toString();
        ui->tableHistory->setItem(row, 0, new QTableWidgetItem(dtStr)); // 时间
        ui->tableHistory->setItem(row, 1, new QTableWidgetItem(QString::number(query.value(1).toDouble(), 'f', 1))); // 速度
        ui->tableHistory->setItem(row, 2, new QTableWidgetItem(QString::number(query.value(2).toDouble(), 'f', 1))); // 加速度
        ui->tableHistory->setItem(row, 3, new QTableWidgetItem(QString::number(query.value(3).toDouble(), 'f', 1))); // 位移
        // ui->tableHistory->setItem(row, 4, new QTableWidgetItem(QString::number(queryData.value(4).toDouble(), 'f', 1))); // 位置

        // 设置居中
        for(int c=0; c<4; c++) {
            ui->tableHistory->item(row, c)->setTextAlignment(Qt::AlignCenter);
            ui->tableHistory->item(row, c)->setForeground(Qt::white); // 确保文字白色
        }
        row++;
    }

    // 4. 更新底部页码标签
    updateHistoryPageInfo();
}

void DataView::updateHistoryPageInfo()
{
    // 更新 Label 显示，例如 "2 / 15"
    ui->lblPageInfo->setText(QString("%1 / %2").arg(m_historyCurrentPage).arg(m_historyTotalPages));

    // 控制按钮状态 (第一页不能点上一页，最后一页不能点下一页)
    ui->btnHisPrev->setEnabled(m_historyCurrentPage > 1);
    ui->btnHisNext->setEnabled(m_historyCurrentPage < m_historyTotalPages);
}

// 接口调用结果回调
void DataView::onApiResult(bool success, const QString &apiName, const QJsonObject &data, const QString &errorMsg)
{
    QString title = success ? "操作成功" : "操作失败";
    QString msg;

    if (success) {
        // 解析成功信息，例如: {"applied": [1,2], "failed": []}
        QJsonArray applied = data["applied"].toArray();
        msg = QString("接口调用成功！\nAPI: %1\n生效相机数量: %2").arg(apiName).arg(applied.size());
    } else {
        // 解析失败信息
        msg = QString("接口调用失败！\nAPI: %1\n错误信息: %2").arg(apiName).arg(errorMsg);
    }

    // 弹窗显示 (Information 或 Warning)
    if (success) {
        QMessageBox::information(this, title, msg);
    } else {
        QMessageBox::warning(this, title, msg);
    }
}

// 抓拍图片回调
void DataView::onSnapshotReceived(const QPixmap &pixmap)
{
    // 这里我们简单弹窗显示一下图片大小，或者您可以弹出一个对话框显示图片
    QString msg = QString("抓拍成功！\n图片尺寸: %1 x %2").arg(pixmap.width()).arg(pixmap.height());
    QMessageBox::information(this, "抓拍完成", msg);

    // 如果您想在界面上显示这张图，可以在这里操作 UI
    // 例如：ui->labelPreview->setPixmap(pixmap);
}

/********************************接口调用***********************************************/
// ===============================================
// 图像采集类设置
// ===============================================

// 1. 设置帧率
void DataView::on_btnSetFps_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    double fps = ui->dsbFps->value();
    m_api->setFrameRate(isGlobal, camId, fps);
}

// 2. 设置曝光
void DataView::on_btnSetExposure_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    float exposure = ui->dsbExposure->value();
    m_api->setExposure(isGlobal, camId, exposure);
}

// 3. 设置增益
void DataView::on_btnSetGain_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    float gain = ui->dsbGain->value();
    m_api->setGain(isGlobal, camId, gain);
}

// 4. 设置像素格式
void DataView::on_btnSetFormat_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    QString format = ui->comboPixelFormat->currentText();
    m_api->setPixelFormat(isGlobal, camId, format);
}

// 5. 设置缩放
void DataView::on_btnSetZoom_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    float factor = ui->dsbZoom->value();
    m_api->setZoom(isGlobal, camId, factor);
}

// ===============================================
// 触发类设置
// ===============================================

// 6. 设置触发模式
void DataView::on_btnSetTrigMode_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    QString mode = ui->comboTriggerMode->currentText(); // "On" / "Off"
    m_api->setTriggerMode(isGlobal, camId, mode);
}

// 7. 设置触发源
void DataView::on_btnSetTrigSource_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    QString source = ui->comboTriggerSource->currentText();
    m_api->setTriggerSource(isGlobal, camId, source);
}

// 8. 设置触发沿
void DataView::on_btnSetTrigActive_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    QString edge = ui->comboTriggerActive->currentText();
    m_api->setTriggerActivation(isGlobal, camId, edge);
}

// ===============================================
// OSD 与 抓拍
// ===============================================

// 9. 设置十字光标 (Demo: 固定红色，线宽2)
void DataView::on_btnSetCrosshair_clicked()
{
    bool isGlobal = ui->chkGlobalScope->isChecked();
    int camId = ui->sbTargetCamId->value();
    bool enabled = ui->chkCrosshair->isChecked();

    // 这里简单起见，坐标默认设为 1024,1024 (或者您可以加输入框)
    int x = 1024;
    int y = 1024;
    int size = 100;

    m_api->setCrosshair(isGlobal, camId, enabled, x, y, size, m_crosshairColor, 2);
}

// 10. 抓拍 (获取单帧图片)
void DataView::on_btnSnapshot_clicked()
{
    QString savePath = ui->lineEdit->text().trimmed(); // 获取路径并去空格

    if (savePath.isEmpty()) {
        // --- 模式 A：仅预览 (内存获取) ---
        m_api->getSnapshot();
        // 结果会触发 onSnapshotReceived -> 弹窗显示图片
    } else {
        // --- 模式 B：服务器保存 ---
        // 注意：这个接口不支持 global，必须指定相机ID
        int camId = ui->sbTargetCamId->value();

        // 调用保存接口
        m_api->saveSnapshotToServer(camId, savePath);

        // 结果会触发 onApiResult -> 弹窗提示 "操作成功"
    }
}

void DataView::on_btnPickColor_clicked()
{
    // 弹出颜色选择对话框，默认选中当前颜色
    QColor color = QColorDialog::getColor(m_crosshairColor, this, "选择十字光标颜色");

    if (color.isValid()) {
        m_crosshairColor = color; // 保存颜色

        // 可选：修改按钮背景色，给用户直观反馈
        QString style = QString("background-color: %1; color: %2; border: 1px solid gray;")
                            .arg(color.name())
                            .arg(color.lightness() > 128 ? "black" : "white"); // 自动调整文字颜色
        ui->btnPickColor->setStyleSheet(style);
    }
}
