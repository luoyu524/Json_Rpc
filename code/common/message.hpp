// message.hpp
#pragma once
#include "details.hpp"
#include "Base.hpp"
#include "fields.hpp"
#include <utility>
#include <vector>

namespace RPC
{
    typedef std::pair<std::string, int> Address; // ip + port

    class JsonMessage : public BaseMessage
    {
    public:
        using ptr = std::shared_ptr<JsonMessage>;

        virtual std::string serialize() override
        {   
            std::string body;
            if(JSON::serialize(_body, &body) == false)
            {
                return std::string();
            }
            return body;
        }

        virtual bool unserialize(const std::string& msg) override
        {
            return JSON::unserialize(msg, &_body);
        }

    protected:
        Json::Value _body;
    };



    class JsonRequest : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonRequest>;
    };

    class RpcRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<RpcRequest>;

        virtual bool check() override
        {
            // Rpc请求中，应该包含请求方法名称(字符串)、参数信息(对象)
            if(_body[KEY_METHOD].isNull() || _body[KEY_METHOD].isString() == false)
            {
                LOG(LogLevel::ERROR) << "Rpc请求中没有方法名称或方法名称类型错误!";
                return false;
            }
            if(_body[KEY_PARAMS].isNull() || _body[KEY_PARAMS].isObject() == false)
            {
                LOG(LogLevel::ERROR) << "Rpc请求中没有参数信息或参数信息类型错误!";
                return false;
            }
            return true;
        }

        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        void setMethod(const std::string& method)
        {
            _body[KEY_METHOD] = method;
        }

        Json::Value params()
        {
            return _body[KEY_PARAMS];
        }

        void setParams(const Json::Value& params)
        {
            _body[KEY_PARAMS] = params;
        }

    };

    class TopicRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<TopicRequest>;
        virtual bool check() override
        {
            // Topic请求中，应该包含主题名称(字符串)、操作类型
            if(_body[KEY_TOPIC_KEY].isNull() || _body[KEY_TOPIC_KEY].isString() == false)
            {
                LOG(LogLevel::ERROR) << "主题请求中没有主题名称或主题名称类型错误!";
                return false;
            }
            if(_body[KEY_OPTYPE].isNull() || _body[KEY_OPTYPE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "主题请求中没有操作类型信息或操作类型的类型错误!";
                return false;
            }

            // 如果Topic请求的操作类型是“发布主题”，则还应该包含消息信息字段
            if(_body[KEY_OPTYPE].asInt() == (int)TopicOpType::TOPIC_PUBLISH &&
               (_body[KEY_TOPIC_MSG].isNull() || _body[KEY_TOPIC_MSG].isString() == false))
            {
                LOG(LogLevel::ERROR) << "主题发布请求中没有消息信息或消息信息的类型错误!";
                return false;
            }

            return true;
        }

        std::string topicKey()
        {
            return _body[KEY_TOPIC_KEY].asString();
        }

        void setTopicKey(const std::string& key)
        {
            _body[KEY_TOPIC_KEY] = key;
        }

        TopicOpType optype()
        {
            return (TopicOpType)_body[KEY_OPTYPE].asInt();
        }

        void setOptype(TopicOpType optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        std::string topicMsg() 
        {
            return _body[KEY_TOPIC_MSG].asString();
        }
            
        void setTopicMsg(const std::string &msg)
        {
            _body[KEY_TOPIC_MSG] = msg;
        }
    };

    class ServiceRequest: public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;
        virtual bool check() override
        {
            // Service请求中，应该包含服务名称(字符串)、操作类型
            if(_body[KEY_METHOD].isNull() || _body[KEY_METHOD].isString() == false)
            {
                LOG(LogLevel::ERROR) << "服务请求中没有服务名称或服务名称类型错误!";
                return false;
            }
            if(_body[KEY_OPTYPE].isNull() || _body[KEY_OPTYPE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "服务请求中没有操作类型信息或操作类型的类型错误!";
                return false;
            }

            // 如果操作类型不是“发现请求”，而是其他的“服务注册、服务上线、服务下线”，则请求中还应该有主机地址信息字段
            if(_body[KEY_OPTYPE].asInt() != (int)(ServiceOpType::SERVICE_DISCOVERY) &&
               (_body[KEY_HOST].isNull() || _body[KEY_HOST].isObject() == false ||
                _body[KEY_HOST][KEY_HOST_IP].isNull() || _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
                _body[KEY_HOST][KEY_HOST_PORT].isNull() || _body[KEY_HOST][KEY_HOST_PORT].isInt() == false))
            {
                LOG(LogLevel::ERROR) << "服务请求中主机地址信息错误!";
                return false;
            }

            return true;
        }

        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        void setMethod(const std::string& method)
        {
            _body[KEY_METHOD] = method;
        }

        ServiceOpType optype()
        {
            return (ServiceOpType)_body[KEY_OPTYPE].asInt();
        }

        void setOptype(ServiceOpType optype)
        {   
            _body[KEY_OPTYPE] = (int)optype;
        }

        Address host()
        {
            Address addr;
            addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
            return addr;
        }

        void setHost(const Address& host)
        {
            Json::Value val;
            val[KEY_HOST_IP] = host.first;
            val[KEY_HOST_PORT] = host.second;
            _body[KEY_HOST] = val;
        }
    };



    class JsonResponse : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonResponse>;
        virtual bool check() override
        {
            // 大部分响应只有RspCode
            // 只要判断响应字段是否存在，类型是否正确即可
            if(_body[KEY_RCODE].isNull())
            {
                LOG(LogLevel::ERROR) << "响应中没有RCode!";
                return false;
            }
            if(_body[KEY_RCODE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "响应中RCode类型错误!";
                return false;
            }
            return true;
        }

        virtual RCode rcode()
        {
            return (RCode)_body[KEY_RCODE].asInt();
        }

        virtual void setRCode(RCode rcode)
        {
            _body[KEY_RCODE] = (int)rcode;
        }
    };

    class RpcResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<RpcResponse>;
        virtual bool check() override
        {
            if(_body[KEY_RCODE].isNull() || _body[KEY_RCODE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "RPC响应中RCode不存在或RCode类型错误!";
                return false;
            }

            // 服务的返回结果可能是各种类型，这里不判断类型
            if(_body[KEY_RESULT].isNull())
            {
                LOG(LogLevel::ERROR) << "RPC响应中没有RPC调用结果!";
                return false;
            }
            return true;
        }

        Json::Value result()
        {
            return _body[KEY_RESULT];
        }

        void setResult(const Json::Value& result)
        {
            _body[KEY_RESULT] = result;
        }
    };

    class TopicResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<TopicResponse>;
        // Topic响应中只会有RCode, 父类中已经实现好check了
    };

    class ServiceResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<ServiceResponse>;
        virtual bool check() override
        {
            if(_body[KEY_RCODE].isNull() || _body[KEY_RCODE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "服务响应中RCode不存在或RCode类型错误!";
                return false;
            }
            if (_body[KEY_OPTYPE].isNull() || _body[KEY_OPTYPE].isInt() == false) 
            {
                LOG(LogLevel::ERROR) << "服务响应中没有操作类型或操作类型的类型错误!";
                return false;
            }

            // 如果操作类型是“服务发现”，则响应中还应该有method和host数组字段
            if(_body[KEY_OPTYPE].asInt() == (int)(ServiceOpType::SERVICE_DISCOVERY) &&
              (_body[KEY_METHOD].isNull() || _body[KEY_METHOD].isString() == false ||
               _body[KEY_HOST].isNull() || _body[KEY_HOST].isArray() == false))
            {
                LOG(LogLevel::ERROR) << "服务发现响应中信息字段错误!";
                return false;
            }
            return true;
        }

        ServiceOpType optype()
        {
            return (ServiceOpType)_body[KEY_OPTYPE].asInt();
        }

        void setOptype(ServiceOpType optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        std::string method() 
        {
            return _body[KEY_METHOD].asString();
        }
            
        void setMethod(const std::string &method) 
        {
            _body[KEY_METHOD] = method;
        }

        void setHost(std::vector<Address> addrs)
        {
            for(auto& addr : addrs)
            {
                Json::Value val;
                val[KEY_HOST_IP] = addr.first;
                val[KEY_HOST_PORT] = addr.second;
                _body[KEY_HOST].append(val);
            }
        }

        std::vector<Address> hosts()
        {
            std::vector<Address> addrs;
            int sz = _body[KEY_HOST].size();
            for(int i = 0; i < sz; i++)
            {
                Address addr;
                addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
                addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
                addrs.push_back(addr);
            }
            return addrs;
        }

    };


    // 工厂模式，实现一个消息对象的简单生产工厂
    class MsgFactory
    {   
    public:
        // 两种生产方法

        static BaseMessage::ptr create(MsgType mtype)
        {
            switch(mtype) 
            {
                case MsgType::REQ_RPC: return std::make_shared<RpcRequest>();
                case MsgType::RSP_RPC: return std::make_shared<RpcResponse>();
                case MsgType::REQ_TOPIC: return std::make_shared<TopicRequest>();
                case MsgType::RSP_TOPIC: return std::make_shared<TopicResponse>();
                case MsgType::REQ_SERVICE: return std::make_shared<ServiceRequest>();
                case MsgType::RSP_SERVICE: return std::make_shared<ServiceResponse>();
            }
            return BaseMessage::ptr();
        }

        template<typename T, typename ...Args>
        static std::shared_ptr<T> create(Args&& ...args)
        {
            return std::make_shared<T>(std::forward(args)...);
        }
    };

}