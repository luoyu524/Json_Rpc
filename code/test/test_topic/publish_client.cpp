#include "../../client/client.hpp"

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
    // 3. 向主题发布消息
    for (int i = 0; i < 10; i++)
    {
        client->publish("topic_hello", "Hello World-" + std::to_string(i));
    }
    client->shutdown();

    return 0;
}