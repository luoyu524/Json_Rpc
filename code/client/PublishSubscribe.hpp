// client---PublishSubscribe.hpp
#pragma once
#include "../common/net.hpp"
#include "../common/message.hpp"
#include "Requestor.hpp"
#include <memory>
#include <string>
#include <utility>

namespace RPC 
{
    namespace client 
    {
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;
            using SubCallback = std::function<void(const std::string& key, const std::string& msg)>;

            TopicManager(const Requestor::ptr& req)
                : _requestor(req)
            {}

            bool createTopic(const BaseConnection::ptr& conn, const std::string& key)
            {
                return commonRequest(conn, key, TopicOpType::TOPIC_CREATE);
            }

            bool removeTopic(const BaseConnection::ptr& conn, const std::string& key)
            {
                return commonRequest(conn, key, TopicOpType::TOPIC_REMOVE);
            }

            bool subscribe(const BaseConnection::ptr& conn, const std::string& key, const SubCallback& cb)
            {
                addSubscribe(key, cb);
                bool ret = commonRequest(conn, key, TopicOpType::TOPIC_SUBSCRIBE);
                if (ret == false)
                {
                    delSubscribe(key);
                    return false;
                }
                return true;
            }

            bool cancel(const BaseConnection::ptr& conn, const std::string& key)
            {
                delSubscribe(key);
                return commonRequest(conn, key, TopicOpType::TOPIC_CANCEL);
            }

            bool publish(const BaseConnection::ptr& conn, const std::string& key, const std::string& msg)
            {
                return commonRequest(conn, key, TopicOpType::TOPIC_PUBLISH, msg);
            }

            // 这个函数要注册到主题订阅端的dispatcher模块，针对主题消息发布类型的请求的业务回调函数
            void onPublish(const BaseConnection::ptr& conn, const TopicRequest::ptr& msg)
            {
                if(msg->optype() != TopicOpType::TOPIC_PUBLISH)
                {
                    LOG(LogLevel::ERROR) << "收到了错误类型的主题请求操作！";
                    return;
                }
                std::string key = msg->topicKey();
                std::string topic_msg = msg->topicMsg();
                auto callback = getSubcribe(key);
                if(!callback)
                {
                    LOG(LogLevel::ERROR) << "收到了主题消息发布请求操作，但是没有处理回调函数！";
                    return;
                }
                callback(key, topic_msg);
            }
        
        private:
            void addSubscribe(const std::string& key, const SubCallback& cb)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.insert(std::make_pair(key, cb));
            }

            void delSubscribe(const std::string& key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.erase(key);
            }

            const SubCallback getSubcribe(const std::string& key)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _topic_callbacks.find(key);
                if(it == _topic_callbacks.end())
                {
                    return SubCallback();
                }
                return it->second;
            }

            bool commonRequest(const BaseConnection::ptr& conn, const std::string& key, TopicOpType type, const std::string& msg = "")
            {
                //1. 构造请求对象，并填充数据
                auto msg_req = MsgFactory::create<TopicRequest>();
                msg_req->setId(UUID::uuid());
                msg_req->setMsgtype(MsgType::REQ_TOPIC);
                msg_req->setTopicKey(key);
                msg_req->setOptype(type);
                if(type == TopicOpType::TOPIC_PUBLISH)
                {
                    msg_req->setTopicMsg(msg);
                }
                //2. 向服务端发送请求，等待响应
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->send(conn, msg_req, msg_rsp);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "主题操作请求失败！";
                    return false;
                } 
                // 3. 判断请求处理是否成功
                auto topic_rsp_msg = std::dynamic_pointer_cast<TopicResponse>(msg_rsp);
                if (!topic_rsp_msg)
                {
                    LOG(LogLevel::ERROR) << "主题操作响应，向下类型转换失败！";
                    return false;
                }
                if (topic_rsp_msg->rcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "主题操作请求出错: " << errReason(topic_rsp_msg->rcode());
                    return false;
                }
                return true;
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, SubCallback> _topic_callbacks;
            Requestor::ptr _requestor;
        };
    }
}
