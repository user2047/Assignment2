#pragma once
#include <vector>
using std::vector;
std::vector<int> sampleSortVersionA(std::vector<int>& array, int numChunks);

void splitIntoEqualChunks(const std::vector<int>& input,
                          std::vector<std::vector<int>>& chunks);

std::vector<int> chooseRegularSamples(const std::vector<int>& chunk, int k);
std::vector<int> chooseGlobalSplitters(const std::vector<int>& allSamples, int k);
int findBucket(int x, const std::vector<int>& splitters);
void append(std::vector<int>& dest, const std::vector<int>& src);