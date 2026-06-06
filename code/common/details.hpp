/*
    实现项目中用到的一些琐碎功能代码
    * 日志
    * json的序列化和反序列化
    * uuid的生成
*/
#pragma once
#include "Logger.hpp"
#include <atomic>
#include <iomanip>
#include <ios>
#include <jsoncpp/json/json.h>
#include <memory>
#include <random>
#include <sstream>
#include <string>

namespace RPC
{
    class JSON
    {
    public:
        // 实现数据的序列化
        static bool serialize(const Json::Value& val, std::string* body)
        {
            std::stringstream ss;
            // 先实例化一个工厂类对象
            Json::StreamWriterBuilder swb;
            // 通过工厂类对象来生产派生类对象
            std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
            int ret = sw->write(val, &ss);
            if (ret != 0)
            {
                LOG(LogLevel::FATAL) << "json serialize failed!";
                return false;
            }
            *body = ss.str();
            return true;
        }

        // 实现json字符串的反序列化
        static bool unserialize(const std::string& body, Json::Value* val)
        {
            // 实例化工厂类对象
            Json::CharReaderBuilder crb;
            // 生产CharReader对象
            std::string errs;
            std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
            bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), val, &errs);
            if (ret == false)
            {
                LOG(LogLevel::FATAL) << "json unserialize failed: " << errs;
                return false;
            }
            return true;
        }
    };


    class UUID
    {
    public:
        static std::string uuid()
        {
            std::stringstream ss;
 
            // 构造一个机器随机数对象
            std::random_device rd;
            // 以机器随机数为种子构造随机数对象
            std::mt19937 generator(rd());
            // 构造限定数据范围的对象
            std::uniform_int_distribution<int> distribution(0, 255);
            // 生成8个随机数，按照特定格式组织成为16进制数字字符的字符串
            for(int i = 0; i < 8; i++)
            {
                if(i == 4 || i == 6)
                    ss << '-';
                ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
            }
            ss << '-'; 
            // 定义一个8字节序号，逐字节组织为16进制数字字符的字符串
            static std::atomic<size_t> seq(1);
            size_t cur = seq.fetch_add(1);
            for(int i = 7; i >= 0; i--)
            {
                if(i == 5)
                    ss << '-';
                ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i*8) & 0xFF));
            }
            return ss.str();
        }
        
    };

}