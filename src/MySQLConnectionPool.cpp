#define _CRT_SECURE_NO_WARNINGS 1
#include "MySQLConnectionPool.h"
#include "Public.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <cstdlib>
#include <stdexcept>
#include <chrono>

MySQLConnectionPool::MySQLConnectionPool()
{
    // 加载数据库配置文件
    if (!loadConfigFile("./../src/mysql.cnf"))
    {
        LOG("启动数据库连接池失败");
        exit(1);
    }

    // 初始化连接池，创建 _initSize 个数据库连接
    initializeConnection();
}


MySQLConnectionPool::~MySQLConnectionPool()
{
    // 关闭生产者线程
    stopProduceThread();

    // 关闭监视者线程
    stopMonitorThread();
}

/**
 * 获取数据库连接池的单例对象
 */
MySQLConnectionPool* MySQLConnectionPool::getInstance()
{
    static MySQLConnectionPool mySQLConnectionPool;
    return &mySQLConnectionPool;
}


/**
 * 删除字符串里开头和结尾的所有空白字符
 */
static std::string& trim(std::string& str)
{
    // 从前往后，找到第一个不是空格的字符，返回它的迭代器位置
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch)->bool 
        { return !std::isspace(ch); }));

    // 从后往前，找到第一个不是空格的字符，返回它的迭代器位置
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch)->bool 
        { return !std::isspace(ch); }).base(), str.end());
    return str;
}


/**
 * 读取数据库配置文件里的配置信息
 */
bool MySQLConnectionPool::loadConfigFile(const std::string& configPath)
{
    std::ifstream input(configPath);
    if (!input.is_open()) //如果文件没有成功打开
    {
        LOG("open" + configPath + "failed");
        return false;
    }

    std::string line;
    while (std::getline(input, line)) //用 getline 读取时，\n 会被去掉，但 \r 可能还在行尾
    {
        if (line.empty() || line.front() == '#') //跳过空行或者注释
        {
            continue;
        }
		if (!line.empty() && line.back() == '\r') //在 Windows 系统中，换行是 \r\n，而 Linux 是 \n
		{
			line.pop_back();
		}
        auto pos = line.find('=');
        if (pos == std::string::npos)
        {
            continue;
        }
        else
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // key 和 value 前后可能有空格，需要除去
            trim(key);
            trim(value);

            if (key == "ip") _ip = value;
            else if (key == "port") _port = static_cast<uint16_t>(std::stoi(value));
            else if (key == "username") _username = value;
            else if (key == "password") _password = value;
            else if (key == "dbname") _dbname = value;
            else if (key == "initSize") _initSize = static_cast<uint32_t>(std::stoi(value));
            else if (key == "maxSize") _maxSize = static_cast<uint32_t>(std::stoi(value));
            else if (key == "maxFreeTime") _maxFreeTime = static_cast<uint32_t>(std::stoi(value));
            else if (key == "connectionTimeOut")  _connectionTimeOut = static_cast<uint32_t>(std::stoi(value));
        }
    }

    return true;
}


/**
 * 从连接池里面获取数据库连接对象
 */
std::unique_ptr<Connection, std::function<void(Connection*)>> MySQLConnectionPool::getConnection()
{
	std::unique_lock<std::mutex> lock(_queueMutex);

	// 等待：直到队列非空，或超时
	bool hasConn = _cv.wait_for(
		lock,
		std::chrono::milliseconds(_connectionTimeOut), // 连接超时时间
		[this]() { return !_connectionQueue.empty(); }
	);

	if (!hasConn)
	{
		LOG("get connection failed");
        if (_connectionCnt < _maxSize)
        {
			_cv.notify_all(); // 只有在连接数没到达上限时，才通知生产者线程，连接池里已经没有空闲的数据库连接了
        }
        return { nullptr, [](Connection*) {} }; // 超时，获取失败
	}
	
	auto deleter = [this](Connection* pConn)
		{
			std::unique_lock<std::mutex> lock(_queueMutex);
            pConn->refreshAliveTime(); // 连接进入空闲状态时，刷新存活的起始时间
			_connectionQueue.push(std::move(std::unique_ptr<Connection>(pConn)));
		};

    std::unique_ptr<Connection> pConn = std::move(_connectionQueue.front());
    _connectionQueue.pop();

	// 队列非空，取出连接
    auto ret = std::unique_ptr<Connection, std::function<void(Connection*)>>(pConn.release(), deleter);
    return ret;
}


