#include "../../client/client.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
    std::string role;
    std::string ip = "127.0.0.1";
    int port = 19003;
    std::string topic = "bench_topic";
    int count = 10000;
    int payload = 64;
    int duration = 10;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--role" && i + 1 < argc)
        {
            role = argv[++i];
        }
        else if (arg == "--server" && i + 2 < argc)
        {
            ip = argv[++i];
            port = std::stoi(argv[++i]);
        }
        else if (arg == "--topic" && i + 1 < argc)
        {
            topic = argv[++i];
        }
        else if (arg == "--count" && i + 1 < argc)
        {
            count = std::stoi(argv[++i]);
        }
        else if (arg == "--payload" && i + 1 < argc)
        {
            payload = std::stoi(argv[++i]);
        }
        else if (arg == "--duration" && i + 1 < argc)
        {
            duration = std::stoi(argv[++i]);
        }
    }

    if (role != "pub" && role != "sub")
    {
        std::cerr << "usage: " << argv[0]
                  << " --role pub|sub --server ip port --topic name"
                  << " [--count N] [--payload N] [--duration sec]\n";
        return 2;
    }

    RPC::client::TopicClient client(ip, port);
    client.createTopic(topic);

    if (role == "sub")
    {
        std::atomic<uint64_t> received{0};
        bool ok = client.subscribe(topic, [&received](const std::string&, const std::string&) {
            received.fetch_add(1, std::memory_order_relaxed);
        });
        if (ok == false)
        {
            std::cerr << "subscribe failed\n";
            return 1;
        }

        std::fprintf(stderr, "SUB_READY\n");
        std::fflush(stdout);

        int slices = duration * 10;
        for (int i = 0; i < slices; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::fprintf(stderr, "SUB_RECEIVED %llu\n",
                    static_cast<unsigned long long>(received.load()));
    }
    else
    {
        std::string msg(static_cast<size_t>(payload), 'x');
        uint64_t ok_count = 0;

        auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < count; ++i)
        {
            if (client.publish(topic, msg))
            {
                ++ok_count;
            }
        }
        auto t1 = std::chrono::steady_clock::now();

        double seconds = std::chrono::duration<double>(t1 - t0).count();
        double qps = (seconds > 0.0) ? static_cast<double>(ok_count) / seconds : 0.0;
        std::fprintf(stderr, "PUB_SENT %llu PUB_TOTAL %d PUB_SECONDS %.3f PUB_QPS %.2f\n",
                    static_cast<unsigned long long>(ok_count), count, seconds, qps);
    }

    client.shutdown();
    return 0;
}
