#include "sample_sort.h"
#include <vector>
using std::vector;

void splitIntoEqualChunks(const vector<int>& input, vector<vector<int>>& chunks)
{
    int n = static_cast<int>(input.size());
    int p = static_cast<int>(chunks.size());

    int baseSize = n / p;
    int extra = n % p;

    int start = 0;
    for (int i = 0; i < p; i++)
    {
        int currentSize = baseSize + (i < extra ? 1 : 0);
        chunks[i].assign(input.begin() + start, input.begin() + start + currentSize);
        start += currentSize;
    }
}