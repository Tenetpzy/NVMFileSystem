#pragma once
#include <stdatomic.h>
#include <stddef.h>
#include "Util.h"

typedef atomic_uchar lock_bitset_atomic_t;
typedef unsigned char lock_bitset_t;

#define CACHE_LINE_SIZE 64
#define ENTRY_NUMBER 7

// maybe a larger prime number...
#define HASH_TABLE_BUCKET_NUMBER 101

#define LOCK_BITSET_SIZE (sizeof(lock_bitset_atomic_t) * 8)
#define LOCK_BITSET_NUMBER ROUND_UP(HASH_TABLE_BUCKET_NUMBER, LOCK_BITSET_SIZE)

struct hash_bucket_node;

struct hash_bucket {
    struct hash_bucket_node *head;
};

struct hash_table_common_operation {
    int (*compare_hash_key)(void *key1, void *key2);
    void *(*get_hash_key)(void *value);
    size_t (*hash)(void *key);
};

struct hash_table {
    struct hash_bucket bucket_list[HASH_TABLE_BUCKET_NUMBER];

    /*
     * 保护bucket的锁
     * 可以一个bucket一个锁，也可以相邻多个bucket一个锁
     */
    lock_bitset_atomic_t mtx[LOCK_BITSET_NUMBER];

    struct hash_table_common_operation *op;
};

enum hash_operation_exceptions { alloc_bucket_node_fail = 1, bucket_lock_error, bucket_unlock_error };

void hash_table_init(struct hash_table *self, struct hash_table_common_operation *op);

void hash_table_destroy(struct hash_table *self);

// 获取关键字key在哈希表中的桶下标
size_t hash_table_get_bucket_index(struct hash_table *self, void *key);

// 在给定桶中查找并返回关键字为key的value，不存在返回NULL
// 若pvalue非空，将其置为桶中保存value的项的地址
void *hash_bucket_lookup_value(struct hash_table *self, size_t index, void *key, unsigned long *pvalue);

// 将value插入桶中（不判断value的key是否对应这个桶）
// 若pvalue非空，将其置为桶中保存value的项的地址
int hash_bucket_insert_value(struct hash_table *self, size_t index, void *value, unsigned long *pvalue);

// 删除桶结点中pentry指向的项
void hash_bucket_node_delete_entry(unsigned long pentry);

// 锁住下标为index的桶
void hash_table_lock_bucket(struct hash_table *self, size_t index);

void hash_table_unlock_bucket(struct hash_table *self, size_t index);