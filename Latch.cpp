#include "Latch.h"

#include <stdexcept>

using std::invalid_argument;
using std::ptrdiff_t;
using std::logic_error;
using std::lock_guard;
using std::unique_lock;
using std::mutex;
using std::condition_variable;
using std::move;


Latch::Latch(ptrdiff_t count)
    : count_(count)
{
    if (count < 0)
    {
        throw invalid_argument("Latch count cannot be negative.");
    }
}

void Latch::countDown(ptrdiff_t n)
{
    if (n < 0)
    {
        throw invalid_argument("Latch countDown value cannot be negative.");
    }

    bool reachedZero = false;

    {
        lock_guard<mutex> lock(mutex_);

        if (n > count_)
        {
            throw logic_error("Latch countDown would make count negative.");
        }

        count_ -= n;
        reachedZero = (count_ == 0);
    }

    if (reachedZero)
    {
        cv_.notify_all();
    }
}

void Latch::wait()
{
    unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]()
    {
        return count_ == 0;
    });
}

ptrdiff_t Latch::count() const
{
    lock_guard<mutex> lock(mutex_);
    return count_;
}