#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

struct BucketStats
{
    std::vector<int> sizes;

    bool empty() const { return sizes.empty(); }

    int minSize() const
    {
        if (sizes.empty()) return 0;
        return *std::min_element(sizes.begin(), sizes.end());
    }

    int maxSize() const
    {
        if (sizes.empty()) return 0;
        return *std::max_element(sizes.begin(), sizes.end());
    }

    double avgSize() const
    {
        if (sizes.empty()) return 0.0;
        double sum = 0.0;
        for (int s : sizes) sum += s;
        return sum / static_cast<double>(sizes.size());
    }

    // max bucket / average bucket  (1.0 = perfectly balanced)
    double imbalanceRatio() const
    {
        double avg = avgSize();
        return avg > 0.0 ? maxSize() / avg : 1.0;
    }

    double stdDev() const
    {
        if (sizes.size() < 2) return 0.0;
        double avg = avgSize();
        double sq = 0.0;
        for (int s : sizes) sq += (s - avg) * (s - avg);
        return std::sqrt(sq / static_cast<double>(sizes.size()));
    }
};

struct SortStats
{
    double wallTimeSeconds = 0.0;
    double cpuTimeSeconds  = 0.0;   // approximate CPU time via std::clock()
    BucketStats bucketStats;
};
