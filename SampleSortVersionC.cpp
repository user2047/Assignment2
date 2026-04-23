// Version C: Oversampling Sample Sort with Thread Pool
//
// Optimization strategy: Adaptive oversampling.
// A standard sample sort picks (p-1) samples per chunk.  When the data is
// skewed (heavy duplicates or a very uneven gap distribution) those splitters
// cluster together and leave some buckets much larger than others, making the
// final per-bucket sort the bottleneck.
//
// Version C fixes this by:
//   1. Running a cheap preview sort + sample pass in parallel (Phase 1).
//   2. Analysing the merged preview samples on the main thread to detect skew.
//   3. If skew is detected, increasing the sample count (oversample factor 4x
//      instead of 1x) and running a second parallel sampling pass (Phase 3).
//   4. Choosing splitters from the larger pool, which spreads elements more
//      evenly across buckets even for skewed inputs.
//   5. Using the same low-overhead output path as Version A / Version B:
//      pre-sized output array, direct scatter, in-place sort per bucket.
//      No intermediate finalBuckets, no extra copies.

#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <vector>

using std::max;
using std::min;
using std::sort;
using std::vector;

namespace
{
    // Returns the number of samples to draw from a chunk, clamped to a safe range.
    int safeSampleCount(int chunkSize, int desired)
    {
        if (chunkSize <= 1) return 1;
        return max(1, min(desired, chunkSize - 1));
    }

    // Inspects the merged preview samples and returns an oversampling multiplier.
    // Returns 4 when heavy skew is detected, 2 otherwise.
    int estimateOversampleFactor(const vector<int>& samples)
    {
        if (static_cast<int>(samples.size()) < 2) return 2;

        int    repeated    = 0;
        long long totalGap = 0;
        long long maxGap   = 0;

        for (size_t i = 1; i < samples.size(); i++)
        {
            long long gap = static_cast<long long>(samples[i]) -
                            static_cast<long long>(samples[i - 1]);
            if (gap == 0) repeated++;
            if (gap > maxGap) maxGap = gap;
            totalGap += gap;
        }

        double dupRatio  = static_cast<double>(repeated) /
                           static_cast<double>(samples.size() - 1);
        double gapSkew   = (totalGap > 0)
                           ? static_cast<double>(maxGap) / static_cast<double>(totalGap)
                           : 1.0;

        return (dupRatio > 0.25 || gapSkew > 0.35) ? 4 : 2;
    }
}

