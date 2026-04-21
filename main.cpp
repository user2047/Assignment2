#include "sample_sort.h"
#include "PerformanceTest.h"
#include <iostream>
#include <vector>
using std::vector;
using std::cout;
using std::endl;

vector<int> sampleSortVersionA(vector<int>& Array, int numChunks);

int main()
{
    vector<int> data = {9, 3, 7, 1, 8, 2, 6, 5, 4, 0};
    int numChunks = 4;

    vector<int> sorted = sampleSortVersionA(data, numChunks);

    for (int x : sorted)
        cout << x << " ";
    cout << endl;

	runPerformanceTest("sample_sort_a_results.csv", numChunks);

    return 0;
}