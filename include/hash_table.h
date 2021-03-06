#pragma once
#include <stddef.h>
#include "Util.h"

typedef struct hash_table_common_operation hash_table_common_operation;
typedef struct hash_table hash_table;

struct hash_table_common_operation {
    int (*compare_hash_key)(void *key1, void *key2);
    void *(*get_hash_key)(void *value);
    size_t (*hash)(void *key);
};

struct hash_table {
    struct hash_bucket_node *bucket_array;
    hash_table_common_operation *op;
    size_t bucket_array_size;
};

enum hash_operation_exceptions { alloc_fail = 1, insert_already_exist, remove_not_exist };

int hash_table_init(hash_table *self, size_t sz, hash_table_common_operation *op);

void hash_table_destroy(hash_table *self, void(*release_instance)(void*));

/* 在hash中查找关键码为key的对象
 *
 * 若找到且get_instance不为空
 * 则在找到的对象上回调get_instance
 * 
 * 查找成功返回对象的地址，否则返回NULL
*/
void* hash_table_lookup(hash_table *self, void *key, void(*get_instance)(void*));
void* hash_table_lookup_no_lock(hash_table *self, void *key, void(*get_instance)(void*));

/* 在hash表中插入value
 * 
 * 若插入成功且get_instance不为空
 * 则在value指向的对象上回调get_instance
 * 
 * 插入成功返回0，否则返回hash_operation_exceptions中的正整数
*/
int hash_table_insert(hash_table *self, void *value, void(*get_instance)(void*));
int hash_table_insert_no_lock(hash_table *self, void *value, void(*get_instance)(void*));

/* 移除关键码为key的对象
 * 
 * 若存在对象且release_instance不为空
 * 则在该对象上回调release_instance
 * 
 * 移除成功返回0，否则返回hash_operation_exceptions中的正整数
*/
int hash_table_remove_use_key(hash_table *self, void *key, void(*release_instance)(void*));
int hash_table_remove_use_key_no_lock(hash_table *self, void *key, void(*release_instance)(void*));