vector<int> sampleSortVersionC(vector<int>& Array, int numChunks, SortStats* stats)
{
    int arraySize = static_cast<int>(Array.size());

    std::clock_t cpuStart = std::clock();
    auto wallStart = std::chrono::high_resolution_clock::now();

    // Phase 0: Split input Array into Chunks

    // Create a vector of length numChunks
    vector<vector<int>> chunkArrays(numChunks);

    // Split input Array into numChunks and put fragments into chunkArrays
    splitIntoEqualChunks(Array, chunkArrays);

    // previewLocalSamples[i] = cheap preview samples from chunk i (numChunks-1 samples)
    vector<vector<int>> previewLocalSamples(numChunks);

    // localSamples[i] = final (possibly oversampled) samples from chunk i
    vector<vector<int>> localSamples(numChunks);

    // localBuckets[i][b] = elements from chunk i that belong in bucket b
    vector<vector<vector<int>>> localBuckets(numChunks, vector<vector<int>>(numChunks));

    // counts[i][b] = number of elements in localBuckets[i][b]
    vector<vector<int>> counts(numChunks, vector<int>(numChunks, 0));

    // Create a thread pool with one thread per chunk
    ThreadPool pool(numChunks);

    // Phase 1: Sort each chunk and collect preview samples in parallel

    // Create a latch to wait for all Phase 1 tasks to complete
    Latch phase1Latch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit a sort+sample task to the thread pool for chunk i
        pool.submitTask([&, i]()
        {
            // Sort the chunk
            sort(chunkArrays[i].begin(), chunkArrays[i].end());

            // Pick numChunks-1 preview samples from the sorted chunk
            int count = safeSampleCount(static_cast<int>(chunkArrays[i].size()), numChunks - 1);
            previewLocalSamples[i] = chooseRegularSamples(chunkArrays[i], count);

            // Signal that this task is done
            phase1Latch.countDown();
        });
    }

    // Wait for all Phase 1 tasks to finish before proceeding
    phase1Latch.wait();

    // Phase 2: Analyse preview samples on the main thread to detect data skew

    // Merge all preview samples into one sorted vector
    vector<int> previewAllSamples;
    for (int i = 0; i < numChunks; i++)
        append(previewAllSamples, previewLocalSamples[i]);

    sort(previewAllSamples.begin(), previewAllSamples.end());

    // Choose an oversampling factor based on detected skew (1x or 4x)
    int oversampleFactor      = estimateOversampleFactor(previewAllSamples);
    int desiredSamplesPerChunk = oversampleFactor * (numChunks - 1);

    // Phase 3: Collect oversampled local samples in parallel
    // (If oversampleFactor == 1 this is identical to Phase 1 samples and reuses them)

    // Create a latch to wait for all Phase 3 tasks to complete
    Latch phase3Latch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit an oversampling task to the thread pool for chunk i
        pool.submitTask([&, i]()
        {
            // Draw desiredSamplesPerChunk samples from the already-sorted chunk
            int count = safeSampleCount(static_cast<int>(chunkArrays[i].size()),
                                        desiredSamplesPerChunk);
            localSamples[i] = chooseRegularSamples(chunkArrays[i], count);

            // Signal that this task is done
            phase3Latch.countDown();
        });
    }

    // Wait for all Phase 3 tasks to finish before proceeding
    phase3Latch.wait();

    // Merge all oversampled local samples and choose global splitters
    vector<int> allSamples;
    for (int i = 0; i < numChunks; i++)
        append(allSamples, localSamples[i]);

    sort(allSamples.begin(), allSamples.end());
    vector<int> splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 4: Partition each chunk into local buckets in parallel

    // Create a latch to wait for all Phase 4 tasks to complete
    Latch phase4Latch(numChunks);

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Submit a partitioning task to the thread pool for chunk i
        pool.submitTask([&, i]()
        {
            // Iterate over every value in that chunk
            for (int value : chunkArrays[i])
            {
                // Determine which bucket this value belongs in
                int b = findBucket(value, splitters);

                // Place the value into the correct local bucket
                localBuckets[i][b].push_back(value);
            }

            // Record bucket element counts for the prefix-sum in Phase 5
            for (int b = 0; b < numChunks; b++)
                counts[i][b] = static_cast<int>(localBuckets[i][b].size());

            // Signal that this task is done
            phase4Latch.countDown();
        });
    }

    // Wait for all Phase 4 tasks to finish before proceeding
    phase4Latch.wait();

    // Phase 5: Compute final bucket sizes and output positions (main thread)

    // Create a vector of integers called bucketSizes of length numChunks, initialized to 0
    vector<int> bucketSizes(numChunks, 0);

    // Sum up the counts for each bucket across all chunks
    for (int b = 0; b < numChunks; b++)
        for (int i = 0; i < numChunks; i++)
            bucketSizes[b] += counts[i][b];

    // Determine the starting output index for each bucket by doing a prefix sum on bucketSizes
    vector<int> bucketStart(numChunks, 0);
    for (int b = 1; b < numChunks; b++)
        bucketStart[b] = bucketStart[b - 1] + bucketSizes[b - 1];

    // Allocate the output array once - no further copies needed
    vector<int> output(arraySize);

    // Phase 6: Merge each bucket's pieces directly into output and sort in place

    // Create a latch to wait for all Phase 6 tasks to complete
    Latch phase6Latch(numChunks);

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
            phase6Latch.countDown();
        });
    }

    // Wait for all Phase 6 tasks to finish before proceeding
    phase6Latch.wait();

    // Shut down the thread pool now that all tasks are complete
    pool.shutdown();

    // If stats is not null, fill in the timing and bucket size information
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

    // Return the sorted output array
    return output;
}