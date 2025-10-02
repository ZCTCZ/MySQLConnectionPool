#define _CRT_SECURE_NO_WARNINGS 1

// Connection.cpp
#include "Connection.h"
#include "Public.h"
#include <mysql_driver.h>
#include <cppconn/driver.h>
#include <stdexcept>

bool Connection::connect(
	const std::string& ip,
	unsigned short port,
	const std::string& username,
	const std::string& password,
	const std::string& dbname)
{
	try 
	{
		// 创建连接 URL
		std::string host = "tcp://" + ip + ":" + std::to_string(port);

		// 获取驱动单例
		sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();

		// 建立连接
		_conn.reset(driver->connect(host, username, password));

		// 选择数据库
		_conn->setSchema(dbname);

		return true;
	}
	catch (const sql::SQLException& e) 
	{
		std::string str("ERROR: ");
		str.append(e.what());
		str.append(" (error code: ");
		str += e.getErrorCode();
		str += ")";
		LOG(str);
		return false;
	}
}

bool Connection::execute(
	const std::string& sql_template,
	const std::vector<Param>& params)
{
	if (!_conn) return false;

	try 
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(_conn->prepareStatement(sql_template));

		// 绑定参数（索引从 1 开始）
		for (size_t i = 0; i < params.size(); ++i) 
		{
			switch (params[i].type)
			{
			case Param::STRING:
				pstmt->setString(static_cast<int>(i + 1), params[i].str_val);
				break;
			case Param::ENUM_STR:
				pstmt->setString(static_cast<int>(i + 1), params[i].str_val);
				break;
			case Param::UINT32:
				pstmt->setUInt(static_cast<int>(i + 1), params[i].uint_val);
				break;
			default:
				break;
			}
		}

		// execute() 返回 true 表示有结果集（如 SELECT），false 表示无结果集（如 INSERT）
		// 对于 DML，我们只关心是否成功执行，不关心是否有结果集
		pstmt->execute();
		return true;
	}
	catch (const sql::SQLException& e) {
		std::string str("ERROR: ");
		str.append(e.what());
		str.append(" (error code: ");
		str += e.getErrorCode();
		str += ")";
		LOG(str);
		return false;
	}
}

std::unique_ptr<sql::ResultSet> Connection::query(
	const std::string& sql_template,
	const std::vector<Param>& params)
{
	if (!_conn) return nullptr;

	try 
	{
		std::unique_ptr<sql::PreparedStatement> pstmt(_conn->prepareStatement(sql_template));

		for (size_t i = 0; i < params.size(); ++i)
		{
			switch (params[i].type)
			{
			case Param::STRING:
				pstmt->setString(static_cast<int>(i + 1), params[i].str_val);
				break;
			case Param::ENUM_STR:
				pstmt->setString(static_cast<int>(i + 1), params[i].str_val);
				break;
			case Param::UINT32:
				pstmt->setUInt(static_cast<int>(i + 1), params[i].uint_val);
				break;
			default:
				break;
			}
		}

		return std::unique_ptr<sql::ResultSet>(pstmt->executeQuery());
	}
	catch (const sql::SQLException& e) 
	{
		std::string str("ERROR: ");
		str.append(e.what());
		str.append(" (error code: ");
		str += e.getErrorCode();
		str += ")";
		LOG(str);
		return nullptr;
	}
}
