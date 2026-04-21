#pragma once

#include <condition_variable>
#include <cstddef>
#include <mutex>

using std::condition_variable;
using std::mutex;
using std::ptrdiff_t;

class Latch
{
public:
    explicit Latch(ptrdiff_t count);

    Latch(const Latch&) = delete;
    Latch& operator=(const Latch&) = delete;

    void countDown(ptrdiff_t n = 1);
    void wait();
    ptrdiff_t count() const;

private:
    mutable mutex mutex_;
    condition_variable cv_;
    ptrdiff_t count_;
};