#include "sample_sort.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <thread>
#include <vector>
using std::vector;
using std::thread;
using std::clock;
using std::chrono::high_resolution_clock;
using std::chrono::duration;

// function definition
vector<int> sampleSortVersionA(vector<int>& Array, int numChunks, SortStats* stats) {

    // Array: input Array
    // numChunks: number of chunks

	// Length of the input Array
    int ArraySize = Array.size();

    clock_t cpuStart = clock();
    auto wallStart = high_resolution_clock::now();

    // Phase 0: Split input Array into Chunks

    // Create a vector of length numChunks
    vector<vector<int>> chunkArrays(numChunks);

    // Split input Array into numChunks and put fragments into chunkArrays
    splitIntoEqualChunks(Array, chunkArrays);

    // localSamples[i] = samples chosen from chunk i
    vector<vector<int>> localSamples(numChunks);

    // localBuckets[i][b] = elements from chunk i that belong in bucket b
    vector<vector<vector<int>>> localBuckets(numChunks, vector<vector<int>>(numChunks));


    // counts[i][b] = number of elements in localBuckets[i][b]
    vector<vector<int>> counts(numChunks, vector<int>(numChunks, 0));

    // Phase 1: One fixed task per initial chunk

    // Create a vector of threads called tasks
    vector<thread> tasks;

    for (int i = 0; i < numChunks; i++)
    {
        // Appends a new thread into tasks
        tasks.push_back(thread([&, i]()
            {
                // Sorts the chunk
                sort(chunkArrays[i].begin(), chunkArrays[i].end());

                // Picks regular sample values from the sorted chunk and stores them in the matching slot
                localSamples[i] = chooseRegularSamples(chunkArrays[i], numChunks - 1);
            }));
    }

    // Iterates over every thread and calls join on each one, guaranteeing all results are completed before the next phase
    for (auto& t : tasks)
        t.join();

    // Phase 2: Gather samples and choose splitters

    // Create a vector if integers called allSamples
    vector<int> allSamples;

    //Iterate i from 0 to numChunks -1
    for (int i = 0; i < numChunks; i++) {

    // Merges each local sample into one vector of all samples
    append(allSamples, localSamples[i]);
    }

    // Sort the samples so global boundaries can be chosen in the right order
    sort(allSamples.begin(), allSamples.end());

    // choose global splitters from all samples
    vector<int> splitters = chooseGlobalSplitters(allSamples, numChunks - 1);

    // Phase 3: One fixed task per partitioning step

    // Removes all thread objects in tasks
    tasks.clear();

    // Iterate i from 0 to numChunks-1
    for (int i = 0; i < numChunks; i++)
    {
        // Starts a thread and add it to tasks
        tasks.push_back(thread([&, i]()
            {
                // Iterate over every value in that chunk
                for (int x : chunkArrays[i])
                {
                    // Determine which bucket x should go into
                    int b = findBucket(x, splitters);   // returns bucket index in [0, p-1]
                    
                    // Puts x into the correct bucket
                    localBuckets[i][b].push_back(x);
                }

                // Iterate b over the number of buckets  
                for (int b = 0; b < numChunks; b++)
                    
					// Count the number of elements in each bucket and store it in counts[i][b]
                    counts[i][b] = localBuckets[i][b].size();
            }));
    }

    // Iterate over each thread
    for (auto& t : tasks)

		// Calls join on each thread, guaranteeing partitioning is completed before the next phase
        t.join();


    // Phase 4: Compute final bucket sizes and output positions

	// Create a vector of integers called bucketSizes of length numChunks, initialized to 0
    vector<int> bucketSizes(numChunks, 0);

	// Iterate b from 0 to numChunks-1
    for (int b = 0; b < numChunks; b++)
    {
		// Iterate i from 0 to numChunks-1
        for (int i = 0; i < numChunks; i++)

            // Sum up the counts for bucket b across all chunks and store it in bucketSizes
            bucketSizes[b] += counts[i][b];
    }

	// Create a vector of integers called bucketStart of length numChunks, initialized to 0
    vector<int> bucketStart(numChunks, 0);

	// Iterate b from 1 to numChunks-1 (we start from 1 so we can do a prefix sum within the loop)
    for (int b = 1; b < numChunks; b++)

		// Determine the starting output index for each bucket by doing a prefix sum on bucketSizes and store it in bucketStart
        bucketStart[b] = bucketStart[b - 1] + bucketSizes[b - 1];

	// Create a vector of integers called output of length ArraySize
    vector<int> output(ArraySize);

    // Phase 5: One fixed task per final bucket sort

	// Remove all thread objects in tasks
    tasks.clear();

	// Iterate b from 0 to numChunks-1
    for (int b = 0; b < numChunks; b++)
    {
		// Start a thread and add it to tasks for each bucket b
        tasks.push_back(thread([&, b]()
            {
				// Create a vector of integers called finalBucket
                vector<int> finalBucket;

                // Iterate i from 0 to numChunks-1
                for (int i = 0; i < numChunks; i++)

					// Merge all the pieces of bucket b from each chunk into finalBucket
                    append(finalBucket, localBuckets[i][b]);

				// Sort finalBucket
                sort(finalBucket.begin(), finalBucket.end());

				// Copy finalBucket into the correct position in output using bucketStart to find the right index
                int start = bucketStart[b];

				// Iterate k from 0 to the size of finalBucket
                for (int k = 0; k < finalBucket.size(); k++)

                    // Copy each element from finalBucket to the correct position in output
                    output[start + k] = finalBucket[k];
            }));
    }

	// Iterate over each thread
    for (auto& t : tasks)
		// Calls join on each thread, guaranteeing all sorting and copying is completed before the function returns
        t.join();

	// If stats is not null, fill in the timing and bucket size information
    if (stats)
    {
		// Get the end time for both wall clock and CPU time
        auto wallEnd = high_resolution_clock::now();
		// Note: std::clock() gives CPU time used by the current process, which is an approximation of actual CPU time spent on the task
        clock_t cpuEnd = clock();

		// Calculate the elapsed time for both wall clock and CPU time and store it in stats
        stats->wallTimeSeconds = duration<double>(wallEnd - wallStart).count();

		// CPU time is given in clock ticks, so we divide by CLOCKS_PER_SEC to convert to seconds
        stats->cpuTimeSeconds  = static_cast<double>(cpuEnd - cpuStart) / CLOCKS_PER_SEC;
        
		// Store the bucket size information in stats
        stats->bucketStats.sizes.resize(numChunks);

		// Iterate b from 0 to numChunks-1
        for (int b = 0; b < numChunks; b++)

			// Store the size of each bucket in stats
            stats->bucketStats.sizes[b] = bucketSizes[b];
    }

	// Return the sorted output Array
    return output;
}