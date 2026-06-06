// server---DiscoverRegistry.hpp
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include <unordered_map>

namespace RPC 
{
    namespace server 
    {
        class ProviderManager
        {
        public:
            using ptr = std::shared_ptr<ProviderManager>;

            struct Provider
            {
                using ptr =  std::shared_ptr<Provider>;
                std::mutex mutex;
                BaseConnection::ptr conn;
                Address host;
                std::vector<std::string> methods;

                Provider(const BaseConnection::ptr& c, const Address& h)
                    : conn(c), host(h)
                {}

                void appendMethod(const std::string& method)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    methods.emplace_back(method);
                }
            };

            // 一个服务提供者, 注册一个新服务时调用
            void addProvider(const BaseConnection::ptr& c, const Address& h, const std::string method)
            {
                Provider::ptr provider;
                // 查找当前服务提供者是否已存在,找到则获取; 找不到则新建映射,并建立关联
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if(it == _conns.end())
                    {
                        provider = std::make_shared<Provider>(c, h);
                        _conns.insert(std::make_pair(c, provider));
                    }
                    else 
                    {
                        provider = it->second;
                    }
                    _providers[method].insert(provider);
                }
                provider->appendMethod(method);
            }

            // 当一个服务提供者断开连接时，需要知道他的信息，以便对他提供的服务的发现者做下线通知
            Provider::ptr getProvider(const BaseConnection::ptr& c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    return Provider::ptr();
                }
                return it->second;
            }

            // 当一个服务提供者断开连接时，删除它的关联信息
            void delProvider(const BaseConnection::ptr& c)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    return;
                }
                // 找这个服务提供者提供的所有服务，从服务提供者表中删除当前服务提供者
                for(auto& method : it->second->methods)
                {
                    _providers[method].erase(it->second);
                }
                // 从连接映射表中删除该连接
                _conns.erase(it);
            }

            // 获取一个服务的所有提供者主机地址
            std::vector<Address> methodHosts(const std::string& method)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _providers.find(method);
                if(it == _providers.end())
                {
                    return std::vector<Address>();
                }
                std::vector<Address> ret;
                for(auto& provider : it->second)
                {
                    ret.push_back(provider->host);
                }
                return ret;
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, std::set<Provider::ptr>> _providers; // <一个服务---所有提供这个服务的提供者>
            std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns; // <一个连接---对应的提供者>
        };


        class DiscovererManager
        {
        public:
            using ptr = std::shared_ptr<DiscovererManager>;
            
            struct Discoverer
            {
                using ptr = std::shared_ptr<Discoverer>;
                std::mutex mutex;
                BaseConnection::ptr conn; // 发现者关联的客户端连接
                std::vector<std::string> methods; // 发现过的服务名称
                
                Discoverer(const BaseConnection::ptr& c)
                    : conn(c)
                {}

                void appendMethod(const std::string& method)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    methods.emplace_back(method);
                }
            };

            // 一个服务发现者, 发现一个服务时调用
            Discoverer::ptr addDiscoverer(const BaseConnection::ptr& c, const std::string& method)
            {
                Discoverer::ptr discoverer;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(c);
                    if(it == _conns.end())
                    {
                        discoverer = std::make_shared<Discoverer>(c);
                        _conns.insert(std::make_pair(c, discoverer));
                    }
                    else 
                    {
                        discoverer = it->second;
                    }
                    _discoverers[method].insert(discoverer);
                }
                discoverer->appendMethod(method);
                return discoverer;
            }

            //发现者客户端断开连接时，找到发现者信息，删除关联数据
            void delDiscoverer(const BaseConnection::ptr &c) 
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _conns.find(c);
                if(it == _conns.end())
                {
                    return;
                }
                for(auto& method : it->second->methods)
                {
                    _discoverers[method].erase(it->second);
                }
                _conns.erase(it);
            }

            void onlineNotify(const std::string& method, const Address& host)
            {
                notify(method, host, ServiceOpType::SERVICE_ONLINE);
            }

            void offlineNotify(const std::string& method, const Address& host)
            {
                notify(method, host, ServiceOpType::SERVICE_OFFLINE);
            }
        
        private:
            // 组织一条服务上线/下线的服务请求消息，发送给每个这个服务的发现者
            void notify(const std::string& method, const Address& host, ServiceOpType optype)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _discoverers.find(method);
                if(it == _discoverers.end()) // 代表这个服务没有发现者
                {
                    return;
                }
                auto msg_req = MsgFactory::create<ServiceRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMsgtype(MsgType::REQ_SERVICE);
                msg_req->setOptype(optype);
                msg_req->setMethod(method);
                msg_req->setHost(host);
                for(auto& discover : it->second)
                {
                    discover->conn->send(msg_req);
                }
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers; // <一个服务---所有它的发现者>
            std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns; // <一个连接---对应的发现者>
        };


        class PDManager
        {
        public: 
            using ptr = std::shared_ptr<PDManager>;
            PDManager()
                : _providers(std::make_shared<ProviderManager>())
                , _discoverers(std::make_shared<DiscovererManager>())
            {}
            
            // 这个函数会注册到注册中心服务端dispatcher模块，是针对服务请求类型消息(服务注册/服务发现)的业务回调函数
            void onServiceMessgage(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                ServiceOpType optype = msg->optype();
                if(optype == ServiceOpType::SERVICE_REGISTRY)
                {
                    LOG(LogLevel::INFO) << msg->host().first << ":" << msg->host().second << "注册服务" << msg->method();
                    _providers->addProvider(conn, msg->host(), msg->method());
                    _discoverers->onlineNotify(msg->method(), msg->host());
                    registryResponse(conn, msg);
                }
                else if(optype == ServiceOpType::SERVICE_DISCOVERY)
                {
                    LOG(LogLevel::INFO) << msg->host().first << ":" << msg->host().second << "要发现服务" << msg->method();
                    _discoverers->addDiscoverer(conn, msg->method());
                    discoveryResponse(conn, msg);
                }
                else 
                {
                    LOG(LogLevel::ERROR) << "收到服务操作请求，但是操作类型错误!";
                    errorResponse(conn, msg);
                }
            }

            // 这个函数会设置到连接关闭时的回调函数中
            void onConnShutdown(const BaseConnection::ptr& conn)
            {
                auto provider = _providers->getProvider(conn);
                if (provider.get() != nullptr)
                {
                    LOG(LogLevel::INFO) << provider->host.first << ":" << provider->host.second << "服务下线";
                    for (auto& method : provider->methods)
                    {
                        _discoverers->offlineNotify(method, provider->host);
                    }
                    _providers->delProvider(conn);
                }
                _discoverers->delDiscoverer(conn);
            }

        private:
            void errorResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                auto msg_rsp = MsgFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMsgtype(MsgType::RSP_SERVICE);
                msg_rsp->setRCode(RCode::RCODE_INVALID_OPTYPE);
                msg_rsp->setOptype(ServiceOpType::SERVICE_UNKNOW);
                conn->send(msg_rsp);
            }

            // 服务注册的响应信息，正文只需要包含结果是否成功即可
            void registryResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                auto msg_rsp = MsgFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMsgtype(MsgType::RSP_SERVICE);
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setOptype(ServiceOpType::SERVICE_REGISTRY);
                conn->send(msg_rsp);
            }

            // 服务发现的响应信息，正文中还需包含服务的提供者都有哪些主机
            void discoveryResponse(const BaseConnection::ptr& conn, const ServiceRequest::ptr& msg)
            {
                auto msg_rsp = MsgFactory::create<ServiceResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMsgtype(MsgType::RSP_SERVICE);
                msg_rsp->setOptype(ServiceOpType::SERVICE_DISCOVERY);
                std::vector<Address> hosts = _providers->methodHosts(msg->method());
                if (hosts.empty())
                {
                    msg_rsp->setRCode(RCode::RCODE_NOT_FOUND_SERVICE);
                    return conn->send(msg_rsp);
                }
                msg_rsp->setRCode(RCode::RCODE_OK);
                msg_rsp->setMethod(msg->method());
                msg_rsp->setHost(hosts);
                return conn->send(msg_rsp);
            }

        private:
            ProviderManager::ptr _providers;
            DiscovererManager::ptr _discoverers;
        };

    }
}