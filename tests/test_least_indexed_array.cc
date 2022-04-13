#include <algorithm>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>
#include "gtest/gtest.h"

extern "C" {
#define LIA_DEBUG
#include "least_indexed_array.h"
}

class least_indexed_array_Test : public ::testing::Test {
   protected:
    void SetUp() override { array = least_indexed_array_new(5000, 5000); }
    void TearDown() override { least_indexed_array_delete(array); }
    struct least_indexed_array *array;
};

TEST_F(least_indexed_array_Test, BasicTest) {
    std::unordered_set<int> handles;
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(i, least_indexed_array_alloc(array));
        handles.insert(i);
    }
    auto it = handles.begin();
    for (int i = 0; i < 49; i++) {
        int v = *it;
        it = handles.erase(it);
        least_indexed_array_free(array, v, LIAOP_CHECK);
    }
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(handles.count(i), !least_indexed_array_is_free(array, i, LIAOP_CHECK));
    }
}

TEST_F(least_indexed_array_Test, ConcurrentTest1) {
    using namespace std;
    vector<thread> thread_pool_;
    vector<int> fds;
    mutex fds_mutex;
    auto test1 = [&](int l, int r) {
        vector<int> lfds;
        for (int i = l; i < r; i++) {
            lfds.push_back(least_indexed_array_alloc(array));
        }
        // for (int i = 0; i < lfds.size(); i++)
        // {
        //     file_struct_free_fd_slot(pfs, lfds[i]);
        // }
        lock_guard<mutex> g(fds_mutex);
        for (auto x : lfds) fds.push_back(x);
    };
    thread_pool_.emplace_back(test1, 0, 5000);
    thread_pool_.emplace_back(test1, 5000, 10000);
    thread_pool_.emplace_back(test1, 10000, 15000);
    thread_pool_.emplace_back(test1, 15000, 20000);
    for (auto &t : thread_pool_) {
        if (t.joinable()) {
            t.join();
        }
    }
    sort(fds.begin(), fds.end());
    EXPECT_EQ(20000, fds.size());
    for (int i = 0; i < fds.size(); i++) {
        EXPECT_EQ(i, fds[i]);
    }
}

TEST_F(least_indexed_array_Test, ConcurrentTest2) {
    const int Test2_Times = 3;
    using namespace std;
    vector<thread> thread_pool_;
    vector<int> fds;
    mutex fds_mutex;
    srand(time(NULL));
    int T = Test2_Times;
    while (T--) {
        auto test2 = [&](int n, int seed) {
            default_random_engine generator(seed);
            uniform_int_distribution<int> distr(0, n);
            vector<int> lfds;
            for (int i = 0; i < n; i++) {
                lfds.push_back(least_indexed_array_alloc(array));
            }
            for (int i = 0; i < n; i += 2) {
                int idx = distr(generator) % lfds.size();
                int num = lfds[idx];
                swap(lfds[idx], lfds.back());
                lfds.pop_back();
                ASSERT_TRUE(least_indexed_array_free(array, num, LIAOP_CHECK));
            }
            lock_guard<mutex> g(fds_mutex);
            for (auto x : lfds) {
                fds.push_back(x);
            }
        };
        thread_pool_.emplace_back(test2, 400000, rand());
        thread_pool_.emplace_back(test2, 400000, rand());
        thread_pool_.emplace_back(test2, 400000, rand());
        thread_pool_.emplace_back(test2, 400000, rand());
        for (auto &t : thread_pool_) {
            t.join();
        }
        sort(fds.begin(), fds.end());
        ASSERT_EQ(unique(fds.begin(), fds.end()), fds.end());
        // for (auto fd : fds) {
        //     EXPECT_TRUE(!file_struct_is_fd_slot_empty(pfs, fd));
        // }
        unordered_set<int> avai_fds(fds.begin(), fds.end());
        for (int i = 0; i < 400000 * 4; i++) {
            bool eq = (!least_indexed_array_is_free(array, i, LIAOP_CHECK)) == avai_fds.count(i);
            ASSERT_EQ(!least_indexed_array_is_free(array, i, LIAOP_CHECK), avai_fds.count(i))
                << "T: " << T << ", i: " << i;
        }
        thread_pool_.clear();
        fds.clear();
        least_indexed_array_consistency_check(array);
        least_indexed_array_delete(array);
        array = least_indexed_array_new(5000, 5000);
    }
}
