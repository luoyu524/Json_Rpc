// server.hpp
#pragma once
#include "../common/dispatcher.hpp"
#include "../client/client.hpp"
#include "RpcRouter.hpp"
#include "DiscoverRegistry.hpp"
#include "PublishSubscribe.hpp"
#include <memory>

namespace RPC 
{
    namespace server 
    {
        // 服务注册中心 服务端
        // 只需要针对服务注册与发现请求进行处理即可
        class RegistryServer
        {
        public:
            using ptr = std::shared_ptr<RegistryServer>;
            
            RegistryServer(int port)
                : _pd_manager(std::make_shared<PDManager>())
                , _dispatcher(std::make_shared<Dispatcher>())
                , _server(ServerFactory::create(port))
            {
                auto service_cb = std::bind(&PDManager::onServiceMessgage, _pd_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<ServiceRequest>(MsgType::REQ_SERVICE, service_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallBack(message_cb);

                auto close_cb = std::bind(&RegistryServer::onConnShutdown, this, std::placeholders::_1);
                _server->setCloseCallBack(close_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            void onConnShutdown(const BaseConnection::ptr& conn)
            {
                _pd_manager->onConnShutdown(conn);
            }

        private:
            PDManager::ptr _pd_manager;
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server;
        };

        // Rpc服务注册方法提供端，封装服务注册客户端
        class RpcServer
        {
        public:
            using ptr = std::shared_ptr<RpcServer>;
            // rpc服务端有两套地址信息：
            // 1. rpc服务提供端地址信息--必须是rpc服务器对外访问地址，但是这里云服务器的监听地址和访问地址不同
            // 2. 注册中心服务端地址信息--启用服务注册后，连接注册中心进行服务注册用的
            RpcServer(const Address& access_addr, bool enableRegistry = false, const Address& registry_server_addr = Address())
                : _enableRegistry(enableRegistry)
                , _access_addr(access_addr)
                , _router(std::make_shared<RpcRouter>())
                , _dispatcher(std::make_shared<Dispatcher>())
                , _server(ServerFactory::create(access_addr.second))
            {
                if (enableRegistry)
                {
                    _reg_client = std::make_shared<client::RegistryClient>(registry_server_addr.first, registry_server_addr.second);
                }

                auto rpc_cb = std::bind(&RpcRouter::onRpcRequest, _router.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<RpcRequest>(MsgType::REQ_RPC, rpc_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallBack(message_cb);
            }

            void registerMethod(const ServiceDescribe::ptr& service)
            {
                if (_enableRegistry)
                {
                    _reg_client->registerMethod(service->method(), _access_addr);
                }
                _router->registerMethod(service);
            }

            void start()
            {
                _server->start();
            }

        private: 
            bool _enableRegistry;
            Address _access_addr;
            client::RegistryClient::ptr _reg_client;
            RpcRouter::ptr _router;
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server; //成员_server是一个rpcserver，用于提供rpc服务的
        };

        // 主题消息中转中心，服务端
        class TopicServer
        {
        public:
            using ptr = std::shared_ptr<TopicServer>;
            TopicServer(int port)
                : _topic_manager(std::make_shared<TopicManager>())
                , _dispatcher(std::make_shared<Dispatcher>())
                , _server(ServerFactory::create(port))
            {
                auto topic_cb = std::bind(&TopicManager::onTopicRequest, _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->registerHandler<TopicRequest>(MsgType::REQ_TOPIC, topic_cb);

                auto message_cb = std::bind(&Dispatcher::onMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _server->setMessageCallBack(message_cb);
                auto close_cb = std::bind(&TopicServer::onConnShutdown, this, std::placeholders::_1);
                _server->setCloseCallBack(close_cb);
            }

            void start()
            {
                _server->start();
            }

        private:
            void onConnShutdown(const BaseConnection::ptr& conn)
            {
                _topic_manager->onShutDown(conn);
            }

        private:
            TopicManager::ptr _topic_manager;
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server;
        };
    }
}