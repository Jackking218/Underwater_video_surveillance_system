#include "dbmanager.h"
#include <QLibrary>
#include <QLibraryInfo>
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSettings>
#include <QCoreApplication>
#include <cmath> // 引入 cmath 以使用 std::isnan 和 std::isinf
// 获取单例
DBManager& DBManager::instance()
{
    static DBManager instance;
    return instance;
}

DBManager::DBManager(QObject *parent) : QObject(parent)
{}

DBManager::~DBManager()
{
    closeDb();
}


bool DBManager::connectToDb()
{
    QLibrary lib("libmysql.dll");
    if(lib.load()) {
        qDebug() << "libmysql.dll 加载成功！";
    } else {
        qDebug() << "libmysql.dll 加载失败：" << lib.errorString();
    }
    if(m_db.isOpen()) return true;

    // 1. 读取配置文件 (config.ini) 路径：生成的 exe 文件同级目录下的 config.ini
    QString configPath = QCoreApplication::applicationDirPath() + "/config.ini";

    QSettings settings(configPath, QSettings::IniFormat);// 使用 QSettings 读取 ini文件

    // 如果配置文件不存在或没值，使用默认值
    QString host = settings.value("Database/Host", "127.0.0.1").toString();
    int port     = settings.value("Database/Port", 3306).toInt();
    QString dbName = settings.value("Database/DbName", "underwater_sys").toString();
    QString user = settings.value("Database/User", "root").toString();
    QString pwd  = settings.value("Database/Password", "123456").toString();

    qDebug() << "正在连接数据库:" << host << "用户:" << user;

    // 2. 配置 MySQL 连接
    qDebug() << "可用数据库驱动:" << QSqlDatabase::drivers();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qDebug() << "插件路径:" << QLibraryInfo::path(QLibraryInfo::PluginsPath);
#else
    qDebug() << "插件路径:" << QLibraryInfo::location(QLibraryInfo::PluginsPath);
#endif
    if(QSqlDatabase::contains("mysql_conn"))
    {
        m_db = QSqlDatabase::database("mysql_conn");
    }else {
        m_db = QSqlDatabase::addDatabase("QMYSQL","mysql_conn");
    }

    m_db.setHostName(host);
    m_db.setPort(port);
    m_db.setDatabaseName(dbName);
    m_db.setUserName(user);
    m_db.setPassword(pwd);

    // 3. 尝试打开
    if (!m_db.open()) {
        qDebug() << "MySQL 连接失败:" << m_db.lastError().text();
        qDebug() << "提示: 请检查 1.MySQL服务是否启动 2.config.ini配置是否正确 3.libmysql.dll是否在exe目录下";
        return false;
    }

    qDebug() << "MySQL 连接成功!";

    //4.建表
    QSqlQuery query(m_db);
    QString createSql = "CREATE TABLE IF NOT EXISTS ship_logs("
                        "id INT AUTO_INCREMENT PRIMARY KEY,"
                        "log_time DATETIME,"
                        "speed DOUBLE,"
                        "accel DOUBLE,"
                        "dist DOUBLE)";
    if(!query.exec(createSql)) {
        qDebug() << "建表失败:" << query.lastError().text();
    }

    return true;
}

void DBManager::closeDb()
{
    if (m_db.isOpen()) m_db.close();
}

// 插入数据
bool DBManager::insertLog(const QString& time, double speed, double accel, double dist)
{
    if (!m_db.isOpen()) return false;

    // 简单防护：过滤异常值
    auto clamp = [](double v, double minV, double maxV){
        if (std::isnan(v) || std::isinf(v)) return minV;
        if (v < minV) return minV;
        if (v > maxV) return maxV;
        return v;
    };
    double s = clamp(speed, 0.0, 100.0);
    double a = clamp(accel, -100.0, 100.0);
    double d = clamp(dist, 0.0, 1e9);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO ship_logs (log_time, speed, accel, dist) "
                  "VALUES (:time, :speed, :accel, :dist)");
    query.bindValue(":time", time);
    query.bindValue(":speed", s);
    query.bindValue(":accel", a);
    query.bindValue(":dist", d);

    if(!query.exec()) {
        qDebug() << "插入失败:" << query.lastError().text();
        return false;
    }
    qDebug() << "插入成功:" << time << "speed" << s << "accel" << a << "dist" << d;
    return true;
}

//查询分页
QSqlQuery DBManager::getHistoryLogs(int limit, int offset)
{
    QSqlQuery query(m_db);
    // MySQL 分页语法: LIMIT 数量 OFFSET 偏移量
    query.prepare("SELECT log_time, speed, accel, dist FROM ship_logs "
                  "ORDER BY id DESC LIMIT :limit OFFSET :offset");
    query.bindValue(":limit", limit);
    query.bindValue(":offset", offset);
    query.exec();
    return query;
}

// 查询总数
int DBManager::getTotalCount()
{
    if (!m_db.isOpen()) return 0;

    QSqlQuery query(m_db);
    query.exec("SELECT COUNT(*) FROM ship_logs");
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}
