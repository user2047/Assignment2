#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <atomic>
#include <vector>

using std::max;
using std::min;
using std::sort;
using std::vector;

namespace
{
    int safeSampleCount(const vector<int>& chunk, int desiredCount)
    {
        if (chunk.size() <= 1)
        {
            return 1;
        }

        int maxUseful = static_cast<int>(chunk.size()) - 1;
        return max(1, min(desiredCount, maxUseful));
    }

    int estimateOversampleFactor(const vector<int>& previewSamples)
    {
        if (previewSamples.size() < 2)
        {
            return 2;
        }

        int repeatedNeighbors = 0;
        long long totalGap = 0;
        long long largestGap = 0;

        for (size_t i = 1; i < previewSamples.size(); i++)
        {
            long long gap = static_cast<long long>(previewSamples[i]) -
                            static_cast<long long>(previewSamples[i - 1]);

            if (previewSamples[i] == previewSamples[i - 1])
            {
                repeatedNeighbors++;
            }

            if (gap > largestGap)
            {
                largestGap = gap;
            }

            totalGap += gap;
        }

        double duplicateRatio =
            static_cast<double>(repeatedNeighbors) /
            static_cast<double>(previewSamples.size() - 1);

        double gapSkew = (totalGap > 0)
            ? static_cast<double>(largestGap) / static_cast<double>(totalGap)
            : 1.0;

        if (duplicateRatio > 0.25 || gapSkew > 0.35)
        {
            return 4;
        }

        return 2;
    }
}

vector<int> sampleSortVersionC(vector<int>& array, int numChunks)
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

    vector<vector<int>> previewLocalSamples(numChunks);
    vector<int> previewAllSamples;

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

    // Phase 1: local sort + preview sampling
    Latch sortLatch(numChunks);
    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            try
            {
                sort(chunks[i].begin(), chunks[i].end());

                int previewCount = safeSampleCount(chunks[i], numChunks - 1);
                previewLocalSamples[i] = chooseRegularSamples(chunks[i], previewCount);
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

    // Phase 2: estimate data skew and choose oversampling
    for (int i = 0; i < numChunks; i++)
    {
        append(previewAllSamples, previewLocalSamples[i]);
    }

    sort(previewAllSamples.begin(), previewAllSamples.end());

    int oversampleFactor = estimateOversampleFactor(previewAllSamples);
    int desiredSamplesPerChunk = oversampleFactor * (numChunks - 1);

    // Phase 3: full oversampling pass
    Latch resampleLatch(numChunks);
    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            try
            {
                int sampleCount = safeSampleCount(chunks[i], desiredSamplesPerChunk);
                localSamples[i] = chooseRegularSamples(chunks[i], sampleCount);
            }
            catch (...)
            {
                taskFailed.store(true);
            }

            resampleLatch.countDown();
        });
    }
    resampleLatch.wait();

    if (taskFailed.load())
    {
        pool.shutdown();
        vector<int> fallback = array;
        sort(fallback.begin(), fallback.end());
        return fallback;
    }

    for (int i = 0; i < numChunks; i++)
    {
        append(allSamples, localSamples[i]);
    }

    sort(allSamples.begin(), allSamples.end());
    splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 4: partition tasks
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

    // Phase 5: merge tasks
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

    // Phase 6: final sort tasks
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

    return result;
}