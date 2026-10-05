#include "ThreadPool.h"

// 构造函数：线程池开始工作，threads指定线程数量
ThreadPool::ThreadPool(const Config &config) : stop(false), config_(config)
{
    if (config_.thread_count == 0)
    {
        throw std::invalid_argument("thread count must be greater than 0");
    }

    if (config_.max_queue_size == 0)
    {
        throw std::invalid_argument("max queue size must be greater than 0");
    }

    // 预先分配好线程池的消费者线程数量，避免频繁扩容
    workers.reserve(config_.thread_count);

    try
    {
        for (size_t i = 0; i < config_.thread_count; i++)
        {
            // emplace_lock 直接在底层构造线程对象
            workers.emplace_back([this]()
                                 {
            //这是一个死循环：只要消费者不被释放，就一直在这轮转
            while (true)
            {
                std::function<void()> task; //准备一个任务，来接任务缓冲队列中的任务
                {
                    // === 临界区开始：准备从任务缓冲队列拿任务，先加锁 ===
                    std::unique_lock<std::mutex> lock (this->queue_mutex); //上锁，保护任务缓冲队列

                    // 核心魔法：条件变量的 wait！
                    // 消费者在这里阻塞睡觉。他什么时候醒？满足以下两个条件之一才醒：
                    // 1. 线程池关闭了 (this->stop == true)
                    // 2. 任务缓冲队列里有新任务了 (!this->tasks.empty())

                    // wait的第一个参数是一个 unique_lock 对象，第二个参数是一个 lambda 表达式，表示唤醒的条件。
                    // wait之前必须有锁
                    this->condition.wait(lock , [this]{     //lock是wait的参数，用来唤醒对应的消费者，第二个参数是唤醒的条件
                        return this->stop || !this->tasks.empty();
                    });

                    // 醒来来之后如果线程池关闭了并且任务缓冲队列也空了，就可以释放了
                    if (this->stop && this->tasks.empty())
                    {
                        return ;
                    }

                    //从小票夹中取出第一个任务
                    task = std::move(this->tasks.front()); // 取出任务缓冲队列的第一个任务,move的时间复杂度是O(1)
                    this->tasks.pop(); //把这个任务从任务缓冲队列里拿掉
                }//临界区结束
                task(); // 调用这个任务 --- 这就是线程工作的过程！
            } });
        }
    }
    catch (...)
    {
        // 如果创建线程失败了，需要把已经创建的线程释放掉
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true; // 先把线程池状态改成关闭，通知所有消费者
        }

        condition.notify_all(); // 叫醒所有wait的消费者

        for (std::thread &worker : workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        /*
            前面 try 里已经有一个异常抛出来了
            catch (...) 把它接住了
            你在 catch 里先做清理
            清理完之后，用 throw; 把“刚才那个原始异常”继续往外抛
        */
        throw;
    }
}

// 线程池关闭：通知所有消费者停止工作，并且等待他们结束
void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex);

        // 幂等：如果已经关闭，直接返回
        if (stop)
        {
            return;
        }

        // 禁止 worker 线程内部调用 shutdown
        for (auto &worker : workers)
        {
            if (worker.get_id() == std::this_thread::get_id())
            {
                throw std::runtime_error("shutdown() must not be called from a worker thread");
            }
        }

        stop = true;
    }

    condition.notify_all();

    for (auto &worker : workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

ThreadPool::~ThreadPool()
{
    // {
    //     std::unique_lock<std::mutex> lock(queue_mutex);
    //     // if (stop)
    //     // {
    //     //     return;
    //     // }
    //     // 这里可以不需要提前返回，因为可能出现worker.join();失败的情况，所以建议再次检查一遍
    //     stop = true; // 关闭线程池
    // }

    // this->condition.notify_all(); // 叫醒所有wait的消费者

    // // join所有的消费者线程，等他们都释放了
    // for (std::thread &worker : workers)
    // {
    //     if (worker.joinable())
    //     {
    //         worker.join();
    //     }
    // }

    shutdown(); // 直接调用 shutdown() 来实现析构函数的功能，避免代码重复
}
