// RpcRouter.hpp
#pragma once
#include "../common/message.hpp"
#include <jsoncpp/json/value.h>
#include <memory>
#include <mutex>
#include <vector>

namespace RPC 
{
    namespace server
    {
        // 枚举每一种Json::Value成员类型
        enum class ValType
        {
            BOOL,
            INTEGRAL,
            NUMERIC,
            STRING,
            ARRAY,
            OBJECT,
        };

        class ServiceDescribe
        {
        public:
            using ptr = std::shared_ptr<ServiceDescribe>;
            using ServiceCallBack = std::function<void(const Json::Value&, Json::Value&)>; // 参数集合--返回值
            using ParamsDescribe = std::pair<std::string, ValType>; //参数名字--参数类型
            
            ServiceDescribe(std::string&& mname, std::vector<ParamsDescribe>&& desc, ValType vtype, ServiceCallBack &&handler)
                : _method_name(std::move(mname))
                , _callback(std::move(handler))
                , _params_desc(std::move(desc))
                , _return_type(vtype)
            {}

            const std::string& method()
            {
                return _method_name;
            }

            // 检查提供的参数字段/类型，是否满足这个服务要求
            bool paramCheck(const Json::Value& params)
            {
                for(auto& desc : _params_desc)
                {
                    if(params.isMember(desc.first) == false)
                    {
                        LOG(LogLevel::ERROR) << "参数字段完整性校验失败!" << desc.first << "字段缺失!";
                        return false;
                    }
                    if(checkType(desc.second, params[desc.first]) == false)
                    {
                        LOG(LogLevel::ERROR) << "参数类型校验失败!" << desc.first << "类型错误!";
                        return false;
                    }
                }
                return true; 
            }

            // 真正调用服务的方法功能
            bool call(const Json::Value& params, Json::Value& result)
            {
                _callback(params, result);
                if(checkType(_return_type, result) == false)
                {
                    LOG(LogLevel::ERROR) << "回调处理函数中的响应信息类型错误";
                    return false;
                }
                return true;
            }

        private:
            bool checkType(ValType vtype, const Json::Value& val) 
            {
                switch(vtype) 
                {
                    case ValType::BOOL : return val.isBool();
                    case ValType::INTEGRAL : return val.isIntegral();
                    case ValType::NUMERIC : return val.isNumeric();
                    case ValType::STRING : return val.isString();
                    case ValType::ARRAY : return val.isArray();
                    case ValType::OBJECT : return val.isObject();
                }
                return false;
            }

        private:
            std::string _method_name;  // 方法名称
            ServiceCallBack _callback; // 一个服务的真正方法功能函数
            std::vector<ParamsDescribe> _params_desc; // 参数字段描述
            ValType _return_type; // 方法函数返回值类型
        };

        // 建造者模式
        class ServiceDescFactory
        {
        public:
            void setMethodName(const std::string& name) 
            {
                _method_name = name;
            }

            void setReturnType(ValType vtype) 
            {
                _return_type = vtype;
            }
                
            void setParamsDesc(const std::string& pname, ValType vtype) 
            {
                _params_desc.push_back(ServiceDescribe::ParamsDescribe(pname, vtype));
            }
                
            void setCallback(const ServiceDescribe::ServiceCallBack& cb) 
            {
                _callback = cb;
            }

            ServiceDescribe::ptr build() 
            {
                return std::make_shared<ServiceDescribe>(std::move(_method_name), std::move(_params_desc), _return_type, std::move(_callback));
            }
        private:
            std::string _method_name;
            ServiceDescribe::ServiceCallBack _callback;  
            std::vector<ServiceDescribe::ParamsDescribe> _params_desc; 
            ValType _return_type; 
        };

        // 所有服务的管理类
        class ServiceManager
        {
        public:
            using ptr = std::shared_ptr<ServiceManager>;

            // 新增一个服务
            void insert(const ServiceDescribe::ptr& desc)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services.insert(std::make_pair(desc->method(), desc));
            }

            // 查找一个服务
            ServiceDescribe::ptr select(const std::string& method_name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _services.find(method_name);
                if (it == _services.end()) 
                {
                    return ServiceDescribe::ptr();
                }
                return it->second;
            }

            // 移除一个服务
            void remove(const std::string &method_name) 
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services.erase(method_name);
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, ServiceDescribe::ptr> _services;
        };

        class RpcRouter
        {
        public:
            using ptr = std::shared_ptr<RpcRouter>;

            RpcRouter()
                : _service_manager(std::make_shared<ServiceManager>())
            {}

            // 这个函数会注册到Dispatcher模块，是服务端针对收到一条Rpc请求进行回调处理的业务函数
            void onRpcRequest(const BaseConnection::ptr& conn, RpcRequest::ptr& request)
            {
                // 1.查询客户端请求的方法是否存在
                auto service = _service_manager->select(request->method());
                if(service.get() == nullptr)
                {
                    LOG(LogLevel::INFO) << "当前服务未找到: " << request->method();
                    response(conn, request, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
                    return;
                }
                // 2.校验请求中的参数是否符合服务要求
                if(service->paramCheck(request->params()) == false)
                {
                    LOG(LogLevel::INFO) << "当前服务参数校验失败: " << request->method();
                    response(conn, request, Json::Value(), RCode::RCODE_INVALID_PARAMS);
                    return;
                }
                // 3.调用业务回调接口进行处理
                Json::Value result;
                bool ret = service->call(request->params(), result);
                if(ret == false)
                {
                    LOG(LogLevel::INFO) << "当前服务内部失败: " << request->method();
                    response(conn, request, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                    return;
                }
                // 4.处理完毕得到结果，组织响应向客户端发送
                response(conn, request, result, RCode::RCODE_OK);
            }

            void registerMethod(const ServiceDescribe::ptr& service)
            {
                _service_manager->insert(service);
            }
            
        private:
            void response(const BaseConnection::ptr& conn, const RpcRequest::ptr& req, const Json::Value &res, RCode rcode) 
            {
                auto msg = MsgFactory::create<RpcResponse>();
                msg->setId(req->rid());
                msg->setMsgtype(MsgType::RSP_RPC);
                msg->setRCode(rcode);
                msg->setResult(res);

                conn->send(msg);
            }

        private:
            ServiceManager::ptr _service_manager;
        };
    }
}
