#include "../../common/details.hpp"
#include "../../server/server.hpp"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cout << "错误，你应该输入注册中心的port" << std::endl;
        return 0;
    }
    int port = std::stoi(argv[1]);
    RPC::server::RegistryServer reg_server(port);
    reg_server.start();
    
    return 0;
}