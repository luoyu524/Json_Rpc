// server---PublishSubscribe.hpp
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RPC
{
    namespace server
    {
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;
            
            TopicManager(){}

            // 这个函数注册到主题中转中心的dispatcher模块，是针对收到一条主题请求的业务回调函数
            void onTopicRequest(const BaseConnection::ptr& conn, TopicRequest::ptr& msg)
            {
                TopicOpType optype = msg->optype();
                bool ret = true;
                switch(optype)
                {
                    case TopicOpType::TOPIC_CREATE: 
                        topicCreate(conn, msg);
                        break;
                    case TopicOpType::TOPIC_REMOVE: 
                        topicRemove(conn, msg);
                        break;
                    case TopicOpType::TOPIC_SUBSCRIBE: 
                        ret = topicSubscribe(conn, msg);
                        break;
                    case TopicOpType::TOPIC_CANCEL: 
                        topicCancel(conn, msg);
                    case TopicOpType::TOPIC_PUBLISH:
                        ret = topicPublish(conn, msg);
                        break;
                    default:  
                        errorResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);
                        return;
                }

                if(ret == false)
                {
                    errorResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
                    return;
                }
                topicResponse(conn, msg);
            }

            // 一个订阅者在连接断开时的处理，删除其关联的数据
            // 发布者断开连接不需要任何操作
            void onShutDown(const BaseConnection::ptr& conn)
            {
                std::vector<Topic::ptr> topics;
                Subscriber::ptr subscriber;
                {
                    // 1. 判断断开连接的是否是订阅者，不是的话则直接返回
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _subscribers.find(conn);
                    if(it == _subscribers.end())
                    {
                        return;
                    }
                    subscriber = it->second;
                    // 2. 获取断开的订阅者订阅的主题对象
                    for(auto& topic_name : subscriber->topics)
                    {
                        auto topic_it = _topics.find(topic_name);
                        if(topic_it != _topics.end())
                        {
                            topics.push_back(topic_it->second);
                        }
                    }
                    _subscribers.erase(it);
                }
                // 3.从受影响的主题对象中，移除断开的订阅者信息   
                for(auto& topic : topics)
                {
                    topic->removeSubsciber(subscriber);
                }
            }
        private:
            void errorResponse(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg, RCode rcode)
            {
                auto msg_rsp = MsgFactory::create<TopicResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMsgtype(MsgType::RSP_TOPIC);
                msg_rsp->setRCode(rcode);
                conn->send(msg_rsp);
            }

            void topicResponse(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                auto msg_rsp = MsgFactory::create<TopicResponse>();
                msg_rsp->setId(msg->rid());
                msg_rsp->setMsgtype(MsgType::RSP_TOPIC);
                msg_rsp->setRCode(RCode::RCODE_OK);
                conn->send(msg_rsp);
            }

            void topicCreate(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                std::string topic_name = msg->topicKey();
                auto topic = std::make_shared<Topic>(topic_name);
                _topics.insert(std::make_pair(topic_name, topic));
            }

            void topicRemove(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                // 1. 查看当前主题有哪些订阅者，然后从订阅者中将该主题信息删除掉
                // 2. 删除主题的关联数据
                std::string topic_name = msg->topicKey();
                std::unordered_set<Subscriber::ptr> subscribers;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(topic_name);
                    if(it == _topics.end())
                    {
                        return;
                    }
                    subscribers = it->second->subscribers;
                    _topics.erase(it);
                }
                for(auto& sub : subscribers)
                {
                    sub->removeTopic(topic_name);
                }
            }

            bool topicSubscribe(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                // 1. 先找出主题对象，以及订阅者对象
                // 如果没有找到主题--返回错误。但是如果没有找到订阅者对象--就要构造一个订阅者
                std::string topic_name = msg->topicKey();
                Topic::ptr topic;
                Subscriber::ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it1 = _topics.find(topic_name);
                    if(it1 == _topics.end())
                    {
                        return false;
                    }
                    topic = it1->second;

                    auto it2 = _subscribers.find(conn);
                    if(it2 == _subscribers.end())
                    {
                        subscriber = std::make_shared<Subscriber>(conn);
                        _subscribers.insert(std::make_pair(conn, subscriber));
                    }
                    else 
                    {
                        subscriber = it2->second;
                    }
                }
                // 2. 在主题对象中新增一个订阅者对象关联的连接，在订阅者对象中新增一个订阅主题
                topic->appendSubscribe(subscriber);
                subscriber->append(topic_name);
                return true;
            }

            void topicCancel(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                // 1. 先找出主题对象，以及订阅者对象
                std::string topic_name = msg->topicKey();
                Topic::ptr topic;
                Subscriber::ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it1 = _topics.find(topic_name);
                    if (it1 != _topics.end())
                    {
                        topic = it1->second;
                    }
                    auto it2 = _subscribers.find(conn);
                    if (it2 != _subscribers.end())
                    {
                        subscriber = it2->second;
                    }
                }
                // 2. 从主题对象中删除当前的订阅者。从订阅者信息中删除订阅主题
                if (subscriber)
                {
                    subscriber->removeTopic(topic_name);
                }
                if (topic && subscriber)
                {
                    topic->removeSubsciber(subscriber);
                }
            }

            bool topicPublish(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                Topic::ptr topic;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(msg->topicKey());
                    if (it == _topics.end())
                    {
                        return false;
                    }
                    topic = it->second;
                }
                topic->broadMessage(msg);
                return true;
            }

        private:
            struct Subscriber
            {
                using ptr = std::shared_ptr<Subscriber>;
                std::mutex _mutex;
                BaseConnection::ptr conn;
                std::unordered_set<std::string> topics; // 一个订阅者的所有订阅主题

                Subscriber(const BaseConnection::ptr& c)
                    : conn(c)
                {}

                // 订阅主题时调用
                void append(const std::string& topic)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.insert(topic);
                }

                // 主题被删除或取消订阅时调用
                void removeTopic(const std::string& topic)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    topics.erase(topic);
                }
            };
            
            struct Topic
            {
                using ptr = std::shared_ptr<Topic>;
                std::mutex _mutex;
                std::string topic_name;
                std::unordered_set<Subscriber::ptr> subscribers; // 一个主题的所有订阅者

                Topic(const std::string& topic)
                    : topic_name(topic)
                {}

                // 一个主题新增被订阅者时调用
                void appendSubscribe(const Subscriber::ptr& subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.insert(subscriber);
                }

                // 一个主题被取消订阅或订阅者连接断开时调用
                void removeSubsciber(const Subscriber::ptr& subscriber)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    subscribers.erase(subscriber);
                }

                // 收到消息发布请求时调用: 广播通知
                void broadMessage(const BaseMessage::ptr& msg)
                {   
                    std::unique_lock<std::mutex> lock(_mutex);
                    for(auto& subscriber : subscribers)
                    {
                        subscriber->conn->send(msg);
                    }
                }
            };

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, Topic::ptr> _topics;
            std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers;
        };
    }
}