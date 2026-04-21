#include "sample_sort.h"

#include <vector>
using std::vector;

int findBucket(int x, const vector<int>& splitters)
{
    for (int b = 0; b < static_cast<int>(splitters.size()); b++)
    {
        if (x <= splitters[b])
            return b;
    }

    return static_cast<int>(splitters.size()); // last bucket
}