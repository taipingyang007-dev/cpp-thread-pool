#include "ThreadPool.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
// 统一打印单个测试用例的结果。
void print_result(const std::string &name, bool passed)
{
  std::cout << (passed ? "[PASS] " : "[FAIL] ") << name << '\n';
}

// 验证 thread_count = 0 时会被构造函数拒绝。
bool test_invalid_thread_count()
{
  try
  {
    ThreadPool::Config config;
    config.thread_count = 0;
    ThreadPool pool(config);
    (void)pool;
    return false;
  }
  catch (const std::invalid_argument &)
  {
    return true;
  }
}

// 验证 max_queue_size = 0 时会被构造函数拒绝。
bool test_invalid_queue_size()
{
  try
  {
    ThreadPool::Config config;
    config.max_queue_size = 0;
    ThreadPool pool(config);
    (void)pool;
    return false;
  }
  catch (const std::invalid_argument &)
  {
    return true;
  }
}

// 验证当任务队列已满时，线程池会拒绝继续接收新任务。
bool test_queue_limit_rejects_overflow()
{
  ThreadPool::Config config;
  config.thread_count = 1;
  config.max_queue_size = 1;

  ThreadPool pool(config);

  std::promise<void> release_first_task;
  std::shared_future<void> gate = release_first_task.get_future().share();
  std::atomic<bool> first_task_started{false};

  // 第一个任务先占住唯一的 worker，不让它太快结束。
  auto first = pool.enqueue([&]() {
    first_task_started = true;
    gate.wait();
    return 1;
  });

  // 等到第一个任务确认已经被 worker 取走，再塞第二个任务进队列。
  while (!first_task_started.load())
  {
    std::this_thread::yield();
  }

  // 这时队列中已经有一个排队任务了，再提交第三个任务应该触发拒绝策略。
  auto second = pool.enqueue([]() {
    return 2;
  });

  bool rejected = false;
  try
  {
    auto third = pool.enqueue([]() {
      return 3;
    });
    (void)third;
  }
  catch (const std::runtime_error &)
  {
    rejected = true;
  }

  release_first_task.set_value();
  first.get();
  second.get();
  pool.shutdown();
  return rejected;
}

// 验证 shutdown 会等待旧任务完成，并拒绝 shutdown 之后的新任务。
bool test_shutdown_drains_existing_tasks_and_rejects_new_tasks()
{
  ThreadPool::Config config;
  config.thread_count = 2;
  config.max_queue_size = 8;

  ThreadPool pool(config);
  std::atomic<int> finished{0};
  std::vector<std::future<void>> futures;

  // 先提交一批短任务，观察 shutdown 是否会等它们全部执行完。
  for (int i = 0; i < 4; ++i)
  {
    futures.emplace_back(pool.enqueue([&finished]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      ++finished;
    }));
  }

  pool.shutdown();

  // shutdown 返回后，所有历史任务都应该已经可安全收尾。
  for (auto &future : futures)
  {
    future.wait();
    if (future.valid())
    {
      future.get();
    }
  }

  // shutdown 之后继续 enqueue，应该立即抛异常。
  bool rejected = false;
  try
  {
    auto after_shutdown = pool.enqueue([]() {
      return 42;
    });
    (void)after_shutdown;
  }
  catch (const std::runtime_error &)
  {
    rejected = true;
  }

  return finished.load() == 4 && rejected;
}

// 验证 shutdown 可重复调用，不会因为二次关闭而崩溃。
bool test_shutdown_is_idempotent()
{
  ThreadPool::Config config;
  config.thread_count = 2;
  ThreadPool pool(config);

  auto future = pool.enqueue([]() {
    return 7;
  });

  if (future.get() != 7)
  {
    return false;
  }

  pool.shutdown();
  pool.shutdown();
  return true;
}

// 验证 worker 线程内部调用 shutdown 会抛出 runtime_error，
// 异常通过 future.get() 传播给任务提交者。
bool test_shutdown_called_from_worker()
{
  ThreadPool::Config config;
  config.thread_count = 1;
  config.max_queue_size = 4;

  ThreadPool pool(config);

  auto future = pool.enqueue([&pool]() -> int {
    pool.shutdown();
    return 123;
  });

  bool caught = false;
  try
  {
    future.get();
  }
  catch (const std::runtime_error &)
  {
    caught = true;
  }

  // 线程池因为 shutdown 抛异常，stop 可能已被设为 true，
  // 析构函数调用 shutdown() 是幂等的，不会出问题。
  return caught;
}

// 验证任务内部抛异常时，future.get() 能重新抛出该异常。
bool test_task_exception_propagates_through_future()
{
  ThreadPool::Config config;
  config.thread_count = 1;

  ThreadPool pool(config);

  auto future = pool.enqueue([]() -> int {
    throw std::runtime_error("task failed");
  });

  bool caught = false;
  try
  {
    future.get();
  }
  catch (const std::runtime_error &e)
  {
    caught = (std::string(e.what()) == "task failed");
  }

  pool.shutdown();
  return caught;
}
} // namespace

int main()
{
  std::cout << "--- ThreadPool 自测开始 ---\n";

  // 用统一表驱动的方式组织测试，后面继续加用例会更方便。
  struct TestCase
  {
    std::string name;
    bool (*fn)();
  };

  const std::vector<TestCase> tests = {
      {"非法线程数会抛异常", test_invalid_thread_count},
      {"非法队列容量会抛异常", test_invalid_queue_size},
      {"队列满时拒绝新任务", test_queue_limit_rejects_overflow},
      {"shutdown 会等旧任务完成并拒绝新任务", test_shutdown_drains_existing_tasks_and_rejects_new_tasks},
      {"shutdown 可重复调用", test_shutdown_is_idempotent},
      {"任务内部调用 shutdown 会抛出预期异常", test_shutdown_called_from_worker},
      {"任务抛异常后 future.get() 能捕获异常", test_task_exception_propagates_through_future},
  };

  int passed = 0;
  for (const auto &test : tests)
  {
    bool ok = false;
    try
    {
      ok = test.fn();
    }
    catch (const std::exception &e)
    {
      std::cout << "[FAIL] " << test.name << "，异常信息：" << e.what() << '\n';
      continue;
    }

    print_result(test.name, ok);
    if (ok)
    {
      ++passed;
    }
  }

  std::cout << "--- 自测结束：通过 " << passed << " / " << tests.size() << " ---\n";
  return passed == static_cast<int>(tests.size()) ? 0 : 1;
}
