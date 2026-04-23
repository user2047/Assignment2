#include "sample_sort.h"
#include "PerformanceTest.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

using std::vector;
using std::string;
using std::random_device;
using std::mt19937;
using std::uniform_int_distribution;
using std::chrono::high_resolution_clock;
using std::ofstream;
using std::fixed;
using std::setprecision;
using std::setw;
using std::left;
using std::endl;
using std::cout;
using std::scientific;

vector<int> generateRandomData(int size, int seed)
{
    vector<int> data(size);
    static thread_local mt19937 defaultGen(random_device{}());
    mt19937 seededGen;
    mt19937* genPtr = &defaultGen;
    if (seed != -1) { seededGen = mt19937(seed); genPtr = &seededGen; }
    uniform_int_distribution<int> dist(1, 1000000);
    for (int i = 0; i < size; i++) data[i] = dist(*genPtr);
    return data;
}

bool verifySorted(const vector<int>& data)
{
    for (size_t i = 1; i < data.size(); i++)
        if (data[i-1] > data[i]) return false;
    return true;
}

bool verifyCorrect(const vector<int>& original, const vector<int>& result)
{
    if (original.size() != result.size()) {
        std::cerr << "  [FAIL] Size mismatch: input=" << original.size() << " output=" << result.size() << endl;
        return false;
    }
    if (!verifySorted(result)) { std::cerr << "  [FAIL] Output not sorted." << endl; return false; }
    vector<int> expected = original;
    std::sort(expected.begin(), expected.end());
    for (size_t i = 0; i < expected.size(); i++) {
        if (expected[i] != result[i]) {
            std::cerr << "  [FAIL] Mismatch at " << i << ": expected " << expected[i] << " got " << result[i] << endl;
            return false;
        }
    }
    return true;
}

double testSequentialSort(const vector<int>& data, bool verify)
{
    vector<int> w = data;
    auto t0 = high_resolution_clock::now();
    std::sort(w.begin(), w.end());
    auto t1 = high_resolution_clock::now();
    if (verify && !verifyCorrect(data, w)) std::cerr << "ERROR: Sequential failed!" << endl;
    return std::chrono::duration<double>(t1 - t0).count();
}

double runSequentialTrials(int dataSize, int numTrials)
{
    double total = 0.0;
    for (int t = 0; t < numTrials; t++) { auto d = generateRandomData(dataSize); total += testSequentialSort(d, t==0); }
    return numTrials > 0 ? total / numTrials : -1.0;
}

static void accumulateStats(SortStats& acc, const SortStats& s, int n)
{
    acc.wallTimeSeconds += s.wallTimeSeconds / n;
    acc.cpuTimeSeconds  += s.cpuTimeSeconds  / n;
    if (!s.bucketStats.sizes.empty()) {
        if (acc.bucketStats.sizes.empty()) acc.bucketStats.sizes.resize(s.bucketStats.sizes.size(), 0);
        for (size_t b = 0; b < s.bucketStats.sizes.size(); b++)
            acc.bucketStats.sizes[b] += s.bucketStats.sizes[b] / n;
    }

    // Keep capability flags if observed in any trial
    acc.avx2Detected = acc.avx2Detected || s.avx2Detected;
    acc.avx2Used = acc.avx2Used || s.avx2Used;
}

double testSampleSortVersionA(const vector<int>& data, int numChunks, bool verify, SortStats* stats)
{
    vector<int> w = data; SortStats local;
    vector<int> r = sampleSortVersionA(w, numChunks, stats ? stats : &local);
    if (verify && !verifyCorrect(data, r)) std::cerr << "ERROR: Version A failed!" << endl;
    return stats ? stats->wallTimeSeconds : local.wallTimeSeconds;
}

double runSampleSortTrialsA(int dataSize, int numTrials, int numChunks, SortStats* avgStats)
{
    double total = 0.0; SortStats acc{};
    for (int t = 0; t < numTrials; t++) {
        auto data = generateRandomData(dataSize); SortStats s{}; auto w = data;
        auto r = sampleSortVersionA(w, numChunks, &s);
        if (t == 0 && !verifyCorrect(data, r)) std::cerr << "ERROR: Version A correctness failed!" << endl;
        total += s.wallTimeSeconds;
        if (avgStats) accumulateStats(acc, s, numTrials);
    }
    if (avgStats) *avgStats = acc;
    return numTrials > 0 ? total / numTrials : -1.0;
}

