# Json_Rpc

这是一个在 Linux 系统上实现的，基于 Json 和 Muduo 库的 Rpc 组件。
具体实现过程文档：https://blog.csdn.net/2402_86681376/category_13166697.html

build子目录：muduo库资源
test子目录：部分语法测试demo
code子目录：核心内容

## code 目录模块说明

以下说明覆盖 `code/` 目录中除 `code/test/` 子目录外的所有 `.hpp` 文件。

### common 公共层

| 文件 | 模块 | 作用 |
| --- | --- | --- |
| `code/common/fields.hpp` | 协议字段与枚举定义 | 定义消息字段名、消息类型、返回码、请求类型、主题操作类型、服务注册发现操作类型，是各层共享的协议常量。 |
| `code/common/Base.hpp` | 抽象接口层 | 定义 Buffer、Message、Connection、Protocol、Server、Client 等抽象接口，把业务层与具体网络库解耦。 |
| `code/common/details.hpp` | 通用工具层 | 提供 JSON 序列化/反序列化和请求 ID 生成等工具，供消息构造和请求跟踪使用。 |
| `code/common/Logger.hpp` | 日志模块 | 提供日志级别、日志输出策略和日志对象，用于各模块输出运行状态与错误信息。 |
| `code/common/message.hpp` | 消息模型层 | 定义 RPC、Topic、Service 三类请求/响应消息，以及消息工厂，负责把业务语义映射成可传输的 JSON 消息对象。 |
| `code/common/net.hpp` | 网络适配与传输协议层 | 基于 Muduo 实现 TCP 客户端/服务端、连接封装、缓冲区适配和自定义报文封包/拆包。 |
| `code/common/dispatcher.hpp` | 消息分发层 | 按消息类型注册和调用对应处理函数，把网络层收到的消息分发给 RPC、注册发现或发布订阅模块。 |

### client 客户端层

| 文件 | 模块 | 作用 |
| --- | --- | --- |
| `code/client/Requestor.hpp` | 请求管理模块 | 管理已发送但未完成的请求，通过请求 ID 匹配响应，支持同步等待、异步 future 和回调三种响应处理方式。 |
| `code/client/RpcCaller​.hpp` | RPC 调用构造模块 | 面向客户端调用方封装 RPC 请求构造、发送和结果解析，把方法名与参数转换为 RPC 请求消息。|
| `code/client/DiscoverRegistry.hpp` | 客户端服务注册/发现模块 | 提供服务注册请求和服务发现请求逻辑，并在客户端维护方法到服务地址的缓存与简单轮询选择。 |
| `code/client/PublishSubscribe.hpp` | 客户端发布订阅模块 | 封装主题创建、删除、订阅、取消订阅和发布消息请求，并保存订阅回调用于处理服务端推送。 |
| `code/client/client.hpp` | 客户端门面模块 | 组合 Requestor、Dispatcher、RpcCaller、服务发现和发布订阅模块，对外提供 RegistryClient、DiscoveryClient、RpcClient、TopicClient 等易用接口。 |

### server 服务端层

| 文件 | 模块 | 作用 |
| --- | --- | --- |
| `code/server/RpcRouter.hpp` | RPC 服务路由模块 | 管理服务方法描述，校验请求参数，调用注册的本地业务函数，并生成 RPC 响应。 |
| `code/server/DiscoverRegistry.hpp` | 服务注册中心模块 | 维护服务提供者、服务发现者和方法到地址的关系，处理服务注册、服务发现以及上下线通知。 |
| `code/server/PublishSubscribe.hpp` | 服务端主题中转模块 | 管理主题和订阅者关系，处理主题创建、删除、订阅、取消订阅和消息广播。 |
| `code/server/server.hpp` | 服务端门面模块 | 组合网络服务端、Dispatcher 和具体业务模块，对外提供 RegistryServer、RpcServer、TopicServer 三类服务端入口。 |

## 模块之间的使用关系

### 基础依赖关系

`fields.hpp` 定义协议常量和枚举，`Base.hpp` 基于这些定义抽象出网络、消息和客户端/服务端接口。`details.hpp` 和 `Logger.hpp` 提供 JSON、ID、日志等通用能力。`message.hpp` 使用公共字段和 JSON 工具定义具体消息类型，`net.hpp` 使用 Muduo 实现连接、客户端、服务端和报文传输，`dispatcher.hpp` 则把收到的消息按类型转交给业务模块。

### RPC 调用链路

