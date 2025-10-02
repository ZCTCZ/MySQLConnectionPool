#define _CRT_SECURE_NO_WARNINGS 1

#include "Connection.h"
#include "Param.h"
#include "MySQLConnectionPool.h"
#include <iostream>
#include <chrono>

/*
int main(int argc, char** argv)
{
	Connection conn;
	if (!conn.connect("127.0.0.1", 3306, "root", "zct010601", "test_db"))
	{
		std::cerr << "连接数据库失败！" << std::endl;
	}
	else
	{
		// 执行插入操作
		if (conn.execute("INSERT INTO people(peop_name, peop_sex) VALUES(?, ?)", { Param::String("fuckme"), Param::String("female") }))
		{
			std::cout << "插入数据成功！" << std::endl;
		}
		else
		{
			std::cerr << "插入数据失败！" << std::endl;
		}

		// 执行查询操作
		auto res = std::move(conn.query("SELECT * FROM people WHERE peop_name = ?", { Param::String("fuckme") }));
		if (res == nullptr)
		{
			std::cout << "Not Found" << std::endl;
		}
		else
		{
			std::vector<std::vector<std::string>> table;
			const unsigned int colCount = res->getMetaData()->getColumnCount(); // 获取列数
			while (res->next())
			{
				std::vector<std::string> rowData;
				rowData.reserve(colCount);
				for (int i=1; i<=colCount; ++i) // JDBC 列号从 1 开始
				{
					rowData.emplace_back(res->getString(i));
				}
				table.emplace_back(rowData);
			}
			
			for (const auto& row : table)
			{
				for (const auto& col : row)
				{
					std::cout << col << "  ";
				}
				std::cout << std::endl;
			}
		}
	}

	return 0;
}
*/

void test_insert_without_pool(uint32_t nums)
{
	for (size_t i = 0; i < nums; ++i)
	{
		Connection conn;
		conn.connect("127.0.0.1", 3306, "root", "zct010601", "test_db");
		conn.execute("INSERT INTO people(peop_name, peop_sex) VALUES(?, ?)", { Param::String("fuckme"), Param::String("female") });
	}
}

void test_insert_with_pool(uint32_t nums)
{
	MySQLConnectionPool* pcp = MySQLConnectionPool::getInstance(); // 获取连接池的单例对象
	// 保证在多线程的环境下，一个线程池对象只关联一个生产者线程和一个监视者线程
	static std::once_flag flag;
	std::call_once(flag, [&] {
		pcp->startProduceThread();
		pcp->startMonitorThread();
		});
	
	for (size_t i = 0; i < nums; ++i)
	{
		auto pConn = pcp->getConnection();
		pConn->execute("INSERT INTO people(peop_name, peop_sex) VALUES(?, ?)", { Param::String("fuckme"), Param::String("female") });
	}
}

//四个线程测试--------------------------------------

void test_insert_without_pool_mt(uint32_t n, uint32_t nums)
{
	std::vector<std::thread> threads;
	for (size_t i = 0; i < n; ++i)
	{
		threads.emplace_back(std::thread(test_insert_without_pool, nums/n));
	}

	for (auto& e : threads)
	{
		e.join();
	}
}

void test_insert_with_pool_mt(uint32_t n, uint32_t nums)
{
	MySQLConnectionPool::getInstance(); // 在主线程里面获取连接池的单例对象
	std::vector<std::thread> threads;
	for (size_t i = 0; i < n; ++i)
	{
		threads.emplace_back(std::thread(test_insert_with_pool, nums / n));
	}

	for (auto& e : threads)
	{
		e.join();
	}
}


int main(int argc, char** argv)
{
	uint32_t nums = 1000;
	// uint32_t nums = 5000;
	// uint32_t nums = 10000;

	// clock_t start = clock(); 
	auto start = std::chrono::steady_clock::now();

	// test_insert_without_pool(nums);
	test_insert_with_pool(nums);
	//test_insert_without_pool_mt(4, nums);
	// test_insert_with_pool_mt(4, nums);

	// clock_t end = clock();
	auto end = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	std::cout << "插入" << nums << "条数据耗时：" << ms << "ms" << std::endl;
	return 0;
}