#include "sample_sort.h"
#include "PerformanceTest.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
using std::vector;
using std::string;
using std::random_device;
using std::mt19937;
using std::uniform_int_distribution;
using std::chrono::high_resolution_clock;
using std::chrono::duration;
using std::ofstream;
using std::fixed;
using std::setprecision;
using std::cerr;
using std::endl;
using std::cout;
using std::to_string;
using std::min;
using std::max;
using std::scientific;
 

// Helper function definition to generate random, unsorted data
vector<int> generateRandomData(int size, int seed)
{
    vector<int> data(size);

    random_device rd;
    mt19937 gen(seed == -1 ? rd() : seed);
    uniform_int_distribution<int> dist(1, 1000000);

    for (int i = 0; i < size; i++)
    {
        data[i] = dist(gen);
    }

    return data;
}

// Helper function definition to verify that a vector is sorted in non-decreasing order
bool verifySorted(const vector<int>& data)
{
    for (size_t i = 1; i < data.size(); i++)
    {
        if (data[i - 1] > data[i])
            return false;
    }
    return true;
}

double testSampleSortVersionA(const vector<int>& data, int numChunks, bool verify)
{
    vector<int> workingData = data; // copy so original input stays unchanged

    auto start = high_resolution_clock::now();
    vector<int> result = sampleSortVersionA(workingData, numChunks);
    auto end = high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    if (verify)
    {
        if (!verifySorted(result))
        {
            std::cerr << "ERROR: Sample Sort Version A verification failed!" << std::endl;
        }
    }

    return elapsed;
}

double runSampleSortTrials(int dataSize, int numTrials, int numChunks)
{
    double totalTime = 0.0;
    int successfulTrials = 0;

    for (int trial = 0; trial < numTrials; trial++)
    {
        vector<int> data = generateRandomData(dataSize);
        double time = testSampleSortVersionA(data, numChunks, trial == 0);

        if (time >= 0.0)
        {
            totalTime += time;
            successfulTrials++;
        }
    }

    return (successfulTrials > 0) ? (totalTime / successfulTrials) : -1.0;
}

void runPerformanceTest(const string& outputFilename, int numChunks)
{
    vector<int> testSizes = {
        256,
        512,
        1024,
        2048,
        4096,
        8192,
        16384,
        32768,
        65536,
        131072,
        262144,
        524288,
        1048576,
        2097152,
        4194304,
        8388608,
        16777216
    };

    auto getNumTrials = [](int size) {
        if (size <= 8192) return 10;
        if (size <= 131072) return 5;
        if (size <= 1048576) return 3;
        return 1;
        };

    ofstream csvFile(outputFilename);
    csvFile << "Size,Algorithm,AverageTime(s),NumTrials,NumChunks" <<  endl;

    cout << "===========================================================" << endl;
    cout << "         SAMPLE SORT VERSION A PERFORMANCE TEST" << endl;
    cout << "===========================================================" << endl;
    cout << "Output file: " << outputFilename << endl;
    cout << "Chunks: " << numChunks << endl;
    cout << "Testing sizes from 2^8 (" << testSizes.front()
         << ") to 2^24 (" << testSizes.back() << ")" << endl;
    cout << "===========================================================" << endl;
    cout << endl;

    for (int size : testSizes)
    {
        int numTrials = getNumTrials(size);

        cout << "Testing size n = " << size
            << " (" << numTrials << " trials)..." << endl;
        double avgTime = runSampleSortTrials(size, numTrials, numChunks);

        if (avgTime >= 0.0)
        {
            cout << "  SampleSortVersionA : "
                << fixed << setprecision(6)
                << avgTime << " seconds" << endl;

            csvFile << size << ","
                << "SampleSortVersionA" << ","
                << scientific << setprecision(10) << avgTime << ","
                << numTrials << ","
                << numChunks << endl;
        }
        else
        {
            cout << "  SampleSortVersionA : SKIPPED" << endl;
        }

        cout << endl;
    }

    csvFile.close();

    cout << "===========================================================" << endl;
    cout << "Performance test complete!" << endl;
    cout << "Results saved to: " << outputFilename << endl;
    cout << "===========================================================" << endl;
}