#include <algorithm>
#include <thread>
#include <vector>
using std::vector;

vector<int> sampleSortVersionA(vector<int>& Array, int numChunks) {
	// Array: input Array
	// numChunks: number of chunks
	int ArraySize = Array.size();

	// Phase 0: Split input Array into Chunks

	vector<vector<int>> chunkArrays(numChunks);
	splitIntoEqualChunks(Array, chunkArrays);

	// localSamples[i] = samples chosen from chunk i
    vector<vector<int>> localSamples(numChunks);

	// localBuckets[i][b] = elements from chunk i that belong in bucket b
	vector<vector<vector<int>>> localBuckets(numChunks, vector<vector<int>>(numChunks));


	// counts[i][b] = number of elements in localBuckets[i][b]
	vector<vector<int>> counts(numChunks, vector<int>(numChunks, 0));

	// Phase 1: One fixed task per initial chunk

    vector<thread> tasks;

    for (int i = 0; i < numChunks; i++)
    {
        tasks.push_back(thread([&, i]()
            {
                sort(chunkArrays[i].begin(), chunkArrays[i].end());
                localSamples[i] = chooseRegularSamples(chunkArrays[i], numChunks - 1);
            }));
    }

    for (auto& t : tasks)
        t.join();

    // Phase 2: Gather samples and choose splitters
 
    vector<int> allSamples;

    for (int i = 0; i < numChunks; i++)
        append(allSamples, localSamples[i]);

    sort(allSamples.begin(), allSamples.end());

    vector<int> splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 3: One fixed task per partitioning step

    tasks.clear();

    for (int i = 0; i < numChunks; i++)
    {
        tasks.push_back(thread([&, i]()
            {
                for (int x : chunkArrays[i])
                {
                    int b = findBucket(x, splitters);   // returns bucket index in [0, p-1]
                    localBuckets[i][b].push_back(x);
                }

                for (int b = 0; b < numChunks; b++)
                    counts[i][b] = localBuckets[i][b].size();
            }));
    }

    for (auto& t : tasks)
        t.join();


    // Phase 4: Compute final bucket sizes and output positions

    vector<int> bucketSizes(numChunks, 0);

    for (int b = 0; b < numChunks; b++)
    {
        for (int i = 0; i < numChunks; i++)
            bucketSizes[b] += counts[i][b];
    }

    vector<int> bucketStart(numChunks, 0);
    bucketStart[0] = 0;

    for (int b = 1; b < numChunks; b++)
        bucketStart[b] = bucketStart[b - 1] + bucketSizes[b - 1];

    vector<int> output(ArraySize);

    // Phase 5: One fixed task per final bucket sort

    tasks.clear();

    for (int b = 0; b < numChunks; b++)
    {
        tasks.push_back(thread([&, b]()
            {
                vector<int> finalBucket;

                for (int i = 0; i < numChunks; i++)
                    append(finalBucket, localBuckets[i][b]);

                sort(finalBucket.begin(), finalBucket.end());

                int start = bucketStart[b];
                for (int k = 0; k < finalBucket.size(); k++)
                    output[start + k] = finalBucket[k];
            }));
    }

    for (auto& t : tasks)
        t.join();

    return output;
}