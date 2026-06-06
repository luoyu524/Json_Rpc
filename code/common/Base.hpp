// Base.hpp
// 抽象层
#pragma once
#include "fields.hpp"
#include <functional>
#include <memory>
#include <string>

namespace RPC
{
    class BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<BaseBuffer>;

        virtual size_t readableSize() = 0;
        virtual int32_t peekInt32() = 0;   // 尝试取出前四字节数据
        virtual void retrieveInt32() = 0;  // 删除前四字节数据 

        virtual int32_t readInt32() = 0;   // 取出并删除前四字节数据
        virtual std::string retrieveAsString(size_t len) = 0;
    };

    class BaseMessage
    {
    public:
        using ptr = std::shared_ptr<BaseMessage>;

        virtual ~BaseMessage(){};

        virtual void setId(const std::string& id)
        {
            _rid = id;
        }
        virtual void setMsgtype(MsgType mtype)
        {
            _mtype = mtype;
        }
        virtual std::string rid()
        {
            return _rid;
        }
        virtual MsgType mtype()
        {
            return _mtype;
        }

        virtual std::string serialize() = 0;
        virtual bool unserialize(const std::string& msg) = 0;
        virtual bool check() = 0;

    private:
        MsgType _mtype;
        std::string _rid;
    };

    class BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;
        virtual void send(const BaseMessage::ptr& msg) = 0;
        virtual void shutdown() = 0;
        virtual bool connected() = 0;
    };

    class BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;
        virtual bool canProcessed(const BaseBuffer::ptr& buf) = 0;
        virtual bool onMessage(const BaseBuffer::ptr& buf, BaseMessage::ptr& msg) = 0;
        virtual std::string serialize(const BaseMessage::ptr& msg) = 0;
    };

    using ConnectionCallBack = std::function<void(const BaseConnection::ptr&)>;
    using CloseCallBack = std::function<void(const BaseConnection::ptr&)>;
    using MessageCallBack = std::function<void(const BaseConnection::ptr&, BaseMessage::ptr&)>;
    class BaseServer
    {
    public:
        using ptr = std::shared_ptr<BaseServer>;
        virtual void setConnectionCallBack(const ConnectionCallBack& cb)
        {
            _cb_connection = cb;
        }
        virtual void setCloseCallBack(const CloseCallBack& cb)
        {
            _cb_close = cb;
        }
        virtual void setMessageCallBack(const MessageCallBack& cb)
        {
            _cb_message = cb;
        }
        virtual void start() = 0;

    protected:
        ConnectionCallBack _cb_connection;
        CloseCallBack _cb_close;
        MessageCallBack _cb_message;
    };

    class BaseClient
    {
    public:
        using ptr = std::shared_ptr<BaseClient>;
        virtual void setConnectionCallBack(const ConnectionCallBack& cb)
        {
            _cb_connection = cb;
        }
        virtual void setCloseCallBack(const CloseCallBack& cb)
        {
            _cb_close = cb;
        }
        virtual void setMessageCallBack(const MessageCallBack& cb)
        {
            _cb_message = cb;
        }

        virtual void connect() = 0;
        virtual void shutdown() = 0;
        virtual bool send(const BaseMessage::ptr& msg) = 0;
        virtual BaseConnection::ptr connection() = 0;
        virtual bool connected() = 0;
        
    protected:
        ConnectionCallBack _cb_connection;
        CloseCallBack _cb_close;
        MessageCallBack _cb_message;
    };

}