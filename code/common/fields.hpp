// fields.hpp
#pragma once
#include <string>
#include <unordered_map>

namespace RPC 
{
    #define KEY_METHOD      "method"
    #define KEY_PARAMS      "parameters"
    #define KEY_TOPIC_KEY   "topic_key"
    #define KEY_TOPIC_MSG   "topic_msg"
    #define KEY_OPTYPE      "opertion_type"
    #define KEY_HOST        "host"
    #define KEY_HOST_IP     "host_ip"
    #define KEY_HOST_PORT   "host_port"
    #define KEY_RCODE       "rsp_code"
    #define KEY_RESULT      "result"

    enum class MsgType
    {
        REQ_RPC,
        RSP_RPC,
        REQ_TOPIC,
        RSP_TOPIC,
        REQ_SERVICE,
        RSP_SERVICE
    };

    enum class RCode
    {
        RCODE_OK,
        RCODE_PARSE_FAILED,
        RCODE_ERROR_MSGTYPE,
        RCODE_INVALID_MSG,
        RCODE_DISCONNECTED,
        RCODE_INVALID_PARAMS,
        RCODE_NOT_FOUND_SERVICE,
        RCODE_NOT_FOUND_TOPIC,
        RCODE_INVALID_OPTYPE,
        RCODE_INTERNAL_ERROR
    };

    static std::string errReason(RCode code)
    {
        static std::unordered_map<RCode, std::string> err_map = 
        {
            {RCode::RCODE_OK,                 "成功处理"},
            {RCode::RCODE_PARSE_FAILED,       "消息解析失败"},
            {RCode::RCODE_ERROR_MSGTYPE,      "消息类型错误"},
            {RCode::RCODE_INVALID_MSG,        "消息无效"},
            {RCode::RCODE_DISCONNECTED,       "连接已断开"},
            {RCode::RCODE_INVALID_PARAMS,     "RPC参数无效"},
            {RCode::RCODE_NOT_FOUND_SERVICE,  "没有找到对应的服务"},
            {RCode::RCODE_NOT_FOUND_TOPIC,    "没有找到对应的主题"},
            {RCode::RCODE_INVALID_OPTYPE,     "操作类型无效"},
            {RCode::RCODE_INTERNAL_ERROR,     "内部错误"}
        };

        if(err_map.count(code) == 0)
            return "未知错误";
        
        return err_map[code];
    }

    enum class ReqType
    {
        REQ_ASYNC,
        REQ_CALLBACK
    };

    enum class TopicOpType
    {
        TOPIC_CREATE,
        TOPIC_REMOVE,
        TOPIC_SUBSCRIBE,
        TOPIC_CANCEL,
        TOPIC_PUBLISH
    };

    enum class ServiceOpType
    {
        SERVICE_REGISTRY,
        SERVICE_DISCOVERY,
        SERVICE_ONLINE,
        SERVICE_OFFLINE,
        SERVICE_UNKNOW
    };

}