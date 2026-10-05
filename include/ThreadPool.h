// 进行类和函数声明
#pragma once

#include <condition_variable>
#include <functional>
#include <future> // 用于获取异步返回值的取餐小票
#include <memory> // 用于智能指针 shared_ptr
#include <mutex>
#include <queue>
#include <stdexcept> // 用于抛出异常
#include <thread>
#include <vector>

class ThreadPool // 线程池类声明
{
public:
  struct Config
  {
    size_t thread_count = 4;
    size_t max_queue_size = 10000;
  };

  // 线程池创建：初始化几个消费者线程
  explicit ThreadPool(const Config &config);

  // 线程池关闭：清理资源，释放消费者线程
  ~ThreadPool();

  // 生产者往任务缓冲队列里塞新任务
  template <class F, class... Args>
  auto enqueue(F &&f, Args &&...args)
      -> std::future<typename std::result_of<F(Args...)>::type>
  {
    using return_type = typename std::result_of<F(Args...)>::type;

    // 2、将函数和参数“封装打包”，变成一个不需要外部参数的void 函数(packaged_task)
    auto task = std::make_shared<std::packaged_task<                 // make_shared std::packaged_task不支持拷贝，只能移动，而 std::function<void()>定位就是可以被随处传递、随处复制的、可调用对象，所以用shared_ptr来管理这个任务对象的生命周期
        return_type()>>(                                             // std::packaged_task< T > > Move-Only（仅限移动，禁止拷贝）
                                                                     // 内部的T不是要一个数据类型，而是要一个函数特征（签名），“数据类型（）”，不需要返回值并且执行结果吐出一个return_type的函数
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)); // bind函数把函数 f 和未来执行它所需要的具体数据 args... 绑定在一起
                                                                     // forward的作用是完美转发，保持参数的左值/右值属性不变

    // 3.std::future是一个占位符，代表了这个任务未来的结果。get_future()会返回一个std::future对象，这个对象可以用来获取任务执行后的结果。
    std::future<return_type> res = task->get_future();

    // 4. 生产者往任务缓冲队列里塞新任务，先加锁，保护任务缓冲队列
    {
      std::unique_lock<std::mutex> lock(queue_mutex);

      // 线程池关闭了，就不能再往里塞任务了
      if (stop)
      {
        throw std::runtime_error("enqueue on stopped ThreadPool");
      }

      // 线程池没有关闭，但任务缓冲队列满了，也不能再往里塞任务了
      if (tasks.size() >= config_.max_queue_size)
      {
        throw std::runtime_error("task queue is full");
      }

      // 往任务缓冲队列里塞一个任务
      tasks.emplace([task]()
                    { (*task)(); });
    }

    // 5. 唤醒一个消费者
    condition.notify_one();

    // 6. 返回这个任务未来的结果的取餐小票
    return res;
  }

  // 线程池关闭：通知所有消费者停止工作，并且等待他们结束
  void shutdown();

private:
  // 1、消费者集群 (Workers/Consumers)
  std::vector<std::thread> workers;

  // 2、任务缓冲队列 (Task Queue / Buffer)
  std::queue<std::function<void()>> tasks; // std::function<void()>定位就是可以被随处传递、随处复制的、可调用对象（函数对象、lambda表达式、函数指针等），并且这个可调用对象不需要参数，返回值也被忽略了（void)。
  
  // 3、并发同步原语 (Synchronization Primitives)
  std::mutex queue_mutex; // 保护任务缓冲队列的互斥锁
  
  std::condition_variable condition; // 条件变量，用于通知消费者有新任务

  // 4、线程池的状态：如果线程池要关闭了（stop = true）, 通知所有的消费者停止工作
  bool stop;

  // 5、线程池的配置
  Config config_;
};
