#include "../../server/server.hpp"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "错误，你应该输入主题中转中心的port" << std::endl;
        return 0;
    }
    int port = std::stoi(argv[1]);
    auto server = std::make_shared<RPC::server::TopicServer>(port);
    server->start();
    return 0;
}