#include "hash_table.h"
#include <stdatomic.h>
#include <malloc.h>

typedef struct hash_bucket_node hash_bucket_node;
typedef _Atomic(hash_bucket_node*) atomic_bucket_node_ptr;

#define CACHE_LINE_SIZE 64
#define BUCKET_ENTRY_NUMBER (CACHE_LINE_SIZE - sizeof(atomic_bucket_node_ptr)) / sizeof(void*)

struct hash_bucket_node {
    void *value[BUCKET_ENTRY_NUMBER];
    atomic_bucket_node_ptr next;
};


static hash_bucket_node *alloc_bucket_node() 
{
    // 之后可能不用malloc，自己管理缓存池
    return memalign(CACHE_LINE_SIZE, sizeof(hash_bucket_node));
}

static void free_bucket_node(hash_bucket_node *self) 
{
    // 之后可能不用malloc，此处也不调free
    free(self);
}

static hash_bucket_node* alloc_bucket_array(size_t sz)
{
    return (hash_bucket_node*)memalign(CACHE_LINE_SIZE, sizeof(hash_bucket_node) * sz);
}

static void free_bucket_array(hash_bucket_node *bucket_array)
{
    free(bucket_array);
}


static void bucket_node_init(hash_bucket_node *self) 
{
    atomic_store(&self->next, NULL);
    for (int i = 0; i < BUCKET_ENTRY_NUMBER; ++i) self->value[i] = NULL;
}

static int bucket_node_get_next_used_entry(hash_bucket_node *self, int *entry) 
{
    int index = *entry + 1;
    for (; index < BUCKET_ENTRY_NUMBER; ++index)
        if (!IS_NULL_PTR(self->value[index])) break;
    *entry = index;
    return index == BUCKET_ENTRY_NUMBER ? -1 : 0;
}

static int bucket_node_get_unused_entry(hash_bucket_node *self) 
{
    int entry = 0;
    for (; entry < BUCKET_ENTRY_NUMBER; ++entry)
        if (IS_NULL_PTR(self->value[entry])) break;
    return entry == BUCKET_ENTRY_NUMBER ? -1 : entry;
}

static void bucket_node_call_func_on_every_entry(hash_bucket_node *self, void(*func)(void*))
{
    int entry = 0;
    for (; entry < BUCKET_ENTRY_NUMBER; ++entry)
        if (!IS_NULL_PTR(self->value[entry]))
            func(self->value[entry]);
}

static inline void bucket_node_set_entry(hash_bucket_node *self, int entry, void *value) 
{
    self->value[entry] = value;
}

static inline void bucket_node_clear_entry(unsigned long entry_ptr)
{
    *(void**)entry_ptr = NULL;
}

static inline hash_bucket_node* bucket_node_get_next_ptr(hash_bucket_node *self) 
{
    unsigned long mask_high_bit = ~(1UL << 63);
    return (hash_bucket_node*)((unsigned long)self->next & mask_high_bit);
}


int hash_table_init(hash_table *self, size_t sz, hash_table_common_operation *op)
{
    self->bucket_array = alloc_bucket_array(sz);
    self->op = op;
    self->bucket_array_size = sz;
    if (self->bucket_array == NULL)
        return alloc_fail;
    for (int i = 0; i < sz; ++i)
        bucket_node_init(&self->bucket_array[i]);
    return 0;
}

static void hash_table_destroy_without_release_instance(hash_table *self)
{
    for (size_t i = 0; i < self->bucket_array_size; ++i)
    {
        hash_bucket_node *node = bucket_node_get_next_ptr(&self->bucket_array[i]);
        while (node != NULL)
        {
            hash_bucket_node *next = bucket_node_get_next_ptr(node);
            free_bucket_node(node);
            node = next;
        }
    }
    
    free_bucket_array(self->bucket_array);   
}

void hash_table_destroy(hash_table *self, void(*release_instance)(void*))
{
    if (release_instance == NULL)
    {
        hash_table_destroy_without_release_instance(self);
        return;
    }
    
    for (size_t i = 0; i < self->bucket_array_size; ++i)
    {
        bucket_node_call_func_on_every_entry(&self->bucket_array[i], release_instance);

        hash_bucket_node *node = bucket_node_get_next_ptr(&self->bucket_array[i]);
        while (node != NULL)
        {
            hash_bucket_node *next = bucket_node_get_next_ptr(node);
            bucket_node_call_func_on_every_entry(node, release_instance);
            free_bucket_node(node);
            node = next;
        }
    }
    
    free_bucket_array(self->bucket_array);
}


static void hash_bucket_lock(hash_bucket_node *self) 
{
    unsigned long high_bit = 1UL << 63;
    for (;;) 
    {
        unsigned long expect = (unsigned long)atomic_load(&self->next) & (~high_bit);
        unsigned long target = (unsigned long)expect | high_bit;
        if (atomic_compare_exchange_weak(&self->next, (hash_bucket_node**)&expect, (hash_bucket_node*)target)) 
            break;
    }
}

static void hash_bucket_unlock(hash_bucket_node *self) 
{
    ptrdiff_t mask_high_bit = ~(1UL << 63);
    atomic_fetch_and(&self->next, mask_high_bit);
}


static void hash_bucket_insert_node(hash_bucket_node *self, hash_bucket_node *node) 
{
    unsigned long high_bit = ((unsigned long)self->next >> 63) << 63;
    unsigned long mask_high_bit = ~(1UL << 63);
    node->next = (atomic_bucket_node_ptr)((unsigned long)self->next & mask_high_bit);
    self->next = node;
    self->next = (atomic_bucket_node_ptr)((unsigned long)self->next | high_bit);
}

