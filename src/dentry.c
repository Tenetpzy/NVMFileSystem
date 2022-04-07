#include "dentry.h"
#include "hash_table.h"
#include <string.h>
#include <stdlib.h>

/* *********************** */
/* dentry hash table BEGIN */

#define TO_HASH_KEY_PTR(x) ((struct dentry_hash_key *)x)

static int dentry_compare_hash_key(void *key1, void *key2) 
{
    return !((TO_HASH_KEY_PTR(key1)->fa_ino == TO_HASH_KEY_PTR(key2)->fa_ino) &&
             (strcmp(TO_HASH_KEY_PTR(key1)->name, TO_HASH_KEY_PTR(key2)->name) == 0));
}

static void *dentry_get_hash_key(void *dt) 
{ 
    return &((struct dentry *)dt)->hash_key; 
}

static size_t dentry_hash(void *key) 
{
    return 0;
}

// 全局的：dentry的哈希表与特定操作
struct hash_table dentry_hash_table;
struct hash_table_common_operation dentry_hash_table_operation = {
    .compare_hash_key = dentry_compare_hash_key, 
    .get_hash_key = dentry_get_hash_key, 
    .hash = dentry_hash
};

static void get_dentry_instance(void *dt)
{
    dentry_inc_count((struct dentry*)dt);
}

static void release_dentry_instance(void *dt)
{
    dentry_dec_count((struct dentry*)dt);
}

int dentry_hash_table_init()
{
    return hash_table_init(&dentry_hash_table, &dentry_hash_table_operation);
}

int dentry_hash_table_insert(struct dentry* dt)
{
    return hash_table_insert(&dentry_hash_table, dt, get_dentry_instance);
}

struct dentry* dentry_hash_table_lookup(struct dentry_hash_key* key)
{
    return (struct dentry*)hash_table_lookup(&dentry_hash_table, key, get_dentry_instance);
}

int dentry_hash_table_remove(struct dentry* dt)
{
    return hash_table_remove_use_key(&dentry_hash_table, dentry_get_hash_key(dt), release_dentry_instance);
}

/* dentry hash table END */
/* ********************* */

void dentry_inc_count(struct dentry* dt)
{
    ++dt->ref_count;
}

void dentry_dec_count(struct dentry* dt)
{
    --dt->ref_count;
}


struct dentry* alloc_dentry()
{
    return (struct dentry*)malloc(sizeof(struct dentry));
}

void free_dentry(struct dentry* dt)
{
    free(dt);
}

