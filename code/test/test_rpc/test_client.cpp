#include "../../client/client.hpp"
#include "../../common/details.hpp"
#include <unistd.h>

void callback(const Json::Value& result)
{
    std::cout << "Callback rpc result: " << result.asInt() << std::endl;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "错误，你应该输入服务端的“ip + port”" << std::endl;
        return 0;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
    RPC::client::RpcClient client(false, ip, port);

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

    // 异步方式rpc调用测试
    RPC::client::RpcCaller::JsonAsyncResponse res_future;
    params["num1"] = 12;
    params["num2"] = 24;
    ret = client.call("Add", params, res_future);
    if (ret == true)
    {
        result = res_future.get();
        std::cout << "Async rpc result: " << result.asInt() << std::endl;
    }

    sleep(2);

    // 回调方式rpc调用测试
    params["num1"] = 900;
    params["num2"] = 360;
    ret = client.call("Add", params, callback);

    sleep(2);

    // 故意传错误的参数类型测试一下
    params["num1"] = "abcd";
    params["num2"] = 10.12;
    ret = client.call("Add", params, result);
    if (ret == true)
    {
        std::cout << "Sync rpc result: " << result.asInt() << std::endl;
    }

    return 0;
}