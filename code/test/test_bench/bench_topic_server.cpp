#include "../../server/server.hpp"
#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    int port = 19003;
    if (argc >= 3 && std::string(argv[1]) == "--port")
    {
        port = std::stoi(argv[2]);
    }

    RPC::server::TopicServer server(port);
    std::cerr << "bench_topic_server ready on port " << port << std::endl;
    server.start();
    return 0;
}
