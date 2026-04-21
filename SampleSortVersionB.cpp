#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <vector>

using std::sort;
using std::vector;

vector<int> sampleSortVersionB(vector<int>& array, int numChunks, SortStats* stats)
{
    int arraySize = static_cast<int>(array.size());

    if (arraySize <= 1)
    {
        return array;
    }

    if (numChunks <= 0)
    {
        numChunks = 1;
    }

    if (numChunks > arraySize)
    {
        numChunks = arraySize;
    }

    vector<vector<int>> chunks(numChunks);
    splitIntoEqualChunks(array, chunks);

    vector<vector<int>> localSamples(numChunks);
    vector<int> allSamples;
    vector<int> splitters;

    vector<vector<vector<int>>> localBuckets(
        numChunks,
        vector<vector<int>>(numChunks)
    );

    vector<vector<int>> finalBuckets(numChunks);

    ThreadPool pool(numChunks);
    std::atomic<bool> taskFailed{ false };

    std::clock_t cpuStart = std::clock();
    auto wallStart = std::chrono::high_resolution_clock::now();

    // Phase 1: one fine-grained task per chunk
    Latch sortLatch(numChunks);
    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
            {
                try
                {
                    sort(chunks[i].begin(), chunks[i].end());
                    localSamples[i] = chooseRegularSamples(chunks[i], numChunks - 1);
                }
                catch (...)
                {
                    taskFailed.store(true);
                }

                sortLatch.countDown();
            });
    }
    sortLatch.wait();

    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = array;
        sort(fallback.begin(), fallback.end());
        return fallback;
    }

    // Phase 2: gather and select splitters
    for (int i = 0; i < numChunks; i++)
    {
        append(allSamples, localSamples[i]);
    }

    sort(allSamples.begin(), allSamples.end());
    splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 3: partition tasks
    Latch partitionLatch(numChunks);
    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
            {
                try
                {
                    for (int value : chunks[i])
                    {
                        int bucketIndex = findBucket(value, splitters);
                        localBuckets[i][bucketIndex].push_back(value);
                    }
                }
                catch (...)
                {
                    taskFailed.store(true);
                }

                partitionLatch.countDown();
            });
    }
    partitionLatch.wait();

    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = array;
        sort(fallback.begin(), fallback.end());
        return fallback;
    }

    // Phase 4: merge tasks
    Latch mergeLatch(numChunks);
    for (int b = 0; b < numChunks; b++)
    {
        pool.submitTask([&, b]()
            {
                try
                {
                    vector<int> merged;

                    for (int i = 0; i < numChunks; i++)
                    {
                        append(merged, localBuckets[i][b]);
                    }

                    finalBuckets[b] = std::move(merged);
                }
                catch (...)
                {
                    taskFailed.store(true);
                }

                mergeLatch.countDown();
            });
    }
    mergeLatch.wait();

    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = array;
        sort(fallback.begin(), fallback.end());
        return fallback;
    }

    // Phase 5: final bucket sorts
    Latch finalSortLatch(numChunks);
    for (int b = 0; b < numChunks; b++)
    {
        pool.submitTask([&, b]()
            {
                try
                {
                    sort(finalBuckets[b].begin(), finalBuckets[b].end());
                }
                catch (...)
                {
                    taskFailed.store(true);
                }

                finalSortLatch.countDown();
            });
    }
    finalSortLatch.wait();

    vector<int> result;
    result.reserve(arraySize);

    for (int b = 0; b < numChunks; b++)
    {
        append(result, finalBuckets[b]);
    }

    pool.shutdown();

    if (taskFailed.load())
    {
        vector<int> fallback = array;
        sort(fallback.begin(), fallback.end());
        return fallback;
    }

    if (stats)
    {
        auto wallEnd = std::chrono::high_resolution_clock::now();
        std::clock_t cpuEnd = std::clock();
        stats->wallTimeSeconds = std::chrono::duration<double>(wallEnd - wallStart).count();
        stats->cpuTimeSeconds  = static_cast<double>(cpuEnd - cpuStart) / CLOCKS_PER_SEC;
        stats->bucketStats.sizes.resize(numChunks);
        for (int b = 0; b < numChunks; b++)
            stats->bucketStats.sizes[b] = static_cast<int>(finalBuckets[b].size());
    }

    return result;
}