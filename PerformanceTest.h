#pragma once

#include <string>
#include <vector>

using std::vector;
using std::string;

vector<int> generateRandomData(int size, int seed = -1);
bool verifySorted(const vector<int>& data);

double testSampleSortVersionA(const vector<int>& data, int numChunks, bool verify = false);
double runSampleSortTrialsA(int dataSize, int numTrials, int numChunks);

double testSampleSortVersionB(const vector<int>& data, int numChunks, bool verify = false);
double runSampleSortTrialsB(int dataSize, int numTrials, int numChunks);

double testSampleSortVersionC(const vector<int>& data, int numChunks, bool verify = false);
double runSampleSortTrialsC(int dataSize, int numTrials, int numChunks);

void runPerformanceTest(const string& outputFilename = "sample_sort_results.csv", int numChunks = 4);