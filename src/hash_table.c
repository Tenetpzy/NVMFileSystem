#include "hash_table.h"
#include <malloc.h>

typedef uint8_t bitset_t;

struct hash_bucket_node
{
    void *value[ENTRY_NUMBER];
    struct hash_bucket_node *next;
    bitset_t bitset; // 1:可用 0:已占用
} __attribute__((aligned(CACHE_LINE_SIZE)));

#define set_bit(s, n) (s) |= ((bitset_t)1 << (n))
#define reset_bit(s, n) (s) &= (~((bitset_t)1 << (n)))
#define is_set_bit(s, n) ((s) & ((bitset_t)1 << (n)))
#define is_reset_bit(s, n) (!((s) & ((bitset_t)1 << (n))))

struct hash_bucket_node* alloc_bucket_node()
{
    // 之后可能不用malloc，自己管理缓存池
    return memalign(CACHE_LINE_SIZE, sizeof(struct hash_bucket_node));
}

void free_bucket_node(struct hash_bucket_node* self)
{
    // 之后可能不用malloc，此处也不调free
    free(self);
}

void bucket_node_init(struct hash_bucket_node *self)
{
    self->bitset = 0;
    self->next = NULL;
    for (int i = 0; i < ENTRY_NUMBER; ++i)
    {
        self->value[i] = NULL;
        set_bit(self->bitset, i);
    }
}

int bucket_node_get_next_used_entry(struct hash_bucket_node *self, int *entry)
{
    int index = *entry + 1;
    for (; index < ENTRY_NUMBER; ++index)
        if (is_reset_bit(self->bitset, index))
            break;
    *entry = index;
    return index == ENTRY_NUMBER ? -1 : 0;
}

int bucket_node_get_unused_entry(struct hash_bucket_node *self)
{
    int entry = 0;
    for (; entry < ENTRY_NUMBER; ++entry)
        if (is_set_bit(self->bitset, entry))
            break;
    return entry == ENTRY_NUMBER ? -1 : entry;
}

static inline void bucket_node_set_entry(struct hash_bucket_node *self, int entry, void *value)
{
    reset_bit(self->bitset, entry);
    self->value[entry] = value;
}

// 给定指针value，返回它所属bucket_node的地址
static inline unsigned long value_ptr_to_bucket_node_ptr(unsigned long value)
{
    unsigned long mask = ~((unsigned long)CACHE_LINE_SIZE - 1);
    return value & mask;
}

// 给定指向node中value数组元素的指针value，返回该元素下标
static inline int value_ptr_to_bucket_node_value_index(unsigned long value)
{
    return (value - value_ptr_to_bucket_node_ptr(value)) / sizeof(void *);
}

void hash_bucket_node_delete_entry(unsigned long pentry)
{
    int index = value_ptr_to_bucket_node_value_index(pentry);
    struct hash_bucket_node *node = (struct hash_bucket_node*)value_ptr_to_bucket_node_ptr(pentry);
    set_bit(node->bitset, index);
    node->value[index] = NULL;
}

static inline void hash_bucket_insert_node(struct hash_bucket *self, struct hash_bucket_node *node)
{
    node->next = self->head;
    self->head = node;
}

void free_hash_bucket(struct hash_bucket *self)
{
    for (;;)
    {
        struct hash_bucket_node *node = self->head;
        if (node == NULL)
            break;
        self->head = node->next;
        free_bucket_node(node);
    }
}

void hash_table_init(struct hash_table *self, int (*cmp)(void *, void *), void *(*getkey)(void *), bucket_index_t (*hash_fun)(void *))
{
    self->compare_hash_key = cmp;
    self->get_hash_key = getkey;
    self->hash = hash_fun;
    for (int i = 0; i < HASH_TABLE_SIZE; ++i)
    {
        self->bucket_list[i].head = NULL;
        pthread_mutex_init(&self->mtx[i], NULL);
    }
}

int hash_table_lock_bucket(struct hash_table *self, int index)
{
    int ret = pthread_mutex_lock(&self->mtx[index]);
    return ret == 0 ? 0 : bucket_lock_error;
}

int hash_table_unlock_bucket(struct hash_table *self, int index)
{
    int ret = pthread_mutex_unlock(&self->mtx[index]);
    return ret == 0 ? 0 : bucket_unlock_error;
}

struct hash_bucket *hash_table_get_bucket(struct hash_table *self, void *key)
{
    bucket_index_t index = self->hash(key) % HASH_TABLE_SIZE;
    return &(self->bucket_list[index]);
}

void *hash_bucket_lookup_value(struct hash_table *self, struct hash_bucket *hb, void *key, unsigned long *pvalue)
{
    void *result = NULL;
    struct hash_bucket_node *node = hb->head;
    int entry = -1;
    while (node != NULL) // 遍历冲突链中所有的BucketNode
    {
        // 找到当前node中下一个被占用的项
        int res = bucket_node_get_next_used_entry(node, &entry);
        if (res == -1) // 当前BucketNode中已经找完了，在下一个node中继续找
        {
            node = node->next;
            entry = -1;
            continue;
        }
        void *entry_key = self->get_hash_key(node->value[entry]);
        if (self->compare_hash_key(entry_key, key) == 0) // 比较key, 返回0表示相等，即找到目标
        {
            result = node->value[entry];
            if (pvalue != NULL)
                *pvalue = (unsigned long)&(node->value[entry]);
            break;
        }
    }
    return result;
}

int hash_bucket_insert_value(struct hash_bucket *self, void *value)
{
    struct hash_bucket_node *node = self->head;
    for (;;)
    {
        if (node == NULL)
        {
            node = alloc_bucket_node();
            if (node == NULL)
                return alloc_bucket_node_fail;
            bucket_node_init(node);
            hash_bucket_insert_node(self, node);
        }
        int index = bucket_node_get_unused_entry(node);
        if (index == -1)
        {
            node = node->next;
            continue;
        }
        bucket_node_set_entry(node, index, value);
        break;
    }
    return 0;
}