#pragma once

#include "Connection.h"
#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <thread>
#include <condition_variable>

class MySQLConnectionPool
{
public:
	// 禁止拷贝
	MySQLConnectionPool(const MySQLConnectionPool&) = delete;
	MySQLConnectionPool& operator=(const MySQLConnectionPool&) = delete;

	~MySQLConnectionPool();

	// 获取连接池的单例对象
	static MySQLConnectionPool* getInstance();

	// 从配置文件mysql.ini加载配置信息
	bool loadConfigFile(const std::string& configPath = "mysql.cnf");

	// 从连接池里面获取数据库连接对象
	std::unique_ptr<Connection, std::function<void(Connection*)>> getConnection();

	// 启动生产者线程，负责生产数据库连接
	void startProduceThread();

	// 启动监视者线程
	void startMonitorThread();


private:
	// 构造函数私有化
	MySQLConnectionPool();

	// 初始化线程池，创建_initSize个_connectionQueue成员
	void initializeConnection();
	
	// 关闭生产者线程
	void stopProduceThread();
	
	// _produceThread线程的回调函数,负责生产线程
	void produceTask();

	// 关闭监视者线程
	void stopMonitorThread();

	// _monitorThread线程的回调函数,负责生产线程
	void monitorTask();

	std::string _ip;                         // 数据库所在主机的IP地址
	uint16_t _port = 0;		                 // 端口号
	std::string _username;                   // 登录数据库的用户名
	std::string _password;                   // 登录数据库的密码
	std::string _dbname;                     // 登录的数据库的名称
	uint32_t _initSize = 0;                  // 连接池最初容量
	uint32_t _maxSize = 0;                   // 连接池最大容量
	uint32_t _maxFreeTime = 0;               // 连接的最大空闲时间
	uint32_t _connectionTimeOut = 0;         // 连接的超时时间

	std::queue<std::unique_ptr<Connection>> _connectionQueue; // 存储数据库连接的队列
	std::mutex _queueMutex;                                   // 维持 _connectionQueue 能够在多线程环境中被安全访问的互斥量
	std::atomic<uint16_t> _connectionCnt{0};                  // 记录_connectionQueue里面现有的连接的个数
	std::condition_variable _cv;                              // 条件变量
	std::thread _produceThread;                               // 生产者线程，负责生产数据库连接
	std::atomic<bool> _produceThreadShutdown{ false };        // 标志生产者线程是否结束
	std::thread _monitorThread;                               // 监视者线程，用来监视连接池里面超出_initSize的空闲连接
	std::atomic<bool> _monitorThreadShutdown{ false };        // 标志监视者线程是否结束
};