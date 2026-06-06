// client.hpp
#pragma once
#include "../common/dispatcher.hpp"
#include "Requestor.hpp"
#include "RpcCaller​.hpp"
#include "DiscoverRegistry.hpp"
#include "PublishSubscribe.hpp"

namespace RPC
{
    namespace client
    {
        // 服务注册客户端
        class RegistryClient
        {
        public:
            using ptr = std::shared_ptr<RegistryClient>;

            //构造函数传入注册中心的地址信息，用于连接注册中心
            RegistryClient(const std::string& ip, int port)
                : _requestor(std::make_shared<Requestor>())
                , _provider(std::make_shared<client::Provider>(_requestor))
                , _dispatcher(std::make_shared<Dispatcher>())
                , _client(ClientFactory::create(ip, port))
            {
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MsgType::RSP_SERVICE, rsp_cb);
            
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client->setMessageCallBack(message_cb);
                _client->connect();
            }

            // 向外提供的服务注册接口
            bool registerMethod(const std::string& method, const Address& host)
            {
                return _provider->registerMethod(_client->connection(), method, host);
            }

        private:
            Requestor::ptr _requestor;
            client::Provider::ptr _provider;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };
        
        // 服务发现客户端
        class DiscoveryClient
        {
        public:
            using ptr = std::shared_ptr<DiscoveryClient>;
            //构造函数传入注册中心的地址信息，用于连接注册中心。同时需要提供服务提供者下线时的处理方法
            DiscoveryClient(const std::string& ip, int port, const Discoverer::OfflineCallback& cb)
                : _requestor(std::make_shared<Requestor>())
                , _discoverer(std::make_shared<client::Discoverer>(_requestor, cb))
                , _dispatcher(std::make_shared<Dispatcher>())
                , _client(ClientFactory::create(ip, port))
            {
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MsgType::RSP_SERVICE, rsp_cb);

                auto req_cb = std::bind(&client::Discoverer::onServiceRequest, _discoverer.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<ServiceRequest>(MsgType::REQ_SERVICE, req_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client->setMessageCallBack(message_cb);
                _client->connect();
            }

            //向外提供的服务发现接口
            bool serviceDiscovery(const std::string& method, Address& host)
            {
                return _discoverer->serviceDiscover(_client->connection(), method, host);
            }

        private: 
            Requestor::ptr _requestor;
            client::Discoverer::ptr _discoverer;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };
        
        // Rpc调用客户端
        class RpcClient
        {
        public:
            using ptr = std::shared_ptr<RpcClient>;
            // enableDiscovery--是否启用服务发现功能，也决定了传入的地址信息是注册中心的地址，还是服务提供者的地址
            RpcClient(bool enableDiscovery, const std::string& ip, int port)
                : _enableDiscovery(enableDiscovery)
                , _requestor(std::make_shared<Requestor>())
                , _dispatcher(std::make_shared<Dispatcher>())
                , _caller(std::make_shared<client::RpcCaller>(_requestor))
            {
                auto rsp_cb = std::bind(&client::Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MsgType::RSP_RPC, rsp_cb);

                //如果启用了服务发现，传入地址是注册中心的地址，是服务发现客户端需要连接的地址，则通过地址信息实例化_discovery_client
                //如果没有启用服务发现，则传入地址信息是服务提供者的地址，则直接实例化好_rpc_client
                if (_enableDiscovery)
                {
                    auto offline_cb = std::bind(&RpcClient::delClient, this, std::placeholders::_1);
                    _discovery_client = std::make_shared<DiscoveryClient>(ip, port, offline_cb);
                }
                else
                {
                    auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                    _rpc_client = ClientFactory::create(ip, port);
                    _rpc_client->setMessageCallBack(message_cb);
                    _rpc_client->connect();
                }
            }

            // 同步方式调用
            bool call(const std::string& method, const Json::Value& params, Json::Value& result)
            {
                BaseClient::ptr client = getClient(method);
                if(client.get() == nullptr)
                {
                    return false;
                }
                return _caller->call(client->connection(), method, params, result);
            }

