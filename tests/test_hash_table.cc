#include "gtest/gtest.h"
#include <thread>
#include <vector>
#include <atomic>

extern "C"
{
#include "hash_table.h"

#define BUCKET_NUMBER 101

// -----------------------------------------------------------
// 测试用
size_t hash_table_bucket_count_member(struct hash_table *self, size_t index);
size_t hash_table_bucket_count_node(struct hash_table *self, size_t index);
}

struct int_key_value
{
    unsigned int key;
};

int compare_hash_key(void *key1, void *key2)
{
    return !(*(unsigned int*)key1 == *(unsigned int*)key2);
}

void* get_hash_key(void *value)
{
    return &(((int_key_value*)value)->key);
}

size_t my_hash(void *key)
{
    return *((unsigned int*)key);
}

hash_table_common_operation int_hash_op = {
    .compare_hash_key = compare_hash_key,
    .get_hash_key = get_hash_key,
    .hash = my_hash
};

class test_int_hash : public ::testing::Test
{
protected:
    hash_table ht;

    void SetUp()
    {
        hash_table_init(&ht, &int_hash_op);
    }

    void TearDown()
    {
        hash_table_destroy(&ht);
    }
};

// TEST_F(test_int_hash, simpleCURDTest)
// {
//     int len;
//     printf("\ninput max element value for CURD test: ");
//     scanf("%d", &len);
//     int_key_value *arr = (int_key_value*)malloc(len * sizeof(int_key_value));
// //    ASSERT_NE(NULL, (void*)arr);

//     // 每个桶插入一个数
//     for (int i = 0; i < len; ++i)
//         arr[i].key = i;
//     for (int i = 0; i < len; ++i)
//     {
//         int res = hash_table_insert(&ht, &arr[i], NULL);
//         ASSERT_EQ(0, res);
//     }

//     // 查找每个数
//     for (int i = 0; i < len; ++i)
//     {
//         void *result = hash_table_lookup(&ht, &arr[i].key, NULL);
//         EXPECT_EQ(&arr[i].key, result);
//     }

//     for (int i = 0; i < len; ++i)
//     {
//         int res = hash_table_remove_use_key(&ht, &arr[i].key, NULL);
//         EXPECT_EQ(0, res);
//     }

//     for (int i = 0; i < BUCKET_NUMBER; ++i)
//     {
//         auto cnt = hash_table_bucket_count_member(&ht, i);
//         EXPECT_EQ(0, cnt);
//     }

//     free(arr);
// }

// TEST_F(test_int_hash, noLockCURDTest)
// {
//     int len;
//     printf("\ninput max element value for no lock CURD test: ");
//     scanf("%d", &len);
//     int_key_value *arr = (int_key_value*)malloc(len * sizeof(int_key_value));
// //    ASSERT_NE(NULL, (void*)arr);

//     // 每个桶插入一个数
//     for (int i = 0; i < len; ++i)
//         arr[i].key = i;
//     for (int i = 0; i < len; ++i)
//     {
//         int res = hash_table_insert_no_lock(&ht, &arr[i], NULL);
//         ASSERT_EQ(0, res);
//     }

//     // 查找每个数
//     for (int i = 0; i < len; ++i)
//     {
//         void *result = hash_table_lookup_no_lock(&ht, &arr[i].key, NULL);
//         EXPECT_EQ(&arr[i].key, result);
//     }

//     for (int i = 0; i < len; ++i)
//     {
//         int res = hash_table_remove_use_key_no_lock(&ht, &arr[i].key, NULL);
//         EXPECT_EQ(0, res);
//     }

//     for (int i = 0; i < BUCKET_NUMBER; ++i)
//     {
//         auto cnt = hash_table_bucket_count_member(&ht, i);
//         EXPECT_EQ(0, cnt);
//     }

//     free(arr);
// }

// TEST_F(test_int_hash, multikeyInOneBucketTest)
// {
//     printf("\ninput element number in one bucket: ");
//     int len;
//     scanf("%d", &len);
//     int_key_value *arr = new int_key_value[len];

//     // 全部都插入第0个桶
//     for (int i = 0; i < len; ++i)
//         arr[i].key = i * BUCKET_NUMBER;

