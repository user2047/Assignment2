#include "sample_sort.h"
#include "ThreadPool.h"
#include "Latch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <ctime>
#include <immintrin.h>
#include <intrin.h>
#include <thread>
#include <vector>

using std::max;
using std::min;
using std::sort;
using std::vector;

namespace
{
    bool cpuSupportsAvx2()
    {
        int cpuInfo[4] = { 0 };
        __cpuid(cpuInfo, 0);
        if (cpuInfo[0] < 7)
            return false;

        __cpuidex(cpuInfo, 1, 0);
        const bool osxsave = (cpuInfo[2] & (1 << 27)) != 0;
        const bool avx = (cpuInfo[2] & (1 << 28)) != 0;
        if (!osxsave || !avx)
            return false;

        const unsigned long long xcr0 = _xgetbv(0);
        if ((xcr0 & 0x6) != 0x6)
            return false;

        __cpuidex(cpuInfo, 7, 0);
        return (cpuInfo[1] & (1 << 5)) != 0;
    }

    int safeSampleCount(const vector<int>& chunk, int desiredCount)
    {
        if (chunk.empty()) return 0;
        if (chunk.size() <= 1) return 1;
        int maxUseful = static_cast<int>(chunk.size()) - 1;
        return max(1, min(desiredCount, maxUseful));
    }

    int estimateOversampleFactor(const vector<int>& previewSamples)
    {
        if (previewSamples.size() < 2)
            return 2;

        int repeatedNeighbors = 0;
        long long totalGap = 0;
        long long largestGap = 0;

        for (size_t i = 1; i < previewSamples.size(); i++)
        {
            long long gap = static_cast<long long>(previewSamples[i]) -
                static_cast<long long>(previewSamples[i - 1]);

            if (previewSamples[i] == previewSamples[i - 1])
                repeatedNeighbors++;

            if (gap > largestGap)
                largestGap = gap;

            totalGap += gap;
        }

        double duplicateRatio = static_cast<double>(repeatedNeighbors) /
            static_cast<double>(previewSamples.size() - 1);

        double gapSkew = (totalGap > 0)
            ? static_cast<double>(largestGap) / static_cast<double>(totalGap)
            : 1.0;

        if (duplicateRatio > 0.25 || gapSkew > 0.35)
            return 4;

        return 2;
    }

    int findBucketAvx2(int value, const vector<int>& splitters)
    {
#if defined(__AVX2__) || defined(_M_X64)
        const __m256i vx = _mm256_set1_epi32(value);
        const int n = static_cast<int>(splitters.size());

        int i = 0;
        for (; i + 8 <= n; i += 8)
        {
            const __m256i vs = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&splitters[i]));
            const __m256i cmp = _mm256_cmpgt_epi32(vx, vs);
            const int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp));

            if (mask != 0xFF)
            {
                const unsigned int inv = static_cast<unsigned int>(~mask) & 0xFFu;
                unsigned long bitIndex = 0;
                _BitScanForward(&bitIndex, inv);
                return i + static_cast<int>(bitIndex);
            }
        }

        for (; i < n; i++)
            if (value <= splitters[i])
                return i;

        return n;
#else
        return findBucket(value, splitters);
#endif
    }

    void radixSortSigned32(vector<int>& data)
    {
        if (data.size() < 2)
            return;

        vector<int> temp(data.size());
        vector<int>* in = &data;
        vector<int>* out = &temp;

        for (int pass = 0; pass < 4; pass++)
        {
            int counts[256] = { 0 };
            const int shift = pass * 8;

            for (int v : *in)
            {
                unsigned int key = static_cast<unsigned int>(v) ^ 0x80000000u;
                counts[(key >> shift) & 0xFFu]++;
            }

            int offsets[256];
            offsets[0] = 0;
            for (int i = 1; i < 256; i++)
                offsets[i] = offsets[i - 1] + counts[i - 1];

            for (int v : *in)
            {
                unsigned int key = static_cast<unsigned int>(v) ^ 0x80000000u;
                int bucket = static_cast<int>((key >> shift) & 0xFFu);
                (*out)[offsets[bucket]++] = v;
            }

            std::swap(in, out);
        }

        if (in != &data)
            data.swap(*in);
    }
}

