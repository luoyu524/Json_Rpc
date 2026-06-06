// dispatcher.hpp
#pragma once
#include "Base.hpp"
#include "Logger.hpp"
#include "net.hpp"
#include "message.hpp"
#include "fields.hpp"
#include <memory>

namespace RPC
{
    class Dispatcher
    {
    private:
        class CallBack
        {
        public:
            using ptr = std::shared_ptr<CallBack>;
            virtual void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) = 0;
        };
        template <typename T> class CallBackT : public CallBack
        {
        public:
            using ptr = std::shared_ptr<CallBackT<T>>;
            using MessageCallback = std::function<void(const BaseConnection::ptr& conn, std::shared_ptr<T>& msg)>;

            CallBackT(const MessageCallback& handler) : _handler(handler) {}

            void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) override
            {
                auto type_msg = std::dynamic_pointer_cast<T>(msg);
                _handler(conn, type_msg);
            }

        private:
            MessageCallback _handler;
        };

    public:
        using ptr = std::shared_ptr<Dispatcher>;

        // 这个函数对外提供: 注册<消息类型——回调函数>映射的接口
        template<typename T>
        void registerHandler(MsgType mtype, const typename CallBackT<T>::MessageCallback& handler)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto cb = std::make_shared<CallBackT<T>>(handler);
            _handlers.insert(std::make_pair(mtype, cb));
        }

        // 这个函数对外提供: 收到一条消息时，判断这条消息是什么类型，根据消息类型寻找注册到自己_handlers中的业务函数，回调处理
        void onMessage(const BaseConnection::ptr& conn, BaseMessage::ptr& msg) 
        {
            //找到消息类型对应的业务处理函数，进行调用即可
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _handlers.find(msg->mtype());
            if (it != _handlers.end()) 
            {
                return it->second->onMessage(conn, msg);
            }
            LOG(LogLevel::ERROR) << "收到未知类型的消息: " << (int)msg->mtype();
            conn->shutdown();
        }


    private:
        std::mutex _mutex;
        std::unordered_map<MsgType, CallBack::ptr> _handlers; // <消息类型---回调函数>的映射
    };

}