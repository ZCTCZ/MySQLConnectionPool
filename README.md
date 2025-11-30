采用Connector/C++库和C++11设计的数据库连接池
# 技术点

- 基于Connector/C++库（9.4.0 64位 Debug版）的MySQL数据库编程
- SQL防注入
- 多线程编程（线程同步、mutex互斥量、unique_lock、condition_variable、原子数据类型）
- 生产者消费者模型
- unique_ptr智能指针
- stl的容器适配器queue
- lambda表达式
- 单例模式

# 使用介绍

1. 用户获取MySQLConnectionPool单例对象，并通过调用`startProduceThread()`和`startMonitorThread()`启动生产数据库连接的生产者线程和监视连接池中空闲连接存活时间的监视者线程。
2. 用户通过调用`getConnection()`函数获取连接池中的数据库连接对象，使用该对象的`execute()`函数执行非查询相关的SQL语句，使用`query()`函数执行和查询相关的SQL语句。
3. 当用户获取的数据库连接不再需要使用时，无需手动释放连接。`getConnection()`函数返回的是智能指针对象，并且自定义了删除器，当智能指针对象析构的时候，数据库连接将自动被添加到数据库连接池中。

# 具体设计

## 参数包装类Param

1. 枚举类型的成员变量type，用来记录SQL语句里参数的类型，和参数绑定环节映射。
2. 各种数据类型的变量，用来记录参数的值。
3. 静态工厂方法，用来构造各种数据类型的Param对象。

## 数据库连接类Connection

1. `std::unique_ptr<sql::Connection>`类型的智能指针对象，用来管理通过Connection/C++ API建立的数据库连接。当Connection对象析构的时候，智能指针会自动将数据库连接的句柄delete掉。
2. `clock_t _aliveTime` ，用来记录Connection对象（数据库连接）进入数据库连接池的时刻，以此为连接处于空闲状态的起点。
3. `void refreshAliveTime()`函数，当有连接进入连接池时，刷新连接的`_aliveTime`属性。
4. `clock_t getAliveTime()`函数，返回线程池里的连接存活的空闲时间。
5. `bool execute(const std::string& sql_template,const std::vector<Param>& params = {}
   )`函数，用来执行和查询无关的SQL操作。
6. `std::unique_ptr<sql::ResultSet> query(const std::string& sql_template,const std::vector<Param>& params = {})`函数，用来执行和查询相关的操作，返回智能指针管理。
7. `bool connect(const std::string& ip,unsigned short port,const std::string& username,
   const std::string& password,const std::string& dbname)`函数，负责连接数据库。
8. 由于成员变量里面有unique_ptr，所以需要将拷贝构造和拷贝赋值禁止，使用移动构造和移动赋值函数。

## 数据库连接池类MySQLConnectionPool

1. `std::queue<std::unique_ptr<Connection>> _connectionQueue`：存放数据库连接对象的队列。
2. `uint32_t _initSize`：连接池的初始容量，当连接池对象实例化的时候，需要在_connectionQueue队列里构造出__initSize个数据库连接对象。
3. `int32_t _maxSize`：连接池里数据库连接的最大连接数。由于数据库连接需要占用系统的套接字资源，所以无法将连接池里的连接数量无限增加，需要设置一个最大数量。当连接池里面的连接数量已经达到了最大数量，但是却没有空闲连接可用的时候，用户将会申请数据库连接失败。
4. `uint32_t _maxFreeTime`：连接池里空闲连接的最大存活时间，当处于空闲状态的连接存在时间超过_maxFreeTime时，该连接将会被释放。_maxFreeTime只在连接池里的连接数量超过连接池的初始容量时才起作用，因为必须保持连接池里有初始容量个数据库连接。
5. `uint32_t _connectionTimeOut`：请求数据库连接时的超时时间，在_connectionTimeOut时间内都没有请求到数据库连接，将被断定为请求失败。
6. `std::thread _produceThread`：线程对象，用来存放生成数据库连接的生产者线程。
7. `std::thread _monitorThread`：线程对象，用来存放监视连接池里空闲连接存活时间的监视者线程。
8. `std::unique_ptr<Connection, std::function<void(Connection*)>> getConnection()`：获取连接池里数据库连接的函数，返回值使用智能指针，同时需要自定义删除器，将智能指针管理的数据库连接存放到_connectionQueue的队尾，而不是释放掉。如果获取数据库连接失败，将会唤醒生产者线程。

## 数据库连接配置文件

定义一份mysql.ini（Linux系统则为mysql.cnf）的数据库配置文件，以“键=值”的方式配置相关参数。在连接池对象的构造函数里，将会读取mysql.ini里面的配置信息。

# 压力测试

基于Windows系统

| 插入数据量 | 不使用连接池耗时（三次操作取平均值）   | 使用连接池耗时（三次操作取平均值）      |
| ---------- | -------------------------------------- | --------------------------------------- |
| 1000       | 单线程：8.7s              四线程：3.4s | 单线程：3.3s              四线程：1.3s  |
| 5000       | 单线程：47.4s            四线程：15.6s | 单线程：16.7s            四线程：7.6s   |
| 10000      | 单线程：101s             四线程：30.9s | 单线程：38s               四线程：14.2s |

