#include "ThreadPool.h"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <vector>

// 模拟一个数据处理服务：
// 启动线程池 -> 提交混合任务 -> 收集结果 -> 优雅关闭
int main()
{
    // ===== 1. 创建线程池 =====
    ThreadPool::Config config;
    config.thread_count = 4;
    config.max_queue_size = 100;

    ThreadPool pool(config);
    std::cout << "[启动] 线程池已创建，4 个 worker 就绪\n\n";

    // ===== 2. 提交一批混合任务 =====
    std::vector<std::future<int>> int_results;
    std::vector<std::future<std::string>> str_results;

    // 2a. 计算任务：平方运算
    for (int i = 1; i <= 5; ++i)
    {
        int_results.emplace_back(pool.enqueue([i] {
            int result = i * i;
            std::cout << "  [worker] " << i << "^2 = " << result << "\n";
            return result;
        }));
    }

    // 2b. 字符串处理任务：拼接问候语
    for (int i = 1; i <= 3; ++i)
    {
        str_results.emplace_back(pool.enqueue([i] {
            std::string msg = "Hello, task #" + std::to_string(i);
            std::cout << "  [worker] 生成: " << msg << "\n";
            return msg;
        }));
    }

    // 2c. 延迟任务：模拟耗时操作
    auto slow_future = pool.enqueue([]() -> int {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        std::cout << "  [worker] 慢任务完成\n";
        return 999;
    });

    // 2d. 会失败的任务：展示异常传播
    auto fail_future = pool.enqueue([]() -> int {
        std::cout << "  [worker] 即将抛异常...\n";
        throw std::runtime_error("database connection failed");
    });

    std::cout << "[提交] 10 个任务已入队\n\n";

    // ===== 3. 收集结果 =====
    int success_count = 0;
    int fail_count = 0;

    std::cout << "[结果] 计算任务:\n";
    for (size_t i = 0; i < int_results.size(); ++i)
    {
        try
        {
            int val = int_results[i].get();
            std::cout << "  任务 " << (i + 1) << " 成功: " << val << "\n";
            ++success_count;
        }
        catch (const std::exception &e)
        {
            std::cout << "  任务 " << (i + 1) << " 失败: " << e.what() << "\n";
            ++fail_count;
        }
    }

    std::cout << "[结果] 字符串任务:\n";
    for (size_t i = 0; i < str_results.size(); ++i)
    {
        try
        {
            std::string val = str_results[i].get();
            std::cout << "  任务 " << (i + 1) << " 成功: " << val << "\n";
            ++success_count;
        }
        catch (const std::exception &e)
        {
            std::cout << "  任务 " << (i + 1) << " 失败: " << e.what() << "\n";
            ++fail_count;
        }
    }

    // 慢任务
    try
    {
        int val = slow_future.get();
        std::cout << "[结果] 慢任务成功: " << val << "\n";
        ++success_count;
    }
    catch (const std::exception &e)
    {
        std::cout << "[结果] 慢任务失败: " << e.what() << "\n";
        ++fail_count;
    }

    // 失败任务
    try
    {
        fail_future.get();
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "[结果] 预期失败: " << e.what() << "\n";
        ++fail_count;
    }

    std::cout << "\n[统计] 成功: " << success_count << ", 失败: " << fail_count << "\n\n";

    // ===== 4. 优雅关闭 =====
    pool.shutdown();
    std::cout << "[关闭] shutdown 完成，所有 worker 已退出\n\n";

    // ===== 5. 关闭后尝试提交 =====
    try
    {
        auto f = pool.enqueue([] { return 0; });
    }
    catch (const std::runtime_error &e)
    {
        std::cout << "[拒绝] shutdown 后 enqueue: " << e.what() << "\n";
    }

    std::cout << "\nDemo 完成。\n";
    return 0;
}
