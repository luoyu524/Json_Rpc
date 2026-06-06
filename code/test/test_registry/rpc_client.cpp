#include "../../client/client.hpp"
#include "../../common/details.hpp"

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "错误，你应该输入注册中心的“ip + port”" << std::endl;
        return 0;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    // true---启用注册中心
    RPC::client::RpcClient client(true, ip, port);

    while (1)
    {
        Json::Value params, result;

        // 同步方式rpc调用测试
        params["num1"] = 100;
        params["num2"] = 200;
        bool ret = client.call("Add", params, result);
        if (ret == true)
        {
            std::cout << "Sync rpc result: " << result.asInt() << std::endl;
        }

        sleep(2);
    }

    return 0;
}