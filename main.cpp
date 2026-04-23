#include "sample_sort.h"
#include "PerformanceTest.h"
#include <iostream>
#include <vector>
using std::vector;
using std::cout;
using std::endl;

int main()
{
    vector<int> data = {9, 3, 7, 1, 8, 2, 6, 5, 4, 0};
    int numChunks = 8;

    vector<int> sortedA = sampleSortVersionA(data, numChunks);
    vector<int> sortedB = sampleSortVersionB(data, numChunks);
    vector<int> sortedC = sampleSortVersionC(data, numChunks);
    vector<int> sortedD = sampleSortVersionD(data, numChunks);

    cout << "Version A: ";
    for (int x : sortedA)
        cout << x << " ";
    cout << endl;

    cout << "Version B: ";
    for (int x : sortedB)
        cout << x << " ";
    cout << endl;

    cout << "Version C: ";
    for (int x : sortedC)
        cout << x << " ";
    cout << endl;

    cout << "Version D: ";
    for (int x : sortedD)
        cout << x << " ";
    cout << endl;

    runPerformanceTest("sample_sort_results.csv", numChunks);

    return 0;
}