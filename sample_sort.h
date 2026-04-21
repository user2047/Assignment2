#pragma once
#include <vector>
#include "SortStats.h"
using std::vector;

vector<int> sampleSortVersionA(vector<int>& array, int numChunks, SortStats* stats = nullptr);
vector<int> sampleSortVersionB(vector<int>& array, int numChunks, SortStats* stats = nullptr);
vector<int> sampleSortVersionC(vector<int>& array, int numChunks, SortStats* stats = nullptr);

void splitIntoEqualChunks(const vector<int>& input,vector<vector<int>>& chunks);

vector<int> chooseRegularSamples(const vector<int>& chunk, int k);
vector<int> chooseGlobalSplitters(const vector<int>& allSamples, int k);
int findBucket(int x, const vector<int>& splitters);
void append(vector<int>& dest, const vector<int>& src);