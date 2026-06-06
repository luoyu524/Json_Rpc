// client---DiscoverRegistry.hpp
#pragma once
#include "Requestor.hpp"
#include <memory>

namespace RPC 
{
    namespace client 
    {
        class Provider
        {
        public:
            using ptr = std::shared_ptr<Provider>;

            Provider(const Requestor::ptr &requestor) 
                : _requestor(requestor)
            {}

            bool registerMethod(const BaseConnection::ptr& conn, const std::string& method, const Address& host)
            {
                auto msg_req = MsgFactory::create<ServiceRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMsgtype(MsgType::REQ_SERVICE);
                msg_req->setMethod(method);
                msg_req->setHost(host);
                msg_req->setOptype(ServiceOpType::SERVICE_REGISTRY);

                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if(ret == false)
                {
                    LOG(LogLevel::ERROR) << method << "服务注册失败!";
                    return false;
                }
                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if(service_rsp.get() == nullptr)
                {
                    LOG(LogLevel::ERROR) << "响应类型向下转换失败!";
                    return false;
                }
                if (service_rsp->rcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "服务注册失败!" << errReason(service_rsp->rcode());
                    return false;
                }
                return true;
            }

        private:
            Requestor::ptr _requestor;
        };


        
        class Discoverer
        {
        
        public:
            using ptr = std::shared_ptr<Discoverer>;
            using OfflineCallback = std::function<void(const Address&)>;

            Discoverer(const Requestor::ptr &requestor, const OfflineCallback &cb) 
                : _requestor(requestor), _offline_callback(cb)
            {}

            // 服务发现函数，如果当前的_method_hosts中存在该服务并且有能提供服务的主机，选择一个。如果没有，则发起一次服务发现请求
            bool serviceDiscover(const BaseConnection::ptr& conn, const std::string& method, Address& host)
            {
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _method_hosts.find(method);
                    if(it != _method_hosts.end())
                    {
                        if(it->second->empty() == false)
                        {
                            host = it->second->chooseHost();
                            return true;
                        }
                    }
                }

                // 当前没有能提供服务的主机，向注册中心发现一次服务发现请求
                auto msg_req = MsgFactory::create<ServiceRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMsgtype(MsgType::REQ_SERVICE);
                msg_req->setMethod(method);
                msg_req->setOptype(ServiceOpType::SERVICE_DISCOVERY);

                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if(ret == false)
                {
                    LOG(LogLevel::ERROR) << "服务发现请求失败!";
                    return false;
                }
                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if(service_rsp.get() == nullptr)
                {
                    LOG(LogLevel::ERROR) << "服务发现响应类型向下转换失败!";
                    return false;
                }
                if (service_rsp->rcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "服务发现失败!" << errReason(service_rsp->rcode());
                    return false;
                }

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto method_host = std::make_shared<MethodHost>(service_rsp->hosts());
                    if(method_host->empty())
                    {
                        // 说明注册中心中，这个服务就没有提供者
                        LOG(LogLevel::INFO) << method << "服务发现失败! 没有能提供该服务的主机!";
                        return false;
                    }
                    host = method_host->chooseHost();
                    _method_hosts[method] = method_host;
                    return true;
                }
            }

            // 这个函数要注册到dispatcher模块，服务发现客户端针对服务上下线请求处理的回调函数
            void onServiceRequest(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                auto optype = msg->optype();
                std::string method = msg->method();

                std::unique_lock<std::mutex> lock(_mutex);
                if (optype == ServiceOpType::SERVICE_ONLINE)
                {
                    // 如果是上线请求：找到MethodHost，向其中新增一个主机地址
                    auto it = _method_hosts.find(method);
                    if (it == _method_hosts.end())
                    {
                        auto method_host = std::make_shared<MethodHost>();
                        method_host->appendHost(msg->host());
                        _method_hosts[method] = method_host;
                    }
                    else
                    {
                        it->second->appendHost(msg->host());
                    }
                }
                else if (optype == ServiceOpType::SERVICE_OFFLINE)
                {
                    // 如果是下线请求：找到MethodHost，从其中删除这个下线的主机地址
                    auto it = _method_hosts.find(method);
                    if (it == _method_hosts.end())
                    {
                        return;
                    }
                    it->second->removeHost(msg->host());
                    _offline_callback(msg->host());
                }
            }   

        private:
            class MethodHost
            {
            public:
                using ptr = std::shared_ptr<MethodHost>;

                MethodHost() : _idx(0) {}

                MethodHost(const std::vector<Address>& hosts) : _hosts(hosts), _idx(0) {}

                void appendHost(const Address& host)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _hosts.push_back(host);
                }

                void removeHost(const Address& host)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    for (auto it = _hosts.begin(); it != _hosts.end(); it++)
                    {
                        if (*it == host)
                        {
                            _hosts.erase(it);
                            break;
                        }
                    }
                }

                // 从一个服务的众多提供者中选择一个，这里采用轮转访问，分散载荷压力
                Address chooseHost()
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    size_t pos = _idx % _hosts.size();
                    _idx++;
                    return _hosts[pos];
                };

                bool empty()
                {
                    return _hosts.empty();
                }

            private:
                std::mutex _mutex;
                size_t _idx;
                std::vector<Address> _hosts;
            };

        private:
            OfflineCallback _offline_callback;
            std::mutex _mutex;
            std::unordered_map<std::string, MethodHost::ptr> _method_hosts;
            Requestor::ptr _requestor;
        };
    }
}