double testSampleSortVersionB(const vector<int>& data, int numChunks, bool verify, SortStats* stats)
{
    vector<int> w = data; SortStats local;
    vector<int> r = sampleSortVersionB(w, numChunks, stats ? stats : &local);
    if (verify && !verifyCorrect(data, r)) std::cerr << "ERROR: Version B failed!" << endl;
    return stats ? stats->wallTimeSeconds : local.wallTimeSeconds;
}

double runSampleSortTrialsB(int dataSize, int numTrials, int numChunks, SortStats* avgStats)
{
    double total = 0.0; SortStats acc{};
    for (int t = 0; t < numTrials; t++) {
        auto data = generateRandomData(dataSize); SortStats s{}; auto w = data;
        auto r = sampleSortVersionB(w, numChunks, &s);
        if (t == 0 && !verifyCorrect(data, r)) std::cerr << "ERROR: Version B correctness failed!" << endl;
        total += s.wallTimeSeconds;
        if (avgStats) accumulateStats(acc, s, numTrials);
    }
    if (avgStats) *avgStats = acc;
    return numTrials > 0 ? total / numTrials : -1.0;
}

double testSampleSortVersionC(const vector<int>& data, int numChunks, bool verify, SortStats* stats)
{
    vector<int> w = data; SortStats local;
    vector<int> r = sampleSortVersionC(w, numChunks, stats ? stats : &local);
    if (verify && !verifyCorrect(data, r)) std::cerr << "ERROR: Version C failed!" << endl;
    return stats ? stats->wallTimeSeconds : local.wallTimeSeconds;
}

double runSampleSortTrialsC(int dataSize, int numTrials, int numChunks, SortStats* avgStats)
{
    double total = 0.0; SortStats acc{};
    for (int t = 0; t < numTrials; t++) {
        auto data = generateRandomData(dataSize); SortStats s{}; auto w = data;
        auto r = sampleSortVersionC(w, numChunks, &s);
        if (t == 0 && !verifyCorrect(data, r)) std::cerr << "ERROR: Version C correctness failed!" << endl;
        total += s.wallTimeSeconds;
        if (avgStats) accumulateStats(acc, s, numTrials);
    }
    if (avgStats) *avgStats = acc;
    return numTrials > 0 ? total / numTrials : -1.0;
}

double testSampleSortVersionD(const vector<int>& data, int numChunks, bool verify, SortStats* stats)
{
    vector<int> w = data; SortStats local;
    vector<int> r = sampleSortVersionD(w, numChunks, stats ? stats : &local);
    if (verify && !verifyCorrect(data, r)) std::cerr << "ERROR: Version D failed!" << endl;
    return stats ? stats->wallTimeSeconds : local.wallTimeSeconds;
}

double runSampleSortTrialsD(int dataSize, int numTrials, int numChunks, SortStats* avgStats)
{
    double total = 0.0; SortStats acc{};
    for (int t = 0; t < numTrials; t++) {
        auto data = generateRandomData(dataSize); SortStats s{}; auto w = data;
        auto r = sampleSortVersionD(w, numChunks, &s);
        if (t == 0 && !verifyCorrect(data, r)) std::cerr << "ERROR: Version D correctness failed!" << endl;
        total += s.wallTimeSeconds;
        if (avgStats) accumulateStats(acc, s, numTrials);
    }
    if (avgStats) *avgStats = acc;
    return numTrials > 0 ? total / numTrials : -1.0;
}

