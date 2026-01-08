#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QSettings> // 用于读取配置文件
#include <QCoreApplication>
#include <QDateTime>


class DBManager : public QObject
{
    Q_OBJECT
public:
    static DBManager& instance(); // 单例模式：获取全局唯一实例

    // --- 1. 连接管理 ---
    bool connectToDb(); // 读取配置文件并连接
    void closeDb();     // 关闭连接

    // --- 2. 业务接口 (增删改查) ---
    // 插入一条日志 (注意：根据之前的修改，我们只有4个字段，去掉了位置)
    bool insertLog(const QString& time, double speed, double accel, double dist);

    // 获取历史数据 (分页查询)
    QSqlQuery getHistoryLogs(int limit, int offset);

    // 获取数据总条数 (用于计算页码)
    int getTotalCount();
private:
    ~DBManager();
    explicit DBManager(QObject *parent = nullptr);
    // 禁止拷贝
    DBManager(const DBManager&) = delete;
    DBManager& operator=(const DBManager&) = delete;

    QSqlDatabase m_db;
};

#endif // DBMANAGER_H
