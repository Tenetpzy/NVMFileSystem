#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_set>
#include <vector>

extern "C" {
#include "file_struct.h"
}

class file_struct_Test : public ::testing::Test {
   protected:
    void SetUp() override { pfs = file_struct_new(); }
    void TearDown() override { file_struct_free(pfs); }
    struct file_struct *pfs;
};

TEST_F(file_struct_Test, BasicAllocFreeFD) {
    EXPECT_EQ(0, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(1, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(2, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(3, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(4, file_struct_alloc_fd_slot(pfs));
    EXPECT_EQ(5, file_struct_alloc_fd_slot(pfs));
    file_struct_free_fd_slot(pfs, 3);
    EXPECT_EQ(3, file_struct_alloc_fd_slot(pfs));
    file_struct_free_fd_slot(pfs, 0);
    file_struct_free_fd_slot(pfs, 1);
    file_struct_free_fd_slot(pfs, 2);
    file_struct_free_fd_slot(pfs, 3);
    file_struct_free_fd_slot(pfs, 4);
    file_struct_free_fd_slot(pfs, 5);
}

TEST_F(file_struct_Test, ManyAllocFreeFD) {
    for (int i = 0; i < 10000; i++) {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 2500; i < 5000; i++) {
        file_struct_free_fd_slot(pfs, i);
    }
    for (int i = 1000; i < 1500; i++) {
        file_struct_free_fd_slot(pfs, i);
    }
    for (int i = 6000; i < 7000; i++) {
        file_struct_free_fd_slot(pfs, i);
    }
    for (int i = 1000; i < 1500; i++) {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 2500; i < 5000; i++) {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 6000; i < 7000; i++) {
        EXPECT_EQ(i, file_struct_alloc_fd_slot(pfs));
    }
    for (int i = 0; i < 10000; i++) {
        EXPECT_NE(nullptr, file_struct_access_fd_slot(pfs, i));
    }
}

TEST_F(file_struct_Test, ConcurrentTest1) {
    using namespace std;
    vector<thread> thread_pool_;
    vector<int> fds;
    mutex fds_mutex;
    auto test1 = [&](int l, int r) {
        vector<int> lfds;
        for (int i = l; i < r; i++) {
            lfds.push_back(file_struct_alloc_fd_slot(pfs));
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

TEST_F(file_struct_Test, ConcurrentTest2) {
    const int Test2_Times = 233;
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
                lfds.push_back(file_struct_alloc_fd_slot(pfs));
            }
            for (int i = 0; i < n; i += 2) {
                int idx = distr(generator) % lfds.size();
                int num = lfds[idx];
                swap(lfds[idx], lfds.back());
                lfds.pop_back();
                file_struct_free_fd_slot(pfs, num);
            }
            lock_guard<mutex> g(fds_mutex);
            for (auto x : lfds) {
                fds.push_back(x);
            }
        };
        thread_pool_.emplace_back(test2, 10000, rand());
        thread_pool_.emplace_back(test2, 10000, rand());
        thread_pool_.emplace_back(test2, 10000, rand());
        thread_pool_.emplace_back(test2, 10000, rand());
        for (auto &t : thread_pool_) {
            t.join();
        }
        sort(fds.begin(), fds.end());
        EXPECT_EQ(unique(fds.begin(), fds.end()), fds.end());
        // for (auto fd : fds) {
        //     EXPECT_TRUE(!file_struct_is_fd_slot_empty(pfs, fd));
        // }
        unordered_set<int> avai_fds(fds.begin(), fds.end());
        for (int i = 0; i < MAXFDSIZE; i++) {
            EXPECT_EQ(!file_struct_is_fd_slot_empty(pfs, i), avai_fds.count(i));
        }
        thread_pool_.clear();
        fds.clear();
        file_struct_consistency_validation(pfs);
        file_struct_free(pfs);
        pfs = file_struct_new();
    }
}
