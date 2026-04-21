#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <vector>

using std::sort;
using std::vector;

vector<int> sampleSortVersionB(vector<int>& array, int numChunks)
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

    // Phase 0: Setup

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

    // Phase 1: Local sort + local sample selection
    Latch sortLatch(numChunks);

    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
            {
                sort(chunks[i].begin(), chunks[i].end());
                localSamples[i] = chooseRegularSamples(chunks[i], numChunks - 1);
                sortLatch.countDown();
            });
    }

    sortLatch.wait();

    // Phase 2: Gather all samples and choose splitters
    for (int i = 0; i < numChunks; i++)
    {
        append(allSamples, localSamples[i]);
    }

    sort(allSamples.begin(), allSamples.end());
    splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 3: Partition each chunk into local buckets
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

    // Phase 4: Merge local bucket pieces into final buckets
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

    // Phase 5: Final sort of each bucket

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

    // Phase 6: Concatenate all final buckets
    vector<int> result;
    result.reserve(arraySize);

    for (int b = 0; b < numChunks; b++)
    {
        append(result, finalBuckets[b]);
    }

    pool.shutdown();
    return result;
}