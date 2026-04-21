#include "sample_sort.h"

#include <vector>
using std::vector;

vector<int> chooseRegularSamples(const vector<int>& chunk, int k)
{
    vector<int> samples;

    if (chunk.empty()) return samples;

    for (int j = 1; j <= k; j++)
    {
        int index = (j * static_cast<int>(chunk.size())) / (k + 1);
        samples.push_back(chunk[index]);
    }

    return samples;
}