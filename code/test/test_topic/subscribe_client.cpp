#include "../../client/client.hpp"
#include <unistd.h>

void callback(const std::string& key, const std::string& msg)
{
    std::cout << key << "主题收到推送过来的消息: " << msg << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "错误，你应该输入主题中转中心的ip+port" << std::endl;
        return 0;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    // 1. 实例化客户端对象
    auto client = std::make_shared<RPC::client::TopicClient>(ip, port);
    // 2. 创建主题
    bool ret = client->createTopic("topic_hello");
    if (ret == false)
    {
        std::cerr << "创建主题失败！" << std::endl;
    }
    // 3. 订阅主题
    ret = client->subscribe("topic_hello", callback);
    if (ret == false)
    {
        std::cerr << "订阅主题失败！" << std::endl;
    }

    sleep(10);
    // 4. 等待->退出
    client->shutdown();

    return 0;
}