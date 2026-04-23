#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <iostream>
#include <vector>

using std::sort;
using std::vector;

// function definition
vector<int> sampleSortVersionB(vector<int>& Array, int numChunks, SortStats* stats)
{
    // Array: input Array
    // numChunks: number of chunks

    // Length of the input Array
    int arraySize = static_cast<int>(Array.size());

    // Start wall clock and CPU timers
    std::clock_t cpuStart = std::clock();
    auto wallStart = std::chrono::high_resolution_clock::now();

    // Phase 0: Split input Array into Chunks

    // Create a vector of length numChunks
    vector<vector<int>> chunkArrays(numChunks);

    // Split input Array into numChunks and put fragments into chunkArrays
    splitIntoEqualChunks(Array, chunkArrays);

    // localSamples[i] = samples chosen from chunk i
    vector<vector<int>> localSamples(numChunks);

    // Create a vector of integers called allSamples
    vector<int> allSamples;

    // localBuckets[i][b] = elements from chunk i that belong in bucket b
    vector<vector<vector<int>>> localBuckets(numChunks, vector<vector<int>>(numChunks));

    // finalBuckets[b] = all elements that belong in bucket b, merged across all chunks
    vector<vector<int>> finalBuckets(numChunks);

    // Create a thread pool with one thread per chunk
    ThreadPool pool(numChunks);

    // Flag set to true if any task throws an exception
    std::atomic<bool> taskFailed{ false };

    // Phase 1: One fine-grained task per chunk - sort each chunk and choose local samples

    // Create a latch to wait for all Phase 1 tasks to complete
    Latch sortLatch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit a task to the thread pool for chunk i
        pool.submitTask([&, i]()
            {
                try
                {
                    // Sort the chunk
                    sort(chunkArrays[i].begin(), chunkArrays[i].end());

                    // Pick regular sample values from the sorted chunk and store them in the matching slot
                    localSamples[i] = chooseRegularSamples(chunkArrays[i], numChunks - 1);
                }
                catch (...)
                {
                    std::cerr << "[VersionB] Task failed in Phase 1 (sort/sample), chunk " << i << "\n";
                    taskFailed.store(true);
                }

                // Signal that this task is done
                sortLatch.countDown();
            });
    }

    // Wait for all Phase 1 tasks to finish before proceeding
    sortLatch.wait();

    // If any task failed, shut down the pool and fall back to sequential sort
    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = Array;
        sort(fallback.begin(), fallback.end());
        if (stats) stats->usedFallback = true;
        return fallback;
    }

    // Phase 2: Gather samples and choose splitters

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Merge each local sample list into one vector of all samples
        append(allSamples, localSamples[i]);
    }

    // Sort the samples so global boundaries can be chosen in the right order
    sort(allSamples.begin(), allSamples.end());

    // Choose global splitters from all samples
    vector<int> splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 3: One fine-grained task per chunk - partition each chunk into local buckets

    // Create a latch to wait for all Phase 3 tasks to complete
    Latch partitionLatch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit a partitioning task to the thread pool for chunk i
        pool.submitTask([&, i]()
            {
                try
                {
                    // Iterate over every value in that chunk
                    for (int value : chunkArrays[i])
                    {
                        // Determine which bucket this value belongs in
                        int bucketIndex = findBucket(value, splitters);

                        // Place the value into the correct local bucket
                        localBuckets[i][bucketIndex].push_back(value);
                    }
                }
                catch (...)
                {
                    std::cerr << "[VersionB] Task failed in Phase 3 (partition), chunk " << i << "\n";
                    taskFailed.store(true);
                }

                // Signal that this task is done
                partitionLatch.countDown();
            });
    }

    // Wait for all Phase 3 tasks to finish before proceeding
    partitionLatch.wait();

    // If any task failed, shut down the pool and fall back to sequential sort
    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = Array;
        sort(fallback.begin(), fallback.end());
        if (stats) stats->usedFallback = true;
        return fallback;
    }

    // Phase 4: One fine-grained task per bucket - merge local bucket pieces across all chunks

    // Create a latch to wait for all Phase 4 tasks to complete
    Latch mergeLatch(numChunks);

    // Iterate b from 0 to numChunks-1
    for (int b = 0; b < numChunks; b++)
    {
        // Submit a merge task to the thread pool for bucket b
        pool.submitTask([&, b]()
            {
                try
                {
                    // Create a vector of integers called merged
                    vector<int> merged;

                    // Iterate i from 0 to numChunks-1
                    for (int i = 0; i < numChunks; i++)
                    {
                        // Merge all pieces of bucket b from each chunk into merged
                        append(merged, localBuckets[i][b]);
                    }

                    // Move the merged result into the final bucket slot
                    finalBuckets[b] = std::move(merged);
                }
                catch (...)
                {
                    std::cerr << "[VersionB] Task failed in Phase 4 (merge), bucket " << b << "\n";
                    taskFailed.store(true);
                }

                // Signal that this task is done
                mergeLatch.countDown();
            });
    }

    // Wait for all Phase 4 tasks to finish before proceeding
    mergeLatch.wait();

    // If any task failed, shut down the pool and fall back to sequential sort
    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = Array;
        sort(fallback.begin(), fallback.end());
        if (stats) stats->usedFallback = true;
        return fallback;
    }

    // Phase 5: One fine-grained task per bucket - sort each final bucket

    // Create a latch to wait for all Phase 5 tasks to complete
    Latch finalSortLatch(numChunks);

    // Iterate b from 0 to numChunks-1
    for (int b = 0; b < numChunks; b++)
    {
        // Submit a sort task to the thread pool for bucket b
        pool.submitTask([&, b]()
            {
                try
                {
                    // Sort the final bucket in place
                    sort(finalBuckets[b].begin(), finalBuckets[b].end());
                }
                catch (...)
                {
                    std::cerr << "[VersionB] Task failed in Phase 5 (final sort), bucket " << b << "\n";
                    taskFailed.store(true);
                }

                // Signal that this task is done
                finalSortLatch.countDown();
            });
    }

    // Wait for all Phase 5 tasks to finish before proceeding
    finalSortLatch.wait();

    // Create a vector of integers called result, reserved to the size of the input array
    vector<int> result;
    result.reserve(arraySize);

    // Iterate b from 0 to numChunks-1
    for (int b = 0; b < numChunks; b++)
    {
        // Concatenate each sorted final bucket into result
        append(result, finalBuckets[b]);
    }

    // Shut down the thread pool now that all tasks are complete
    pool.shutdown();

    // If any task failed during Phase 5, fall back to sequential sort
    if (taskFailed.load())
    {
        vector<int> fallback = Array;
        sort(fallback.begin(), fallback.end());
        if (stats) stats->usedFallback = true;
        return fallback;
    }

    // If stats is not null, fill in the timing and bucket size information
    if (stats)
    {
        // Get the end time for both wall clock and CPU time
        auto wallEnd = std::chrono::high_resolution_clock::now();

        // Note: std::clock() gives CPU time used by the current process, which is an approximation of actual CPU time spent on the task
        std::clock_t cpuEnd = std::clock();

        // Calculate the elapsed time for both wall clock and CPU time and store it in stats
        stats->wallTimeSeconds = std::chrono::duration<double>(wallEnd - wallStart).count();

        // CPU time is given in clock ticks, so we divide by CLOCKS_PER_SEC to convert to seconds
        stats->cpuTimeSeconds  = static_cast<double>(cpuEnd - cpuStart) / CLOCKS_PER_SEC;

        // Store the bucket size information in stats
        stats->bucketStats.sizes.resize(numChunks);

        // Iterate b from 0 to numChunks-1
        for (int b = 0; b < numChunks-1; b++)

            // Store the size of each final bucket in stats
            stats->bucketStats.sizes[b] = static_cast<int>(finalBuckets[b].size());
    }

    // Return the sorted result array
    return result;
}