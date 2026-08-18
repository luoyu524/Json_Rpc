// RpcCaller.hpp
#pragma once
#include "Requestor.hpp"
#include <jsoncpp/json/value.h>
#include <memory>

namespace RPC
{
    namespace client
    {
        // 用户通过RpcCaller的接口发送Rpc请求，RpcCaller内部组织好一条请求，再通过Requestor发送
        class RpcCaller
        {
        public:
            using ptr = std::shared_ptr<RpcCaller>;
            using JsonAsyncResponse = std::future<Json::Value>;
            using JsonResponseCallback = std::function<void(const Json::Value&)>;

            RpcCaller(const Requestor::ptr& requestor)
                : _requestor(requestor)
            {}

            // 同步调用
            bool call(const BaseConnection::ptr& conn, const std::string& method, const Json::Value& params, Json::Value& result) 
            {
                // 1.组织请求
                auto req_msg = MsgFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMsgtype(MsgType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);

                // 2.发送请求
                BaseMessage::ptr rsp_msg;
                    // 调用重载函数必须参数类型一致
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), rsp_msg);
                if (ret == false) 
                {
                    LOG(LogLevel::ERROR) << "同步Rpc请求失败!";
                    return false;
                }

                // 3.等待响应
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
                if(!rpc_rsp_msg)
                {
                    LOG(LogLevel::ERROR) << "rpc响应向下类型转换失败!";
                    return false;
                }
                if(rpc_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "rpc请求出错: " << errReason(rpc_rsp_msg->rcode());
                    return false;
                }

                // 4.设置结果
                result = rpc_rsp_msg->result();
                return true;
            }

            // 异步调用
            bool call(const BaseConnection::ptr& conn, const std::string& method, const Json::Value& params, JsonAsyncResponse& result) 
            {
                auto req_msg = MsgFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMsgtype(MsgType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);

                auto json_promise = std::make_shared<std::promise<Json::Value>>();
                result = json_promise->get_future();
                
                Requestor::RequestCallback cb = std::bind(&RpcCaller::AsyncCallback, this, json_promise, std::placeholders::_1);
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), cb);
                if(ret == false)
                {
                    LOG(LogLevel::ERROR) << "异步rpc请求失败!";
                    return false;
                }
                return true;
            }

            // 回调函数调用
            bool call(const BaseConnection::ptr& conn, const std::string& method, const Json::Value& params, const JsonResponseCallback& cb)
            {
                auto req_msg = MsgFactory::create<RpcRequest>();
                req_msg->setId(UUID::uuid());
                req_msg->setMsgtype(MsgType::REQ_RPC);
                req_msg->setMethod(method);
                req_msg->setParams(params);

                Requestor::RequestCallback req_cb = std::bind(&RpcCaller::CbCallback, this, cb, std::placeholders::_1);
                bool ret = _requestor->send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), req_cb);
                if(ret == false)
                {
                    LOG(LogLevel::ERROR) << "回调rpc请求失败!";
                    return false;
                }
                return true;
            }

        private:
            void AsyncCallback(std::shared_ptr<std::promise<Json::Value>> result, const BaseMessage::ptr& msg)  
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp_msg) 
                {
                    LOG(LogLevel::ERROR) << "rpc响应向下类型转换失败!";
                    return;
                }
                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) 
                {
                    LOG(LogLevel::ERROR) << "rpc异步请求出错: " << errReason(rpc_rsp_msg->rcode());
                    return;
                }
                result->set_value(rpc_rsp_msg->result());
            }

            void CbCallback(const JsonResponseCallback& cb, const BaseMessage::ptr& msg)
            {
                auto rpc_rsp_msg = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp_msg) 
                {
                    LOG(LogLevel::ERROR) << "rpc响应向下类型转换失败!";
                    return;
                }
                if (rpc_rsp_msg->rcode() != RCode::RCODE_OK) 
                {
                    LOG(LogLevel::ERROR) << "rpc回调请求出错: " << errReason(rpc_rsp_msg->rcode());
                    return;
                }
                cb(rpc_rsp_msg->result());
            }
            
        private:
            Requestor::ptr _requestor;
        };

    } 
} 