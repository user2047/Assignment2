#include "pch.h"
#include "CppUnitTest.h"
#include "../sample_sort.h"
#include <algorithm>
#include <random>
#include <vector>
using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using std::mt19937;
using std::uniform_int_distribution;
using std::vector;

namespace SampleSortABPerfTest
{
    TEST_CLASS(SampleSortABPerfTest)
    {
    private:
        static vector<int> MakeData(int size, int seed = 42)
        {
            mt19937 gen(seed);
            uniform_int_distribution<int> dist(1, 1000000);
            vector<int> data(size);
            for (int i = 0; i < size; ++i) data[i] = dist(gen);
            return data;
        }

        static void AssertSortedEqual(const vector<int>& original, const vector<int>& result)
        {
            vector<int> expected = original;
            std::sort(expected.begin(), expected.end());
            Assert::AreEqual(expected.size(), result.size(), L"Size mismatch");
            for (size_t i = 0; i < expected.size(); ++i)
                Assert::AreEqual(expected[i], result[i], L"Sort mismatch");
        }

    public:
        TEST_METHOD(VersionA_Large)
        {
            const int numChunks = 8;
            auto data = MakeData(1 << 20);
            auto w = data;
            auto r = sampleSortVersionA(w, numChunks);
            AssertSortedEqual(data, r);
        }

        TEST_METHOD(VersionB_Large)
        {
            const int numChunks = 8;
            auto data = MakeData(1 << 20);
            auto w = data;
            auto r = sampleSortVersionB(w, numChunks);
            AssertSortedEqual(data, r);
        }
    };
}
