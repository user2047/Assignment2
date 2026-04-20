#include <vector>
using std::vector;

vector<int> chooseGlobalSplitters(const vector<int>& allSamples, int k) {
    vector<int> splitters;

    if (allSamples.empty())
        return splitters;

    for (int j = 1; j <= k; j++)
    {
        int index = (j * static_cast<int>(allSamples.size())) / (k + 1);

        if (index >= static_cast<int>(allSamples.size()))
            index = static_cast<int>(allSamples.size()) - 1;

        splitters.push_back(allSamples[index]);
    }

    return splitters;
}