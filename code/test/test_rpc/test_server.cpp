#include "../../common/details.hpp"
#include "../../server/server.hpp"

void Add(const Json::Value& req, Json::Value& rsp)
{
    int num1 = req["num1"].asInt();
    int num2 = req["num2"].asInt();
    rsp = num1 + num2;
}

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cout << "错误，你应该输入“ip + port”" << std::endl;
        return 0;
    }
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);

    std::unique_ptr<RPC::server::ServiceDescFactory> desc_factory(new RPC::server::ServiceDescFactory());
    desc_factory->setMethodName("Add");
    desc_factory->setParamsDesc("num1", RPC::server::ValType::INTEGRAL);
    desc_factory->setParamsDesc("num2", RPC::server::ValType::INTEGRAL);
    desc_factory->setReturnType(RPC::server::ValType::INTEGRAL);
    desc_factory->setCallback(Add);

    RPC::server::RpcServer server(RPC::Address(ip, port));
    server.registerMethod(desc_factory->build());

    server.start();
    
    return 0;
}