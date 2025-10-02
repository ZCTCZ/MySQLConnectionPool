#pragma once
#include "Param.h"
#include <string>
#include <vector>
#include <memory>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <ctime>


class Connection 
{
public:
	// 构造：初始化驱动（不立即连接）
	Connection() = default;

	// 析构：自动释放连接
	~Connection() = default;

	// 移动构造
	Connection(Connection&&) = default;

	// 移动赋值
	Connection& operator=(Connection&&) = default;  

	// 禁用拷贝）
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

	// 连接数据库
	bool connect(
		const std::string& ip,
		unsigned short port,
		const std::string& username,
		const std::string& password,
		const std::string& dbname
	);

	// 执行非查询语句（INSERT/UPDATE/DELETE）—— 参数化，防注入
	bool execute(
		const std::string& sql_template,
		const std::vector<Param>& params = {}
	);

	// 执行查询语句（SELECT）—— 参数化，返回结果集
	std::unique_ptr<sql::ResultSet> query(
		const std::string& sql_template,
		const std::vector<Param>& params = {}
	);

	// 刷新 _aliveTime
	void refreshAliveTime() { _aliveTime = clock(); }

	// 返回处于空闲状态的连接存户的时间
	clock_t getAliveTime() { return clock() - _aliveTime; }

private:
	std::unique_ptr<sql::Connection> _conn; // unique_ptr不支持拷贝和赋值
	clock_t _aliveTime = 0;                // 连接进入空闲状态后的起始存活时间，毫秒级别
};