// 初始化线程池，创建_initSize个_connectionQueue成员
void MySQLConnectionPool::initializeConnection()
{
    for (uint16_t i = 0; i < _initSize; ++i)
    {
        std::unique_ptr<Connection> pConn = std::make_unique<Connection>(); // 在堆上面构造一个 std::unique_ptr<Connection> 对象
        try
        {
            if (pConn->connect(_ip, _port, _username, _password, _dbname) == false)
            {
                throw(std::runtime_error("connect failed"));
            }
            pConn->refreshAliveTime(); // 连接进入空闲状态时，刷新存活的起始时间
            _connectionQueue.push(std::move(pConn));
            ++_connectionCnt;
        }
        catch (const std::exception& e)
        {
            LOG(e.what());
        }
    }
}


// 启动生产者线程，负责生产数据库连接
void MySQLConnectionPool::startProduceThread()
{
    _produceThread = std::thread(&MySQLConnectionPool::produceTask, this);
}


// 关闭生产者线程
void MySQLConnectionPool::stopProduceThread()
{
    _produceThreadShutdown.store(true);
    _cv.notify_all(); //唤醒可能在阻塞的生产者线程

    if (_produceThread.joinable())
    {
        _produceThread.join();
    }
}

// _produceThread线程的回调函数,负责生产线程
void MySQLConnectionPool::produceTask()
{
    while (!_produceThreadShutdown) // 通过原子型布尔变量来控制生产者线程的结束
    {
        std::unique_lock<std::mutex> lock(_queueMutex);
        _cv.wait(lock, [this]() 
            {
            return (_produceThreadShutdown.load() || 
                ((this->_connectionQueue.empty()) && _connectionCnt < _maxSize)); 
            }
        ); // 被消费者线程唤醒之后，将自动持有锁

        if (_produceThreadShutdown.load())
        {
            break;
        }

        std::unique_ptr<Connection> pConn = std::make_unique<Connection>(); // 在堆上面构造一个 std::unique_ptr<Connection> 对象
		try
		{
			if (pConn->connect(_ip, _port, _username, _password, _dbname) == false)
			{
				throw(std::runtime_error("connect failed"));
			}
			pConn->refreshAliveTime(); // 连接进入空闲状态时，刷新存活的起始时间
			_connectionQueue.push(std::move(pConn));
			++_connectionCnt;
            _cv.notify_all(); // 唤醒消费者线程，可以从_connectionQueue里面获取数据库连接了
		}
		catch (const std::exception& e)
		{
			LOG(e.what());
			// 如果连接一直失败，线程会持续占用 mutex 并快速重试，影响其他线程（如消费者）获取锁
			// 所以在失败之后短暂的休眠，让其他线程能够有机会获得锁
			lock.unlock();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
		}
    }
    std::cout << "producerThread exit" << std::endl;
}

// 启动监视者线程
void MySQLConnectionPool::startMonitorThread()
{
    _monitorThread = std::thread(&MySQLConnectionPool::monitorTask, this);
}

// 关闭监视者线程
void MySQLConnectionPool::stopMonitorThread()
{
    _monitorThreadShutdown.store(true);
    _cv.notify_all();

    if (_monitorThread.joinable())
    {
        _monitorThread.join();
    }
}

// _monitorThread线程的回调函数,负责监视队列里的空闲连接的存活时间
void MySQLConnectionPool::monitorTask()
{
    while (!_monitorThreadShutdown)
    {
        std::unique_lock<std::mutex> lock(_queueMutex);
        bool readyShutdown = _cv.wait_for(lock, std::chrono::seconds(_maxFreeTime), [this]() {return _monitorThreadShutdown.load(); });

        if (readyShutdown && _monitorThreadShutdown.load())
        {
            break;
        }
        
        while (_connectionCnt > _initSize && !_connectionQueue.empty()) // 只清理超出_initSize部分的空闲线程
        {
            // 由于连接对象是依次入队的，所以排在队头的连接的空闲时间一定比后面连接的空闲时间长
            if (_connectionQueue.front()->getAliveTime() / CLOCKS_PER_SEC >= static_cast<signed>(_maxFreeTime))
            {
                _connectionQueue.front().reset(); // 释放队头的连接
                _connectionQueue.pop();
                --_connectionCnt;
            }
            else //队头连接的空闲时间低于_maxFreeTime秒，则整个队列里连接的空闲时间都低于_maxFreeTime秒
            {
                break;
            }
        }
    }
    std::cout << "monitorThread exit" << std::endl;
}