#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
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

    // --------------------------------------------------
    // Phase 0: Setup
    // --------------------------------------------------
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

    // --------------------------------------------------
    // Phase 1: Local sort + preview sampling
    // --------------------------------------------------
    Latch sortLatch(numChunks);

    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            sort(chunks[i].begin(), chunks[i].end());

            int previewCount = safeSampleCount(chunks[i], numChunks - 1);
            previewLocalSamples[i] = chooseRegularSamples(chunks[i], previewCount);

            sortLatch.countDown();
        });
    }

    sortLatch.wait();

    // --------------------------------------------------
    // Phase 2: Estimate skew and choose oversampling factor
    // --------------------------------------------------
    for (int i = 0; i < numChunks; i++)
    {
        append(previewAllSamples, previewLocalSamples[i]);
    }

    sort(previewAllSamples.begin(), previewAllSamples.end());

    int oversampleFactor = estimateOversampleFactor(previewAllSamples);
    int desiredSamplesPerChunk = oversampleFactor * (numChunks - 1);

    // --------------------------------------------------
    // Phase 3: Full oversampling pass
    // --------------------------------------------------
    Latch resampleLatch(numChunks);

    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            int sampleCount = safeSampleCount(chunks[i], desiredSamplesPerChunk);
            localSamples[i] = chooseRegularSamples(chunks[i], sampleCount);
            resampleLatch.countDown();
        });
    }

    resampleLatch.wait();

    for (int i = 0; i < numChunks; i++)
    {
        append(allSamples, localSamples[i]);
    }

    sort(allSamples.begin(), allSamples.end());
    splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // --------------------------------------------------
    // Phase 4: Partition each chunk into local buckets
    // --------------------------------------------------
    Latch partitionLatch(numChunks);

    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            for (int value : chunks[i])
            {
                int bucketIndex = findBucket(value, splitters);
                localBuckets[i][bucketIndex].push_back(value);
            }

            partitionLatch.countDown();
        });
    }

    partitionLatch.wait();

    // --------------------------------------------------
    // Phase 5: Merge local bucket pieces into final buckets
    // --------------------------------------------------
    Latch mergeLatch(numChunks);

    for (int b = 0; b < numChunks; b++)
    {
        pool.submitTask([&, b]()
        {
            vector<int> merged;

            for (int i = 0; i < numChunks; i++)
            {
                append(merged, localBuckets[i][b]);
            }

            finalBuckets[b] = std::move(merged);
            mergeLatch.countDown();
        });
    }

    mergeLatch.wait();

    // --------------------------------------------------
    // Phase 6: Final sort of each bucket
    // --------------------------------------------------
    Latch finalSortLatch(numChunks);

    for (int b = 0; b < numChunks; b++)
    {
        pool.submitTask([&, b]()
        {
            sort(finalBuckets[b].begin(), finalBuckets[b].end());
            finalSortLatch.countDown();
        });
    }

    finalSortLatch.wait();

    // --------------------------------------------------
    // Phase 7: Concatenate results
    // --------------------------------------------------
    vector<int> result;
    result.reserve(arraySize);

    for (int b = 0; b < numChunks; b++)
    {
        append(result, finalBuckets[b]);
    }

    pool.shutdown();
    return result;
}