#pragma once

#include <string>
#include <vector>
#include "SortStats.h"

using std::vector;
using std::string;

// -------------------------------------------------------
// Data helpers
// -------------------------------------------------------
vector<int> generateRandomData(int size, int seed = -1);

// Correctness checks

// Basic: only checks ascending order
bool verifySorted(const vector<int>& data);

// Strong: checks order AND that the result is a permutation of the original
bool verifyCorrect(const vector<int>& original, const vector<int>& result);

// Sequential baseline
double testSequentialSort(const vector<int>& data, bool verify = false);
double runSequentialTrials(int dataSize, int numTrials);

// Version A  (fixed-task)
double testSampleSortVersionA(const vector<int>& data, int numChunks,
                               bool verify = false, SortStats* stats = nullptr);
double runSampleSortTrialsA(int dataSize, int numTrials, int numChunks,
                             SortStats* avgStats = nullptr);

// Version B  (thread-pool + work-stealing)
double testSampleSortVersionB(const vector<int>& data, int numChunks,
                               bool verify = false, SortStats* stats = nullptr);
double runSampleSortTrialsB(int dataSize, int numTrials, int numChunks,
                             SortStats* avgStats = nullptr);

// Version C  (adaptive oversampling)
double testSampleSortVersionC(const vector<int>& data, int numChunks,
                               bool verify = false, SortStats* stats = nullptr);
double runSampleSortTrialsC(int dataSize, int numTrials, int numChunks,
                             SortStats* avgStats = nullptr);

// Version D  (dynamic sampling + adaptive chunking)
double testSampleSortVersionD(const vector<int>& data, int numChunks,
                               bool verify = false, SortStats* stats = nullptr);
double runSampleSortTrialsD(int dataSize, int numTrials, int numChunks,
                             SortStats* avgStats = nullptr);

// Full benchmark
void runPerformanceTest(const string& outputFilename = "sample_sort_results.csv",
                        int numChunks = 4);