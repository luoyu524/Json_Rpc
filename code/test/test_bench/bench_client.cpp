#include "../../client/client.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct Config
{
    bool discovery = false;
    std::string server_ip = "127.0.0.1";
    int server_port = 0;
    std::string reg_ip = "127.0.0.1";
    int reg_port = 0;

    int threads = 1;
    int requests = 20000;
    int warmup = 100;
    bool shared = false;
    std::string method = "Add";
    int payload = 32;
    std::string tag = "run";

    bool probe = false;
    std::string probe_ip;
    int probe_port = 0;
};

class StartGate
{
public:
    explicit StartGate(int total) : _total(total) {}

    void arrive()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        ++_ready;
        _cv.notify_all();
        _cv.wait(lock, [this] { return _go; });
    }

    void waitReady()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _cv.wait(lock, [this] { return _ready == _total; });
    }

    void release()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _go = true;
        _cv.notify_all();
    }

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    int _total;
    int _ready = 0;
    bool _go = false;
};

static std::unique_ptr<RPC::client::RpcClient> makeClient(const Config& cfg)
{
    if (cfg.discovery)
    {
        return std::make_unique<RPC::client::RpcClient>(true, cfg.reg_ip, cfg.reg_port);
    }
    return std::make_unique<RPC::client::RpcClient>(false, cfg.server_ip, cfg.server_port);
}

static bool doCall(RPC::client::RpcClient* client, const Config& cfg, int tid, int i)
{
    Json::Value params;
    Json::Value result;

    if (cfg.method == "Add")
    {
        params["num1"] = i;
        params["num2"] = tid;
    }
    else
    {
        params["data"] = std::string(static_cast<size_t>(cfg.payload), 'x');
    }

    bool ok = client->call(cfg.method, params, result);
    if (ok == false)
    {
        return false;
    }

    if (cfg.method == "Add")
    {
        return result.isInt() && result.asInt() == i + tid;
    }
    return result.isString() && static_cast<int>(result.asString().size()) == cfg.payload;
}

