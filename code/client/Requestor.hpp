// Requestor.hpp
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <future>
#include <functional>

namespace RPC
{
    namespace client 
    {
        // 提供发送请求的接口
        // 内部需要对每条请求进行管理起来，以便收到一条响应后找到对应的请求再具体处理
        class Requestor
        {
        public:
            using ptr = std::shared_ptr<Requestor>;
            using RequestCallback = std::function<void(const BaseMessage::ptr&)>;
            using AsyncResponse = std::future<BaseMessage::ptr>;
            struct RequestDescribe 
            {
                using ptr = std::shared_ptr<RequestDescribe>;
                BaseMessage::ptr request;
                ReqType rtype;

                std::promise<BaseMessage::ptr> response;
                RequestCallback callback;
            };

            // 这个函数会注册到Dispatcher模块，是客户端针对收到一条Rpc响应进行回调处理的业务函数
            void onResponse(const BaseConnection::ptr& conn, BaseMessage::ptr& msg)
            {   
                // 根据rid，找到这条响应对应的请求，根据请求中的ReqType判断怎么处理响应
                std::string rid = msg->rid();
                RequestDescribe::ptr rdp = getDescribe(rid);
                if(rdp.get() == nullptr)
                {
                    LOG(LogLevel::ERROR) << "收到响应" << rid << "但是未找到对应的请求描述!";
                    return;
                }

                if(rdp->rtype == ReqType::REQ_ASYNC)
                {
                    rdp->response.set_value(msg);
                }
                else if(rdp->rtype == ReqType::REQ_CALLBACK)
                {
                    if(rdp->callback)
                        rdp->callback(msg);
                }
                else 
                {
                    LOG(LogLevel::ERROR) << "未知的请求类型!";
                }
                delDescribe(rid);
            }
            
            // 同步方式请求
            bool send(const BaseConnection::ptr& conn, const BaseMessage::ptr& req, BaseMessage::ptr& rsp) 
            {
                AsyncResponse rsp_future;
                bool ret = send(conn, req, rsp_future);
                if (ret == false) 
                {
                    return false;
                }
                // 其实还是异步的send，只是这里同步获取结果，外界感知还是同步的
                rsp = rsp_future.get();
                return true;
            }

            // 异步方式请求
            bool send(const BaseConnection::ptr& conn, const BaseMessage::ptr& req, AsyncResponse& async_rsp)
            {
                RequestDescribe::ptr rdp = newDescribe(req, ReqType::REQ_ASYNC);
                if (rdp.get() == nullptr) 
                {
                    LOG(LogLevel::ERROR) << "构造请求描述对象失败!";
                    return false;
                }
                conn->send(req);
                async_rsp = rdp->response.get_future();
                return true;
            }

            // 回调方式请求
            bool send(const BaseConnection::ptr& conn, const BaseMessage::ptr& req, const RequestCallback& cb) 
            {
                RequestDescribe::ptr rdp = newDescribe(req, ReqType::REQ_CALLBACK, cb);
                if (rdp.get() == nullptr) 
                {
                    LOG(LogLevel::ERROR) << "构造请求描述对象失败!";
                    return false;
                }
                conn->send(req);
                return true;
            }

        private:
            RequestDescribe::ptr newDescribe(const BaseMessage::ptr& req, ReqType rtype, const RequestCallback& cb = RequestCallback())
            {
                std::unique_lock<std::mutex> lock(_mutex);
                RequestDescribe::ptr rd = std::make_shared<RequestDescribe>();
                rd->request = req;
                rd->rtype = rtype;
                if (rtype == ReqType::REQ_CALLBACK && cb) 
                {
                    rd->callback = cb;
                }
                _request_desc.insert(std::make_pair(req->rid(), rd));
                return rd;
            }

            RequestDescribe::ptr getDescribe(const std::string &rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _request_desc.find(rid);
                if(it == _request_desc.end())
                {
                    return RequestDescribe::ptr();
                }
                return it->second;
            }

            void delDescribe(const std::string &rid)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _request_desc.erase(rid);
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, RequestDescribe::ptr> _request_desc; // <rid---一条请求>的映射
        };
    }
}