vector<int> sampleSortVersionD(vector<int>& array, int numChunks, SortStats* stats)
{
    const int arraySize = static_cast<int>(array.size());

    if (arraySize <= 1)
        return array;

    if (numChunks <= 0)
        numChunks = 1;

    unsigned int hw = std::thread::hardware_concurrency();
    if (hw > 0)
        numChunks = max(numChunks, static_cast<int>(hw));

    if (numChunks > arraySize)
        numChunks = arraySize;

    std::clock_t cpuStart = std::clock();
    auto wallStart = std::chrono::high_resolution_clock::now();

    vector<vector<int>> chunks(numChunks);
    splitIntoEqualChunks(array, chunks);

    vector<vector<int>> previewLocalSamples(numChunks);
    vector<int> previewAllSamples;

    vector<vector<int>> localSamples(numChunks);
    vector<int> allSamples;
    vector<int> splitters;

    vector<vector<int>> finalBuckets(numChunks);

    ThreadPool pool(numChunks);
    std::atomic<bool> taskFailed{ false };

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

    for (int i = 0; i < numChunks; i++)
        append(previewAllSamples, previewLocalSamples[i]);

    sort(previewAllSamples.begin(), previewAllSamples.end());
    int oversampleFactor = estimateOversampleFactor(previewAllSamples);
    int desiredSamplesPerChunk = oversampleFactor * (numChunks - 1);

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

    for (int i = 0; i < numChunks; i++)
        append(allSamples, localSamples[i]);

    sort(allSamples.begin(), allSamples.end());
    splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    const bool useAvx2 = cpuSupportsAvx2();

    vector<vector<int>> localCounts(numChunks, vector<int>(numChunks, 0));

    Latch countLatch(numChunks);
    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            try
            {
                for (int value : chunks[i])
                {
                    int bucketIndex = useAvx2 ? findBucketAvx2(value, splitters)
                        : findBucket(value, splitters);
                    localCounts[i][bucketIndex]++;
                }
            }
            catch (...)
            {
                taskFailed.store(true);
            }
            countLatch.countDown();
        });
    }
    countLatch.wait();

    vector<int> bucketSizes(numChunks, 0);
    vector<vector<int>> writeOffsets(numChunks, vector<int>(numChunks, 0));

    for (int b = 0; b < numChunks; b++)
    {
        int running = 0;
        for (int i = 0; i < numChunks; i++)
        {
            writeOffsets[i][b] = running;
            running += localCounts[i][b];
        }
        bucketSizes[b] = running;
        finalBuckets[b].resize(bucketSizes[b]);
    }

    Latch scatterLatch(numChunks);
    for (int i = 0; i < numChunks; i++)
    {
        pool.submitTask([&, i]()
        {
            try
            {
                vector<int> cursor = writeOffsets[i];
                for (int value : chunks[i])
                {
                    int bucketIndex = useAvx2 ? findBucketAvx2(value, splitters)
                        : findBucket(value, splitters);
                    finalBuckets[bucketIndex][cursor[bucketIndex]++] = value;
                }
            }
            catch (...)
            {
                taskFailed.store(true);
            }
            scatterLatch.countDown();
        });
    }
    scatterLatch.wait();

    Latch finalSortLatch(numChunks);
    for (int b = 0; b < numChunks; b++)
    {
        pool.submitTask([&, b]()
        {
            try
            {
                if (finalBuckets[b].size() >= 4096)
                    radixSortSigned32(finalBuckets[b]);
                else
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
        append(result, finalBuckets[b]);

    pool.shutdown();

    if (stats)
    {
        auto wallEnd = std::chrono::high_resolution_clock::now();
        std::clock_t cpuEnd = std::clock();
        stats->wallTimeSeconds = std::chrono::duration<double>(wallEnd - wallStart).count();
        stats->cpuTimeSeconds = static_cast<double>(cpuEnd - cpuStart) / CLOCKS_PER_SEC;
        stats->bucketStats.sizes = bucketSizes;

        stats->avx2Detected = useAvx2;
        stats->avx2Used = useAvx2;
    }

    if (taskFailed.load())
    {
        vector<int> fallback = array;
        sort(fallback.begin(), fallback.end());
        return fallback;
    }

    return result;
}
