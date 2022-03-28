#include "dentry.h"
#include <string.h>
#include "hash_table.h"

#define TO_HASH_KEY_PTR(x) ((struct dentry_hash_key *)x)

int dentry_compare_hash_key(void *key1, void *key2) {
    return !((TO_HASH_KEY_PTR(key1)->fa_ino == TO_HASH_KEY_PTR(key2)->fa_ino) &&
             (strcmp(TO_HASH_KEY_PTR(key1)->name, TO_HASH_KEY_PTR(key2)->name) == 0));
}

void *dentry_get_hash_key(void *dt) { return &((struct dentry *)dt)->hash_key; }

size_t dentry_hash(void *key) {}

struct hash_table dentry_hash_table;
struct hash_table_common_operation dentry_hash_table_operation = {
    .compare_hash_key = dentry_compare_hash_key, .get_hash_key = dentry_get_hash_key, .hash = dentry_hash};

int dentry_insert_hash_table(struct dentry *dentry) {
    void *key = dentry_get_hash_key(dentry);
    size_t bucket = hash_table_get_bucket_index(&dentry_hash_table, key);
    hash_table_lock_bucket(&dentry_hash_table, bucket);
    if (hash_bucket_lookup_value(&dentry_hash_table, bucket, key, NULL) != NULL) {
        hash_table_unlock_bucket(&dentry_hash_table, bucket);
        return insert_already_exist;
    }
    int res = hash_bucket_insert_value(&dentry_hash_table, bucket, dentry, &dentry->hash_pos_ptr);
    hash_table_unlock_bucket(&dentry_hash_table, bucket);
    return res;
}