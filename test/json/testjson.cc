#include <iostream>
#include <jsoncpp/json/json.h>
#include <memory>
#include <sstream>
#include <string>

// 实现数据的序列化
bool serialize(const Json::Value& val, std::string* body)
{
    std::stringstream ss;
    // 先实例化一个工厂类对象
    Json::StreamWriterBuilder swb;
    // 通过工厂类对象来生产派生类对象
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
    int ret = sw->write(val, &ss);
    if (ret != 0)
    {
        std::cout << "json serialize failed!\n";
        return false;
    }
    *body = ss.str();
    return true;
}

// 实现json字符串的反序列化
bool unserialize(const std::string& body, Json::Value* val)
{
    // 实例化工厂类对象
    Json::CharReaderBuilder crb;
    // 生产CharReader对象
    std::string errs;
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
    bool ret = cr->parse(body.c_str(), body.c_str() + body.size(), val, &errs);
    if (ret == false)
    {
        std::cout << "json unserialize failed: " << errs << std::endl;
        return false;
    }
    return true;
}

int main()
{
    const char* name = "小明";
    int age = 18;
    const char* sex = "男";
    float score[3] = {88, 77.5, 66};

    Json::Value student;
    student["姓名"] = name;
    student["年龄"] = age;
    student["性别"] = sex;
    student["成绩"].append(score[0]);
    student["成绩"].append(score[1]);
    student["成绩"].append(score[2]);

    std::string body;
    serialize(student, &body);
    std::cout << "序列化结果：" << std::endl;
    std::cout << body << std::endl;

    Json::Value root;
    bool ret = unserialize(body, &root);
    if (ret == false)
        return -1;
    std::cout << "反序列化结果：" << std::endl;
    std::cout << "姓名: " << root["姓名"].asString() << std::endl;
    std::cout << "年龄: " << root["年龄"].asInt() << std::endl;
    std::cout << "性别: " << root["性别"].asCString() << std::endl;
    int sz = root["成绩"].size();
    for (int i = 0; i < sz; i++)
    {
        std::cout << "成绩: " << root["成绩"][i].asFloat() << std::endl;
    }
    return 0;
}