static void *hash_bucket_lookup_value(hash_table *self, size_t index, void *key, unsigned long *pvalue) 
{
    void *result = NULL;
    hash_bucket_node *node = &self->bucket_array[index];

    int entry = -1;
    while (node != NULL)  // 遍历冲突链中所有的BucketNode
    {
        // 找到当前node中下一个被占用的项
        int res = bucket_node_get_next_used_entry(node, &entry);
        if (res == -1)  // 当前BucketNode中已经找完了，在下一个node中继续找
        {
            node = bucket_node_get_next_ptr(node);
            entry = -1;
            continue;
        }

        void *entry_key = self->op->get_hash_key(node->value[entry]);
        if (self->op->compare_hash_key(entry_key, key) == 0)  // 比较key, 返回0表示相等，即找到目标
        {
            result = node->value[entry];
            if (pvalue != NULL)
                *pvalue = (unsigned long)&node->value[entry];
            break;
        }
    }
    return result;
}

static int hash_bucket_insert_value(hash_table *self, size_t index, void *value, unsigned long *pvalue) 
{
    hash_bucket_node *node = &self->bucket_array[index];
    for (;;) 
    {
        int entry = bucket_node_get_unused_entry(node);
        if (entry == -1) 
        {
            node = bucket_node_get_next_ptr(node);
            if (node == NULL) 
            {
                node = alloc_bucket_node();
                if (node == NULL) return alloc_fail;
                bucket_node_init(node);
                hash_bucket_insert_node(&self->bucket_array[index], node);
            }
            continue;
        }
        bucket_node_set_entry(node, entry, value);
        if (pvalue != NULL) *pvalue = (unsigned long)&node->value[entry];
        break;
    }
    return 0;
}

void* hash_table_lookup(hash_table *self, void *key, void(*get_instance)(void*)) 
{
    size_t index = self->op->hash(key) % self->bucket_array_size;
    hash_bucket_lock(&self->bucket_array[index]);

    void *result = hash_bucket_lookup_value(self, index, key, NULL);
    if (result != NULL && get_instance != NULL)
        get_instance(result);

    hash_bucket_unlock(&self->bucket_array[index]);
    return result;
}

void* hash_table_lookup_no_lock(hash_table *self, void *key, void(*get_instance)(void*))
{
    size_t index = self->op->hash(key) % self->bucket_array_size;
    
    void *result = hash_bucket_lookup_value(self, index, key, NULL);
    if (result != NULL && get_instance != NULL)
        get_instance(result);
    
    return result;
}

int hash_table_insert(hash_table *self, void *value, void(*get_instance)(void*))
{
    void *key = self->op->get_hash_key(value);
    size_t index = self->op->hash(key) % self->bucket_array_size;

    hash_bucket_lock(&self->bucket_array[index]);

    // 先查找关键码为key的对象是否已存在
    void *res = hash_bucket_lookup_value(self, index, key, NULL);
    if (res != NULL)
    {
        hash_bucket_unlock(&self->bucket_array[index]);
        return insert_already_exist;
    }

    // 有可能分配空间失败，将插入结果保存在result中
    int result = hash_bucket_insert_value(self, index, value, NULL);
    if (get_instance != NULL && result == 0)
        get_instance(value);

    hash_bucket_unlock(&self->bucket_array[index]);
    return result;
}

int hash_table_insert_no_lock(hash_table *self, void *value, void(*get_instance)(void*))
{
    void *key = self->op->get_hash_key(value);
    size_t index = self->op->hash(key) % self->bucket_array_size;

    void *res = hash_bucket_lookup_value(self, index, key, NULL);
    if (res != NULL)
        return insert_already_exist;

    int result = hash_bucket_insert_value(self, index, value, NULL);
    if (get_instance != NULL && result == 0)
        get_instance(value);

    return result;
}

int hash_table_remove_use_key(hash_table *self, void *key, void(*release_instance)(void*))
{
    size_t index = self->op->hash(key) % self->bucket_array_size;
    hash_bucket_lock(&self->bucket_array[index]);

    unsigned long pvalue;
    void *result = hash_bucket_lookup_value(self, index, key, &pvalue);
    if (result == NULL)
    {
        hash_bucket_unlock(&self->bucket_array[index]);
        return remove_not_exist;
    }

    bucket_node_clear_entry(pvalue);
    if (release_instance != NULL)
        release_instance(result);

    hash_bucket_unlock(&self->bucket_array[index]);
    return 0;
}

int hash_table_remove_use_key_no_lock(hash_table *self, void *key, void(*release_instance)(void*))
{
    size_t index = self->op->hash(key) % self->bucket_array_size;

    unsigned long pvalue;
    void *result = hash_bucket_lookup_value(self, index, key, &pvalue);
    if (result == NULL)
        return remove_not_exist;

    bucket_node_clear_entry(pvalue);
    if (release_instance != NULL)
        release_instance(result);    

    return 0;
}


// -------------------------------------------------------------------------
// debug used

size_t hash_table_bucket_count_member(hash_table *self, size_t index)
{
    hash_bucket_lock(&self->bucket_array[index]);

    size_t result = 0;
    hash_bucket_node *node = &self->bucket_array[index];

    while (node != NULL)
    {
        int entry = -1;
        while (bucket_node_get_next_used_entry(node, &entry) == 0)
            ++result;
        node = bucket_node_get_next_ptr(node);
    }

    hash_bucket_unlock(&self->bucket_array[index]);

    return result;
}

size_t hash_table_bucket_count_node(hash_table *self, size_t index)
{
    hash_bucket_lock(&self->bucket_array[index]);

    size_t result = 0;
    hash_bucket_node *node = &self->bucket_array[index];

    while (node != NULL)
    {
        ++result;
        node = bucket_node_get_next_ptr(node);
    }

    hash_bucket_unlock(&self->bucket_array[index]);

    return result;
}