//     for (int i = 0; i < len; ++i)
//     {
//         int res = hash_table_insert(&ht, &arr[i], NULL);
//         EXPECT_EQ(0, res);
//     }

//     size_t node_number = hash_table_bucket_count_node(&ht, 0);
//     size_t entry_number = hash_table_bucket_count_member(&ht, 0);
//     EXPECT_EQ(ROUND_UP(len, 7), node_number);
//     EXPECT_EQ(len, entry_number);

//     for (int i = 0; i < len; ++i)
//     {
//         void *res = hash_table_lookup(&ht, &arr[i].key, NULL);
//         EXPECT_EQ((void*)&arr[i], res);
//     }

//     for (int i = 0; i < len; ++i)
//     {
//         int res = hash_table_remove_use_key(&ht, &arr[i].key, NULL);
//         EXPECT_EQ(0, res);
//     }

//     node_number = hash_table_bucket_count_node(&ht, 0);
//     entry_number = hash_table_bucket_count_member(&ht, 0);
//     EXPECT_EQ(ROUND_UP(len, 7), node_number);
//     EXPECT_EQ(0, entry_number);

//     delete[] arr;
// }

struct int_count
{
    unsigned int key;
    std::atomic_int refcount;

    int_count()
    {
        refcount = 0;
    }
};

void* int_count_get_hash_key(void *value)
{
    return &(((int_count*)value)->key);
}

void get_int_count_instance(void *o)
{
    ++((int_count*)o)->refcount;
}

void release_int_count_instance(void *o)
{
    --((int_count*)o)->refcount;
}

hash_table_common_operation int_count_hash_op = {
    .compare_hash_key = compare_hash_key,
    .get_hash_key = int_count_get_hash_key,
    .hash = my_hash
};

class test_int_count_hash : public ::testing::Test
{
protected:
    hash_table ht;

    void SetUp()
    {
        hash_table_init(&ht, &int_count_hash_op);
    }

    void TearDown()
    {
        hash_table_destroy(&ht);
    }
};

TEST_F(test_int_count_hash, Concurrent_test)
{
    using namespace std;
    
    int_count *arr = new int_count[BUCKET_NUMBER];
    for (int i = 0; i < BUCKET_NUMBER; ++i)
        arr[i].key = i;
    
    auto insertArr = [&]()
    {
        for (int i = 0; i < BUCKET_NUMBER; ++i)
            hash_table_insert(&ht, &arr[i], get_int_count_instance);
    };

    int threads_number;
    printf("\nAll threads will do the same thing: insert, lookup and remove each element in arr.");
    printf("\nInput thread number: ");
    scanf("%d", &threads_number);

    vector<thread> threads(threads_number);

    // concurrent insert
    for (int i = 0; i < threads_number; ++i)
        threads[i] = std::move(thread(insertArr));
    for (int i = 0; i < threads_number; ++i)
        threads[i].join();
    
    // check insert
    for (int i = 0; i < BUCKET_NUMBER; ++i)
        EXPECT_EQ(1, hash_table_bucket_count_member(&ht, i));
    for (int i = 0; i < BUCKET_NUMBER; ++i)
        EXPECT_EQ(1, arr[i].refcount);
    
    // concurrent lookup
    auto lookup = [&]()
    {
        for (int i = 0; i < BUCKET_NUMBER; ++i)
            EXPECT_EQ(&arr[i], hash_table_lookup(&ht, &arr[i].key, NULL));
    };

    for (int i = 0; i < threads_number; ++i)
        threads[i] = std::move(thread(lookup));
    for (int i = 0; i < threads_number; ++i)
        threads[i].join();

    // concurrent remove
    auto remv = [&]()
    {
        for (int i = 0; i < BUCKET_NUMBER; ++i)
            hash_table_remove_use_key(&ht, &arr[i].key, release_int_count_instance);
    };
    for (int i = 0; i < threads_number; ++i)
        threads[i] = std::move(thread(remv));
    for (int i = 0; i < threads_number; ++i)
        threads[i].join();
    
    // check remove
    for (int i = 0; i < BUCKET_NUMBER; ++i)
        EXPECT_EQ(0, hash_table_bucket_count_member(&ht, i));
    for (int i = 0; i < BUCKET_NUMBER; ++i)
        EXPECT_EQ(0, arr[i].refcount);
    
    printf("\nOK!\n");
}