#include <future>
#include <iostream>
#include <memory>
#include <thread>

int Add(int num1, int num2)
{
    std::cout << "into add!\n";
    return num1 + num2;
}

int main()
{
    // 1. 封装任务
    std::shared_ptr<std::packaged_task<int(int, int)>> task = std::make_shared<std::packaged_task<int(int, int)>>(Add);

    // 2. 获取任务包关联的future对象
    std::future<int> res = task->get_future();

    // 3. 创建一个线程执行任务
    std::thread thr([task]() { (*task)(1, 2); });

    // 4. 获取结果
    std::cout << res.get() << std::endl;
    thr.join();
    return 0;
}