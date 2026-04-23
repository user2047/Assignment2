#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <chrono>
#include <ctime>
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

    std::clock_t cpuStart = std::clock();
    auto wallStart = std::chrono::high_resolution_clock::now();

    // Phase 0: Split input Array into Chunks
    vector<vector<int>> chunkArrays(numChunks);
    splitIntoEqualChunks(Array, chunkArrays);

    // localSamples[i] = samples chosen from chunk i
    vector<vector<int>> localSamples(numChunks);

    // localBuckets[i][b] = elements from chunk i that belong in bucket b
    vector<vector<vector<int>>> localBuckets(numChunks, vector<vector<int>>(numChunks));

    // counts[i][b] = number of elements in localBuckets[i][b]
    vector<vector<int>> counts(numChunks, vector<int>(numChunks, 0));

    // Create a thread pool with one thread per chunk
    ThreadPool pool(numChunks);

    // Phase 1: One fine-grained task per chunk - sort each chunk and choose local samples

    // Create a latch to wait for all Phase 1 tasks to complete
    Latch phase1Latch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit a task to the thread pool for chunk i
        pool.submitTask([&, i]()
        {
            // Sort the chunk
            sort(chunkArrays[i].begin(), chunkArrays[i].end());

            // Pick regular sample values from the sorted chunk and store them in the matching slot
            localSamples[i] = chooseRegularSamples(chunkArrays[i], numChunks - 1);

            // Signal that this task is done
            phase1Latch.countDown();
        });
    }

    // Wait for all Phase 1 tasks to finish before proceeding
    phase1Latch.wait();

    // Phase 2: Gather samples and choose splitters
    vector<int> allSamples;
    for (int i = 0; i < numChunks; i++)
        append(allSamples, localSamples[i]);

    sort(allSamples.begin(), allSamples.end());
    vector<int> splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 3: One fine-grained task per chunk - partition each chunk into local buckets

    // Create a latch to wait for all Phase 3 tasks to complete
    Latch phase3Latch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit a partitioning task to the thread pool for chunk i
        pool.submitTask([&, i]()
        {
            // Iterate over every value in that chunk
            for (int x : chunkArrays[i])
            {
                // Determine which bucket this value belongs in
                int b = findBucket(x, splitters);

                // Place the value into the correct local bucket
                localBuckets[i][b].push_back(x);
            }

            // Count the number of elements in each bucket and store it in counts[i][b]
            for (int b = 0; b < numChunks; b++)
                counts[i][b] = static_cast<int>(localBuckets[i][b].size());

            // Signal that this task is done
            phase3Latch.countDown();
        });
    }

    // Wait for all Phase 3 tasks to finish before proceeding
    phase3Latch.wait();

    // Phase 4: Compute final bucket sizes and output positions
    vector<int> bucketSizes(numChunks, 0);
    for (int b = 0; b < numChunks; b++)
    {
        for (int i = 0; i < numChunks; i++)
            bucketSizes[b] += counts[i][b];
    }

    vector<int> bucketStart(numChunks, 0);
    for (int b = 1; b < numChunks; b++)
        bucketStart[b] = bucketStart[b - 1] + bucketSizes[b - 1];

    vector<int> output(arraySize);

    // Phase 5: One fine-grained task per bucket - merge into output and sort in place
    // Work-stealing handles load imbalance across uneven bucket sizes

    // Create a latch to wait for all Phase 5 tasks to complete
    Latch phase5Latch(numChunks);

    // Iterate b from 0 to numChunks-1
    for (int b = 0; b < numChunks; b++)
    {
        // Submit a merge+sort task to the thread pool for bucket b
        pool.submitTask([&, b]()
        {
            // Write the start position for this bucket in the output array
            int start = bucketStart[b];
            int pos   = start;

            // Merge all pieces of bucket b from each chunk directly into the output array
            for (int i = 0; i < numChunks; i++)
            {
                for (int value : localBuckets[i][b])
                    output[pos++] = value;
            }

            // Sort the bucket's slice of the output array in place
            sort(output.begin() + start, output.begin() + start + bucketSizes[b]);

            // Signal that this task is done
            phase5Latch.countDown();
        });
    }

    // Wait for all Phase 5 tasks to finish before proceeding
    phase5Latch.wait();

    // Shut down the thread pool now that all tasks are complete
    pool.shutdown();

    if (stats)
    {
        auto wallEnd = std::chrono::high_resolution_clock::now();
        std::clock_t cpuEnd = std::clock();

        stats->wallTimeSeconds = std::chrono::duration<double>(wallEnd - wallStart).count();
        stats->cpuTimeSeconds  = static_cast<double>(cpuEnd - cpuStart) / CLOCKS_PER_SEC;

        stats->bucketStats.sizes.resize(numChunks);
        for (int b = 0; b < numChunks; b++)
            stats->bucketStats.sizes[b] = bucketSizes[b];
    }

    return output;
}