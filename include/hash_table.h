#pragma once

#define CACHE_LINE_SIZE 64
#define ENTRY_NUMBER 6
#define HASH_TABLE_SIZE 101

struct hash_bucket_node;
struct hash_bucket;

struct hash_table {
	struct hash_bucket bucket_list[HASH_TABLE_SIZE];

	/* 
	* 保护bucket的锁
	* 可以一个bucket一个锁，也可以相邻多个bucket一个锁
	*/

	int (*compare_key)(void *key1, void *key2);
};

// 获取关键字key在哈希表中的桶
struct hash_bucket *hash_table_get_bucket(struct hash_table *self, void *key);

// 在桶中查找关键字为key的值
void *hash_bucket_lookup_value(struct hash_bucket *self, void *key, int (*comparator)(void *, void *));

// 在桶中删除关键字为key的元素
int hash_bucket_delete_value(struct hash_bucket *self, void *key, int (*comparator)(void *, void *));

// 将value插入桶hb中（不判断value的key是否对应这个桶）
void hash_bucket_insert_value(struct hash_bucket *self, void *value);

// 删除桶结点中下标为entry的项
void hash_bucket_node_delete_entry(struct hash_bucket_node *hbn, int entry);