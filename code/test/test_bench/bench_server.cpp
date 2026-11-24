#include "../../common/details.hpp"
#include "../../server/server.hpp"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

static std::atomic<uint64_t> g_add_count{0};

static void Add(const Json::Value& req, Json::Value& rsp)
{
    int a = req["num1"].asInt();
    int b = req["num2"].asInt();
    g_add_count.fetch_add(1, std::memory_order_relaxed);
    rsp = a + b;
}

static void Echo(const Json::Value& req, Json::Value& rsp)
{
    rsp = req["data"].asString();
}

static void Stats(const Json::Value&, Json::Value& rsp)
{
    rsp = static_cast<int>(g_add_count.load(std::memory_order_relaxed));
}

static void registerOne(RPC::server::RpcServer& server,
                        const std::string& name,
                        const std::vector<std::pair<std::string, RPC::server::ValType>>& params,
                        RPC::server::ValType ret,
                        const RPC::server::ServiceDescribe::ServiceCallBack& cb)
{
    RPC::server::ServiceDescFactory factory;
    factory.setMethodName(name);
    for (const auto& p : params)
    {
        factory.setParamsDesc(p.first, p.second);
    }
    factory.setReturnType(ret);
    factory.setCallback(cb);
    server.registerMethod(factory.build());
}

int main(int argc, char** argv)
{
    int port = 19001;
    std::string reg_ip;
    int reg_port = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc)
        {
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--registry" && i + 2 < argc)
        {
            reg_ip = argv[++i];
            reg_port = std::stoi(argv[++i]);
        }
    }

    std::unique_ptr<RPC::server::RpcServer> server;
    if (reg_port > 0)
    {
        server = std::make_unique<RPC::server::RpcServer>(
            RPC::Address("127.0.0.1", port), true, RPC::Address(reg_ip, reg_port));
    }
    else
    {
        server = std::make_unique<RPC::server::RpcServer>(
            RPC::Address("127.0.0.1", port), false);
    }

    registerOne(*server, "Add",
                {{"num1", RPC::server::ValType::INTEGRAL},
                 {"num2", RPC::server::ValType::INTEGRAL}},
                RPC::server::ValType::INTEGRAL, Add);
    registerOne(*server, "Echo",
                {{"data", RPC::server::ValType::STRING}},
                RPC::server::ValType::STRING, Echo);
    registerOne(*server, "Stats", {},
                RPC::server::ValType::INTEGRAL, Stats);

    std::cerr << "bench_server ready on 127.0.0.1:" << port << std::endl;
    server->start();
    return 0;
}
