#pragma once

#include <stdint.h>
#include <pthread.h>

#define CACHE_LINE_SIZE 64
#define ENTRY_NUMBER 6
#define HASH_TABLE_SIZE 101

struct hash_bucket_node;
struct hash_bucket
{
    struct hash_bucket_node *head;
};

typedef uint32_t bucket_index_t;

struct hash_table
{
	struct hash_bucket bucket_list[HASH_TABLE_SIZE];

	/*
	 * 保护bucket的锁
	 * 可以一个bucket一个锁，也可以相邻多个bucket一个锁
	 */
	pthread_mutex_t mtx[HASH_TABLE_SIZE];

	int (*compare_hash_key)(void *key1, void *key2);
	void *(*get_hash_key)(void *value);
	bucket_index_t (*hash)(void *key);
};

enum hash_operation_exceptions
{
	alloc_bucket_node_fail = 1, bucket_lock_error, bucket_unlock_error
};

void hash_table_init(struct hash_table *self, int (*cmp)(void *, void *), void *(*getkey)(void *), bucket_index_t (*hash_fun)(void *));

// 获取关键字key在哈希表中的桶
struct hash_bucket *hash_table_get_bucket(struct hash_table *self, void *key);

// 在给定桶中查找并返回关键字为key的value，不存在返回NULL
// 若pvalue非空，则将其置为value的地址
void *hash_bucket_lookup_value(struct hash_table *self, struct hash_bucket *hb, void *key, unsigned long *pvalue);

// 将value插入桶中（不判断value的key是否对应这个桶）
int hash_bucket_insert_value(struct hash_bucket *self, void *value);

// 删除桶结点中pentry指向的项
void hash_bucket_node_delete_entry(unsigned long pentry);

// 锁住下标为index的桶
int hash_table_lock_bucket(struct hash_table *self, int index);

int hash_table_unlock_bucket(struct hash_table *self, int index);