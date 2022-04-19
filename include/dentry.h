#pragma once

#include "Common.h"
#include "spin_lock.h"
#include "stdatomic.h"

typedef struct inode inode;

typedef struct dentry_list_node dentry_list_node;
struct dentry_list {
    dentry_list_node *head;
    spinlock_t lock;
};
typedef struct dentry_list dentry_list;

struct dentry_hash_key {
    ino_t fa_ino;
    const char *name;
};
typedef struct dentry_hash_key dentry_hash_key;

#define DENTRY_NAME_MAX_LEN 128

typedef struct dentry dentry;
struct dentry {
    atomic_llong ref_count;  // dentry引用计数

    inode *d_inode;  // 关联到的索引节点
    ino_t d_ino;            // 关联索引节点号

    dentry *father;           // 父目录项
    dentry_list sub_list;     // 子目录项链表
    dentry_list *fa_sub_list;  // 父目录项的子目录项链表

    dentry_hash_key hash_key;

    char name[DENTRY_NAME_MAX_LEN];
};

void dentry_inc_count(dentry*);
void dentry_dec_count(dentry*);

dentry* alloc_dentry();
void free_dentry(dentry* dt);

int dentry_hash_table_init();
int dentry_hash_table_insert(dentry*);
dentry* dentry_hash_table_lookup(dentry_hash_key*);
int dentry_hash_table_remove(dentry*);