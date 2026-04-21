#pragma once

#include <string>
#include <vector>

using std::vector;
using std::string;

// Function declarations for performance testing
vector<int> generateRandomData(int size, int seed = -1);
bool verifySorted(const vector<int>& data);

// New declarations for Sample Sort Version A
double testSampleSortVersionA(const vector<int>& data, int numChunks, bool verify = false);
double runSampleSortTrials(int dataSize, int numTrials, int numChunks);
void runPerformanceTest(const string& outputFilename = "sample_sort_a_results.csv", int numChunks = 4);