#include "gtest/gtest.h"

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

TEST_F(test_int_hash, operationEachOneTest)
{
    int_key_value x;
    x.key = 1;

    hash_table_insert(&ht, &x, NULL);
    void *result = hash_table_lookup(&ht, &x.key, NULL);
    EXPECT_EQ(&x, result);
    int res = hash_table_remove_use_key(&ht, &x.key, NULL);
    EXPECT_EQ(0, res);
}

TEST_F(test_int_hash, simpleCURDTest)
{
    int len;
    printf("\ninput max element value for CURD test: ");
    scanf("%d", &len);
    int_key_value *arr = (int_key_value*)malloc(len * sizeof(int_key_value));
//    ASSERT_NE(NULL, (void*)arr);

    // 每个桶插入一个数
    for (int i = 0; i < len; ++i)
        arr[i].key = i;
    for (int i = 0; i < len; ++i)
    {
        int res = hash_table_insert(&ht, &arr[i], NULL);
        ASSERT_EQ(0, res);
    }

    // 查找每个数
    for (int i = 0; i < len; ++i)
    {
        void *result = hash_table_lookup(&ht, &arr[i].key, NULL);
        EXPECT_EQ(&arr[i].key, result);
    }

    for (int i = 0; i < len; ++i)
    {
        int res = hash_table_remove_use_key(&ht, &arr[i].key, NULL);
        EXPECT_EQ(0, res);
    }

    for (int i = 0; i < BUCKET_NUMBER; ++i)
    {
        auto cnt = hash_table_bucket_count_member(&ht, i);
        EXPECT_EQ(0, cnt);
    }

    free(arr);
}

TEST_F(test_int_hash, multikeyInOneBucketTest)
{
    printf("\ninput element number in one bucket: ");
    int len;
    scanf("%d", &len);
    int_key_value *arr = new int_key_value[len];

    // 全部都插入第0个桶
    for (int i = 0; i < len; ++i)
        arr[i].key = i * BUCKET_NUMBER;

    for (int i = 0; i < len; ++i)
    {
        int res = hash_table_insert(&ht, &arr[i], NULL);
        EXPECT_EQ(0, res);
    }

    size_t node_number = hash_table_bucket_count_node(&ht, 0);
    size_t entry_number = hash_table_bucket_count_member(&ht, 0);
    EXPECT_EQ(ROUND_UP(len, 7), node_number);
    EXPECT_EQ(len, entry_number);

    for (int i = 0; i < len; ++i)
    {
        void *res = hash_table_lookup(&ht, &arr[i].key, NULL);
        EXPECT_EQ((void*)&arr[i], res);
    }

    for (int i = 0; i < len; ++i)
    {
        int res = hash_table_remove_use_key(&ht, &arr[i].key, NULL);
        EXPECT_EQ(0, res);
    }

    node_number = hash_table_bucket_count_node(&ht, 0);
    entry_number = hash_table_bucket_count_member(&ht, 0);
    EXPECT_EQ(ROUND_UP(len, 7), node_number);
    EXPECT_EQ(0, entry_number);

    delete[] arr;
}
