#include "sample_sort.h"
#include "PerformanceTest.h"

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
using std::ofstream;
using std::fixed;
using std::setprecision;
using std::endl;
using std::cout;
using std::scientific;


vector<int> generateRandomData(int size, int seed)
{
    vector<int> data(size);

    static thread_local mt19937 defaultGen(std::random_device{}());
    mt19937 seededGen;

    mt19937* genPtr = &defaultGen;
    if (seed != -1)
    {
        seededGen = mt19937(seed);
        genPtr = &seededGen;
    }

    uniform_int_distribution<int> dist(1, 1000000);

    for (int i = 0; i < size; i++)
    {
        data[i] = dist(*genPtr);
    }

    return data;
}

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

double runSampleSortTrialsA(int dataSize, int numTrials, int numChunks)
{
    double totalTime = 0.0;

    for (int trial = 0; trial < numTrials; trial++)
    {
        vector<int> data = generateRandomData(dataSize);
        double time = testSampleSortVersionA(data, numChunks, trial == 0);
        totalTime += time;
    }

    return (numTrials > 0) ? (totalTime / numTrials) : -1.0;
}

double testSampleSortVersionB(const vector<int>& data, int numChunks, bool verify)
{
    vector<int> workingData = data; // copy so original input stays unchanged

    auto start = high_resolution_clock::now();
    vector<int> result = sampleSortVersionB(workingData, numChunks);
    auto end = high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    if (verify)
    {
        if (!verifySorted(result))
        {
            std::cerr << "ERROR: Sample Sort Version B verification failed!" << std::endl;
        }
    }

    return elapsed;
}

double runSampleSortTrialsB(int dataSize, int numTrials, int numChunks)
{
    double totalTime = 0.0;

    for (int trial = 0; trial < numTrials; trial++)
    {
        vector<int> data = generateRandomData(dataSize);
        double time = testSampleSortVersionB(data, numChunks, trial == 0);
        totalTime += time;
    }

    return (numTrials > 0) ? (totalTime / numTrials) : -1.0;
}

double testSampleSortVersionC(const vector<int>& data, int numChunks, bool verify)
{
    vector<int> workingData = data; // copy so original input stays unchanged

    auto start = high_resolution_clock::now();
    vector<int> result = sampleSortVersionC(workingData, numChunks);
    auto end = high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();
    if (verify)
    {
        if (!verifySorted(result))
        {
            std::cerr << "ERROR: Sample Sort Version C verification failed!" << std::endl;
        }
    }

    return elapsed;
}

double runSampleSortTrialsC(int dataSize, int numTrials, int numChunks)
{
    double totalTime = 0.0;

    for (int trial = 0; trial < numTrials; trial++)
    {
        vector<int> data = generateRandomData(dataSize);
        double time = testSampleSortVersionC(data, numChunks, trial == 0);
        totalTime += time;
    }

    return (numTrials > 0) ? (totalTime / numTrials) : -1.0;
}

void runPerformanceTest(const string& outputFilename, int numChunks)
{
    vector<int> testSizes = {
        256, 512, 1024, 2048, 4096, 8192,
        16384, 32768, 65536, 131072,
        262144, 524288, 1048576
    };

    auto getNumTrials = [](int size) {
        if (size <= 8192) return 10;
        if (size <= 131072) return 5;
        return 3;
    };

    string actualOutputFilename = outputFilename;
    ofstream csvFile(actualOutputFilename, std::ios::out | std::ios::trunc);

    if (!csvFile.is_open())
    {
        size_t dot = outputFilename.find_last_of('.');
        string base = (dot == string::npos) ? outputFilename : outputFilename.substr(0, dot);
        string ext = (dot == string::npos) ? "" : outputFilename.substr(dot);

        for (int attempt = 1; attempt <= 5 && !csvFile.is_open(); attempt++)
        {
            actualOutputFilename = base + "_" + std::to_string(attempt) + ext;
            csvFile.open(actualOutputFilename, std::ios::out | std::ios::trunc);
        }

        if (!csvFile.is_open())
        {
            std::cerr << "ERROR: Could not open output CSV file. It may be locked by another program." << std::endl;
            return;
        }

        std::cout << "Warning: Primary CSV file was unavailable. Writing to: "
                  << actualOutputFilename << std::endl;
    }

    csvFile << "Size,Algorithm,AverageTime(s),NumTrials,NumChunks" << endl;

    cout << "===========================================================" << endl;
    cout << "         SAMPLE SORT PERFORMANCE TEST (A, B and C)" << endl;
    cout << "===========================================================" << endl;
    cout << "Output file: " << actualOutputFilename << endl;
    cout << "Chunks: " << numChunks << endl;
    cout << "===========================================================" << endl << endl;

    for (int size : testSizes)
    {
        int numTrials = getNumTrials(size);

        cout << "Testing size n = " << size
            << " (" << numTrials << " trials)..." << endl;

        double avgA = runSampleSortTrialsA(size, numTrials, numChunks);
        cout << "  SampleSortVersionA : "
            << fixed << setprecision(6) << avgA << " seconds" << endl;
        csvFile << size << ",SampleSortVersionA,"
                << scientific << setprecision(10) << avgA << ","
                << numTrials << "," << numChunks << endl;

        double avgB = runSampleSortTrialsB(size, numTrials, numChunks);
        cout << "  SampleSortVersionB : "
            << fixed << setprecision(6) << avgB << " seconds" << endl;
        csvFile << size << ",SampleSortVersionB,"
                << scientific << setprecision(10) << avgB << ","
                << numTrials << "," << numChunks << endl;

        double avgC = runSampleSortTrialsC(size, numTrials, numChunks);
        cout << "  SampleSortVersionC : "
            << fixed << setprecision(6) << avgC << " seconds" << endl;
        csvFile << size << ",SampleSortVersionC,"
                << scientific << setprecision(10) << avgC << ","
                << numTrials << "," << numChunks << endl;

        cout << endl;
    }

    csvFile.close();

    cout << "===========================================================" << endl;
    cout << "Performance test complete!" << endl;
    cout << "Results saved to: " << actualOutputFilename << endl;
    cout << "===========================================================" << endl;
}