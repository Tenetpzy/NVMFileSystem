#include "hash_table.h"
#include <malloc.h>

// typedef uint8_t bitset_t;

struct hash_bucket_node {
    void *value[ENTRY_NUMBER];
    struct hash_bucket_node *next;
};

// #define set_bit(s, n) (s) |= ((bitset_t)1 << (n))
// #define reset_bit(s, n) (s) &= (~((bitset_t)1 << (n)))
// #define is_set_bit(s, n) ((s) & ((bitset_t)1 << (n)))
// #define is_reset_bit(s, n) (!((s) & ((bitset_t)1 << (n))))

struct hash_bucket_node *alloc_bucket_node() {
    // 之后可能不用malloc，自己管理缓存池
    return memalign(CACHE_LINE_SIZE, sizeof(struct hash_bucket_node));
}

void free_bucket_node(struct hash_bucket_node *self) {
    // 之后可能不用malloc，此处也不调free
    free(self);
}

void bucket_node_init(struct hash_bucket_node *self) {
    self->next = NULL;
    for (int i = 0; i < ENTRY_NUMBER; ++i) self->value[i] = NULL;
}

int bucket_node_get_next_used_entry(struct hash_bucket_node *self, int *entry) {
    int index = *entry + 1;
    for (; index < ENTRY_NUMBER; ++index)
        if (!IS_NULL_PTR(self->value[index])) break;
    *entry = index;
    return index == ENTRY_NUMBER ? -1 : 0;
}

int bucket_node_get_unused_entry(struct hash_bucket_node *self) {
    int entry = 0;
    for (; entry < ENTRY_NUMBER; ++entry)
        if (IS_NULL_PTR(self->value[entry])) break;
    return entry == ENTRY_NUMBER ? -1 : entry;
}

static inline void bucket_node_set_entry(struct hash_bucket_node *self, int entry, void *value) {
    self->value[entry] = value;
}

// 给定指针value，返回它所属bucket_node的地址
static inline unsigned long value_ptr_to_bucket_node_ptr(unsigned long value) {
    unsigned long mask = ~((unsigned long)CACHE_LINE_SIZE - 1);
    return value & mask;
}

// 给定指向node中value数组元素的指针value，返回该元素下标
static inline int value_ptr_to_bucket_node_value_index(unsigned long value) {
    return (value - value_ptr_to_bucket_node_ptr(value)) / sizeof(void *);
}

void hash_bucket_node_delete_entry(unsigned long pentry) {
    int index = value_ptr_to_bucket_node_value_index(pentry);
    struct hash_bucket_node *node = (struct hash_bucket_node *)value_ptr_to_bucket_node_ptr(pentry);
    bucket_node_set_entry(node, index, NULL);
}

static inline void hash_bucket_insert_node(struct hash_bucket *self, struct hash_bucket_node *node) {
    node->next = self->head;
    self->head = node;
}

void free_hash_bucket(struct hash_bucket *self) {
    for (;;) {
        struct hash_bucket_node *node = self->head;
        if (node == NULL) break;
        self->head = node->next;
        free_bucket_node(node);
    }
}

void hash_table_init(struct hash_table *self, struct hash_table_common_operation *op) {
    self->op = op;
    for (size_t i = 0; i < HASH_TABLE_BUCKET_NUMBER; ++i) self->bucket_list[i].head = NULL;
    for (size_t i = 0; i < LOCK_BITSET_NUMBER; ++i) atomic_init(&self->mtx[i], (lock_bitset_t)0);
}

void hash_table_destroy(struct hash_table *self) {
    for (size_t i = 0; i < HASH_TABLE_BUCKET_NUMBER; ++i) free_hash_bucket(&self->bucket_list[i]);
}

void hash_table_lock_bucket(struct hash_table *self, size_t index) {
    size_t idx = index / LOCK_BITSET_SIZE;
    lock_bitset_t bitidx = index & (LOCK_BITSET_SIZE - 1);
    lock_bitset_t set_mask = (lock_bitset_t)1 << bitidx;
    lock_bitset_t not_set_mask = ~set_mask;
    for (;;) {
        lock_bitset_t expect = self->mtx[idx];
        lock_bitset_t target = self->mtx[idx];
        expect &= not_set_mask;
        target |= set_mask;
        if (atomic_compare_exchange_weak(&self->mtx[idx], &expect, target)) break;
    }
}

void hash_table_unlock_bucket(struct hash_table *self, size_t index) {
    size_t idx = index / LOCK_BITSET_SIZE;
    lock_bitset_t bitidx = index & (LOCK_BITSET_SIZE - 1);
    lock_bitset_t mask = ~((lock_bitset_t)1 << bitidx);
    atomic_fetch_and(&self->mtx[idx], mask);
}

size_t hash_table_get_bucket_index(struct hash_table *self, void *key) {
    return self->op->hash(key) % HASH_TABLE_BUCKET_NUMBER;
}

void *hash_bucket_lookup_value(struct hash_table *self, size_t index, void *key, unsigned long *pvalue) {
    void *result = NULL;
    struct hash_bucket_node *node = self->bucket_list[index].head;
    int entry = -1;
    while (node != NULL)  // 遍历冲突链中所有的BucketNode
    {
        // 找到当前node中下一个被占用的项
        int res = bucket_node_get_next_used_entry(node, &entry);
        if (res == -1)  // 当前BucketNode中已经找完了，在下一个node中继续找
        {
            node = node->next;
            entry = -1;
            continue;
        }
        void *entry_key = self->op->get_hash_key(node->value[entry]);
        if (self->op->compare_hash_key(entry_key, key) == 0)  // 比较key, 返回0表示相等，即找到目标
        {
            result = node->value[entry];
            if (pvalue != NULL) *pvalue = (unsigned long)&(node->value[entry]);
            break;
        }
    }
    return result;
}

int hash_bucket_insert_value(struct hash_table *self, size_t index, void *value, unsigned long *pvalue) {
    struct hash_bucket_node *node = self->bucket_list[index].head;
    for (;;) {
        if (node == NULL) {
            node = alloc_bucket_node();
            if (node == NULL) return alloc_bucket_node_fail;
            bucket_node_init(node);
            hash_bucket_insert_node(&self->bucket_list[index], node);
        }
        int index = bucket_node_get_unused_entry(node);
        if (index == -1) {
            node = node->next;
            continue;
        }
        bucket_node_set_entry(node, index, value);
        if (pvalue != NULL) *pvalue = (unsigned long)&node->value[index];
        break;
    }
    return 0;
}