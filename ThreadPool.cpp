#include "ThreadPool.h"

#include <stdexcept>
#include <utility>

using std::atomic;
using std::condition_variable;
using std::deque;
using std::function;
using std::lock_guard;
using std::move;
using std::mutex;
using std::runtime_error;
using std::size_t;
using std::thread;
using std::vector;
using std::unique_lock;

ThreadPool::ThreadPool(size_t numWorkers)
    : queues_(numWorkers == 0 ? 1 : numWorkers),
      pendingCount_(0),
      shuttingDown_(false),
      nextQueue_(0)
{
    const size_t workerCount = queues_.size();
    workers_.reserve(workerCount);

    for (size_t i = 0; i < workerCount; ++i)
    {
        workers_.emplace_back(&ThreadPool::workerLoop, this, i);
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

void ThreadPool::submitTask(Task task)
{
    if (!task)
    {
        return;
    }

    if (shuttingDown_.load())
    {
        throw runtime_error("Cannot submit task to a shutting down ThreadPool.");
    }

    const size_t index = nextQueue_.fetch_add(1) % queues_.size();

    {
        lock_guard<mutex> lock(queues_[index].mutex);
        queues_[index].tasks.push_front(std::move(task));
    }

    {
        lock_guard<mutex> cvLock(cvMutex_);
        ++pendingCount_;
    }

    cv_.notify_one();
}

void ThreadPool::shutdown()
{
    bool expected = false;
    shuttingDown_.compare_exchange_strong(expected, true);

    cv_.notify_all();

    for (std::thread& worker : workers_)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }
}

bool ThreadPool::tryPopOwn(std::size_t workerId, Task& task)
{
    TaskQueue& queue = queues_[workerId];

    lock_guard<mutex> lock(queue.mutex);
    if (queue.tasks.empty())
    {
        return false;
    }

    task = move(queue.tasks.front());
    queue.tasks.pop_front();
    return true;
}

bool ThreadPool::trySteal(size_t workerId, Task& task)
{
    const size_t workerCount = queues_.size();

    for (size_t offset = 1; offset < workerCount; ++offset)
    {
        const size_t victim = (workerId + offset) % workerCount;
        TaskQueue& queue = queues_[victim];

        lock_guard<mutex> lock(queue.mutex);
        if (queue.tasks.empty())
        {
            continue;
        }

        task = move(queue.tasks.back());
        queue.tasks.pop_back();
        return true;
    }

    return false;
}

bool ThreadPool::hasPendingTasks()
{
    for (TaskQueue& queue : queues_)
    {
        lock_guard<mutex> lock(queue.mutex);
        if (!queue.tasks.empty())
        {
            return true;
        }
    }

    return false;
}

void ThreadPool::workerLoop(size_t workerId)
{
    while (true)
    {
        Task task;

        if (tryPopOwn(workerId, task) || trySteal(workerId, task))
        {
            try
            {
                task();
            }
            catch (...)
            {
                // Keep the worker alive even if a task throws.
            }

            {
                lock_guard<mutex> cvLock(cvMutex_);
                if (pendingCount_ > 0)
                    --pendingCount_;
            }

            continue;
        }

        unique_lock<mutex> lock(cvMutex_);
        cv_.wait(lock, [this]()
        {
            return shuttingDown_.load() || pendingCount_ > 0;
        });

        if (shuttingDown_.load() && !hasPendingTasks())
        {
            break;
        }
    }
}