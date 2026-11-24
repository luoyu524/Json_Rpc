#include "../../server/server.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    int port = 19000;
    if (argc >= 3 && std::string(argv[1]) == "--port")
    {
        port = std::stoi(argv[2]);
    }

    RPC::server::RegistryServer server(port);
    std::cerr << "bench_registry ready on port " << port << std::endl;
    server.start();
    return 0;
}
