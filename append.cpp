#include <vector>
using std::vector;

void append(vector<int>& dest, const vector<int>& src)
    {
        dest.insert(dest.end(), src.begin(), src.end());
    }