void runPerformanceTest(const string& outputFilename, int numChunks)
{
	vector<int> testSizes = { 256,512,1024,2048,4096,8192,16384,32768,65536,131072,262144,524288,1048576,2097152,4194304,8388608};
    auto getTrials = [](int sz) -> int {
        if (sz <= 8192) return 3;
        if (sz <= 131072) return 2;
        if (sz <= 8388608) return 1;
        return 1;
    };

    string fname = outputFilename;
    ofstream csv(fname, std::ios::out | std::ios::trunc);
    if (!csv.is_open()) {
        size_t d = outputFilename.find_last_of('.'); string base = d==string::npos?outputFilename:outputFilename.substr(0,d); string ext = d==string::npos?"":outputFilename.substr(d);
        for (int i=1;i<=5&&!csv.is_open();i++) { fname=base+"_"+std::to_string(i)+ext; csv.open(fname,std::ios::out|std::ios::trunc); }
        if (!csv.is_open()) { std::cerr << "ERROR: Cannot open CSV." << endl; return; }
        cout << "Warning: writing to " << fname << endl;
    }

    csv << "Size,Algorithm,AvgWallTime(s),AvgCpuTime(s),CPUUtil(%),BucketImbalance,BucketStdDev,AVX2Detected,AVX2Used,NumTrials,NumChunks" << endl;

    cout << "============================================================" << endl;
    cout << "     SAMPLE SORT PERFORMANCE TEST  (Seq | A | B | C | D)"       << endl;
    cout << "============================================================" << endl;
    cout << "Output: " << fname << "  |  Chunks: " << numChunks           << endl;
    cout << "============================================================" << endl << endl;

    for (int size : testSizes) {
        int nt = getTrials(size);
        cout << "n = " << size << "  (" << nt << " trials)" << endl;

        double seqAvg = runSequentialTrials(size, nt);
        cout << "  " << setw(22) << left << "Sequential" << fixed << setprecision(6) << seqAvg << "s" << endl;
        csv << size << ",Sequential," << scientific << setprecision(10) << seqAvg << "," << seqAvg << ",100.00,1.0000,0.00,false,false," << nt << "," << numChunks << endl;

        const char* names[4] = { "SampleSortVersionA","SampleSortVersionB","SampleSortVersionC","SampleSortVersionD" };
        SortStats   vs[4]    = {};
        double      va[4]    = {};

        va[0] = runSampleSortTrialsA(size, nt, numChunks, &vs[0]);
        va[1] = runSampleSortTrialsB(size, nt, numChunks, &vs[1]);
        va[2] = runSampleSortTrialsC(size, nt, numChunks, &vs[2]);
        va[3] = runSampleSortTrialsD(size, nt, numChunks, &vs[3]);

        for (int v = 0; v < 4; v++) {
            double cu = vs[v].wallTimeSeconds>0.0 ? (vs[v].cpuTimeSeconds/vs[v].wallTimeSeconds)*100.0 : 0.0;
            cout << "  " << setw(22) << left << names[v] << fixed << setprecision(6) << va[v] << "s"
                 << "  CPU:" << fixed << setprecision(1) << cu << "%";
            if (!vs[v].bucketStats.empty())
                cout << "  bkts(min/avg/max):" << vs[v].bucketStats.minSize() << "/" << fixed << setprecision(0) << vs[v].bucketStats.avgSize() << "/" << vs[v].bucketStats.maxSize()
                     << "  imbal:" << fixed << setprecision(3) << vs[v].bucketStats.imbalanceRatio() << "x";

            if (v == 3)
                cout << "  AVX2:" << (vs[v].avx2Detected ? "detected" : "not-detected")
                     << "/" << (vs[v].avx2Used ? "used" : "not-used");

            cout << endl;
            csv << size << "," << names[v] << "," << scientific << setprecision(10) << vs[v].wallTimeSeconds << "," << vs[v].cpuTimeSeconds << "," << fixed << setprecision(2) << cu << "," << fixed << setprecision(4) << vs[v].bucketStats.imbalanceRatio() << "," << fixed << setprecision(2) << vs[v].bucketStats.stdDev() << "," << (vs[v].avx2Detected ? "true" : "false") << "," << (vs[v].avx2Used ? "true" : "false") << "," << nt << "," << numChunks << endl;
        }
        cout << endl;
    }

    csv.close();
    cout << "============================================================" << endl;
    cout << "Complete. Results: " << fname << endl;
    cout << "============================================================" << endl;
}