客户端侧由 `client.hpp` 中的 `RpcClient` 对外提供调用入口。`RpcClient` 选择目标连接后，交给 `RpcCaller​.hpp` 构造 RPC 请求，再由 `Requestor.hpp` 发送并记录请求。网络层 `net.hpp` 完成传输，服务端 `server.hpp` 中的 `RpcServer` 收到消息后通过 `dispatcher.hpp` 分发到 `RpcRouter.hpp`。`RpcRouter.hpp` 查找已注册的方法、校验参数、调用业务回调并返回响应，客户端 `Requestor.hpp` 再根据请求 ID 将响应交还给同步、异步或回调调用方。

### 服务注册发现链路

服务提供方的 `RpcServer` 如果启用注册功能，会通过 `client.hpp` 中的 `RegistryClient` 和 `client/DiscoverRegistry.hpp` 向注册中心发送注册请求。注册中心由 `server.hpp` 中的 `RegistryServer` 启动，实际注册和发现逻辑在 `server/DiscoverRegistry.hpp` 中维护。客户端启用服务发现时，`RpcClient` 会通过 `DiscoveryClient` 查询注册中心得到服务地址，并缓存到本地，后续 RPC 调用复用这些地址。

### 发布订阅链路

发布订阅由 `client.hpp` 中的 `TopicClient` 对外提供接口。客户端侧 `client/PublishSubscribe.hpp` 构造主题操作请求，并注册本地订阅回调。服务端由 `server.hpp` 中的 `TopicServer` 启动，收到主题请求后通过 `dispatcher.hpp` 分发到 `server/PublishSubscribe.hpp`。服务端主题模块维护主题与订阅者关系，发布消息时把消息广播给订阅该主题的客户端，客户端再触发本地回调。

## 整体分层

- `common/` 是公共基础设施，负责协议字段、抽象接口、消息模型、网络传输、日志和工具函数。
- `client/` 是客户端功能封装，负责发起 RPC、注册/发现服务、发布/订阅主题以及处理响应。
- `server/` 是服务端功能封装，负责 RPC 路由、注册中心、主题中转和服务端入口组合。
- `code/test/` 是示例和测试代码demo。

## 性能压测

测试环境：AMD EPYC 7K62 虚拟机（4 vCPU）、3.6 GiB 内存、Ubuntu 22.04、g++ 11.4、`-O2`、单机 loopback；服务端绑 core 0，客户端 core 1-3。压测程序在 `code/test/test_bench/`，完整数据见 `code/test/test_bench/results/RESULTS.md`。

### RPC 吞吐与延迟

| 用例 | 线程/连接 | QPS | avg (ms) | p99 (ms) |
| --- | --- | ---: | ---: | ---: |
| Add | 1 线程 / 多连接 | 8,925 | 0.112 | 0.281 |
| Add | 4 线程 / 多连接 | 27,738 | 0.142 | 0.439 |
| Add | 8 线程 / 多连接 | 34,955 | 0.224 | 0.473 |
| Add | 16 线程 / 多连接 | 36,036 | 0.434 | 0.811 |
| Add | 8 线程 / 单连接共享 | 41,366 | 0.190 | 0.264 |
| Echo 1KB | 8 线程 / 多连接 | 28,539 | 0.277 | 0.475 |
| Echo 16KB | 8 线程 / 多连接 | 6,473 | 1.158 | 1.932 |
| Add + 服务发现（1 Provider） | 8 线程 | 33,424 | 0.232 | 0.408 |
| Add + 服务发现（2 Provider） | 8 线程 | 31,780 | 0.251 | 0.423 |

### 发布订阅与稳定性

| 场景 | 结果 |
| --- | --- |
| 发布 50,000 条消息，2 个订阅者 | 发布 6,586 QPS，两个订阅者各收到 50,000 条 |
| 40 万次 Add 长跑 | 32,553 QPS，0 错误，fd 8→8，RSS 2.3 MB→4.5 MB |

### 主要结论

1. Add 吞吐上限约 **3.5~3.6 万 QPS**，8 线程后基本走平，服务端 CPU 达 97%~98%，瓶颈是单 EventLoop 线程。
2. 饱和点延迟仍较低：8 线程 avg 0.224 ms / p99 0.473 ms；16 线程 p99 升至 0.811 ms，继续加并发主要增加排队延迟。
3. 8 线程共享一条连接的 QPS（41,366）高于 8 条连接（37,247），单连接多路复用在该并发下更划算。
4. payload 影响明显：Echo 1KB 为 28,539 QPS，16KB 降至 6,473 QPS，JSON 序列化和数据拷贝是主要成本。
5. 服务发现稳态开销小（33,424 QPS，接近直连）；两个 Provider 的请求分布为 50.0% / 50.0%。
6. 发布订阅 50,000 条消息全部投递成功，发布侧 6,586 QPS。

> 单机 loopback 下客户端与服务端共享 CPU，以上结果应视为吞吐下界。