static void worker(int tid,
                   const Config& cfg,
                   RPC::client::RpcClient* shared_client,
                   StartGate& gate,
                   std::vector<uint64_t>& latencies,
                   std::atomic<uint64_t>& errors)
{
    std::unique_ptr<RPC::client::RpcClient> own_client;
    RPC::client::RpcClient* client = shared_client;
    if (client == nullptr)
    {
        own_client = makeClient(cfg);
        client = own_client.get();
    }

    for (int i = 0; i < cfg.warmup; ++i)
    {
        doCall(client, cfg, tid, i);
    }

    gate.arrive();

    latencies.reserve(static_cast<size_t>(cfg.requests));
    for (int i = 0; i < cfg.requests; ++i)
    {
        auto t0 = std::chrono::steady_clock::now();
        bool ok = doCall(client, cfg, tid, i);
        auto t1 = std::chrono::steady_clock::now();
        latencies.push_back(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
        if (ok == false)
        {
            errors.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

static double percentile(const std::vector<uint64_t>& sorted, double p)
{
    if (sorted.empty())
    {
        return 0.0;
    }
    double idx = (p / 100.0) * static_cast<double>(sorted.size() - 1);
    size_t i = static_cast<size_t>(idx);
    if (i + 1 < sorted.size())
    {
        double frac = idx - static_cast<double>(i);
        double a = static_cast<double>(sorted[i]);
        double b = static_cast<double>(sorted[i + 1]);
        return (a + (b - a) * frac) / 1e6;
    }
    return static_cast<double>(sorted[i]) / 1e6;
}

static bool parseArgs(int argc, char** argv, Config& cfg)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--server" && i + 2 < argc)
        {
            cfg.server_ip = argv[++i];
            cfg.server_port = std::stoi(argv[++i]);
        }
        else if (arg == "--registry" && i + 2 < argc)
        {
            cfg.discovery = true;
            cfg.reg_ip = argv[++i];
            cfg.reg_port = std::stoi(argv[++i]);
        }
        else if (arg == "--threads" && i + 1 < argc)
        {
            cfg.threads = std::stoi(argv[++i]);
        }
        else if (arg == "--requests" && i + 1 < argc)
        {
            cfg.requests = std::stoi(argv[++i]);
        }
        else if (arg == "--warmup" && i + 1 < argc)
        {
            cfg.warmup = std::stoi(argv[++i]);
        }
        else if (arg == "--shared" && i + 1 < argc)
        {
            cfg.shared = std::stoi(argv[++i]) != 0;
        }
        else if (arg == "--method" && i + 1 < argc)
        {
            cfg.method = argv[++i];
        }
        else if (arg == "--payload" && i + 1 < argc)
        {
            cfg.payload = std::stoi(argv[++i]);
        }
        else if (arg == "--tag" && i + 1 < argc)
        {
            cfg.tag = argv[++i];
        }
        else if (arg == "--probe" && i + 2 < argc)
        {
            cfg.probe = true;
            cfg.probe_ip = argv[++i];
            cfg.probe_port = std::stoi(argv[++i]);
        }
    }

    if (cfg.probe)
    {
        return true;
    }
    return (cfg.discovery && cfg.reg_port > 0) || cfg.server_port > 0;
}

int main(int argc, char** argv)
{
    Config cfg;
    if (parseArgs(argc, argv, cfg) == false)
    {
        std::cerr << "usage: " << argv[0]
                  << " (--server ip port | --registry ip port)"
                  << " [--threads N] [--requests N] [--shared 0|1]"
                  << " [--method Add|Echo] [--payload N] [--tag name]\n"
                  << "       " << argv[0] << " --probe ip port\n";
        return 2;
    }

    if (cfg.probe)
    {
        RPC::client::RpcClient client(false, cfg.probe_ip, cfg.probe_port);
        Json::Value params;
        Json::Value result;
        if (client.call("Stats", params, result) == false)
        {
            std::cerr << "probe failed\n";
            return 1;
        }
        std::fprintf(stderr, "PROBE %d %d\n", cfg.probe_port, result.asInt());
        return 0;
    }

    if (cfg.threads <= 0 || cfg.requests <= 0)
    {
        std::cerr << "threads and requests must be positive\n";
        return 2;
    }

    std::unique_ptr<RPC::client::RpcClient> shared_client;
    RPC::client::RpcClient* shared_ptr = nullptr;
    if (cfg.shared)
    {
        shared_client = makeClient(cfg);
        shared_ptr = shared_client.get();
    }

    StartGate gate(cfg.threads);
    std::vector<std::vector<uint64_t>> latencies(cfg.threads);
    std::atomic<uint64_t> errors{0};

    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(cfg.threads));
    for (int t = 0; t < cfg.threads; ++t)
    {
        threads.emplace_back(worker, t, std::cref(cfg), shared_ptr, std::ref(gate),
                             std::ref(latencies[t]), std::ref(errors));
    }

    gate.waitReady();
    auto t_start = std::chrono::steady_clock::now();
    gate.release();

    for (auto& th : threads)
    {
        th.join();
    }
    auto t_end = std::chrono::steady_clock::now();

    std::vector<uint64_t> all;
    size_t total = 0;
    for (const auto& v : latencies)
    {
        total += v.size();
        all.insert(all.end(), v.begin(), v.end());
    }
    std::sort(all.begin(), all.end());

    double seconds = std::chrono::duration<double>(t_end - t_start).count();
    double qps = (seconds > 0.0 && total > 0) ? static_cast<double>(total) / seconds : 0.0;
    uint64_t sum = 0;
    for (uint64_t x : all)
    {
        sum += x;
    }
    double avg_ms = all.empty() ? 0.0 : (static_cast<double>(sum) / all.size()) / 1e6;
    double max_ms = all.empty() ? 0.0 : static_cast<double>(all.back()) / 1e6;

    std::fprintf(stderr, "%s,%d,%d,%s,%d,%zu,%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%llu\n",
                cfg.tag.c_str(), cfg.threads, cfg.shared ? 1 : 0, cfg.method.c_str(),
                cfg.payload, total, qps, avg_ms, percentile(all, 50.0),
                percentile(all, 90.0), percentile(all, 99.0), max_ms,
                static_cast<unsigned long long>(errors.load()));
    return 0;
}
