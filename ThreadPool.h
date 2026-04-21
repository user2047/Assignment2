#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

using std::size_t;
using std::function;
using std::deque;
using std::mutex;
using std::thread;
using std::vector;
using std::condition_variable;
using std::atomic;
using std::move;


class ThreadPool
{
public:
    using Task = function<void()>;

    explicit ThreadPool(size_t numWorkers);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void submitTask(Task task);
    void shutdown();

private:
    struct TaskQueue
    {
        deque<Task> tasks;
        mutex mutex;
    };

    vector<thread> workers_;
    vector<TaskQueue> queues_;

    condition_variable cv_;
    mutex cvMutex_;

    atomic<bool> shuttingDown_;
    atomic<size_t> nextQueue_;

    void workerLoop(size_t workerId);
    bool tryPopOwn(size_t workerId, Task& task);
    bool trySteal(size_t workerId, Task& task);
    bool hasPendingTasks();
};