// net.hpp
#pragma once
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>
#include "fields.hpp"
#include "Logger.hpp"
#include "Base.hpp"
#include <netinet/in.h>
#include "message.hpp"
#include <mutex>
#include <unordered_map>

namespace RPC
{
    class MuduoBuffer : public BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<MuduoBuffer>;

        MuduoBuffer(muduo::net::Buffer* buf):
            _buf(buf)
        {}

        virtual size_t readableSize() override
        {
            return _buf->readableBytes();
        }

        virtual int32_t peekInt32() override
        {
            return _buf->peekInt32();
        }

        virtual void retrieveInt32() override
        {
            _buf->retrieveInt32();
        }   

        virtual int32_t readInt32() override
        {
            return _buf->readInt32();
        }

        virtual std::string retrieveAsString(size_t len) override
        {
            return _buf->retrieveAsString(len);
        }

    private:
        muduo::net::Buffer* _buf;
    };

    class BufferFactory
    {
    public:
        template<typename ...Args>
        static BaseBuffer::ptr create(Args&& ...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };



    class LVProtocol : public BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<LVProtocol>;

        // 协议规定报文格式:
        // |--len--|--mtype--|--idlen--|--id--|--body--|
        // len字段固定四字节，len值为len字段之后所有内容的长度
        // mtype字段固定四字节
        // idlen字段固定四字节，idlen值为id字段长度
    private:
        const size_t lenFieldsLength = 4;
        const size_t mtypeFieldsLength = 4;
        const size_t idlenFieldsLength = 4;

    public:
        // 判断缓冲区中的数据量是否足够一条消息
        virtual bool canProcessed(const BaseBuffer::ptr& buf) override
        {
            if(buf->readableSize() < lenFieldsLength)
            {
                return false;
            }
            int32_t total_len = buf->peekInt32();
            if(buf->readableSize() < total_len + lenFieldsLength)
            {
                return false;
            }
            return true;
        }

        // 当调用onMessage函数时，默认认为缓冲区中的数据足够一条完整的消息
        // 从缓冲区中取出一条消息，反序列化，记录到参数的msg中
        virtual bool onMessage(const BaseBuffer::ptr& buf, BaseMessage::ptr& msg) override
        {
            int32_t total_len = buf->readInt32();
            MsgType mtype = (MsgType)buf->readInt32();
            int32_t idlen = buf->readInt32();

            int32_t body_len = total_len - mtypeFieldsLength - idlenFieldsLength - idlen;
            std::string id = buf->retrieveAsString(idlen);
            std::string body = buf->retrieveAsString(body_len);

            msg = MsgFactory::create(mtype);
            if(msg.get() == nullptr)
            {
                LOG(LogLevel::ERROR) << "消息类型错误,构建消息对象失败!";
                return false;
            }
            bool ret = msg->unserialize(body);
            if(ret == false)
            {
                return false;
            }
            msg->setId(id);
            msg->setMsgtype(mtype);
            return true;
        }
        
        virtual std::string serialize(const BaseMessage::ptr& msg) override
        {
            // |--len--|--mtype--|--idlen--|--id--|--body--|
            std::string body = msg->serialize();
            std::string id = msg->rid();
            auto mtype = htonl((int32_t)msg->mtype());
            int32_t idlen = htonl(id.size());

            int32_t h_total_len = mtypeFieldsLength + idlenFieldsLength + id.size() + body.size();
            int32_t n_total_len = htonl(h_total_len);

            std::string result;
            result.reserve(h_total_len + lenFieldsLength);
            result.append((char*)&n_total_len, lenFieldsLength);
            result.append((char*)&mtype, mtypeFieldsLength);
            result.append((char*)&idlen, idlenFieldsLength);
            result.append(id);
            result.append(body);

            return result;
        }
    };

    class ProtocolFactory
    {
    public:
        template<typename ...Args>
        static BaseProtocol::ptr create(Args&& ...args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }
    };



    class MuduoConnection : public BaseConnection
    {
    public:
        using ptr = std::shared_ptr<MuduoConnection>;
        MuduoConnection(const muduo::net::TcpConnectionPtr& conn, const BaseProtocol::ptr& protocol)
            : _protocol(protocol)
            , _conn(conn)
        {}

        virtual void send(const BaseMessage::ptr& msg) override
        {
            std::string body = _protocol->serialize(msg);
            _conn->send(body);
        }

        virtual void shutdown() override
        {
            _conn->shutdown();
        }

        virtual bool connected() override
        {
            return _conn->connected();
        }

    private:
        BaseProtocol::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;
    };

    class ConnectionFactory
    {
    public:
        template<typename ...Args>
        static BaseConnection::ptr create(Args&& ...args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }
    };



    class MuduoServer : public BaseServer
    {
    public:
        using ptr = std::shared_ptr<MuduoServer>;

        MuduoServer(int port)
            : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "MuduoServer", muduo::net::TcpServer::kReusePort)
            , _protocol(ProtocolFactory::create())
            // server的0.0.0.0表示监听本机所有网卡，muduo::net::TcpServer::kReusePort表示开启端口复用
        {}

        virtual void start() override
        {
            _server.setConnectionCallback(std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
            _server.setMessageCallback(std::bind(&MuduoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            
            _server.start();  // 先开始监听
            _baseloop.loop(); // 开始循环事件监控
        }

    private:
        // 这个函数会在网络连接改变时，服务端调用。
        // 如果是新来一个连接，就把记录到自己的_conns表中，同时调用新建连接后的业务函数
        // 如果是有一个连接断了，就把他从自己的_conns表中删除，同时调用断开连接后的业务函数
        void onConnection(const muduo::net::TcpConnectionPtr& conn)
        {
            if(conn->connected())
            {
                LOG(LogLevel::INFO) << "连接建立!";
                auto muduo_conn = ConnectionFactory::create(conn, _protocol);
                {
                    // 加锁保护
                    std::unique_lock<std::mutex> lock(_mutex);
                    _conns.insert(std::make_pair(conn, muduo_conn));
                }
                if(_cb_connection)
                    _cb_connection(muduo_conn);
            }
            else 
            {
                LOG(LogLevel::INFO) << "连接断开!";
                BaseConnection::ptr muduo_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if(it == _conns.end())
                    {
                        return;
                    }
                    muduo_conn = it->second;
                    _conns.erase(conn);
                }
                if(_cb_close)
                    _cb_close(muduo_conn);
            }
        }

        // 这个函数会在收到数据时，服务端调用。
        // 如果缓冲区中有一条完整消息，取出，然后调用处理消息的业务函数
        void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp)
        {
            //LOG(LogLevel::INFO) << "有消息到来,开始处理!";
            auto base_buf = BufferFactory::create(buf);
            while(1)
            {
                if(_protocol->canProcessed(base_buf) == false) // 当前缓冲区数据不够一条消息 
                {
                    if(base_buf->readableSize() > maxDataSize)
                    {
                        // 设置一个maxDataSize, 防止有恶意用户发送过大数据
                        LOG(LogLevel::WARNING) << "缓冲区中数据过大";
                        conn->shutdown();
                        return;
                    }
                    //LOG(LogLevel::INFO) << "缓冲区中数据量不足";
                    break;
                }

                // 从缓冲区中取出一条消息
                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if(ret == false)
                {
                    conn->shutdown();
                    LOG(LogLevel::ERROR) << "缓冲区中数据错误!";
                    return;
                }

                BaseConnection::ptr base_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end()) 
                    {
                        conn->shutdown();
                        return;
                    }
                    base_conn = it->second;
                }
                if(_cb_message)
                    _cb_message(base_conn, msg);
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        muduo::net::EventLoop _baseloop;
        muduo::net::TcpServer _server;
        BaseProtocol::ptr _protocol;
        std::mutex _mutex;
        // Muduo库的Connection类与我们自己的Connection类的映射
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns;
    };

    class ServerFactory
    {
    public:
        template<typename ...Args>
        static BaseServer::ptr create(Args&& ...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };



    class MuduoClient : public BaseClient
    {
    public:
        using ptr = std::shared_ptr<MuduoClient>;

        MuduoClient(const std::string& sip, int sport)
            : _protocol(ProtocolFactory::create())
            , _baseloop(_loopthread.startLoop())
            , _downlatch(1)
            , _client(_baseloop, muduo::net::InetAddress(sip, sport), "MuduoClient")
        {}

        virtual void connect() override
        {
            _client.setConnectionCallback(std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
            _client.setMessageCallback(std::bind(&MuduoClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
            _client.connect();
            _downlatch.wait();
            LOG(LogLevel::INFO) << "连接服务器成功!";
        }

        virtual void shutdown() override
        {
            _client.disconnect();
        }

        virtual bool send(const BaseMessage::ptr& msg) override
        {
            if (connected() == false) 
            {
                LOG(LogLevel::INFO) << "连接已断开!";
                return false;
            }
            _conn->send(msg);
            return true;
        }

        virtual BaseConnection::ptr connection() override
        {
            return _conn;
        }

        virtual bool connected() override
        {
            return _conn && _conn->connected();
        }

    private:
        // 这个函数会在网络连接改变时，客户端调用。
        // 当连接建立后，把连接记录到自己的成员_conn中
        // 当连接断开后，释放当前连接对象指针
        void onConnection(const muduo::net::TcpConnectionPtr& conn)
        {
            if (conn->connected()) 
            {
                //LOG(LogLevel::INFO) << "连接建立!";
                _downlatch.countDown();  //计数--,为0时唤醒阻塞
                _conn = ConnectionFactory::create(conn, _protocol);
            }
            else 
            {
                LOG(LogLevel::INFO) << "连接断开!";
                _conn.reset(); // 释放当前连接对象指针
            }
        }

        // 这个函数会在收到数据时，客户端调用。
        // 如果缓冲区中有一条完整消息，取出，然后调用处理消息的业务函数
        void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp)
        {
            //LOG(LogLevel::INFO) << "有消息到来,开始处理!";
            auto base_buf = BufferFactory::create(buf);
            while(1)
            {
                if(_protocol->canProcessed(base_buf) == false) // 当前缓冲区数据不够一条消息 
                {
                    if(base_buf->readableSize() > maxDataSize)
                    {
                        // 设置一个maxDataSize, 防止有恶意用户发送过大数据
                        LOG(LogLevel::WARNING) << "缓冲区中数据过大";
                        conn->shutdown();
                        return;
                    }
                    //LOG(LogLevel::INFO) << "缓冲区中数据量不足";
                    break;
                }

                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if(ret == false)
                {
                    conn->shutdown();
                    LOG(LogLevel::ERROR) << "缓冲区中数据错误!";
                    return;
                }

                if(_cb_message)
                    _cb_message(_conn, msg);
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        BaseProtocol::ptr _protocol;
        BaseConnection::ptr _conn;
        muduo::CountDownLatch _downlatch;
        muduo::net::EventLoopThread _loopthread;
        muduo::net::EventLoop* _baseloop;
        muduo::net::TcpClient _client;
    };

    class ClientFactory
    {
    public:
        template<typename ...Args>
        static BaseClient::ptr create(Args&& ...args)
        {
            return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
        }
    };

}