            // 异步方式调用
            bool call(const std::string& method, const Json::Value& params, RpcCaller::JsonAsyncResponse& result)
            {
                BaseClient::ptr client = getClient(method);
                if(client.get() == nullptr)
                {
                    return false;
                }
                return _caller->call(client->connection(), method, params, result);
            }

            // 回调函数方式调用
            bool call(const std::string& method, const Json::Value& params, const RpcCaller::JsonResponseCallback& cb)
            {
                BaseClient::ptr client = getClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }
                return _caller->call(client->connection(), method, params, cb);
            }

        private:
            BaseClient::ptr newClient(const Address& host)
            {
                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                auto client = ClientFactory::create(host.first, host.second);
                client->setMessageCallBack(message_cb);
                client->connect();
                putClient(host, client);
                return client;
            }

            BaseClient::ptr getClient(const Address& host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _rpc_clients.find(host);
                if (it == _rpc_clients.end())
                {
                    return BaseClient::ptr();
                }
                return it->second;
            }

            BaseClient::ptr getClient(const std::string method)
            {
                BaseClient::ptr client;
                if (_enableDiscovery)
                {
                    // 1. 通过服务发现，获取服务提供者地址信息
                    Address host;
                    bool ret = _discovery_client->serviceDiscovery(method, host);
                    if (ret == false)
                    {
                        LOG(LogLevel::INFO) << "当前" << method << "服务，没有找到服务提供者！";
                        return BaseClient::ptr();
                    }
                    // 2. 查看服务提供者是否已有实例化客户端，有则直接使用，没有则创建
                    client = getClient(host);
                    if (client.get() == nullptr)
                    {
                        client = newClient(host);
                    }
                }
                else
                {
                    client = _rpc_client;
                }
                return client;
            }

            void putClient(const Address& host, BaseClient::ptr& client)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.insert(std::make_pair(host, client));
            }

            void delClient(const Address& host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.erase(host);
            }

        private:
            struct AddressHash
            {
                size_t operator()(const Address& host) const
                {
                    std::string addr = host.first + std::to_string(host.second);
                    return std::hash<std::string>{}(addr);
                }
            };

            bool _enableDiscovery;
            DiscoveryClient::ptr _discovery_client;
            Requestor::ptr _requestor;
            RpcCaller::ptr _caller;
            Dispatcher::ptr _dispatcher;
            std::mutex _mutex;

            BaseClient::ptr _rpc_client; // 用于未启用服务发现
            std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients; // 用于启用服务发现的客户端连接池
        };

        // 主题操作客户端
        class TopicClient
        {
        public:
            using ptr = std::shared_ptr<TopicClient>;

            TopicClient(const std::string& ip, int port)
                : _requestor(std::make_shared<Requestor>())
                , _dispatcher(std::make_shared<Dispatcher>())
                , _topic_manager(std::make_shared<TopicManager>(_requestor)) 
                , _rpc_client(ClientFactory::create(ip, port))
            {
                auto rsp_cb = std::bind(&Requestor::onResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<BaseMessage>(MsgType::RSP_TOPIC, rsp_cb);

                auto msg_cb = std::bind(&TopicManager::onPublish, _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<TopicRequest>(MsgType::REQ_TOPIC, msg_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _rpc_client->setMessageCallBack(message_cb);
                _rpc_client->connect();
            }

            bool createTopic(const std::string& key)
            {
                return _topic_manager->createTopic(_rpc_client->connection(), key);
            }

            bool removeTopic(const std::string& key)
            {
                return _topic_manager->removeTopic(_rpc_client->connection(), key);
            }

            bool subscribe(const std::string& key, const TopicManager::SubCallback& cb)
            {
                return _topic_manager->subscribe(_rpc_client->connection(), key, cb);
            }

            bool cancel(const std::string& key)
            {
                return _topic_manager->cancel(_rpc_client->connection(), key);
            }

            bool publish(const std::string& key, const std::string& msg)
            {
                return _topic_manager->publish(_rpc_client->connection(), key, msg);
            }

            void shutdown()
            {
                _rpc_client->shutdown();
            }

        private: 
            Requestor::ptr _requestor;
            TopicManager::ptr _topic_manager;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _rpc_client;
        };
    }
}